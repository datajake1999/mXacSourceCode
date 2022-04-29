/*************************************************************************************
CMLexicon.cpp - Code for lexicon object

begun 19/9/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"


// LUPT - Toekn parse structure
typedef struct {
   WCHAR          szToken[16];   // token string
   DWORD          dwLen;         // number of characters in szToken
   DWORD          dwMajorID;     // major ID, LUPTMAJOR_XXX
   DWORD          dwMinorID;     // minor ID, used only for phonemes as they're added
} LUPT, *PLUPT;

#define LUPTMAJOR_PHONESTART     0       // where parts of speech starts
#define LUPTMAJOR_PHONESTOP      (LUPTMAJOR_PHONESTART+10)       // where parts of speech starts

#define LUPTMAJOR_PHONESTRESS    (LUPTMAJOR_PHONESTART+0)        // store stressed phonemes
#define LUPTMAJOR_PHONENOSTRESS  (LUPTMAJOR_PHONESTART+1)        // phoneme with no stress
#define LUPTMAJOR_PHONEFINAL     (LUPTMAJOR_PHONESTART+2)        // final phone for chinese
#define LUPTMAJOR_STRESS0        (LUPTMAJOR_PHONESTART+3)        // primary stress
#define LUPTMAJOR_STRESS1        (LUPTMAJOR_PHONESTART+4)        // secondary stress (doesn't exist if remapping to primary stress)
#define LUPTMAJOR_SYLLABLE       (LUPTMAJOR_PHONESTART+5)        // syllable boundary
#define LUPTMAJOR_IGNORE         (LUPTMAJOR_PHONESTART+6)        // symbol to ignore

#define LUPTMAJOR_POSSTART       LUPTMAJOR_PHONESTOP       // where parts of speech starts
#define LUPTMAJOR_POSSTTOP       (LUPTMAJOR_POSSTART + 10)       // where parts of speech starts

#define LUPTMAJOR_POSNOUN        (LUPTMAJOR_POSSTART+0)       // noun
#define LUPTMAJOR_POSPRONOUN     (LUPTMAJOR_POSSTART+1)       // pronoun
#define LUPTMAJOR_POSADJECTIVE   (LUPTMAJOR_POSSTART+2)       // adjective
#define LUPTMAJOR_POSPREPOSITION (LUPTMAJOR_POSSTART+3)       // prep
#define LUPTMAJOR_POSARTICLE     (LUPTMAJOR_POSSTART+4)       // article
#define LUPTMAJOR_POSVERB        (LUPTMAJOR_POSSTART+5)       // verb
#define LUPTMAJOR_POSADVERB      (LUPTMAJOR_POSSTART+6)       // adverb
#define LUPTMAJOR_POSAUXVERB     (LUPTMAJOR_POSSTART+7)       // auxverb
#define LUPTMAJOR_POSCONJUNCTION (LUPTMAJOR_POSSTART+8)       // conj
#define LUPTMAJOR_POSINTERJECTION (LUPTMAJOR_POSSTART+9)       // inter


#define LEXPHONE_SILENCE         254      // special phoneme number for silence
#define LEXPHONE_REFSTRESS       253      // next characters are from LexCharCompress, and are
                                          // reference to pronunciation of another word. Keep
                                          // the stresses
#define LEXPHONE_REFUNSTRESS     252      // like LEXPHONE_REFSTRESSS, but will be destressed



#define PRONCHARS                128      // number of charters in pronunciation buffer

static PWSTR gpszImportMandarinPhoneInitial = L"b\nd\nt\ng\nj\nk\np\nq\nf\nh\nsh\ns\nch\nc\nx\nzh\nz\nm\nn\nl\nr";
static PWSTR gpszImportMandarinPhoneFinal =
   L"a\nai\nao\nan\nang\no\nou\ne\nei\nen\neng\ner\nii\ni\nia\niao\nian\niang\n"
   L"ie\niu\nin\ning\niong\niou\nu\nua\nuo\nuai\nuei\nui\nuan\nuen\nuang\nueng\n"
   L"un\nong\nv\nve\nvan\nvn";

static PWSTR gpszImportUnisynPhoneStress = L"@\n@@r\na\naa\nai\ne\nei\neir\n"
   L"i\nii\niy\ni@\no\noi\noo\nou\now\n"
   L"u\nuh\nur\nuu\nuw";
static PWSTR gpszImportUnisynPhoneNoStress = L"b\nch\nd\ndh\nf\ng\nh\njh\n"
   L"k\nl\nlw\nl!\nm\nm!\nn\nng\nn!\n"
   L"p\nr\ns\nsh\nt\nth\nv\nw\ny\nz\nzh";
static PWSTR gpszImportUnisynStress0 = L"*";
static PWSTR gpszImportUnisynStress1 = L"~";
static PWSTR gpszImportUnisynStress2 = L"-";
static PWSTR gpszImportUnisynStress3 = L".";
static PWSTR gpszImportUnisynSymbolIgnore = L"{\n}\n<\n$\n>";
static PWSTR gpszImportUnisynPOSNoun = L"NN\nNNS\nNNP\nNNPS\nCD\nFW\nLS\nDT|NN\nNN|IN\nNNS|IN\nNNS|NNPS";
static PWSTR gpszImportUnisynPOSPronoun = L"PRP\nWP";
static PWSTR gpszImportUnisynPOSAdjective = L"PRP$\nWP$\nJJ\nJJR\nJJS\nPOS\nPDT\nCD|POS\nDT|JJ\nFW|POS\nNN|POS\nNN|IN|DT\nNNP|POS\nNNPS|POS\nNNS|POS";
static PWSTR gpszImportUnisynPOSPreposition = L"IN\n";
static PWSTR gpszImportUnisynPOSArticle = L"DT\nWDT";
static PWSTR gpszImportUnisynPOSVerb = L"VB\nVBP\nVBZ\nVBD\nVBG\nVBN\n"
   L"CD|VBZ\nEX|VBZ\nFW|VBZ\nIN|VBZ\n"
   L"NN|VBD\nNN|VBZ\nNNP|VBZ\n"
   L"PRP|VB\nPRP|VBD\nPRP|VBN\nPRP|VBP\nPRP|VBZ\n"
   L"RB|VBD\nRB|VBN\nRB|VBP\n"
   L"VB|PRP\nVB|TO\n"
   L"VBD|PRP\nVBN|PRP\nVBN|TO\n"
   L"VBP|RB|VB\nVBP|PRP\nVBP|TO\n"
   L"WP|VBD\nWP|VBN\nWP|VBP\nWP|VBZ\nWP|VBP|PRP\n"
   L"WRB|VBD\nWRB|VBP\nWRB|VBZ";

static PWSTR gpszImportUnisynPOSAdverb = L"RB\nRBR\nRBS\nWRB\nRB|VBZ\nVBD|RB\nVBG|TO\nVBP|RB\nVBZ|RB";

static PWSTR gpszImportUnisynPOSAuxVerb = 
   L"EX\nRP\nTO\nMD|TO\nRP|IN\n"
   L"MD\nMD|RB\nNN|MD\nNNS|MD\nPRP|MD\nRB|MD\nWP|MD\nWRB|MD\nMD|VBP\n";

static PWSTR gpszImportUnisynPOSConjunction = L"CC\n";

static PWSTR gpszImportUnisynPOSInterjection = L"UH";

CRITICAL_SECTION gcsLexiconCache;   // critical section for lexiconf cache


// CLexShard - for storing letter-to-sound information
class CLexShard {
public:
   ESCNEWDELETE;

   CLexShard();
   ~CLexShard();

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CLexShard *Clone (void);

   BOOL TextAndPhoneSet (PWSTR pszText, PBYTE pabPhone);

   // public variables. can modify
   DWORD             m_adwPOSCount[POS_MAJOR_NUM]; // # of times shard appears in word with this POS

   // look at, but dont modify
   PWSTR             m_pszText;  // pointer to text
   PBYTE             m_pbPhone;  // pointer to phone
private:
   CMem              m_mem;      // to store text and phone
};
typedef CLexShard *PCLexShard;


// LEXSHARDNGRAM - For storing the frequency of an ngram of shards
#define NUMSHARDNGRAM         2     // depth of the ngram for shards
typedef struct {
   WORD        awShard[NUMSHARDNGRAM]; // in order, the shards that appear.
                                       // Use -1 for start/end of word, -2 for unknown/backoff
   WORD        wCount;              // number of occurences of this
} LEXSHARDNGRAM, *PLEXSHARDNGRAM;

// TESTSCORESORT - structure to sort test sentences by score
typedef struct {
   fp          fScore;  // with the score
   PCMem       pMem;    // contianing the text
} TESTSCORESORT, *PTESTSCORESORT;


#define NUMREVIEW          50    // review 50 words at a time
#define MAXSENTENCELENGTH  100   // maximum number of words in a sentence

// PEDI - Pronunciation editing info
typedef struct {
   PCMLexicon        pLex;    // lexicon
   DWORD             dwWord;  // word index that modifying, or -1 if new word
   PWSTR             pszWord; // initially filled in with word, and modified in place
   DWORD             dwWordSize; // size of word buffer
   PCListVariable    plForms; // initally filled with word forms, and modified in place
   //PCM3DWave         pWave;   // wave to use for speaking
} PEDI, *PPEDI;

// PMI - Pronunciation main info
#define PTREEBITS    5        // number of bits indicating number of words displayed at once
#define PTREENUM     (1 << PTREEBITS)
typedef struct {
   PCMLexicon        pLex;    // lexicon
   DWORD             dwFirstWord;   // first wordin the display list
   DWORD             dwDisplayLevel;   // if 0 displaying words, 1 groups of 32 words, etc.
   PCListVariable    plOOV;   // out of vocabulary words.. used when scan file
   PCBTree           plTreeReview;     // tree for words to review
   //PCM3DWave         pWave;      // wave used for playing tts
   PCMLexicon        pReviewLex; // lexicon used for reviewing words, or NULL to use file
   PWSTR             pszReviewFind; // string to fill in with what looking for
   int               iReviewLoc; // fill in where looking for it
} PMI, *PPMI;



/*************************************************************************************
*/

// GTSENT - Information about a grammar sentence
typedef struct {
   DWORD          dwWords;    // number of words in the sentence
   DWORD          dwNext;     // next sentnce, byte index in m_memSentences. Use 0 if no next
   double         fScore;     // score for the count

   // NOTE: followed by an array of bytes for each word. DWORD aligned
} GTSENT, *PGTSENT;

// GTRULE - Rule describing conversion of pair of POS into new POS
typedef struct {
   BYTE           bFirst;     // first POS
   BYTE           bSecond;    // second POS
   BYTE           bConvert;   // convert into this
   BYTE           bFiller;    // nothing stored here
} GTRULE, *PGTRULE;

// CGramTrain - Used for training up the grammar rules. This keeps
// information about a list of known sentences, and allows different rules
// to be applied to them
class CGramTrain {
public:
   ESCNEWDELETE;

   CGramTrain (void);
   ~CGramTrain (void);
   DWORD SentenceAdd (DWORD dwNum, PLEXPOSGUESS paLPG, PCMLexicon pLex);
   PBYTE SentenceGet (DWORD dwIndex, DWORD *pdwNumWords, double *pfScore, DWORD *pdwNext);
   CGramTrain *DiscoverNewRules (DWORD dwBlockSize, DWORD dwBlocks,
                              PCMLexicon pLex, PCListFixed plNewGTRULE, PCProgressSocket pProgress);

private:
   DWORD SentenceAdd (DWORD dwNum, BYTE *pabPOS, double fScore);
   DWORD SentenceAddRecurse (DWORD dwNum, WORD *pawBits, BYTE *pabPOS,
                                      double fScore, DWORD dwCur);
   BOOL JitterRule (DWORD dwNum, PGTRULE paGTRULE, DWORD dwModAfter,
      DWORD dwIgnoreIfChangeAfter, DWORD dwForceEffect);
   double Score (void);
   BOOL IdentifyRules (DWORD dwNum, PCMLexicon pLex, PCListFixed plGTRULE);
   CGramTrain *CloneAndApply (DWORD dwNumRules, PGTRULE paGTRULE);
   CGramTrain *CloneSmaller (DWORD dwLimit);
   CGramTrain *DiscoverNewRules (DWORD dwNumRules, PGTRULE paGTRULE, DWORD dwToDiscover,
                              PCMLexicon pLex, PCListFixed plNewGTRULE, PCProgressSocket pProgress);

   CMem           m_memSentences;      // memory containing sentences. GTSENT structure followed by data
   DWORD          m_dwLastSentence;    // location of the last sentence. -1 if none
};
typedef CGramTrain *PCGramTrain;


void POSApplyRules (DWORD *pdwNum, BYTE *pabPOS, DWORD dwNumRules, PGTRULE paGTRULE);


/* globals */
#define SILENCE      L"<s>\0\0\0\0"  // lots of 0's since might memcpy directly from it
static CListFixed  glPCMLexicon;       // list of lexicons
static BOOL gfPCMLexcionValid = FALSE; // set to TRUE if initalized
static WCHAR gszSilence[] = SILENCE;
static LEXPHONE gLexPhoneSilence = {
   L"silence", SILENCE, 0, 0,0
   };


// NOTE - PIS_TOPPHONEGROUP(x) reqwuries that x < PIS_PHONEGROUPNUM
static LEXENGLISHPHONE gaLEXENGLISHPHONE[NUMLEXENGLISHPHONE] = { // NOTE: Must be alphabetical
   PIC_MISC, SILENCE, L"silence", PIS_TOPHONEGROUP(0) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 7, 0,
   PIC_VOWEL | PIC_VOICED, L"aa\0", L"fAther", PIS_TOPHONEGROUP(16) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX, 10, 2,
   PIC_VOWEL | PIC_VOICED, L"ae\0", L"At", PIS_TOPHONEGROUP(16) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_MAX | PIS_VERTOPN_SLIGHT, 11, 1,
   PIC_VOWEL | PIC_VOICED, L"ah\0", L"cUt", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID, 12, 1,
   PIC_VOWEL | PIC_VOICED, L"ao\0", L"lAW", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_FULL | PIS_LATTEN_REST | PIS_VERTOPN_MAX, 13, 3,
   PIC_VOWEL | PIC_VOICED, L"aw\0", L"OUt", PIS_TOPHONEGROUP(13) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_FULL | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX, 14, 9,
   PIC_VOWEL | PIC_VOICED, L"ax\0", L"About", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID, 15, 1,
   PIC_VOWEL | PIC_VOICED, L"ay\0", L"tIE", PIS_TOPHONEGROUP(14) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MAX, 16, 11,
   PIC_CONSONANT | PIC_PLOSIVE | PIC_VOICED, L"b\0\0", L"Bit", PIS_TOPHONEGROUP(12) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 17, 21,
   PIC_CONSONANT, L"ch\0", L"CHin", PIS_TOPHONEGROUP(1) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 18, 16,
   PIC_CONSONANT | PIC_PLOSIVE | PIC_VOICED, L"d\0\0", L"Dip", PIS_TOPHONEGROUP(11) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT, 19, 19,
   PIC_CONSONANT | PIC_VOICED, L"dh\0", L"THis", PIS_TOPHONEGROUP(8) | PIS_TONGUEFRONT_TEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT, 20, 17,
   PIC_VOWEL | PIC_VOICED, L"eh\0", L"bEt", PIS_TOPHONEGROUP(16) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_REST | PIS_VERTOPN_MID,21, 4,
   PIC_VOWEL | PIC_VOICED, L"er\0", L"hUrt", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_FULL | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID, 22, 5,
   PIC_VOWEL | PIC_VOICED, L"ey\0", L"AId", PIS_TOPHONEGROUP(14) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT, 23, 4,
   PIC_CONSONANT, L"f\0\0", L"Fat", PIS_TOPHONEGROUP(7) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_KEEPSHUT | PIS_TEETHTOP_FULL |PIS_LATTEN_SLIGHT | PIS_VERTOPN_CLOSED, 24, 18,
   PIC_CONSONANT | PIC_PLOSIVE | PIC_VOICED, L"g\0\0", L"Give", PIS_TOPHONEGROUP(10) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT, 25, 20,
   PIC_CONSONANT, L"h\0\0", L"Hit", PIS_TOPHONEGROUP(9) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MAX, 26, 12,
   PIC_VOWEL | PIC_VOICED, L"ih\0", L"bIt", PIS_TOPHONEGROUP(16) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_MID, 27, 6,
   PIC_VOWEL | PIC_VOICED, L"iy\0", L"bEET", PIS_TOPHONEGROUP(14) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT, 28, 6,
   PIC_CONSONANT | PIC_VOICED, L"jh\0", L"Joy", PIS_TOPHONEGROUP(1) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 29, 16,
   PIC_CONSONANT | PIC_PLOSIVE, L"k\0\0", L"Kiss", PIS_TOPHONEGROUP(10) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_MAX | PIS_VERTOPN_SLIGHT, 30, 20,
   PIC_CONSONANT | PIC_VOICED, L"l\0\0", L"Lip", PIS_TOPHONEGROUP(3) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_SLIGHT, 31, 14,
   PIC_CONSONANT | PIC_VOICED, L"m\0\0", L"Map", PIS_TOPHONEGROUP(4) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 32, 21,
   PIC_CONSONANT | PIC_VOICED, L"n\0\0", L"Nip", PIS_TOPHONEGROUP(4) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 33, 19,
   PIC_CONSONANT | PIC_VOICED, L"nx\0", L"kiNG", PIS_TOPHONEGROUP(4) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_FULL | PIS_LATTEN_MAX | PIS_VERTOPN_CLOSED, 34, 20,
   PIC_VOWEL | PIC_VOICED, L"ow\0", L"tOE", PIS_TOPHONEGROUP(13) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID, 35, 8,
   PIC_VOWEL | PIC_VOICED, L"oy\0", L"tOY", PIS_TOPHONEGROUP(13) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID, 36, 10,
   PIC_CONSONANT | PIC_PLOSIVE | PIC_VOICED, L"p\0\0", L"Pin", PIS_TOPHONEGROUP(12) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_KEEPSHUT | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 37, 21,
   PIC_CONSONANT | PIC_VOICED, L"r\0\0", L"Red", PIS_TOPHONEGROUP(3) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 38, 13,
   PIC_CONSONANT, L"s\0\0", L"Sip", PIS_TOPHONEGROUP(6) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHBOT_MID | PIS_TEETHTOP_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 39, 15,
   PIC_CONSONANT, L"sh\0", L"SHe", PIS_TOPHONEGROUP(5) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_TEETHBOT_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 40, 16,
   PIC_CONSONANT | PIC_PLOSIVE, L"t\0\0", L"Talk", PIS_TOPHONEGROUP(11) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_ROOF | PIS_TEETHBOT_MID | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 41, 19,
   PIC_CONSONANT, L"th\0", L"THin", PIS_TOPHONEGROUP(8) | PIS_TONGUEFRONT_TEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_SLIGHT | PIS_VERTOPN_SLIGHT, 42, 17,
   PIC_VOWEL | PIC_VOICED, L"uh\0", L"fOOt", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MID, 43, 4,
   PIC_VOWEL | PIC_VOICED, L"uw\0", L"fOOd", PIS_TOPHONEGROUP(15) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_MAX, 44, 7,
   PIC_CONSONANT | PIC_VOICED, L"v\0\0", L"Vat", PIS_TOPHONEGROUP(7) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_FULL | PIS_LATTEN_REST | PIS_VERTOPN_CLOSED, 45, 18,
   PIC_CONSONANT | PIC_VOICED, L"w\0\0", L"Wit", PIS_TOPHONEGROUP(2) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_BOTTOM | PIS_LATTEN_PUCKER | PIS_VERTOPN_SLIGHT, 46, 7,
   PIC_CONSONANT | PIC_VOICED, L"y\0\0", L"Yet", PIS_TOPHONEGROUP(2) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_BOTTOM | PIS_TEETHTOP_MID | PIS_LATTEN_MAX | PIS_VERTOPN_MID, 47, 6,
   PIC_CONSONANT | PIC_VOICED, L"z\0\0", L"Zip", PIS_TOPHONEGROUP(6) | PIS_TONGUEFRONT_BEHINDTEETH | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_TEETHBOT_MID | PIS_LATTEN_PUCKER | PIS_VERTOPN_CLOSED, 48, 15,
   PIC_CONSONANT | PIC_VOICED, L"zh\0", L"aZure", PIS_TOPHONEGROUP(5) | PIS_TONGUEFRONT_PALATE | PIS_TONGUETOP_TEETH | PIS_TEETHTOP_FULL | PIS_LATTEN_PUCKER | PIS_VERTOPN_CLOSED, 49, 16,
};



// CLexPOSHyp - To guess the part of speech
class CLexPOSHyp {
public:
   ESCNEWDELETE;

   CLexPOSHyp (void);
   ~CLexPOSHyp (void);
   CLexPOSHyp *Clone (void);

   CListFixed     m_lPOS;        // list of BYTES containing the POS up until now
   double         m_fScore;      // current score
   BOOL           m_fBackedOff;  // set to TRUE if had to back off
};
typedef CLexPOSHyp *PCLexPOSHyp;

static DWORD gadwPhoneGroupToMega[PIS_PHONEGROUPNUM] =
   {
      0 /*silence*/, 2/*ch,jh*/, 3/*w*/, 3/*l,r*/,
      3 /*m,n*/, 2 /*sh,zh*/, 2 /*s,z*/, 2 /*f,v*/,
      2 /*dh,th*/, 2/*h*/, 1 /*g,k*/, 1 /*d,t*/,
      1 /*b,p*/, 4 /*ou,aw,oy*/, 4 /*ay,ie,ai,ee*/, 4 /*ah,ao,ax*/,
      4 /*aa,ae,eh*/
   };

/*************************************************************************************
LexRegGet - Get a registry key.

inputs
   PCWSTR         pszKey - KeyName.
   PCMem          pMem - If this is NOT NULL, then a unicode string is read in
   PCWSTR         pszDefault - Default string used for pMem if no values. Can be NULL for blank
   DWORD          *pdwValue - If !pMem then this is filled with the value.
   DWORD          dwDefault - Default value used for *pdwValue
returns
   none
*/
void LexRegGet (PCWSTR pszKey, PCMem pMem, PCWSTR pszDefault, DWORD *pdwValue = NULL, DWORD dwDefault = NULL)
{

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyExW (HKEY_CURRENT_USER, RegBaseW(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      goto fromdefault;

   DWORD dw, dwType;
   LONG lRet;
   lRet = RegQueryValueExW (hKey, pszKey, NULL, &dwType, NULL, &dw);
   if ((lRet != ERROR_SUCCESS) || !dw) {
      RegCloseKey (hKey);
      goto fromdefault;
   }


   if (pdwValue) {
      if (dw != sizeof(*pdwValue)) {
         RegCloseKey (hKey);
         goto fromdefault;
      }

      RegQueryValueExW (hKey, pszKey, NULL, &dwType, (PBYTE)pdwValue, &dw);

      return;
   }

   // else, string
   if (!pMem->Required (dw)) {
      RegCloseKey (hKey);
      goto fromdefault;
   }
   RegQueryValueExW (hKey, pszKey, NULL, &dwType, (PBYTE)pMem->p, &dw);
   return;


   RegCloseKey (hKey);

fromdefault:
   if (pdwValue)
      *pdwValue = dwDefault;
   else if (pMem) {
      MemZero (pMem);
      if (pszDefault)
         MemCat (pMem, (PWSTR) pszDefault);
   }
}


/*************************************************************************************
LexRegSet - Get a registry key.

inputs
   PCWSTR         pszKey - KeyName.
   PCWSTR         pszSet - String to set this as. Can be NULL.
   DWORD          *pdwSet - Value to set this as. Used if !pszSet.

               If both pszSet and pdwSet are NULL then delte the entry
returns
   none
*/
void LexRegSet (PCWSTR pszKey, PCWSTR pszSet, DWORD *pdwSet = NULL)
{

   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyExW (HKEY_CURRENT_USER, RegBaseW(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);


   // write the version
   if (!hKey)
      return;

   if (pszSet)
      RegSetValueExW (hKey, pszKey, 0, REG_SZ, (BYTE*) pszSet, (DWORD) (wcslen(pszSet)+1) * sizeof(WCHAR) );
   else if (pdwSet)
      RegSetValueExW (hKey, pszKey, 0, REG_DWORD, (BYTE*) pdwSet, sizeof(*pdwSet) );
   else
      RegDeleteValueW (hKey, pszKey);

   RegCloseKey (hKey);
}


/*************************************************************************************
LexPhoneGroupToMega - Converts a phoneme group (0..16) into its appropraite
mega group (only 5 (PIS_PHONEMEGAGROUPNUM) cateogories)

inputs
   DWORD          dwGroup - Group, from PIS_TOPHONEGROUP ()
returns
   0 - For silence
   1 - Plosives
   2 - Unvoiced/voiced consonant pairs (non-plosive)
   3 - Voiced consonant
   4 - For voiced vowel
*/
DWORD LexPhoneGroupToMega (DWORD dwGroup)
{
   return gadwPhoneGroupToMega[dwGroup];
}



/*************************************************************************************
LexPhoneGroupEvenSmaller - Takes a phoneme group number, from PIS_TOPHONEGROUP(),
and converts it to a value from 0..7.

inputs
   DWORD       dwGroup - Group, 0..16
returns
   DWORD - Grup 0..7
*/
static DWORD gadwGroupRemap[17] =
   {0, 1 /*ch*/, 2 /*w*/, 2 /*l*/,
   3/*m*/, 1 /*sh*/, 1/*s*/, 4/*f*/,
   1/*th*/, 4/*h*/, 5/*k*/, 5/*d*/,
   5/*b*/, 6/*oe*/, 6/*ie*/, 7/*u*/,
   7/*a*/};
DWORD MLexiconEnglishGroupEvenSmaller (DWORD dwGroup)
{
   return gadwGroupRemap[dwGroup];
}

/*************************************************************************************
LexCharDecompress - Decompress some compressed characters.

inputs
   PBYTE          pbComp - Compressed characters
   PWSTR          psz - String to fill characters in
   DWORD          dwAvail - Number of CHARACTERS available in psz
   DWORD          *pdwUsed - Filled with the number of characters copied to psz
returns
   DWORD - Number of bytes from pbComp used.
*/
DWORD LexCharDecompress (PBYTE pbComp, PWSTR psz, DWORD dwAvail, DWORD *pdwUsed)
{
   DWORD dwCompUsed = 0;
   *pdwUsed = 0;  // just to clear out

   // get the number of characters
   DWORD dwChars = *pdwUsed = pbComp[0];
   pbComp++;
   dwCompUsed++;
   
   // copy over
   DWORD i;
   WCHAR c;
   for (i = 0; i < dwChars; i++) {
      // get the value
      if (!(pbComp[0] & 0x80)) { // 1 byte
         c = pbComp[0];

         // update pointer
         dwCompUsed++;
         pbComp++;
      }
      else if (!(pbComp[1] & 0x80)) { // 2 bytes
         c = (pbComp[0] & 0x7f) | ((WORD)pbComp[1] << 7);

         // update pointer
         dwCompUsed += 2;
         pbComp += 2;
      }
      else { // 3 bytes
         c = (pbComp[0] & 0x7f) | ((WORD)(pbComp[1] & 0x7f) << 7) | ((WORD)pbComp[2] << 14);

         // update pointer
         dwCompUsed += 3;
         pbComp += 3;
      }

      if (dwAvail) {
         *(psz++) = c;
         dwAvail--;
      }
   } // i

   return dwCompUsed;
}


/*************************************************************************************
LexCharCompress - Takes a unicode string and outputs a compressed string that
can be stored in an 8-bit value.

inputs
   PWSTR          psz - String
   DWORD          dwChars - Number of characters in psz
   PBYTE          pbBuf - Buffer to store it in
   DWORD          dwAvail - Number of bytes available
returns
   DWORD - Number of bytes used, or -1 if error
*/
size_t LexCharCompress (PWSTR psz, size_t dwChars, PBYTE pbBuf, size_t dwAvail)
{
   size_t dwUsed = dwAvail;

   // make sure can write number of chars
   if (!dwAvail || !dwChars || (dwChars > 255))
      return -1;
   *(pbBuf++) = (BYTE)dwChars;
   dwAvail--;

   for (; dwChars; dwChars--, psz++) {
      if (!psz[0])
         return -1;  // shouldnt pass in NULL string

      if (psz[0] <= 0x7f) {
         if (!dwAvail)
            return -1;
         pbBuf[0] = (BYTE)psz[0];

         // update
         pbBuf++;
         dwAvail -= 1;
      }
      else if (psz[0] <= 0x3fff) {  // 2 bytes
         if (dwAvail < 2)
            return -1;

         pbBuf[0] = (BYTE)psz[0] | 0x80;  // hi-bit indicates more to come
         pbBuf[1] = (BYTE)((WORD) psz[0] >> 7);

         // update
         pbBuf += 2;
         dwAvail -= 2;
      }
      else {   // 3 bytes
         if (dwAvail < 3)
            return -1;

         pbBuf[0] = (BYTE)psz[0] | 0x80;  // hi-bit indicates more to come
         pbBuf[1] = (BYTE)((WORD) psz[0] >> 7) | 0x80;
         pbBuf[1] = (BYTE)((WORD) psz[0] >> 14);

         // update
         pbBuf += 3;
         dwAvail -= 3;
      }
   } // dwChars

   // return, since stored original amount in dwUsed, the amount used = orig
   // - what's left
   return dwUsed - dwAvail;
}


/*************************************************************************************
LexDestress - Takes an array of phonemes WITHOUT any references to other characters,
and removes the stresses.

inputs
   PBYTE          pbPron - Pronunication
   PCMLexcion     pLex - Lexicon to use
returns
   none
*/
void LexDestress (PBYTE pbPron, PCMLexicon pLex)
{
   PLEXPHONE plp;
   DWORD dwNumPhone = pLex->PhonemeNum();

   for (; pbPron[0]; pbPron++) {
      plp = pLex->PhonemeGetUnsort (pbPron[0]-1);
      if (!plp)
         continue;

      if (plp->bStress && (plp->wPhoneOtherStress < dwNumPhone))
         pbPron[0] = (BYTE) plp->wPhoneOtherStress+1;
   } // over pbPron
}


/*************************************************************************************
LexExpand - Takes a pronunciation that may have | (LEXPHONE_REFSTRESS) or
: (LEXPHONE_REFUNSTRESS) to indicate a sub-pronunciation
and expands the pronunciation out completely.

inputs
   PBYTE          pbPron - Pronounciation. First character is the part of speech.
   PCMLexicon     pLex - Lexicon to use.
   BOOL           fUseLTS - If have a LEXPHONE_XXX but cant find the sub-form then
                  use LTS.
   PCListVariable plDontRecurse - List used by the lexicon to make sure it's not
                  recursing in on itself. This should either be empty (if it's
                  the first level calling), or filled with the list of word strings
                  that have already gotten the search this far.
   PCListVariable plPron - This has the alternate pronunciations appended to it,
                  with the first byte being the parts of speech
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL LexExpand (PBYTE pbPron, PCMLexicon pLex, BOOL fUseLTS,
                PCListVariable plDontRecurse, PCListVariable plPron)
{
   // length
   DWORD dwLen = (DWORD)strlen((char*)pbPron+1);

   // see if can find and stress or unstress markers
   DWORD i;
   DWORD dwFirst = -1;
   for (i = 1; i <= dwLen; i++)
      if ((pbPron[i] == LEXPHONE_REFSTRESS) || (pbPron[i] == LEXPHONE_REFUNSTRESS)) {
         dwFirst = i;
         break;
      }

   // if no stress or unstress markers then easy
   if (dwFirst == -1) {
      plPron->Add (pbPron, dwLen+2);
      return TRUE;
   }

   // if get here found stress marker, decompress
   WCHAR szTemp[32];    // 32 chars should be long enough
   DWORD dwCharUsed;
   DWORD dwByteUsed = LexCharDecompress (pbPron + (dwFirst+1), szTemp,
      sizeof(szTemp)/sizeof(WCHAR)-1, &dwCharUsed);
   if (dwCharUsed > sizeof(szTemp)/sizeof(WCHAR)-1)
      return FALSE;  // too large
   szTemp[dwCharUsed] = 0; // NULL terminate

   // get the pronunciation from the lex
   CListVariable lFirst;
   pLex->WordPronunciation (szTemp, &lFirst, fUseLTS, pLex, plDontRecurse);

   // remove stress?
   if (pbPron[dwFirst] == LEXPHONE_REFUNSTRESS) for (i = 0; i < lFirst.Num(); i++) {
      PBYTE pb = ((PBYTE)lFirst.Get(i)) + 1;
      LexDestress (pb, pLex);
   }

   // see if there's anything after
   DWORD dwAfterStart = (dwFirst+1) + dwByteUsed;
   DWORD dwSecond = -1;
   for (i = dwAfterStart; i <= dwLen; i++)
      if ((pbPron[i] == LEXPHONE_REFSTRESS) || (pbPron[i] == LEXPHONE_REFUNSTRESS)) {
         dwSecond = i;
         break;
      }
   CListVariable lAfter;
   if (dwSecond != -1) {
      // if there's a second stress marker then need to get all the alternative
      // for what follows
      if (!LexExpand (pbPron + (dwAfterStart-1), pLex, fUseLTS, plDontRecurse, &lAfter))
          return FALSE; // error
   }
   else  // else, quick solution and just append what follows
      lAfter.Add (pbPron + (dwAfterStart-1), dwLen + 2 - (dwAfterStart-1));
   
   // get rid of identical pronunciations, but different parts of speech
   DWORD j;
   for (i = 0; i < lFirst.Num(); i++) {
      PBYTE pbFirst = ((PBYTE)lFirst.Get(i))+1;
      for (j = i+1; j < lFirst.Num(); j++) {
         PBYTE pbCompare = ((PBYTE)lFirst.Get(j))+1;
         if (!strcmp ((char*)pbFirst, (char*)pbCompare)) {
            // found a match, so remove j
            lFirst.Remove (j);
            j--;  // so when do j++ counteracted
         }
      } // j
   } // i
   for (i = 0; i < lAfter.Num(); i++) {
      PBYTE pbFirst = ((PBYTE)lAfter.Get(i))+1;
      for (j = i+1; j < lAfter.Num(); j++) {
         PBYTE pbCompare = ((PBYTE)lAfter.Get(j))+1;
         if (!strcmp ((char*)pbFirst, (char*)pbCompare)) {
            // found a match, so remove j
            lAfter.Remove (j);
            j--;  // so when do j++ counteracted
         }
      } // j
   } // i

   // make sure have enoug memory
   DWORD dwMax = 0;
   for (i = 0; i < lFirst.Num(); i++) {
      j = (DWORD)lFirst.Size (i);
      dwMax = max(dwMax, j);
   }
   DWORD dwNeed = dwFirst + 2 + dwMax;
   dwMax = 0;
   for (i = 0 ; i < lAfter.Num(); i++) {
      j = (DWORD)lAfter.Size (i);
      dwMax = max(dwMax, j);
   }
   dwNeed += dwMax;
   CMem mem;
   if (!mem.Required (dwNeed))
      return FALSE;
   PBYTE pbTemp = (PBYTE)mem.p;

   // copy the start
   memcpy (pbTemp, pbPron, dwFirst);
   for (i = 0; i < lFirst.Num(); i++) {
      PBYTE pbFirst = ((PBYTE)lFirst.Get(i)) + 1;
      dwLen = (DWORD)strlen((char*)pbFirst);
      memcpy (pbTemp + dwFirst, pbFirst, dwLen);
      dwLen += dwFirst; // so know where to copy to

      plPron->Required (plPron->Num() + lAfter.Num());

      for (j = 0; j < lAfter.Num(); j++) {
         PBYTE pbAfter = ((PBYTE)lAfter.Get(j)) + 1;
         strcpy ((char*)pbTemp + dwLen, (char*)pbAfter);

         // add this
         plPron->Add (pbTemp, strlen((char*)(pbTemp+1)) + 2);
      } // j
   } // i

   // done
   return TRUE;
}

/*************************************************************************************
CLexPOSHyp::Constructor and destructor
*/
CLexPOSHyp::CLexPOSHyp (void)
{
   m_lPOS.Init (sizeof(BYTE));
   m_fScore = 0;
   m_fBackedOff = FALSE;
}

CLexPOSHyp::~CLexPOSHyp (void)
{
   // do nothing
}


/*************************************************************************************
CLexPOSHyp::Clone - Clones the POS hyp
*/
CLexPOSHyp *CLexPOSHyp::Clone (void)
{
   PCLexPOSHyp pNew = new CLexPOSHyp;
   if (!pNew)
      return NULL;

   pNew->m_lPOS.Init (sizeof(BYTE), m_lPOS.Get(0), m_lPOS.Num());
   pNew->m_fScore = m_fScore;
   pNew->m_fBackedOff = m_fBackedOff;

   return pNew;
}

/*************************************************************************************
Sorting functions for hyps
*/
static int __cdecl PCLexPOSHypCompare1 (const void *p1, const void *p2)
{
   PCLexPOSHyp pp1 = *((PCLexPOSHyp*) p1);
   PCLexPOSHyp pp2 = *((PCLexPOSHyp*) p2);

   // NOTE: Will always have the same number of elements in both
   DWORD dwNum = pp1->m_lPOS.Num();
   DWORD dwCompare = min(dwNum, LEXPOSNGRAM);

   int iRet = memcmp ((PBYTE)pp1->m_lPOS.Get(dwNum - dwCompare),
      (PBYTE)pp2->m_lPOS.Get(dwNum - dwCompare), dwCompare * sizeof(BYTE));
   if (iRet)
      return iRet;

   // else, compare by their score
   if (pp1->m_fScore > pp2->m_fScore)
      return -1;
   else if (pp1->m_fScore < pp2->m_fScore)
      return 1;
   else
      return 0;
}

static int __cdecl PCLexPOSHypCompare2 (const void *p1, const void *p2)
{
   PCLexPOSHyp pp1 = *((PCLexPOSHyp*) p1);
   PCLexPOSHyp pp2 = *((PCLexPOSHyp*) p2);

   // else, compare by their score
   if (pp1->m_fScore > pp2->m_fScore)
      return -1;
   else if (pp1->m_fScore < pp2->m_fScore)
      return 1;
   else
      return 0;
}

/*************************************************************************
CLexShard::Constructor and destructor
*/
CLexShard::CLexShard()
{
   memset (m_adwPOSCount, 0, sizeof(m_adwPOSCount));
   m_pszText = NULL;
   m_pbPhone = NULL;
}

CLexShard::~CLexShard()
{
   // nothing for now
}

static PWSTR gpszShard = L"Shard";
static PWSTR gpszText = L"Text";
static PWSTR gpszPhone = L"Phone";
static PWSTR gpszPOSCount = L"POSCount";

/*************************************************************************
CLexShard::MMLTo - Standard API
*/
PCMMLNode2 CLexShard::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszShard);

   MMLValueSet (pNode, gpszPOSCount, (PBYTE)m_adwPOSCount, sizeof(m_adwPOSCount));
   if (m_pszText && m_pszText[0])
      MMLValueSet (pNode, gpszText, m_pszText);
   if (m_pbPhone && m_pbPhone[0])
      MMLValueSet (pNode, gpszPhone, m_pbPhone, (DWORD)strlen((char*)m_pbPhone));

   return pNode;
}

/*************************************************************************
CLexShard::MMLFrom - Standard API
*/
BOOL CLexShard::MMLFrom (PCMMLNode2 pNode)
{
   // clear out
   memset (m_adwPOSCount, 0, sizeof(m_adwPOSCount));
   m_pszText = NULL;
   m_pbPhone = NULL;

   MMLValueGetBinary (pNode, gpszPOSCount, (PBYTE)m_adwPOSCount, sizeof(m_adwPOSCount));

   PWSTR pszText;
   BYTE abPhone[128];
   pszText = MMLValueGet (pNode, gpszText);
   size_t dwLen;
   abPhone[0] = 0;
   dwLen = MMLValueGetBinary (pNode, gpszPhone, abPhone, sizeof(abPhone));
   abPhone[dwLen] = 0;
   
   // set
   TextAndPhoneSet (pszText, abPhone);

   return TRUE;
}

/*************************************************************************
CLexShard::Clone - Standard API
*/
CLexShard *CLexShard::Clone (void)
{
   PCLexShard pNew = new CLexShard;
   if (!pNew)
      return NULL;

   memcpy (pNew->m_adwPOSCount, m_adwPOSCount, sizeof(m_adwPOSCount));
   pNew->TextAndPhoneSet (m_pszText, m_pbPhone);

   return pNew;
}


/*************************************************************************
CLexShard::TextAndPhoneSet - Sets the text and phonemes to use for the
shard.

inputs
   PWSTR             pszText - Text to set. If NULL then an empty string is set
   PBYTE             pabPhone - NULL-terminated phonemes to set. If NULL then empty string
returns
   BOOL - TRUE if success
*/
BOOL CLexShard::TextAndPhoneSet (PWSTR pszText, PBYTE pabPhone)
{
   if (!pszText)
      pszText = L"";
   BYTE b = 0;
   if (!pabPhone)
      pabPhone = &b;
   size_t dwNeed = (wcslen(pszText)+1)*sizeof(WCHAR) + (strlen((char*)pabPhone)+1);
   if (!m_mem.Required (dwNeed))
      return FALSE;

   m_pszText = (PWSTR) m_mem.p;
   m_pbPhone = (PBYTE) (m_pszText + (wcslen(pszText)+1));
   wcscpy (m_pszText, pszText);
   strcpy ((char*)m_pbPhone, (char*)pabPhone);

   return TRUE;
}





/*************************************************************************
MLexiconEnglishPhoneSilence - Returns the silence phoneme
*/
PCWSTR MLexiconEnglishPhoneSilence (void)
{
   return SILENCE;
}

/*************************************************************************
MLexiconEnglishPhoneFind - Given a phoneme string, this finds the index to it.

inputs
   WCHAR        *pszName - Phoneme name
returns
   DWORD - Index, or -1 if cant find
*/
static int __cdecl LEXENGLISHPHONECompare (const void *p1, const void *p2)
{
   PLEXENGLISHPHONE pp1 = (PLEXENGLISHPHONE) p1;
   PLEXENGLISHPHONE pp2 = (PLEXENGLISHPHONE) p2;
   int iRet = _wcsnicmp(pp1->szPhoneLong, pp2->szPhoneLong, sizeof(pp1->szPhoneLong)/sizeof(WCHAR));
   return iRet;
}

DWORD MLexiconEnglishPhoneFind (WCHAR *pszName)
{
   LEXENGLISHPHONE pi;
   memset (pi.szPhoneLong, 0, sizeof(pi.szPhoneLong));
   wcscpy (pi.szPhoneLong, pszName);  // can overrun a bit because followed by sample string

   PLEXENGLISHPHONE ppi;
   ppi = (PLEXENGLISHPHONE) bsearch (&pi, gaLEXENGLISHPHONE, MLexiconEnglishPhoneNum(), sizeof(LEXENGLISHPHONE), LEXENGLISHPHONECompare);
   if (!ppi)
      return -1;
   return (DWORD)((PBYTE)ppi - (PBYTE)&gaLEXENGLISHPHONE[0]) / sizeof(LEXENGLISHPHONE);
}


/*************************************************************************
MLexiconEnglishPhoneGet - Returns the phoneme information for a phoneme based on the
index.

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-1
returns
   PLEXENGLISHPHONE  - information. Do NOT change th einfor
*/
PLEXENGLISHPHONE MLexiconEnglishPhoneGet (DWORD dwIndex)
{
   if (dwIndex >= MLexiconEnglishPhoneNum())
      return NULL;

   return &gaLEXENGLISHPHONE[dwIndex];
}


/*************************************************************************
MLexiconEnglishPhoneGet - Returns the phoneme information for a phoneme based on the
string.

inputs
   char        *pszName - Phoneme name
returns
   PLEXENGLISHPHONE  - information. Do NOT change th einfor
*/
PLEXENGLISHPHONE MLexiconEnglishPhoneGet (WCHAR *pszName)
{
   DWORD dwIndex = MLexiconEnglishPhoneFind (pszName);
   return MLexiconEnglishPhoneGet (dwIndex);
}

/*************************************************************************
MLexiconEnglishPhoneNum - Returns the number of phonemes
*/
DWORD MLexiconEnglishPhoneNum (void)
{
   _ASSERTE (sizeof(gaLEXENGLISHPHONE) / sizeof(LEXENGLISHPHONE) == NUMLEXENGLISHPHONE);

   return sizeof(gaLEXENGLISHPHONE) / sizeof(LEXENGLISHPHONE);
}



/*************************************************************************************
MLexiconCacheAddRef - Adds a reference count to the lexicon

inputs
   PCMLexcion        pLex - Lexicon to close
*/
void MLexiconCacheAddRef (PCMLexicon pLex)
{
   EnterCriticalSection (&gcsLexiconCache);

   pLex->m_dwRefCount++;

   LeaveCriticalSection (&gcsLexiconCache);
}


/*************************************************************************************
MLexiconCacheOpen - Opens a lexicon using the lexicon cache. If the item already
exists in memory then that one is used. If you open a lexicon using this then
you should use MLexiconCacheClose() instead of delete, so that several tools can
use the same lexicon.

inputs
   PWSTR          pszFile - File to open
   BOOL           fCreateIfNotExist - IF TRUE then create a new lexicon if one doesnt exist
returns
   PCMLexicon - Lexicon, or NULL if cant open
*/
DLLEXPORT PCMLexicon MLexiconCacheOpen (PWSTR pszFile, BOOL fCreateIfNotExist)
{
   EnterCriticalSection (&gcsLexiconCache);

   if (!gfPCMLexcionValid) {
      gfPCMLexcionValid = TRUE;
      glPCMLexicon.Init (sizeof(PCMLexicon));
   }

   // find the last '\'
   PWSTR pszFileMinusPath = FindFileNameMinusPath (pszFile);


   // look through existing ones
   PCMLexicon *ppl = (PCMLexicon*) glPCMLexicon.Get(0);
   DWORD i;
   for (i = 0; i < glPCMLexicon.Num(); i++) {
      PWSTR pszLexMinusPath = FindFileNameMinusPath (ppl[i]->m_szFile);

      if (!_wcsicmp(pszFileMinusPath, pszLexMinusPath)) {
         // found
         ppl[i]->m_dwRefCount++;
         LeaveCriticalSection (&gcsLexiconCache);
         return ppl[i];
      }
   } // i

#ifdef _DEBUG
   OutputDebugStringW (L"\r\nMLexiconCacheOpen file = ");
   OutputDebugStringW (pszFile);

   __int64 iMem = EscMemoryAllocated (FALSE);
#endif

   // else cant find
   PCMLexicon pNew;
   pNew = new CMLexicon;
   if (!pNew) {
      LeaveCriticalSection (&gcsLexiconCache);
      return NULL;
   }
   wcscpy (pNew->m_szFile, pszFile);
   if (!pNew->Load ()) {
      if (fCreateIfNotExist)
         pNew->Clear();
      else {
         delete pNew;

#ifdef _DEBUG
         OutputDebugStringW (L"\r\n\tFailed to load");
#endif
         LeaveCriticalSection (&gcsLexiconCache);
         return NULL;
      }
   }

#ifdef _DEBUG
   WCHAR szTemp[64];
   OutputDebugStringW (L"\r\n\tLoaded, memory used = ");
   swprintf (szTemp, L"%d K", (int)(((__int64)EscMemoryAllocated(FALSE) - iMem) / 1024));
   OutputDebugStringW (szTemp);
#endif

   // ref count
   pNew->m_dwRefCount = 1;
   glPCMLexicon.Add (&pNew);
   LeaveCriticalSection (&gcsLexiconCache);
   return pNew;
}


/*************************************************************************************
MLexiconCacheClose - Closes a lexicon opened with MLexiconCacheOpen()

NOTE: Because lexicons can take a very long time to load, this doesn't actually
free the lexicon.

inputs
   PCMLexcion        pLex - Lexicon to close
returns
   BOOL - TRUE if found and closed
*/
DLLEXPORT BOOL MLexiconCacheClose (PCMLexicon pLex)
{
   EnterCriticalSection (&gcsLexiconCache);

   // look through existing ones
   PCMLexicon *ppl = (PCMLexicon*) glPCMLexicon.Get(0);
   DWORD i;
   for (i = 0; i < glPCMLexicon.Num(); i++) {
      if (pLex == ppl[i]) {
         // save it
         if (pLex->m_fDirty)
            pLex->Save();

         // ref count
         if (pLex->m_dwRefCount)
            pLex->m_dwRefCount--;

         // if ref is 0 then note all ref gone
#ifdef _DEBUG
         if (!pLex->m_dwRefCount) {
            OutputDebugStringW (L"\r\nMLexiconCacheClose file = ");
            OutputDebugStringW (pLex->m_szFile);
         }
#endif
         LeaveCriticalSection (&gcsLexiconCache);
         return TRUE;
      }
   } // i


   LeaveCriticalSection (&gcsLexiconCache);
   return FALSE;
}



/*************************************************************************************
MLexiconCacheShutDown - Call this before the app shuts down. This frees up all
the lexicons loaded in the lexicon cache.

inputs
   BOOL        fForce - If TRUE, force a shutdown of all lexicons, even
                  if have a ref count. If FALSE, only shut down those
                  witout a ref count
*/
DLLEXPORT void MLexiconCacheShutDown (BOOL fForce)
{
   EnterCriticalSection (&gcsLexiconCache);

   // look through existing ones
   PCMLexicon *ppl;
   PCMLexicon pLex;
   DWORD i;

   // non-force
   while (TRUE) {
      BOOL fReleased = FALSE;

      ppl = (PCMLexicon*) glPCMLexicon.Get(0);
      for (i = 0; i < glPCMLexicon.Num(); i++) {
         if (ppl[i]->m_dwRefCount)
            continue;

         // save in case it isn't
         if (ppl[i]->m_fDirty)
            ppl[i]->Save();

         pLex = ppl[i];
         glPCMLexicon.Remove (i);
         LeaveCriticalSection (&gcsLexiconCache);
         delete pLex;
         EnterCriticalSection (&gcsLexiconCache);
         fReleased = TRUE;
         break;   // since  may have changed everything
      } // i

      // if didn't release anything then stop here
      if (!fReleased)
         break;
   }


   // forecd
   if (fForce) while (glPCMLexicon.Num()) {
      ppl = (PCMLexicon*) glPCMLexicon.Get(0);
      _ASSERTE (!glPCMLexicon.Num());  // make sure never get here
      for (i = 0; i < glPCMLexicon.Num(); i++) {

         // save in case it isn't
         if (ppl[i]->m_fDirty)
            ppl[i]->Save();

         pLex = ppl[i];
         glPCMLexicon.Remove (i);
         LeaveCriticalSection (&gcsLexiconCache);
         delete pLex;
         EnterCriticalSection (&gcsLexiconCache);
         break;   // since may have changed the list
      } // i
   } // while

   LeaveCriticalSection (&gcsLexiconCache);
}


/*************************************************************************************
CMLexicon::Constructor and destructor
*/
CMLexicon::CMLexicon (void)
{
   m_szFile[0] = 0;
   m_dwRefCount = 0;
   GUIDGen (&m_gID); // always make a unique guid, even if will overwrite
   m_lLEXPHONE.Init (sizeof(LEXPHONE));
   m_lPLEXPHONESort.Init (sizeof(PLEXPHONE));
   m_lWords.Init (sizeof(DWORD));
   m_lLEXSHARDNGRAM.Init (sizeof(LEXSHARDNGRAM));
   m_lPCLexShard.Init (sizeof(PCLexShard));

   m_fExceptions = FALSE;
   m_szMaster[0] = 0;
   //m_szTTSSpeak[0] = 0;
   m_pMasterLex = NULL;

   // and clear out
   Clear();
}

CMLexicon::~CMLexicon (void)
{
   // if there's a master lexicon release it
   if (m_pMasterLex) {
      MLexiconCacheClose (m_pMasterLex);
      m_pMasterLex = NULL;
   }

   DWORD i;
   PCLexShard *ppls = (PCLexShard*) m_lPCLexShard.Get(0);
   for (i = 0; i < m_lPCLexShard.Num(); i++) {
      if (ppls[i])
         delete ppls[i];
   } // i
}

/*************************************************************************************
CMLexicon::Clear - Clears out the lexicon
*/
void CMLexicon::Clear(void)
{
   // if there's a master lexicon then release
   if (m_pMasterLex) {
      MLexiconCacheClose (m_pMasterLex);
      m_pMasterLex = NULL;
   }
   m_szMaster[0] = 0;
   //m_szTTSSpeak[0] = 0;
   m_fExceptions = FALSE;

   m_LangID = GetSystemDefaultLangID();
   m_fDirty = FALSE;
   m_lLEXPHONE.Clear();
   m_lPLEXPHONESort.Clear();
   m_memWords.m_dwCurPosn = 0;
   m_lWords.Clear();

   m_dwStresses = 0;

   ShardClear();

   m_LexParse.Clear();
}

/*************************************************************************************
CMLexicon::Clone - Standard
*/
CMLexicon *CMLexicon::Clone (void)
{
   PCMLexicon pNew = new CMLexicon;
   if (!pNew)
      return NULL;

   pNew->m_dwRefCount = m_dwRefCount;
   pNew->m_gID = m_gID;
   pNew->m_LangID = m_LangID;
   pNew->m_fDirty = m_fDirty;
   pNew->m_dwStresses = m_dwStresses;
   wcscpy (pNew->m_szFile, m_szFile);
   
   pNew->m_lLEXPHONE.Init (sizeof(LEXPHONE), m_lLEXPHONE.Get(0), m_lLEXPHONE.Num());
   pNew->m_lPLEXPHONESort.Init (sizeof(PLEXPHONE), m_lPLEXPHONESort.Get(0), m_lPLEXPHONESort.Num());

   pNew->m_lWords.Init (sizeof(DWORD), m_lWords.Get(0), m_lWords.Num());
   if (!pNew->m_memWords.Required (m_memWords.m_dwCurPosn)) {
      delete pNew;
      return NULL;
   }
   memcpy (pNew->m_memWords.p, m_memWords.p, m_memWords.m_dwCurPosn);
   pNew->m_memWords.m_dwCurPosn = m_memWords.m_dwCurPosn;

   m_LexParse.CloneTo (&pNew->m_LexParse);

   // shards
   pNew->m_lLEXSHARDNGRAM.Init (sizeof(LEXSHARDNGRAM), m_lLEXSHARDNGRAM.Get(0), m_lLEXSHARDNGRAM.Num());
   pNew->m_lPCLexShard.Init (sizeof(PCLexShard), m_lPCLexShard.Get(0), m_lPCLexShard.Num());
   DWORD i;
   PCLexShard *ppls = (PCLexShard*) pNew->m_lPCLexShard.Get(0);
   for (i = 0; i < pNew->m_lPCLexShard.Num(); i++)
      ppls[i] = ppls[i]->Clone();

   // master lexicon clone
   if (pNew->m_pMasterLex) {
      MLexiconCacheClose (pNew->m_pMasterLex);
      pNew->m_pMasterLex = NULL;
   }
   pNew->m_fExceptions = m_fExceptions;
   wcscpy (pNew->m_szMaster, m_szMaster);
   if (m_fExceptions)
      pNew->m_pMasterLex = MLexiconCacheOpen (pNew->m_szMaster, FALSE);

   //wcscpy (pNew->m_szTTSSpeak, m_szTTSSpeak);

   return pNew;
}


/*************************************************************************************
WordSize - Returns the size (in bytes) of a word. The size is rounded up to nearest
word alignment.

inputs
   PBYTE          pbStart - Pointer to the start of the word
returns
   DWORD - NUmber of bytes
*/
static DWORD WordSize (PBYTE pbStart)
{
   DWORD dwSize = 0;

   // length of string
   DWORD dwLen = (DWORD)(wcslen((PWSTR)pbStart) + 1)*sizeof(WCHAR);
   dwSize += dwLen;
   pbStart += dwLen;

   // number of forms
   DWORD dwNum = pbStart[0];
   dwSize++;
   pbStart++;

   // go through forms
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      // skip POS
      dwSize++;
      pbStart++;

      // length of phoneme string
      dwLen = (DWORD)strlen((char*)pbStart)+1;
      dwSize += dwLen;
      pbStart += dwLen;
   } // i

   // word align
   if (dwSize % 2)
      dwSize++;

   // done
   return dwSize;
}

static PWSTR gpszLexicon = L"Lexicon";
static PWSTR gpszGUID = L"GUID";
static PWSTR gpszLangID = L"LangID";
static PWSTR gpszPhoneList = L"PhoneList";
static PWSTR gpszSampleWord = L"SampleWord";
static PWSTR gpszEnglishPhone = L"EnglishPhone";
static PWSTR gpszStress = L"Stress";
static PWSTR gpszPhoneOtherStress = L"PhoneOtherStress";
static PWSTR gpszWords = L"Words";
static PWSTR gpszExceptions = L"Exceptions";
static PWSTR gpszMaster = L"Master";
static PWSTR gpszTTSSpeak = L"TTSSpeak";
static PWSTR gpszNGram = L"NGram";
static PWSTR gpszNGramRules = L"NGramRules";
static PWSTR gpszNGramSingle = L"NGramSingle";
static PWSTR gpszGTRULE = L"GTRULE";
static PWSTR gpszLexParse = L"LexParse";

/*************************************************************************************
CMLexicon::MMLTo - Standard API
*/
PCMMLNode2 CMLexicon::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLexicon);

   MMLValueSet (pNode, gpszGUID, (PBYTE)&m_gID, sizeof(m_gID));
   MMLValueSet (pNode, gpszLangID, (int)m_LangID);

   // write out master lex info
   MMLValueSet (pNode, gpszExceptions, (int)m_fExceptions);
   if (m_szMaster[0])
      MMLValueSet (pNode, gpszMaster, m_szMaster);
   //if (m_szTTSSpeak[0])
   //   MMLValueSet (pNode, gpszTTSSpeak, m_szTTSSpeak);

   // write out phones
   DWORD i;
   PLEXPHONE plp = (PLEXPHONE)m_lLEXPHONE.Get(0);
   PCMMLNode2 pPhoneList= pNode->ContentAddNewNode ();
   if (!pPhoneList) {
      delete pNode;
      return NULL;
   }
   pPhoneList->NameSet (gpszPhoneList);
   for (i = 0; i < m_lLEXPHONE.Num(); i++, plp++) {
      PCMMLNode2 pSub = pPhoneList->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszPhone);

      if (plp->szSampleWord[0])
         MMLValueSet (pSub, gpszSampleWord, plp->szSampleWord);
      if (plp->szPhoneLong[0])
         MMLValueSet (pSub, gpszPhone, plp->szPhoneLong);
      MMLValueSet (pSub, gpszEnglishPhone, (int) plp->bEnglishPhone);
      MMLValueSet (pSub, gpszStress, (int)plp->bStress);
      MMLValueSet (pSub, gpszPhoneOtherStress, (int)plp->wPhoneOtherStress);
   } // i


   PCMMLNode2 pSub;
   pSub = m_LexParse.MMLTo();
   if (pSub)
      pNode->ContentAdd (pSub);

   CMem mem, memRLE;

   // will need to rebuild words chunk before writing
   CMem memTemp;
   if (!memTemp.Required (m_memWords.m_dwCurPosn)) {
      delete pNode;
      return NULL;
   }
   memTemp.m_dwCurPosn = 0;
   DWORD *padwWord = (DWORD*) m_lWords.Get(0);
   for (i = 0; i < m_lWords.Num(); i++) {
      PBYTE pSrc = (PBYTE)m_memWords.p + padwWord[i];
      DWORD dwSize = WordSize (pSrc);

      // copy into temp
      memcpy ((PBYTE)memTemp.p + memTemp.m_dwCurPosn, pSrc, dwSize);
      memTemp.m_dwCurPosn += dwSize;
   } // i
   if (memTemp.m_dwCurPosn)
      MMLValueSet (pNode, gpszWords, (PBYTE)memTemp.p, memTemp.m_dwCurPosn);


   // write out all the shards
   pSub = pNode->ContentAddNewNode ();
   if (pSub) {
      pSub->NameSet (gpszShard);

      PCMMLNode2 pShard;
      PCLexShard *ppls = (PCLexShard*) m_lPCLexShard.Get(0);
      for (i = 0; i < m_lPCLexShard.Num(); i++) {
         if (!ppls[i])
            continue;
         pShard = ppls[i]->MMLTo();
         if (!pShard)
            continue;

         pSub->ContentAdd (pShard);
      } // i

      // write out the Ngram
      if (m_lLEXSHARDNGRAM.Num())
         MMLValueSet (pSub, gpszNGram, (PBYTE)m_lLEXSHARDNGRAM.Get(0),
            m_lLEXSHARDNGRAM.Num() * sizeof(LEXSHARDNGRAM));
   } // shard

   return pNode;
}

/*************************************************************************************
CMLexicon::MMLFrom - Standard API

inputs
   PWSTR             pszSrcFile - If the main lexicon doesnt exist, then the root
                           directory is taken from this and used
   BOOL           fIncludeNGrams - If TRUE, will load in NGrams. If FALSE, won't.
                     Use FALSE when lexicon is for word history or whatever
*/
BOOL CMLexicon::MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile, BOOL fIncludeNGrams)
{
   // will need to clear out first
   Clear();

   MMLValueGetBinary (pNode, gpszGUID, (PBYTE)&m_gID, sizeof(m_gID));
   m_LangID = (LANGID) MMLValueGetInt (pNode, gpszLangID, 0);

   // read out master lex info
   PWSTR psz;
   m_fExceptions = (BOOL) MMLValueGetInt (pNode, gpszExceptions, (int)FALSE);
   psz = MMLValueGet (pNode, gpszMaster);
   if (psz)
      wcscpy (m_szMaster, psz);
   else
      m_szMaster[0] = 0;
   //psz = MMLValueGet (pNode, gpszTTSSpeak);
   //if (psz)
   //   wcscpy (m_szTTSSpeak, psz);
   //else
   //   m_szTTSSpeak[0] = 0;

   // ngram
   CMem memRLE;


   // other elems
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

      if (!_wcsicmp(psz, gpszLexParse)) {
         m_LexParse.MMLFrom (pSub, fIncludeNGrams);
         continue;
      }
      if (!_wcsicmp(psz, gpszPhoneList)) {
         // read in phone list
         DWORD j;
         PCMMLNode2 pPhone;
         for (j = 0; j < pSub->ContentNum(); j++) {
            pPhone = NULL;
            pSub->ContentEnum (j, &psz, &pPhone);
            if (!pPhone)
               continue;
            psz = pPhone->NameGet ();
            if (!psz || _wcsicmp(psz, gpszPhone))
               continue;

            LEXPHONE lp;
            memset (&lp, 0, sizeof(lp));
            psz = MMLValueGet (pPhone, gpszSampleWord);
            if (psz)
               wcscpy (lp.szSampleWord, psz);
            psz = MMLValueGet (pPhone, gpszPhone);
            if (psz)
               wcscpy (lp.szPhoneLong, psz);
            lp.bEnglishPhone = (BYTE) MMLValueGetInt (pPhone, gpszEnglishPhone, (int) 0);
            lp.bStress = (BYTE) MMLValueGetInt (pPhone, gpszStress, (int)0);
            lp.wPhoneOtherStress = (WORD)MMLValueGetInt (pPhone, gpszPhoneOtherStress, (int)0);

            // add
            m_lLEXPHONE.Add (&lp);
         } // j

         continue;
      }  // if phone list
      else if (!_wcsicmp(psz, gpszShard)) {
         PCMMLNode2 pShard;
         DWORD j;
         for (j = 0; j < pSub->ContentNum(); j++) {
            pShard = NULL;
            pSub->ContentEnum (j, &psz, &pShard);
            if (!pShard)
               continue;
            psz = pShard->NameGet();
            if (!psz || _wcsicmp(psz, gpszShard))
               continue;

            // else shard
            PCLexShard pls = new CLexShard;
            if (!pls)
               continue;
            pls->MMLFrom (pShard);
            m_lPCLexShard.Add (&pls);
         } // j

         // get the ngram
         // BUGFIX - use new binary MMLGet
         MMLValueGetBinary (pSub, gpszNGram, &memRLE);
         //psz = MMLValueGet (pSub, gpszNGram);
         if (memRLE.m_dwCurPosn /*psz*/) {
            //if (!memRLE.Required (wcslen(psz)/2))
            //   return FALSE;
            PBYTE pb = (PBYTE)memRLE.p;
            size_t dwSize = memRLE.m_dwCurPosn;
            //DWORD dwSize = MMLBinaryFromString (psz, pb, memRLE.m_dwAllocated);
            m_lLEXSHARDNGRAM.Init (sizeof(LEXSHARDNGRAM), pb,
               (DWORD)(dwSize / sizeof(LEXSHARDNGRAM)));
         }
      } // shard
   } // i

   // read in words
   // BUGFIX - Use binary MMLValueGet
   MMLValueGetBinary (pNode, gpszWords, &m_memWords);
   //psz = MMLValueGet (pNode, gpszWords);
   size_t dwTotal = m_memWords.m_dwCurPosn;
   m_memWords.m_dwCurPosn = 0;
   // dwTotal = psz ? (wcslen(psz)/2) : 0;   // since 2 per...
   //if (!m_memWords.Required (dwTotal))
   //   return FALSE;
   //if (dwTotal != MMLValueGetBinary (pNode, gpszWords, (PBYTE) m_memWords.p,dwTotal))
   //   return FALSE;
   while (m_memWords.m_dwCurPosn < dwTotal) {
      size_t dwCur = m_memWords.m_dwCurPosn;
      PBYTE pSrc = (PBYTE)m_memWords.p + dwCur;
      DWORD dwSize = WordSize (pSrc);

      m_lWords.Add (&dwCur);
      m_memWords.m_dwCurPosn += dwSize;
   }

   // will need to sort phones
   PhonemeSort ();

   // open master lexicon... already know that m_pMasterLex is NULL
   if (m_fExceptions) {
      // apply the code to back-off to other directories in case the master lex
      // cant be found
      if (!LexiconExists (m_szMaster, pszSrcFile))
         return FALSE;

      m_pMasterLex = MLexiconCacheOpen (m_szMaster, FALSE);
   }


   return TRUE;
}


/*************************************************************************************
CMLexicon::MasterLexGet - This returns a pointer to the master lexicon string if
this is an exceptions lexicon. If it isn't it returns null.

returns
   PWSTR - Pointer to string (dont modify) of master lexicon, or NULL if not exceptions
*/
PWSTR CMLexicon::MasterLexGet (void)
{
   if (!m_fExceptions)
      return NULL;
   return m_szMaster;
}


/*************************************************************************************
CMLexicon::MasterLexExists - Returns TRUE if the master lexicon exists, FALSE
if it isn't loaded
*/
BOOL CMLexicon::MasterLexExists (void)
{
   return (m_pMasterLex ? TRUE : FALSE);
}


/*************************************************************************************
CMLexicon::MasterLexSet - Sets the master lexicon to a new value.

inputs
   PWSTR          pszFile - New file. Use NULL if no master lex.
returns
   BOOL - No matter what the file is saved, but this returns TRUE if the master
      lexicon exists, FALSE if it doesnt
*/
BOOL CMLexicon::MasterLexSet (PWSTR pszFile)
{
   // free up current master lex
   if (m_pMasterLex) {
      MLexiconCacheClose (m_pMasterLex);
      m_pMasterLex = NULL;
   }

   // new master lex...
   if (!pszFile) {
      m_szMaster[0] = 0;
      m_fExceptions = FALSE;
      m_fDirty = TRUE;
      return TRUE;
   }

   // new file
   wcscpy (m_szMaster, pszFile);
   m_fExceptions = TRUE;
   m_fDirty = TRUE;
   m_pMasterLex = MLexiconCacheOpen (m_szMaster, FALSE);

   return (m_pMasterLex ? TRUE : FALSE);
}

/*************************************************************************************
CMLexicon::Save - Saves the lexicon to disk using th ename in m_szFile
*/
BOOL CMLexicon::Save (void)
{
   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;

   BOOL fRet;
   fRet = MMLFileSave (m_szFile, &GUID_MLexicon, pNode);
   delete pNode;
   if (!fRet)
      return FALSE;

   m_fDirty = FALSE;

   return TRUE;
}

/*************************************************************************************
CMLexicon::Load - Loads the lexicon from disk. The file name must be stored
in m_szFile

returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::Load (void)
{
   if (!m_szFile[0])
      return FALSE;

   // BUGFIX - Pass in ignore dir for MML
   PCMMLNode2 pNode = MMLFileOpen (m_szFile, &GUID_MLexicon, NULL, TRUE);
   if (!pNode)
      return FALSE;

   if (!MMLFrom(pNode, m_szFile, TRUE)) {
      delete pNode;
      return FALSE;
   }

   delete pNode;
   return TRUE;
}

/*************************************************************************************
CMLexicon::MemoryTouch - Touches a random piece of memory. Can be used to ensure
that TTS voices stay in memory.
*/
DWORD CMLexicon::MemoryTouch (void)
{
   // pick a random word and get it
   DWORD dwNum = WordNum();
   if (!dwNum)
      return 0;   // no words

   DWORD dwIndex = (DWORD)rand();
   if (dwNum > 32767)
      dwIndex *= 1000;
   dwIndex = dwIndex % dwNum;

   CListVariable lForm;
   WCHAR szWord[256];
   WordGet (dwIndex, szWord, sizeof(szWord), &lForm);
   
   return szWord[0];
}

/*************************************************************************************
CMLexicon::LangIDGet - Returns the language ID of the lexicon
*/
LANGID CMLexicon::LangIDGet (void)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->LangIDGet();
      else
         return 0;
   }

   return m_LangID;
}

/*************************************************************************************
CMLexicon::LangIDSet - Sets the language ID of the lexicon
*/
void CMLexicon::LangIDSet (LANGID LangID)
{
   if (m_fExceptions)
      return;  // dont set

   m_LangID = LangID;
   m_fDirty = TRUE;
}

/*************************************************************************************
CMLexicon::GUIDGet - Returns a pointer to the GUID. do NOT change the guid though
*/
GUID *CMLexicon::GUIDGet (void)
{
   return &m_gID;
}




/*************************************************************************************
MLexiconOpenDialog - Dialog box for opening MLexicon

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL MLexiconOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
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
   ofn.lpstrFilter = "Lexicon (*.mlx)\0*.mlx\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the lexicon" : "Open a lexicon";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mlx";
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



/****************************************************************************
LexMainPage
*/
static BOOL LexMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;
         if (!_wcsicmp(psz, L"grammar")) {
            // with new lexicon, fall through
            break;   // fall through
         }
         else if (!_wcsicmp(psz, L"lts")) {
            // if already has training then ask if want to add to
            if (pLex->ShardSomeTraining()) {
               int iRet;
               iRet = pPage->MBYesNo (
                  L"You already have some pronunciation training. Do you want to add to this existing training?",
                  L"The usual answer is \"Yes\", but you can press \"No\" if you wish "
                  L"to clear out your existing training and start over again.",
                  TRUE);
               if (iRet == IDCANCEL)
                  return TRUE;
               if (iRet == IDNO)
                  pLex->ShardClear();   // clear out
            }
            break;   // fall through
         }
         else if (!_wcsicmp(psz, L"newmaster")) {
            WCHAR szTemp[256];
            wcscpy (szTemp, pLex->MasterLexGet());

            if (!MLexiconOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            if (!_wcsicmp(pLex->m_szFile, szTemp)) {
               pPage->MBWarning (L"The lexicon cannot use itself as a master lexicon.");
               return TRUE;
            }

            // set it
            pLex->MasterLexSet (szTemp);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"scan")) {
            // open...
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
            ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Read word pronunciations from a text file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "txt";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;

            // scan it in
            if (!pLex->WordsFromFile (ofn.lpstrFile, pPage))
               pPage->MBWarning (L"The file couldn't be opened.");

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lexicon main page";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFNDEFEXPECTIONS")) {
            PWSTR psz = pLex->MasterLexGet();
            p->pszSubString = psz ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFNDEFEXPECTIONS")) {
            PWSTR psz = pLex->MasterLexGet();
            p->pszSubString = psz ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFDEFEXPECTIONS")) {
            PWSTR psz = pLex->MasterLexGet();
            p->pszSubString = psz ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFDEFEXPECTIONS")) {
            PWSTR psz = pLex->MasterLexGet();
            p->pszSubString = psz ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MASTERLEX")) {
            PWSTR psz = pLex->MasterLexGet();
            p->pszSubString = (psz && pLex->MasterLexExists()) ? psz : L"NO MASTER LEXICON (you MUST choose one)";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
LexImportUnisynPage
*/
static BOOL LexImportUnisynPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         pPage->Message (ESCM_USER+101);
      }
      return TRUE;   // to bypass defpage acceleartor

   case ESCM_USER+101:   // set fields based on defaults and registry
      {
         PCEscControl pControl;
         CMem mem;
         DWORD dw;

         LexRegGet (L"ImportUnisynFile", &mem, L"");
         if (pControl = pPage->ControlFind (L"file"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPhoneStress", &mem, gpszImportUnisynPhoneStress);
         if (pControl = pPage->ControlFind (L"phonestress"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPhoneNoStress", &mem, gpszImportUnisynPhoneNoStress);
         if (pControl = pPage->ControlFind (L"phonenostress"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynStress0", &mem, gpszImportUnisynStress0);
         if (pControl = pPage->ControlFind (L"stress0"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynStress1", &mem, gpszImportUnisynStress1);
         if (pControl = pPage->ControlFind (L"stress1"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynStress2", &mem, gpszImportUnisynStress2);
         if (pControl = pPage->ControlFind (L"stress2"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynStress3", &mem, gpszImportUnisynStress3);
         if (pControl = pPage->ControlFind (L"stress3"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynSymbolIgnore", &mem, gpszImportUnisynSymbolIgnore);
         if (pControl = pPage->ControlFind (L"symbolignore"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSNoun", &mem, gpszImportUnisynPOSNoun);
         if (pControl = pPage->ControlFind (L"POSNoun"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSPronoun", &mem, gpszImportUnisynPOSPronoun);
         if (pControl = pPage->ControlFind (L"POSPronoun"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSAdjective", &mem, gpszImportUnisynPOSAdjective);
         if (pControl = pPage->ControlFind (L"POSAdjective"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSPreposition", &mem, gpszImportUnisynPOSPreposition);
         if (pControl = pPage->ControlFind (L"POSPreposition"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSArticle", &mem, gpszImportUnisynPOSArticle);
         if (pControl = pPage->ControlFind (L"POSArticle"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSVerb", &mem, gpszImportUnisynPOSVerb);
         if (pControl = pPage->ControlFind (L"POSVerb"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSAdverb", &mem, gpszImportUnisynPOSAdverb);
         if (pControl = pPage->ControlFind (L"POSAdverb"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSAuxVerb", &mem, gpszImportUnisynPOSAuxVerb);
         if (pControl = pPage->ControlFind (L"POSAuxVerb"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSConjunction", &mem, gpszImportUnisynPOSConjunction);
         if (pControl = pPage->ControlFind (L"POSConjunction"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPOSInterjection", &mem, gpszImportUnisynPOSInterjection);
         if (pControl = pPage->ControlFind (L"POSInterjection"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportUnisynPhoneSpaces", NULL, NULL, &dw, 1);
         if (pControl = pPage->ControlFind (L"PhoneSpaces"))
            pControl->AttribSetBOOL (Checked(), dw);

         LexRegGet (L"ImportUnisynStressSyllable", NULL, NULL, &dw, 0);
         if (pControl = pPage->ControlFind (L"StressSyllable"))
            pControl->AttribSetBOOL (Checked(), dw);

         LexRegGet (L"ImportUnisynStressRemap", NULL, NULL, &dw, 1);
         if (pControl = pPage->ControlFind (L"StressRemap"))
            pControl->AttribSetBOOL (Checked(), dw);

      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         CMem mem;
         DWORD dwNeeded = 0;
         p->pControl->AttribGet (Text(), NULL, 0, &dwNeeded);
         if (!mem.Required (dwNeeded))
            return FALSE;
         p->pControl->AttribGet (Text(), (PWSTR) mem.p, dwNeeded, &dwNeeded);

         if (!_wcsicmp(psz, L"file"))
            LexRegSet (L"ImportUnisynFile", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"PhoneStress"))
            LexRegSet (L"ImportUnisynPhoneStress", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"PhoneNoStress"))
            LexRegSet (L"ImportUnisynPhoneNoStress", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"Stress0"))
            LexRegSet (L"ImportUnisynStress0", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"Stress1"))
            LexRegSet (L"ImportUnisynStress1", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"Stress2"))
            LexRegSet (L"ImportUnisynStress2", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"Stress3"))
            LexRegSet (L"ImportUnisynStress3", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"SymbolIgnore"))
            LexRegSet (L"ImportUnisynSymbolIgnore", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSNoun"))
            LexRegSet (L"ImportUnisynPOSNoun", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSPronoun"))
            LexRegSet (L"ImportUnisynPOSPronoun", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSAdjective"))
            LexRegSet (L"ImportUnisynPOSAdjective", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSPreposition"))
            LexRegSet (L"ImportUnisynPOSPreposition", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSArticle"))
            LexRegSet (L"ImportUnisynPOSArticle", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSVerb"))
            LexRegSet (L"ImportUnisynPOSVerb", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSAdverb"))
            LexRegSet (L"ImportUnisynPOSAdverb", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSAuxVerb"))
            LexRegSet (L"ImportUnisynPOSAuxVerb", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSConjunction"))
            LexRegSet (L"ImportUnisynPOSConjunction", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"POSInterjection"))
            LexRegSet (L"ImportUnisynPOSInterjection", (PWSTR)mem.p);
         else
            return FALSE;

         return TRUE;
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"Next")) {
            if (pLex->LexImportUnisyn (pPage)) {
               pPage->MBInformation (L"The lexicon has been imported.",
                  L"You should review and rename the phonemes, as well as specifying their English equivalents. "
                  L"You should also do new pronunciation training.");
               pPage->Exit (Back());
               return TRUE;
            }
            else  // error, which would be displayed, so just return to page
               return TRUE;
         }
         else if (!_wcsicmp (psz, L"PhoneSpaces")) {
            DWORD dw = p->pControl->AttribGetBOOL(Checked());
            LexRegSet (L"ImportUnisynPhoneSpaces", NULL, &dw);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"StressSyllable")) {
            DWORD dw = p->pControl->AttribGetBOOL(Checked());
            LexRegSet (L"ImportUnisynStressSyllable", NULL, &dw);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"StressRemap")) {
            DWORD dw = p->pControl->AttribGetBOOL(Checked());
            LexRegSet (L"ImportUnisynStressRemap", NULL, &dw);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"filedialog")) {
            // open...
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
            ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0\0\0";
            ofn.lpstrFile = szTemp;
            ofn.nMaxFile = sizeof(szTemp);
            ofn.lpstrTitle = "Read word pronunciations from a text file";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrDefExt = "txt";
            // nFileExtension 

            if (!GetOpenFileName(&ofn))
               return TRUE;

            WCHAR szw[256];
            MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));

            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"File"))
               pControl->AttribSet (Text(), szw);

            LexRegSet (L"ImportUnisynFile", szw);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"defaults")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to restore default?",
               L"This will delete all the changes you have made on this page."))
               return TRUE;

            // dont ersase this: LexRegSet (L"ImportUnisynFile", NULL);
            LexRegSet (L"ImportUnisynPhoneStress", NULL);
            LexRegSet (L"ImportUnisynPhoneNoStress", NULL);
            LexRegSet (L"ImportUnisynStress0", NULL);
            LexRegSet (L"ImportUnisynStress1", NULL);
            LexRegSet (L"ImportUnisynStress2", NULL);
            LexRegSet (L"ImportUnisynStress3", NULL);
            LexRegSet (L"ImportUnisynSymbolIgnore", NULL);
            LexRegSet (L"ImportUnisynPOSNoun", NULL);
            LexRegSet (L"ImportUnisynPOSPronoun", NULL);
            LexRegSet (L"ImportUnisynPOSAdjective", NULL);
            LexRegSet (L"ImportUnisynPOSPreposition", NULL);
            LexRegSet (L"ImportUnisynPOSArticle", NULL);
            LexRegSet (L"ImportUnisynPOSVerb", NULL);
            LexRegSet (L"ImportUnisynPOSAdverb", NULL);
            LexRegSet (L"ImportUnisynPOSAuxVerb", NULL);
            LexRegSet (L"ImportUnisynPOSConjunction", NULL);
            LexRegSet (L"ImportUnisynPOSInterjection", NULL);
            LexRegSet (L"ImportUnisynPhoneSpaces", NULL);
            LexRegSet (L"ImportUnisynStressSyllable", NULL);
            LexRegSet (L"ImportUnisynStressRemap", NULL);

            // refresh edit
            pPage->Message (ESCM_USER+101);
            return TRUE;
         }
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogScanUnisyn - Import unisyn text file

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogScanUnisyn (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXIMPORTUNISYN, LexImportUnisynPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return FALSE;
}


/****************************************************************************
LexImportMandarinPage
*/
static BOOL LexImportMandarinPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         pPage->Message (ESCM_USER+101);
      }
      return TRUE;   // to bypass defpage acceleartor

   case ESCM_USER+101:   // set fields based on defaults and registry
      {
         PCEscControl pControl;
         CMem mem;
         DWORD dw;

         LexRegGet (L"ImportMandarinDir", &mem, L"");
         if (pControl = pPage->ControlFind (L"dir"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportMandarinUttData", &mem, L"c:\\temp\\Utt.Data");
         if (pControl = pPage->ControlFind (L"uttdata"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportMandarinPhoneInitial", &mem, gpszImportMandarinPhoneInitial);
         if (pControl = pPage->ControlFind (L"phoneinitial"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportMandarinPhoneFinal", &mem, gpszImportMandarinPhoneFinal);
         if (pControl = pPage->ControlFind (L"phonefinal"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportMandarinNumTones", NULL, NULL, &dw, 5);
         DoubleToControl (pPage, L"numtones", dw);

      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         CMem mem;
         DWORD dwNeeded = 0;
         p->pControl->AttribGet (Text(), NULL, 0, &dwNeeded);
         if (!mem.Required (dwNeeded))
            return FALSE;
         p->pControl->AttribGet (Text(), (PWSTR) mem.p, dwNeeded, &dwNeeded);

         if (!_wcsicmp(psz, L"dir"))
            LexRegSet (L"ImportMandarinDir", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"uttdata"))
            LexRegSet (L"ImportMandarinUttData", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"phoneinitial"))
            LexRegSet (L"ImportMandarinPhoneInitial", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"phonefinal"))
            LexRegSet (L"ImportMandarinPhoneFinal", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"numtones")) {
            DWORD dw = (DWORD)DoubleFromControl (pPage, psz);
            LexRegSet (L"ImportMandarinNumTones", NULL, &dw);
            return FALSE;
         }

         return TRUE;
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"Next")) {
            if (pLex->LexImportMandarin (pPage)) {
               pPage->MBInformation (L"The lexicon has been imported.",
                  L"You should review and rename the phonemes, as well as specifying their English equivalents. "
                  L"You should also do new pronunciation training.");
               pPage->Exit (Back());
               return TRUE;
            }
            else  // error, which would be displayed, so just return to page
               return TRUE;
         }
         else if (!_wcsicmp (psz, L"defaults")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to restore default?",
               L"This will delete all the changes you have made on this page."))
               return TRUE;

            // dont ersase this: LexRegSet (L"ImportUnisynFile", NULL);
            LexRegSet (L"ImportMandarinDir", NULL);
            LexRegSet (L"ImportMandarinUttData", NULL);
            LexRegSet (L"ImportMandarinPhoneInitial", NULL);
            LexRegSet (L"ImportMandarinPhoneFinal", NULL);
            LexRegSet (L"ImportMandarinNumTones", NULL);

            // refresh edit
            pPage->Message (ESCM_USER+101);
            return TRUE;
         }
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogScanMandarin - Import unisyn text file

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogScanMandarin (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXIMPORTMANDARIN, LexImportMandarinPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return FALSE;
}




/****************************************************************************
LexImportMandarin2Page
*/
static BOOL LexImportMandarin2Page (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         pPage->Message (ESCM_USER+101);
      }
      return TRUE;   // to bypass defpage acceleartor

   case ESCM_USER+101:   // set fields based on defaults and registry
      {
         PCEscControl pControl;
         CMem mem;

         LexRegGet (L"ImportMandarin2U8File", &mem, L"");
         if (pControl = pPage->ControlFind (L"u8file"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);

         LexRegGet (L"ImportMandarin2EnglishLex", &mem, L"c:\\program files\\mxac\\3D Outside the box\\EnglishInstalled.mlx");
         if (pControl = pPage->ControlFind (L"englishlex"))
            pControl->AttribSet (Text(), (PWSTR)mem.p);
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         CMem mem;
         DWORD dwNeeded = 0;
         p->pControl->AttribGet (Text(), NULL, 0, &dwNeeded);
         if (!mem.Required (dwNeeded))
            return FALSE;
         p->pControl->AttribGet (Text(), (PWSTR) mem.p, dwNeeded, &dwNeeded);

         if (!_wcsicmp(psz, L"u8file"))
            LexRegSet (L"ImportMandarin2U8File", (PWSTR)mem.p);
         else if (!_wcsicmp(psz, L"englishlex"))
            LexRegSet (L"ImportMandarin2EnglishLex", (PWSTR)mem.p);
         return TRUE;
      }

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp (psz, L"Next")) {
            if (pLex->LexImportMandarin2 (pPage)) {
               pPage->MBInformation (L"The lexicon has been imported.");
               pPage->Exit (Back());
               return TRUE;
            }
            else  // error, which would be displayed, so just return to page
               return TRUE;
         }
         else if (!_wcsicmp (psz, L"defaults")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to restore default?",
               L"This will delete all the changes you have made on this page."))
               return TRUE;

            // dont ersase this: LexRegSet (L"ImportUnisynFile", NULL);
            LexRegSet (L"ImportMandarin2U8File", NULL);
            LexRegSet (L"ImportMandarin2EnglishLex", NULL);

            // refresh edit
            pPage->Message (ESCM_USER+101);
            return TRUE;
         }
      }

   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CMLexicon::DialogScanMandarin2 - Import unisyn text file

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogScanMandarin2 (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXIMPORTMANDARIN2, LexImportMandarin2Page, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return FALSE;
}


/*************************************************************************************
CMLexicon::DialogMain - Brings up the main lexicon editing dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogMain (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXMAIN, LexMainPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   if (!_wcsicmp(pszRet, L"langid")) {
      if (DialogLangID(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"phonemes")) {
      if (DialogPhone(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"pronmain")) {
      if (DialogPronMain(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"grammar")) {
      if (DialogGrammar(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"lts")) {
      if (DialogLTS(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"scanunisyn")) {
      if (DialogScanUnisyn(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"scanmandarin")) {
      if (DialogScanMandarin(pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"scanmandarin2")) {
      if (DialogScanMandarin2(pWindow))
         goto redo;
      else
         return FALSE;
   }

   return FALSE;
}

/****************************************************************************
LexLangIDPage
*/
static BOOL LexLangIDPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"langid", pLex->LangIDGet());
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"langid")) {
            DWORD dwVal = p->pszName ? _wtoi(p->pszName) : 0;
            if (dwVal == pLex->LangIDGet())
               break;   // no change

            // else change
            pLex->LangIDSet (dwVal);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lexicon language";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogLangID - Brings up the langiD dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogLangID (PCEscWindow pWindow)
{
   if (m_fExceptions)
      return TRUE; // disable

   PWSTR pszRet;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXLANGID, LexLangIDPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

/*************************************************************************************
CMLexicon::PhonemeSort - This is an internal function that sorts the phonemes
alphabetically so they're fast to seach through. It must be called through after
the phonemes have been modified
*/
static int __cdecl LEXPHONECompare (const void *p1, const void *p2)
{
   PLEXPHONE pp1 = *((PLEXPHONE*) p1);
   PLEXPHONE pp2 = *((PLEXPHONE*) p2);
   int iRet = _wcsnicmp(pp1->szPhoneLong, pp2->szPhoneLong, sizeof(pp1->szPhoneLong)/sizeof(WCHAR));
   return iRet;
}

void CMLexicon::PhonemeSort (void)
{
   // allow to sort even if exceptions, even though wont matter

   DWORD i;
   PLEXPHONE plp = (PLEXPHONE) m_lLEXPHONE.Get(0);
   m_lPLEXPHONESort.Clear();

   // add silence
   PLEXPHONE plpSilence = &gLexPhoneSilence;
   m_lPLEXPHONESort.Add (&plpSilence);

   // other phones
   m_lPLEXPHONESort.Required (m_lPLEXPHONESort.Num() + m_lLEXPHONE.Num());
   for (i = 0; i < m_lLEXPHONE.Num(); i++, plp++)
      m_lPLEXPHONESort.Add (&plp);

   // sort it
   qsort (m_lPLEXPHONESort.Get(0), m_lPLEXPHONESort.Num(), sizeof(PLEXPHONE), LEXPHONECompare);

   // done
}

/*************************************************************************************
CMLexicon::PhonemeSilence - Returns the silence phoneme string.

returns
   PWSTR - Silence phoneme string
*/
PCWSTR CMLexicon::PhonemeSilence (void)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeSilence();
      // else just fall though
   }

   return gszSilence;
}


/*************************************************************************
CMLexicon::PhonemeFind - Given a phoneme string, this finds the index to it.

inputs
   WCHAR        *pszName - Phoneme name
returns
   DWORD - Index, or -1 if cant find. Sorted phoneme number
*/
DWORD CMLexicon::PhonemeFind (PCWSTR pszName)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeFind(pszName);
      else
         return -1;
   }

   LEXPHONE pi;
   PLEXPHONE plp;
   wcscpy (pi.szPhoneLong, pszName);  // can overrun a bit because followed by sample string
   plp = &pi;

   PLEXPHONE ppi;
   ppi = (PLEXPHONE) bsearch (&plp, m_lPLEXPHONESort.Get(0), m_lPLEXPHONESort.Num(), sizeof(PLEXPHONE), LEXPHONECompare);
      // BUGFIX - Was &pi
   if (!ppi)
      return -1;
   return (DWORD)((PBYTE)ppi - (PBYTE)m_lPLEXPHONESort.Get(0)) / sizeof(PLEXPHONE);
}

/*************************************************************************
CMLexicon::PhonemeFindUnsort - Given a phoneme string, this finds the index to it.

inputs
   WCHAR        *pszName - Phoneme name
returns
   DWORD - Index, or -1 if cant find. UNSORTED phoneme number. LEXPHONE_SILENCE for silence
*/
DWORD CMLexicon::PhonemeFindUnsort (PCWSTR pszName)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeFindUnsort(pszName);   // BUGFIX - Was phonemefind
      else
         return -1;
   }

   if (!_wcsicmp(pszName, PhonemeSilence()))
      return LEXPHONE_SILENCE;
   DWORD i;
   PLEXPHONE plp = (PLEXPHONE) m_lLEXPHONE.Get(0);
   for (i = 0; i < m_lLEXPHONE.Num(); i++)
      if (!_wcsicmp(plp[i].szPhoneLong, pszName))
         return i;

   // else none
   return -1;
}


/*************************************************************************
CMLexicon::PhonemeGet - Returns the phoneme information for a phoneme based on the
index.

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-1. Sorted phoneme number
returns
   PLEXPHONE  - information. Do NOT change th einfor
*/
PLEXPHONE CMLexicon::PhonemeGet (DWORD dwIndex)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeGet(dwIndex);
      else
         return NULL;
   }

   PLEXPHONE *pplp = (PLEXPHONE*)m_lPLEXPHONESort.Get(dwIndex);
   if (!pplp)
      return NULL;
   return pplp[0];
}


/*************************************************************************
CMLexicon::Streeses - Returns the number of stresses in the lexicon.
This will be 5 for chinese, 2 (usually) for english
*/
DWORD CMLexicon::Stresses (void)
{
   // see if already calculated
   if (m_dwStresses)
      return m_dwStresses;

   if (m_pMasterLex)
      return m_pMasterLex->Stresses();

   // find
   DWORD dwSum = 0;
   DWORD i;
   for (i = 0; i < PhonemeNum(); i++) {
      PLEXPHONE plp = PhonemeGetUnsort (i);
      if (!plp)
         continue;

      dwSum = max(dwSum, (DWORD)plp->bStress);
   }
   dwSum++;

   m_dwStresses = dwSum;

   _ASSERTE (m_dwStresses <= MAXSTRESSES);

   _ASSERTE (m_dwStresses >= 2);

   return m_dwStresses;
}

/*************************************************************************
CMLexicon::PhonemeGetUnsort - Returns the unsorted phoneme information for a phoneme based on the
index.

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-2. Unsorted phoneme number. 245 for silence
returns
   PLEXPHONE  - information. Do NOT change th einfor
*/
PLEXPHONE CMLexicon::PhonemeGetUnsort (DWORD dwIndex)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeGetUnsort(dwIndex);
      else
         return NULL;
   }

   if (dwIndex == LEXPHONE_SILENCE)
      return &gLexPhoneSilence;

   return (PLEXPHONE) m_lLEXPHONE.Get(dwIndex);
}


/*************************************************************************
CMLexicon::PhonemeGet - Returns the phoneme information for a phoneme based on the
string.

inputs
   char        *pszName - Phoneme name
returns
   PLEXPHONE  - information. Do NOT change th einfor
*/
PLEXPHONE CMLexicon::PhonemeGet (PCWSTR pszName)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeGet(pszName);
      else
         return NULL;
   }

   DWORD dwIndex = PhonemeFind (pszName);
   return PhonemeGet (dwIndex);
}

/*************************************************************************
CMLexicon::PhonemeNum - Returns the number of phonemes
*/
DWORD CMLexicon::PhonemeNum (void)
{
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->PhonemeNum();
      else
         return -1;
   }

   return m_lPLEXPHONESort.Num();
}

/*************************************************************************
CMLexicon::PhonemeAddBlank - Adds a blank phoneme calls "*new"

inputs
   PWSTR          pszName - If NULL use "*new", else use this. This can only be
                  4 chars (+ term null) long
*/
void CMLexicon::PhonemeAddBlank (PWSTR pszName)
{
   if (m_fExceptions)
      return;  // cant add phonemes

   LEXPHONE lp;
   memset (&lp, 0, sizeof(lp));
   lp.bEnglishPhone = 0;
   lp.bStress = 0;
   wcscpy (lp.szPhoneLong, pszName ? pszName : L"*new");
   wcscpy (lp.szSampleWord, L"sample");
   lp.wPhoneOtherStress = 255;

   m_lLEXPHONE.Add (&lp);
   PhonemeSort();
   m_fDirty = TRUE;
}


/*************************************************************************
CMLexicon::PhonemeToGroup - Gets a group number given a phoneme

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-2. Unsorted phoneme number. 245 for silence
returns
   DWORD - Group number, 0..PIS_PHONEGROUPNUM-1. If unknown then returns group for silence (0)
*/
DWORD CMLexicon::PhonemeToGroup (DWORD dwIndex)
{
   PLEXPHONE plp = PhonemeGetUnsort (dwIndex);
   if (!plp)
      return 0;

   PLEXENGLISHPHONE ple = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
   if (!ple)
      return 0;

   return PIS_FROMPHONEGROUP (ple->dwShape);
}


/*************************************************************************
CMLexicon::PhonemeToEnglishPhone - Convert a phoneme to the english phone

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-2. Unsorted phoneme number. 245 for silence
   DWORD       *pdwStress - Filled in with the stress
returns
   DWORD - English phone index
*/
DWORD CMLexicon::PhonemeToEnglishPhone (DWORD dwIndex, DWORD *pdwStress)
{
   PLEXPHONE plp = PhonemeGetUnsort (dwIndex);
   if (pdwStress)
      *pdwStress = plp ? plp->bStress : 0;
   return (plp ? plp->bEnglishPhone : 0);

}


/*************************************************************************
CMLexicon::PhonemeToMegaGroup - Gets a mega-group number given a phoneme

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-2. Unsorted phoneme number. 245 for silence
returns
   DWORD - Group number, 0..PIS_PHONEMEGAGROUPNUM-1. If unknown then returns group for silence (0)
*/
DWORD CMLexicon::PhonemeToMegaGroup (DWORD dwIndex)
{
   return LexPhoneGroupToMega (PhonemeToGroup(dwIndex));
}



/*************************************************************************
CMLexicon::PhonemeToUnstressed - Gets the unstressed version of the phoneme

inputs
   DWORD       dwIndex - 0 to PhonemeNum()-2. Unsorted phoneme number. 245 for silence
returns
   DWORD - Unstressed version of phoneme, or dwIndex if this is unstrtessd
*/
DWORD CMLexicon::PhonemeToUnstressed (DWORD dwIndex)
{
   PLEXPHONE plp = PhonemeGetUnsort (dwIndex);
   if (!plp)
      return dwIndex;

   if (plp->bStress)
      return plp->wPhoneOtherStress;

   // else, just this
   return dwIndex;
}


/*************************************************************************************
CMLexicon::PhonenemeGroupToPhone - Given a phoneme group, come up with a list
of phonemes to represent it.

inputs
   DWORD             dwGroup - Phoneme group (0..16)
   BOOL              fIncludeStresed - If TRUE then include stressed versions of phonemes also
   PCListFixed       plRet - Initialized to sizeof(DWORD) and filled with phonemes.
                        If NULL, then not filled in
returns
   DWORD - One phoneme
*/
DWORD CMLexicon::PhonemeGroupToPhone (DWORD dwGroup, BOOL fIncludeStressed, PCListFixed plRet)
{
   if (plRet)
      plRet->Init (sizeof(DWORD));

   // group 0 is always silence
   DWORD i;
   if (!dwGroup) {
      i = 254;
      if (plRet)
         plRet->Add (&i);
      return i; // silence
   }

   // else, search
   for (i = 0; i < PhonemeNum(); i++) {
      PLEXPHONE plp = PhonemeGetUnsort (i);
      if (!fIncludeStressed && plp && plp->bStress)
         continue; // dont bother showing stressed phonemes

      if (PhonemeToGroup(i) != dwGroup)
         continue;

      // else, match
      if (!plRet)
         return i;   // found right away
      plRet->Add (&i);
   } // i

   return (plRet && plRet->Num()) ? *((DWORD*)plRet->Get(0)) : (DWORD)-1;
}


/*************************************************************************************
CMLexicon::PhonemeGroupToPhoneString - Create a list of strings for the allowed
phonemes.

inputs
   DWORD             dwGroup - Phoneme group (0..16)
   PWSTR             *psz - Filled in
   DWORD             dwBytes - number of bytes in psz
returns
   none
*/
void CMLexicon::PhonemeGroupToPhoneString (DWORD dwGroup, PWSTR psz, DWORD dwBytes)
{
   psz[0] = 0;

   CListFixed lPhones;
   PhonemeGroupToPhone (dwGroup, FALSE, &lPhones);

   // fill this in
   DWORD i;
   DWORD *padwPhones = (DWORD*)lPhones.Get(0);
   DWORD dwLen = 1;  // include terminating 0
   for (i = 0; i < lPhones.Num(); i++) {
      if (i && (dwLen + 2 < dwBytes/sizeof(WCHAR)) )
         wcscat (psz, L", ");

      PLEXPHONE plp = PhonemeGetUnsort (padwPhones[i]);

      if (plp && (dwLen + wcslen(plp->szPhoneLong) < dwBytes/sizeof(WCHAR)) )
         wcscat (psz, plp->szPhoneLong);
   } // i
}

void CMLexicon::PhonemeGroupToPhoneString (DWORD dwGroup, PSTR psz, DWORD dwBytes)
{
   WCHAR szTemp[512];
   PhonemeGroupToPhoneString (dwGroup, szTemp, sizeof(szTemp));
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, psz, dwBytes, 0, 0);
}

/****************************************************************************
LexPhonePage
*/
static BOOL LexPhonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCMLexicon pLex = (PCMLexicon) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // write the phones out
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[64];
         PLEXPHONE plp;
         for (i = 0; i < pLex->PhonemeNum(); i++) {
            plp = pLex->PhonemeGet(i);
            if (!plp)
               continue;

            // phoneme
            swprintf (szTemp, L"nm:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (Text(), plp->szPhoneLong);

            // sample word
            swprintf (szTemp, L"sw:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSet (Text(), plp->szSampleWord);

            // stress
            swprintf (szTemp, L"sn:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            PLEXPHONE pStress = pLex->PhonemeGetUnsort (plp->wPhoneOtherStress);
            if (pControl && pStress)
               pControl->AttribSet (Text(), pStress->szPhoneLong);

            // englush phone
            swprintf (szTemp, L"ep:%d", (int)i);
            ComboBoxSet (pPage, szTemp, (int)plp->bEnglishPhone);

            // stress info
            swprintf (szTemp, L"st:%d", (int)i);
            ComboBoxSet (pPage, szTemp, (int)plp->bStress);
         } // i
      }
      break;

   case ESCM_USER+82:   // get the phone info
      {
         DWORD i;
         PCEscControl pControl;
         WCHAR szTemp[64];
         PLEXPHONE plp;
         DWORD dwNeed;
         ESCMCOMBOBOXGETITEM gi;
         memset (&gi, 0, sizeof(gi));

         for (i = 0; i < pLex->PhonemeNum(); i++) {
            plp = pLex->PhonemeGet(i);
            if (!plp)
               continue;

            // phoneme
            swprintf (szTemp, L"nm:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribGet (Text(), plp->szPhoneLong, sizeof(plp->szPhoneLong), &dwNeed);

            // sample word
            swprintf (szTemp, L"sw:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribGet (Text(), plp->szSampleWord, sizeof(plp->szSampleWord), &dwNeed);

            // englush phone
            swprintf (szTemp, L"ep:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               gi.pszName = NULL;
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  plp->bEnglishPhone = (BYTE)_wtoi(gi.pszName);
            }

            // stress info
            swprintf (szTemp, L"st:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               gi.dwIndex = pControl->AttribGetInt (CurSel());
               gi.pszName = NULL;
               pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
               if (gi.pszName)
                  plp->bStress = (BYTE)_wtoi(gi.pszName);
            }
         } // i


         // go back and get the stress phone...
         DWORD j;
         for (i = 0; i < pLex->PhonemeNum(); i++) {
            plp = pLex->PhonemeGet(i);
            if (!plp)
               continue;

            // stress
            WCHAR szLink[16];
            swprintf (szTemp, L"sn:%d", (int)i);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;
            szLink[0] = 0;
            pControl->AttribGet (Text(), szLink, sizeof(szLink), &dwNeed);

            // find match
            for (j = 0; j < pLex->PhonemeNum(); j++) {
               PLEXPHONE plp2 = pLex->PhonemeGetUnsort (j);
               if (!plp2 || !szLink[0])
                  continue;
               if (!_wcsicmp(plp2->szPhoneLong, szLink))
                  break;
            }
            plp->wPhoneOtherStress = (j < pLex->PhonemeNum()) ? (WORD) j : 255;
         } // i

         // resort
         pLex->PhonemeSort();
         pLex->m_fDirty = TRUE;
      }
      return TRUE;

   case ESCM_CLOSE:
      // make sure to save entries
      pPage->Message (ESCM_USER+82);
      break;

   case ESCM_LINK:
      // make sure to save entries
      pPage->Message (ESCM_USER+82);
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!_wcsicmp(psz, L"add")) {
            if (pLex->PhonemeNum() >= LEXPHONE_SILENCE) {
               pPage->MBWarning (L"You can't add any more phonemes.");
               return TRUE;
            }

            // get contents
            pPage->Message (ESCM_USER+82);

            // add one..
            pLex->PhonemeAddBlank();

            // exit
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Lexicon phonemes";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONEDIT")) {
            MemZero (&gMemTemp);

            // loop over all the sorted
            DWORD i;
            PLEXPHONE plp;
            for (i = 0; i < pLex->PhonemeNum(); i++) {
               plp = pLex->PhonemeGet(i);
               if (!plp || (plp == &gLexPhoneSilence))
                  continue;

               // add...
               MemCat (&gMemTemp, L"<tr><td><bold><edit selall=true width=100%% maxchars=5 name=nm:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold></td><td><bold><edit selall=true width=100%% maxchars=16 name=sw:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold></td><td><xComboEnglishPhone name=ep:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td><td><xComboStressed name=st:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></td><td><bold><edit selall=true width=100%% maxchars=5 name=sn:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold></td></tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CMLexicon::DialogPhone - Brings up the phone editing dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPhone (PCEscWindow pWindow)
{
   if (m_fExceptions)
      return TRUE;   // cant edit phonemes

   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPHONE, LexPhonePage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}



/*************************************************************************************
CMLexicon::WordNum - Returns the number of words in the lexicon
*/
DWORD CMLexicon::WordNum (void)
{
   return m_lWords.Num();
}

/*************************************************************************************
CMLexicon::WordFind - Given a word string, this finds the index (from 0..WordNum()-1)
that matches. If the word cannot be found but fInsert is true this returns the
word number to insert before.

inputs
   PWSTR             pszWord - Word looking for
   BOOL              fInsert - If TRUE then this will return the index of the word
                        or just before it. If FALSE then it only returns if the word
                        is found.
returns
   DWORD - Index of word (or to insert before). -1 if can't find word
*/
DWORD CMLexicon::WordFind (PWSTR pszWord, BOOL fInsert)
{
   if (!pszWord)
      return -1;  // since cant find

   // figure out starting split
   DWORD dwCur, dwSplit, dwNum;
   dwNum = m_lWords.Num();
   for (dwSplit = 1; dwSplit < dwNum; dwSplit *= 2);

   // if there aren't any words tehn return 0
   if (!dwNum)
      return (fInsert ? 0 : -1);

   // binary search
   int iRet;
   DWORD *padwWords = (DWORD*) m_lWords.Get(0);
   DWORD dwTry;
   for (dwCur = 0; dwSplit; dwSplit /= 2) {
      // if too high don't bother
      dwTry = dwCur + dwSplit;
      if (dwTry >= dwNum)
         continue;

      iRet = _wcsicmp (pszWord, (PWSTR)((PBYTE)m_memWords.p + padwWords[dwTry]));
      if (iRet == 0)
         return dwTry;  // exact match
      else if (iRet > 0)
         dwCur = dwTry; // word occrs after the one in question so keep dwTry as next start
      // else, discard dwTry
   } // dwSplit

   // if get here then compare word against dwCur
   iRet = _wcsicmp (pszWord, (PWSTR)((PBYTE)m_memWords.p + padwWords[dwCur]));
   if (iRet == 0)
      return dwCur;  // shouldnt ever get this case, but just in case
   if (!fInsert)
      return -1;  // if not inserting done here

   if (iRet > 0)
      return dwCur+1;
   else
      return dwCur;
}


/*************************************************************************************
CMLexicon::WordExists - You should call this to see if a word is supported by the
lexicon or one of its parents.

inputs
   PWSTR          pszWord - word
returns
   BOOL - TRUE if exists
*/
BOOL CMLexicon::WordExists (PWSTR pszWord)
{
   DWORD dwIndex = WordFind (pszWord);
   if (dwIndex != -1)
      return TRUE;

   // Call into parents
   if (m_fExceptions) {
      if (m_pMasterLex)
         return m_pMasterLex->WordExists(pszWord);
      else
         return FALSE;
   }

   return FALSE;
}


/*************************************************************************************
CMLexicon::WordPronunciation - Gets a word's pronunciation. If the word isn't
in the lexicon it looks in the parent. If all else fails it uses letter to sound.

inputs
   PWSTR          pszWord - word
   PCListVariable plForm - Filled with list of word forms. For each form 1st byte is
                  part-of-speech, rest are null-terminated string.  Can be NULL
   BOOL           fUseLTS - If TRUE then use letter to sound, FALSE then fail if cant find in lex
   PCMLexcion     pMainLex - If there is a word reference in the pronunciation, then this
                  lexicon will be referenced. If this is NULL then references itself.
   PCListVariable plDontRecurse - List used by the lexicon to make sure it's not
                  recursing in on itself. This should either be empty (if it's
                  the first level calling), or filled with the list of word strings
                  that have already gotten the search this far.
                  
                  If this is NULL then the word prounciation will NOT convert word
                  references in the prounicaiton, with | and :.
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::WordPronunciation (PWSTR pszWord, PCListVariable plForm, BOOL fUseLTS,
                                   PCMLexicon pMainLex, PCListVariable plDontRecurse)
{
   CListVariable lTemp;
   DWORD dwIndex = WordFind (pszWord);
   if (dwIndex != -1) {
      if (!WordGet (dwIndex, NULL, 0, &lTemp))
         return FALSE;
      goto references;
   }

   // try parents
   if (m_fExceptions) {
      if (m_pMasterLex) {
         if (!m_pMasterLex->WordPronunciation(pszWord, &lTemp, fUseLTS, pMainLex ? pMainLex : this, plDontRecurse))
            return FALSE;
         goto references;
      }
      else
         return FALSE;
   }

   // try lts
   if (!m_fExceptions && fUseLTS) { // BUGFIX - Add check for fUseLTS
#if 0 // disable because not really using
      // BUGFIX - if all punctuation then ignore
      DWORD i;
      for (i = 0; pszWord[i]; i++)
         if (!iswpunct(pszWord[i]))
            break;
      if (!pszWord[i])
         return FALSE;
#endif // 0

      if (ChineseUse() && ChineseLTS (pszWord, &lTemp, plDontRecurse))
         goto references;

      return ShardLTS (pszWord, plForm);
   }

   return FALSE;

references:
   DWORD i;
   // this code is called when the word pronunciation has been gotten, but need
   // to look at references for code...

   if (!plDontRecurse) {
      // if dont have this list, then we can't get referenced word pronunciations,
      // so just copy over and be done with it
      plForm->Required (plForm->Num() + lTemp.Num());
      for (i = 0; i < lTemp.Num(); i++)
         plForm->Add (lTemp.Get(i), lTemp.Size(i));
      return TRUE;
   }

   // first of make sure that not recusing
   DWORD dwNumList = plDontRecurse->Num();
   for (i = 0; i < dwNumList; i++)
      if (!_wcsicmp((PWSTR) plDontRecurse->Get(i), pszWord))
         return FALSE;
   plDontRecurse->Add (pszWord, (wcslen(pszWord)+1)*sizeof(WCHAR));

   // loop through all the pronunciations so far and clean them up
   if (!pMainLex)
      pMainLex = this;
   plForm->Clear();
   for (i = 0; i < lTemp.Num(); i++)
      if (!LexExpand ((PBYTE)lTemp.Get(i), pMainLex, fUseLTS, plDontRecurse, plForm)) {
         plDontRecurse->Remove (dwNumList);
         return FALSE;
      }

   // remove the element that indicates if recursing
   plDontRecurse->Remove (dwNumList);

   return TRUE;
}


/*************************************************************************************
CMLexicon::WordGet - Gets a word given its index.

NOTE: This only gets words specific to this lexicon, not to its parents.

inputs
   DWORD          dwIndex - Index
   PWSTR          pszWord - Filled in with the word. This can be NULL
   DWORD          dwWordSize - sizeof() what word points to.
   PCListVariable plForm - Filled with list of word forms. For each form 1st byte is
                  part-of-speech, rest are null-terminated string.  Can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::WordGet (DWORD dwIndex, PWSTR pszWord, DWORD dwWordSize, PCListVariable plForm)
{
   DWORD *pdwWord = (DWORD*)m_lWords.Get(dwIndex);
   if (!pdwWord)
      return FALSE;
   PBYTE pbWord = (PBYTE)m_memWords.p + pdwWord[0];

   // length of string
   DWORD dwLen = (DWORD)(wcslen((PWSTR)pbWord)+1)*sizeof(WCHAR);
   if (pszWord) {
      if (dwLen > dwWordSize)
         return FALSE;
      memcpy (pszWord, pbWord, dwLen);
   }
   pbWord += dwLen;

   // if no list for forms return
   if (!plForm)
      return TRUE;
   plForm->Clear();

   // number of elements
   DWORD dwNum;
   dwNum = pbWord[0];
   pbWord++;

   // add all
   DWORD i;
   plForm->Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      dwLen = 1 + (DWORD)strlen((char*)&pbWord[1]) + 1;
      plForm->Add (pbWord, dwLen);
      pbWord += dwLen;
   }

   return TRUE;
}


/*************************************************************************************
CMLexicon::WordGet - Gets a word given its word string

inputs
   PWSTR          pszWord - Word to look for
   PCListVariable plForm - Filled with list of word forms. For each form 1st byte is
                  part-of-speech, rest are null-terminated string.  Can be NULL
returns
   BOOL - TRUE if success. FALSE if can't find the word.
*/
BOOL CMLexicon::WordGet (PWSTR pszWord, PCListVariable plForm)
{
   DWORD dwIndex = WordFind (pszWord);
   if (dwIndex == -1)
      return FALSE;

   return WordGet (dwIndex, NULL, 0, plForm);
}




/*************************************************************************************
CMLexicon::WordSet - Sets a word. If the word already exists it's overwritten.
If it doesn't exist it's added

inputs
   PWSTR          pszWord - Word to set
   PCListVariable plForm - List of word forms. For each form 1st byte is
                  part-of-speech, rest are null-terminated string.
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::WordSet (PWSTR pszWord, PCListVariable plForm)
{
   // fill memory with word
   BYTE abHuge[10000];
   DWORD dwCur = 0;
   DWORD dwLen = (DWORD)(wcslen(pszWord)+1)*sizeof(WCHAR);
   if (dwLen + dwCur > sizeof(abHuge))
      return FALSE;
   memcpy (&abHuge[dwCur], pszWord, dwLen);
   dwCur += dwLen;
   dwLen = 2;
   DWORD i;
   for (i = 0; i < plForm->Num(); i++)
      dwLen += (DWORD)plForm->Size(i);
   if (dwCur + dwLen >= sizeof(abHuge))
      return FALSE;
   abHuge[dwCur] = (BYTE)plForm->Num();
   dwCur++;
   for (i = 0; i < plForm->Num(); i++) {
      PBYTE pbForm = (PBYTE) plForm->Get(i);
      dwLen = 1 + (DWORD)strlen((char*)&pbForm[1]) + 1;
      memcpy (&abHuge[dwCur], pbForm, dwLen);
      dwCur += dwLen;
   }
   if (dwCur % 2)
      dwCur++; // word align

   // see if it already exists
   DWORD dwIndex = WordFind (pszWord);
   size_t dwOffset = -1;
   if (dwIndex != -1) {
      // space exists already...
      // see how large a space have to work with
      dwOffset = *((DWORD*)m_lWords.Get(dwIndex));
      PBYTE pbAvail = (PBYTE)m_memWords.p + dwOffset;
      DWORD dwAvail = WordSize (pbAvail);

      if (dwAvail < dwCur)
         dwOffset = -1; // so append onto end
      // elsem will write at dwOffset

      // remove the item
      m_lWords.Remove (dwIndex);
   }

   if (dwOffset == -1) {
      if (!m_memWords.Required (m_memWords.m_dwCurPosn + dwCur))
         return FALSE;  // cant ESCREALLOC
      dwOffset = m_memWords.m_dwCurPosn;
      m_memWords.m_dwCurPosn += dwCur;
   }

   // copy over
   memcpy ((PBYTE)m_memWords.p + dwOffset, &abHuge[0], dwCur);

   // find out where to insert entry and insert it
   // know that there isn't a duplicate because if found then deleted from the list
   dwIndex = WordFind(pszWord, TRUE);
   m_lWords.Insert (dwIndex, &dwOffset);

   m_fDirty = TRUE;
   return TRUE;
}


/*************************************************************************************
CMLexicon::WordRemove - Deletes a specific word.

inputs
   DWORD       dwIndex - Index of the word to remove
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::WordRemove (DWORD dwIndex)
{
   m_fDirty = TRUE;
   return m_lWords.Remove (dwIndex);
}


/*************************************************************************************
CMLexicon::WordClearAll - Clears all words
*/
void CMLexicon::WordClearAll (void)
{
   m_fDirty = TRUE;
   m_lWords.Clear();
   m_memWords.m_dwCurPosn = 0;
}




/*************************************************************************************
CMLexicon::WordSyllables - Fills in a list of the syllable boundaries for the word.

inputs
   PBYTE          pbPron - Pronunciation, phonemes only. NO references to other words.
                           NULL terminated.
   PWSTR          pszWord - Word string, optional. This can be NULL
   PCListFixed    plBoundary - This is initialized to DWORD units. Each boundary
                  between syllables will be added to the list as an index of the character
                  where the syllable bounday is BEFORE. Thus, h e1 l ow0, would have
                  one entry with 3 and 4, since the boundary is between l and ow0.
                  h ae1 p iy0 n ih0 s, would have three, at 3 (p and iy) and 4 (iy0 and n), and 7.
                  The high byte (<< 24) is the stress value for phoneme, so know if syllable
                  is stressed or not.
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::WordSyllables (PBYTE pbPron, PWSTR pszWord, PCListFixed plBoundary)
{
   if (m_pMasterLex)
      return m_pMasterLex->WordSyllables (pbPron, pszWord, plBoundary);

   // NOTE: This function is ignoring pszWord at the moment, but future releases might
   // include syllable boundaries

   plBoundary->Init (sizeof(DWORD));

   // find all the vowels
   DWORD i, dwSyl;
   DWORD dwLength = (DWORD)strlen((char*)pbPron);
   for (i = 0; pbPron[i]; i++) {
      PLEXPHONE plp = PhonemeGetUnsort (pbPron[i]-1);
      if (!plp)
         continue;

      // find out if it's a vowel
      PLEXENGLISHPHONE pEng = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
      if (!pEng)
         continue;

      if ((pEng->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
         continue;

      // if get here then it's a vowel, so add this to the list
      dwSyl = i | ((DWORD)plp->bStress << 24);
      plBoundary->Add (&dwSyl);
   } // i


   // loop through all the vowels and find the distance between this and the next
   DWORD dwNum = plBoundary->Num();
   if (!dwNum) {
      // no syllables, but need the final stress marker
      DWORD dwVal = dwLength; // no stress indicator though
      return TRUE;   // no syllables
   }

   dwNum--;
   DWORD *padw = (DWORD*)plBoundary->Get(0);
   for (i = 0; i < dwNum; i++) {
      DWORD dwDist = (WORD)padw[i+1] - (WORD) padw[i] - 1;  // dont include self in distance equation

      // determine which has the highest stress
      int iStress1, iStress2;
      DWORD dwStress1 = padw[i] >> 24;
      switch (dwStress1) {
      default:
      case 0:
         iStress1 = 0;
         break;
      case 1:
         iStress1 = 2;
         break;
      case 2:
         iStress1 = 1;
         break;
      }
      switch (padw[i+1] >> 24) {
      default:
      case 0:
         iStress2 = 0;
         break;
      case 1:
         iStress2 = 2;
         break;
      case 2:
         iStress2 = 1;
         break;
      }

      // if left stress > right stress then take more phonemes to left syllable
      if (iStress1 > iStress2)
         dwDist = (dwDist+1)/2;
      else if (iStress1 < iStress2)
         dwDist = dwDist / 2; // right has mroe stress, so tend to give phonemes to it
      else
         dwDist = (dwDist+1)/2;  // favor phonemes to left

      padw[i] = (DWORD)(WORD)padw[i] + dwDist + 1; // so occurs after this vowel
      padw[i] |= (dwStress1 << 24);
   } // i

   // the last element must be set to the number of characters, with a stress marker
   padw[dwNum] = (padw[dwNum] & 0xff000000) | dwLength;

   return TRUE;
}


/****************************************************************************
LexPronMainPage
*/
static BOOL LexPronMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPMI pmi = (PPMI)pPage->m_pUserData;
   PCMLexicon pLex = (PCMLexicon) pmi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set pos
         pLex->POSToStatus (pPage, L"posStat", L"posScroll", 0);  // set to unknown
      }
      break;

   case ESCN_SCROLLING:
   case ESCN_SCROLL:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;
         if (!p || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         // must start with pos
         if (_wcsicmp(psz, L"posScroll"))
            break;

         // get the POS
         BYTE bPOS;
         bPOS = POS_MAJOR_MAKE(p->iPos);
         pLex->POSToStatus (pPage, L"posStat", NULL, bPOS);
         return TRUE;
      }
      break;


   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;
         if (!_wcsicmp(psz, L"clearall")) {
            if (IDYES != pPage->MBYesNo(L"Are you sure you want to delete all the words in the lexicon?",
               L"This will permenantly remove them."))
               return TRUE;

            pLex->WordClearAll();

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"recommend")) {
            PCMMLNode2 pFind;
            pmi->plTreeReview->Clear();
            pFind = pLex->TextScan (NULL, pPage->m_pWindow->m_hWnd, pmi->plTreeReview);
            if (!pFind) {
               pPage->MBWarning (L"The file couldn't be scanned. It may not be a text file.");
               return TRUE;
            }
            delete pFind;

            // fall on through
            break;
         }
         else if (!_wcsicmp(psz, L"reviewpron") || !_wcsicmp(psz, L"reviewPOS")) {
            PCMMLNode2 pFind;
            pmi->plTreeReview->Clear();
            pFind = pLex->TextScan (NULL, pPage->m_pWindow->m_hWnd, pmi->plTreeReview);
            if (pFind)
               delete pFind;
            // fall on through
            break;
         }
         else if (!_wcsicmp(psz, L"reviewpronlist") || !_wcsicmp(psz, L"reviewPOSlist")) {
            pmi->plTreeReview->Clear();

            // randomize word numbers so when add to binary tree won't get a single
            // long list
#define BIGLISTSIZE        1000     // number of words in the list
            DWORD adwIndex[BIGLISTSIZE];
            DWORD i;
            for (i = 0; i < BIGLISTSIZE; i++)
               adwIndex[i] = i;
            for (i = 0; i < BIGLISTSIZE; i++) {
               DWORD dwFrom = rand() % BIGLISTSIZE;
               DWORD dwTo = rand() % BIGLISTSIZE;
               DWORD dwTemp = adwIndex[dwFrom];
               adwIndex[dwFrom] = adwIndex[dwTo];
               adwIndex[dwTo] = dwTemp;
            } // i


            // loop, getting 1000 words for review
            WCHAR szWord[128];
            for (i = 0; i < BIGLISTSIZE; i++) {
               if (!pLex->WordGet (adwIndex[i]+pmi->dwFirstWord, szWord, sizeof(szWord), NULL))
                  continue;

               pmi->plTreeReview->Add (szWord, NULL, 0);
            } // i
            if (!pmi->plTreeReview->Num())
               return TRUE;

            // fall on through
            break;
         }
         else if (!_wcsicmp(psz, L"reviewfix") || !_wcsicmp(psz, L"reviewPOSfix")) {
            BOOL fPOS = !_wcsicmp(psz, L"reviewPOSfix");
            PCEscControl pControl = pPage->ControlFind (fPOS ? L"POSfind" : L"find");
            DWORD dwNeed;
            pmi->pszReviewFind[0] = 0;
            if (pControl)
               pControl->AttribGet (Text(), pmi->pszReviewFind, 64*sizeof(WCHAR), &dwNeed);
            if (!pmi->pszReviewFind[0]) {
               pPage->MBWarning (L"You must type in some text to look for.");
               return TRUE;
            }

            pControl = pPage->ControlFind (fPOS ? L"POSfindloc" : L"findloc");
            int iSel = 0;
            if (pControl)
               iSel = pControl->AttribGetInt (CurSel()) - 1;
            pmi->iReviewLoc = iSel;

            break;   // fall on through
         }
         else if (!_wcsicmp(psz, L"changefix")) {
            WCHAR szTemp[64];
            BYTE abFrom[PRONCHARS];
            BYTE abTo[PRONCHARS];
            PCEscControl pControl;
            DWORD dwNeed;

            // from phoneme
            pControl = pPage->ControlFind (L"phonebulk");
            szTemp[0] = 0;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!pLex->PronunciationFromText (szTemp, abFrom, sizeof(abFrom), &dwNeed)) {
               pPage->MBWarning (L"The \"Using the phonemes\" field has a bad phoneme in it.");
               return TRUE;
            }

            // to phoneme
            pControl = pPage->ControlFind (L"phonetobulk");
            szTemp[0] = 0;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!pLex->PronunciationFromText (szTemp, abTo, sizeof(abTo), &dwNeed)) {
               pPage->MBWarning (L"The \"Convert to phonemes\" field has a bad phoneme in it.");
               return TRUE;
            }

            // text
            pControl = pPage->ControlFind (L"textbulk");
            szTemp[0] = 0;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!szTemp[0]) {
               pPage->MBWarning (L"You must type in some text to look for.");
               return TRUE;
            }

            pControl = pPage->ControlFind (L"locbulk");
            int iSel = 0;
            if (pControl)
               iSel = pControl->AttribGetInt (CurSel());

            DWORD dwNum;
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Converting...", TRUE);
               dwNum = pLex->BulkConvert (szTemp, abFrom, (BOOL)iSel, abTo, &Progress);
            }

            swprintf (szTemp, L"Converted %d words.", (int)dwNum);
            pPage->MBInformation (szTemp);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"generatefix")) {
            DWORD dwWords;
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Generating prefixes", TRUE);
               dwWords = pLex->GeneratePrefix (&Progress);
            }

            WCHAR szTemp[64];

            swprintf (szTemp, L"Converted %d words.", (int)dwWords);
            pPage->MBInformation (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"changePOSfix")) {
            WCHAR szTemp[64];
            PCEscControl pControl;
            DWORD dwNeed;

            // ask if must be known
            int iRet;
            BOOL fMustBeKnown = TRUE;
            iRet = pPage->MBYesNo (L"Do you want to change only words with an \"Unknown\" pronunciation?",
               L"If you press \"No\" then all words, even if they have already have their pronunciation set, will be changed.",
               TRUE);
            if (iRet == IDYES)
               fMustBeKnown = TRUE;
            else if (iRet == IDNO)
               fMustBeKnown = FALSE;
            else
               return TRUE;

            // get the scroll
            pControl = pPage->ControlFind (L"posScroll");
            BYTE bPOS = 0x0;
            if (pControl)
               bPOS = POS_MAJOR_MAKE(pControl->AttribGetInt(Pos()));

            // text
            pControl = pPage->ControlFind (L"POStextbulk");
            szTemp[0] = 0;
            if (pControl)
               pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!szTemp[0]) {
               pPage->MBWarning (L"You must type in some text to look for.");
               return TRUE;
            }

            pControl = pPage->ControlFind (L"POSlocbulk");
            int iSel = 0;
            if (pControl)
               iSel = pControl->AttribGetInt (CurSel());

            DWORD dwNum;
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Converting...", TRUE);
               dwNum = pLex->POSBulkConvert (szTemp, (BOOL)iSel, fMustBeKnown, bPOS, &Progress);
            }

            swprintf (szTemp, L"Converted %d words.", (int)dwNum);
            pPage->MBInformation (szTemp);

            return TRUE;
         }

         else if ((psz[0] == L'd') && (psz[1] == L'l') && (psz[2] == L':')) {
            pmi->dwDisplayLevel = _wtoi(psz + 3);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'c') && (psz[1] == L'l') && (psz[2] == L':')) {
            pmi->dwFirstWord = _wtoi(psz + 3);
            if (pmi->dwDisplayLevel)
               pmi->dwDisplayLevel--;
            else {
               pPage->Exit (L"edit");
               return TRUE;
            }
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'd') && (psz[1] == L'd') && (psz[2] == L':')) {
            DWORD dwDel = _wtoi(psz + 3);
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this word?",
               L"Deleting the word will permenantly remove it."))
               return TRUE;   // ignore

            // delete
            pLex->WordRemove (dwDel);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify pronunciations";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NUMWORDS")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int)pLex->WordNum());
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDLIST")) {
            MemZero (&gMemTemp);

            // figure out how many levels can have
            DWORD dw, dwLevels;
            if (pLex->WordNum())
               pmi->dwFirstWord = min(pmi->dwFirstWord, pLex->WordNum()-1);
            else
               pmi->dwFirstWord = 0;
            for (dw = pLex->WordNum(), dwLevels = 0; dw > PTREENUM; dw >>=PTREEBITS, dwLevels++);

            // dont want current level to be greater than max levels
            pmi->dwDisplayLevel = min(pmi->dwDisplayLevel, dwLevels);

            // draw the predecessors to the level and the level
            DWORD i, k;
            for (i = dwLevels; (i <= dwLevels) && (i >= pmi->dwDisplayLevel); i--) {
               // what's the first and last word here
               DWORD dwFirst, dwLast;
               if (i != pmi->dwDisplayLevel) {
                  dwFirst = (pmi->dwFirstWord >> (i * PTREEBITS));
                  dwLast = dwFirst+1;
                  dwFirst = dwFirst << (i * PTREEBITS);
                  dwLast = dwLast << (i * PTREEBITS);
               }
               else {
                  dwFirst = (pmi->dwFirstWord >> ((i+1) * PTREEBITS));
                  dwLast = dwFirst+1;
                  dwFirst = dwFirst << ((i+1) * PTREEBITS);
                  dwLast = dwLast << ((i+1) * PTREEBITS);
               }
               dwLast = min(dwLast, pLex->WordNum());
               dwFirst = min(dwFirst, dwLast);

               // if it's a top level indicate the start and end words
               WCHAR szFirst[128], szLast[128];
               if (i != pmi->dwDisplayLevel) {
                  // get the text of the start and stop words
                  szFirst[0] = 0;
                  pLex->WordGet (dwFirst, szFirst, sizeof(szFirst), NULL);

                  szLast[0] = 0;
                  pLex->WordGet (dwLast ? (dwLast-1) : 0, szLast, sizeof(szLast), NULL);

                  if (!szFirst[0] || !szLast[0])
                     continue;

                  for (k = 0; k < dwLevels - i; k++)
                     MemCat (&gMemTemp, L"&tab;");

                  // create a link
                  MemCat (&gMemTemp, L"<a href=dl:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, szFirst);
                  MemCat (&gMemTemp, L" - ");
                  MemCatSanitize (&gMemTemp, szLast);
                  MemCat (&gMemTemp, L"</a><br/>");
                  continue;
               }

               // else, list of words...
               DWORD j;
               CListVariable lPron;
               for (j = dwFirst; j < dwLast; j += (1 << (i * PTREEBITS))) {
                  // get the text of the start and stop words
                  szFirst[0] = 0;
                  pLex->WordGet (j, szFirst, sizeof(szFirst), &lPron);

                  for (k = 0; k < dwLevels - i; k++)
                     MemCat (&gMemTemp, L"&tab;");

                  if (i == 0)
                     MemCat (&gMemTemp, L"<bold>");

                  MemCat (&gMemTemp, L"<a href=cl:");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, szFirst);

                  if (i) {
                     szLast[0] = 0;
                     pLex->WordGet (min(j + (1 << (i*PTREEBITS)) - 1, pLex->WordNum()-1),
                        szLast, sizeof(szLast), &lPron);

                     MemCat (&gMemTemp, L" - ");
                     MemCatSanitize (&gMemTemp, szLast);
                     MemCat (&gMemTemp, L"</a><br/>");
                  }
                  else {
                     // show the pronunciations
                     szLast[0] = 0;
                     for (k = 0; k < lPron.Num(); k++) {
                        if (szLast[0] && (wcslen(szLast) < sizeof(szLast)/sizeof(WCHAR)-50))
                           wcscat (szLast, L", ");

                        DWORD dwLen = (DWORD)wcslen(szLast);
                        PBYTE pb = (PBYTE)lPron.Get(k)+1;
                        pLex->PronunciationToText (pb,
                           szLast + dwLen, sizeof(szLast)/sizeof(WCHAR) - dwLen - 1);

                        // show pos
                        wcscat (szLast, L" (");
                        wcscat (szLast, pLex->POSToString (pb[-1]));
                        wcscat (szLast, L")");
                     }

                     MemCat (&gMemTemp, L"</a></bold>");
                     if (szLast[0]) {
                        MemCat (&gMemTemp, L"<small> - ");
                        MemCatSanitize (&gMemTemp, szLast);
                        MemCat (&gMemTemp, L"</small>");
                     }
                     if (i == 0) {
                        MemCat (&gMemTemp, L"&nbsp;&nbsp;<a color=#ff0000 href=dd:");
                        MemCat (&gMemTemp, (int)j);
                        MemCat (&gMemTemp, L">(delete)</a>");
                     }

                     MemCat (&gMemTemp, L"<br/>");
                  } // if show actual word
               } // j
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CMLexicon::DialogPronMain - Brings up the main pronunciation editing dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPronMain (PCEscWindow pWindow)
{
   PWSTR pszRet;
   PMI pmi;
   CListVariable ListOOV;
   CBTree TreeReview;
   WCHAR szReviewFind[64];
   memset (&pmi, 0, sizeof(pmi));
   pmi.dwDisplayLevel = 1000; // just to make sure high
   pmi.dwFirstWord = 0;
   pmi.pLex = this;
   pmi.plOOV = &ListOOV;
   pmi.plTreeReview = &TreeReview;
   pmi.pszReviewFind = szReviewFind;
   pmi.iReviewLoc = 0;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPRONMAIN, LexPronMainPage, &pmi);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, L"add")) {
      DWORD dwIndex = DialogAddWord (pWindow);
      if (dwIndex != -1) {
         pmi.dwFirstWord = dwIndex - (dwIndex %PTREENUM);
         pmi.dwDisplayLevel = 0;
      }
      goto redo;
   }
   else if (!_wcsicmp(pszRet, L"recommend")) {
      if (DialogPronOOV (pWindow, pmi.plTreeReview))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewpron")) {
      if (DialogPronReview (pWindow, pmi.plTreeReview->Num() ? pmi.plTreeReview : NULL,
         FALSE, NULL, 0))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewpronlist")) {
      if (DialogPronReview (pWindow, pmi.plTreeReview->Num() ? pmi.plTreeReview : NULL,
         FALSE, NULL, 0))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewfix")) {
      if (DialogPronReview (pWindow, NULL, TRUE, pmi.pszReviewFind, pmi.iReviewLoc))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewPOS")) {
      if (DialogPOSReview (pWindow, pmi.plTreeReview->Num() ? pmi.plTreeReview : NULL,
         FALSE, NULL, 0))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewPOSlist")) {
      if (DialogPOSReview (pWindow, pmi.plTreeReview->Num() ? pmi.plTreeReview : NULL,
         FALSE, NULL, 0))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"reviewPOSfix")) {
      if (DialogPOSReview (pWindow, NULL, TRUE, pmi.pszReviewFind, pmi.iReviewLoc))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"edit")) {
      WCHAR szWord[64], szWordOrig[64];
      CListVariable lForms;
      szWord[0] = szWordOrig[0] = 0;

      // get the original text
      if (!WordGet (pmi.dwFirstWord, szWord, sizeof(szWord), &lForms))
         goto redo;
      wcscpy (szWordOrig, szWord);

      if (!DialogPronEdit (pWindow, pmi.dwFirstWord, szWord, sizeof(szWord), &lForms))
         goto redo;

      // if no text fail
      if (!szWord[0])
         goto redo;

      // delete the old one?
      if (_wcsicmp(szWord, szWordOrig))
         WordRemove (pmi.dwFirstWord);

      // add it
      if (!WordSet (szWord, &lForms))
         goto redo;

      // find it
      pmi.dwFirstWord = WordFind (szWord);
      goto redo;
   }
   return FALSE;
}

/*************************************************************************************
CMLexicon::GeneratePrefix - Loops through all the words and tries to generate
a prefix, if finds an exact match.

inputs
   PCProgressSocket     pProgress - Progress bar
returns
   DWORD - Number of words converted
*/
DWORD CMLexicon::GeneratePrefix (PCProgressSocket pProgress)
{
   DWORD dwConvert = 0;
   DWORD dwNum = WordNum();

   WCHAR szWord[128], szCompare[128], szLongest[128], szNewString[1024];
   CListVariable lFormWord, lFormCompare;
   CListVariable lDontRecurse;
   DWORD dwLenWord, dwLenCompare;
   DWORD dwLenWordPron, dwLenComparePron;
   BYTE abNewPron[512];

   DWORD i, j, k, l;
   PBYTE pbWordPron, pbComparePron;
   DWORD dwLongestFound, dwOrigPronNum;
   for (i = 0; i < dwNum; i++) {
      if (pProgress && !(i % 16))
         pProgress->Update ((fp)i / (fp)dwNum);

      // get the text
      lFormWord.Clear();
      if (!WordGet (i, szWord, sizeof(szWord), &lFormWord))
         continue;

      dwLenWord = (DWORD)wcslen(szWord);
      dwLenWord--;   // always look for one less
      if (!dwLenWord)
         continue;

      BOOL fChanged = FALSE;
      for (dwOrigPronNum = 0; dwOrigPronNum < lFormWord.Num(); dwOrigPronNum++) {
         // BUGFIX - Allow more than one pron in the original
         pbWordPron = (PBYTE)lFormWord.Get(dwOrigPronNum) + 1;
         dwLenWordPron = (DWORD)strlen((char*)pbWordPron);

         // go back and compare
         dwLongestFound = 0;
         for (j = i - 1; j < i; j--) {
            lFormCompare.Clear();
            if (!WordGet (j, szCompare, sizeof(szCompare), &lFormCompare))
               break;   // error
            dwLenCompare = (DWORD)wcslen (szCompare);

            // compare the characters in common
            if (_wcsnicmp (szWord, szCompare, min(dwLenCompare, dwLenWord)))
               break;   // gone back far enough and haven't found

            // if the length of the compare > word length looking for then skip
            if (dwLenCompare > dwLenWord)
               continue;

            DWORD dwPass;
            for (dwPass = 0; dwPass < 2; dwPass++) {
               // if it's the second pass, expand the first pronunciation out as far
               // as possible and see if get any better than the first pass
               if (dwPass) {
                  pbComparePron = (PBYTE)lFormCompare.Get(0) + 1;
                  dwLenComparePron = (DWORD)strlen((char*)pbComparePron);
                  memcpy (abNewPron, pbComparePron - 1, dwLenComparePron+2);

                  lDontRecurse.Clear();
                  lFormCompare.Clear();
                  if (!LexExpand (abNewPron, this, FALSE, &lDontRecurse, &lFormCompare))
                     continue;   // error, creates infinite loop

               }


               // see if the prounciations are identical
               for (k = 0; k < lFormCompare.Num(); k++) {
                  PBYTE pb1 = (PBYTE)lFormCompare.Get(k) + 1;
                  for (l = k+1; l < lFormCompare.Num(); l++) {
                     PBYTE pb2 = (PBYTE) lFormCompare.Get(l) + 1;

                     if (!strcmp ((char*)pb1, (char*)pb2)) {
                        lFormCompare.Remove (l);
                        l--;
                     }
                  } // i
               } // k

               // if more than one form then skip
               if (lFormCompare.Num() != 1)
                  continue;

               // find out how many characters long, and make sure less
               pbComparePron = (PBYTE)lFormCompare.Get(0) + 1;
               dwLenComparePron = (DWORD)strlen((char*)pbComparePron);
               if (dwLenComparePron > dwLenWordPron)
                  continue;   // pronunciatons cant't compare
               if (strncmp ((char*)pbComparePron, (char*)pbWordPron, dwLenComparePron))
                  continue;   // pronunciations dont match

               // else, found pronunciation that matches...
               // take the longest length
               if (dwLenComparePron <= dwLongestFound)
                  continue;   // not as long as the longest

               // else keep
               dwLongestFound = dwLenComparePron;
               wcscpy (szLongest, szCompare);
            } // dwPass
         } // j

         // if haven't found a longest then continue
         if (!dwLongestFound)
            continue;

         // if the longest is too short then dont bother
         if (dwLongestFound <= dwLenWordPron/2)
            continue;

         // else, have a match... so generate a new string
         swprintf (szNewString, L"|%s ", szLongest);
         if (dwLongestFound < dwLenWordPron)
            PronunciationToText (pbWordPron + dwLongestFound, szNewString + wcslen(szNewString), sizeof(szLongest)/sizeof(WCHAR));

         DWORD dwBad;
         if (!PronunciationFromText (szNewString, abNewPron + 1, sizeof(abNewPron)-1, &dwBad))
            continue; // error

   #ifdef _DEBUG
         OutputDebugStringW (L"\r\n");
         OutputDebugStringW (szWord);
         OutputDebugStringW (L" -> ");
         OutputDebugStringW (szNewString);
   #endif

         // set it
         abNewPron[0] = pbWordPron[-1];

         // make sure don't recurse
         lDontRecurse.Clear();
         lDontRecurse.Add (szWord, (wcslen(szWord)+1)*sizeof(WCHAR));
         lFormCompare.Clear();
         if (!LexExpand (abNewPron, this, FALSE, &lDontRecurse, &lFormCompare))
            continue;   // error, creates infinite loop

         // dont set until after sure
         lFormWord.Set (dwOrigPronNum, abNewPron, 2 + strlen((char*) abNewPron+1));
         fChanged = TRUE;
      }  // dwOrigPronNum

      if (fChanged) {
         WordSet (szWord, &lFormWord);
         dwConvert++;
      }

   } // i

   return dwConvert;
}

/*************************************************************************************
CMLexicon::PronunciationToText - Takes a prounciation (null-terminated array of bytes
which are unsorted phone index+1) and fills in a unicode string with a human-readable
phoneme string separated by spaces

inputs
   PBYTE          pbPron - Pronunciation. NUll-terminated array of bytes. The
                     phoneme number is the unsorted phone index + 1
   PWSTR          pszText - Fills in the text with a string of phonemes separated by spaces
   DWORD          dwChars - Number of characters avaialble in text
returns
   BOOL - TRUE if sucess, FALSE if error
*/
BOOL CMLexicon::PronunciationToText (PBYTE pbPron, PWSTR pszText, DWORD dwChars)
{
   PWSTR pszStart = pszText;

   for (; pbPron[0]; pbPron++) {
      // check to see if it's a reference to another word
      if ((pbPron[0] == LEXPHONE_REFSTRESS) || (pbPron[0] == LEXPHONE_REFUNSTRESS)) {
         BOOL fStress = (pbPron[0] == LEXPHONE_REFSTRESS);
         pbPron++;

         // get this string
         WCHAR szTemp[32];
         DWORD dwUsed;
         DWORD dwBytesUsed = LexCharDecompress (pbPron, szTemp, sizeof(szTemp)/sizeof(WCHAR)-1, &dwUsed);
         pbPron += (dwBytesUsed-1); // subtract 1 since will to pbPron++ later
         if (dwUsed > sizeof(szTemp)/sizeof(WCHAR)-1)
            return FALSE;  // error
         szTemp[dwUsed] = 0;

         // make sure have enough space
         DWORD dwLen = (DWORD)wcslen(szTemp);
         DWORD dwNeed = 3 + dwLen;
         if (dwNeed > dwChars)
            return FALSE;  // not enough
         
         // prepend space?
         if (pszStart != pszText) {
            if (dwChars < 2)
               return FALSE;
            pszText[0] = L' ';
            dwChars--;
            pszText++;
         }

         // the | or : symbol
         *(pszText++) = fStress ? L'|' : L':';
         dwChars--;
         
         // the string
         wcscpy (pszText, szTemp);
         dwChars -= dwLen;
         pszText += dwLen;
         continue;
      }

      // get the phoneme
      PLEXPHONE plp = PhonemeGetUnsort (pbPron[0]-1);
      if (!plp)
         return FALSE;

      // prepend space?
      if (pszStart != pszText) {
         if (dwChars < 2)
            return FALSE;
         pszText[0] = L' ';
         dwChars--;
         pszText++;
      }

      // length?
      DWORD dwLen = (DWORD)wcslen(plp->szPhoneLong);
      if (dwChars <= dwLen)
         return FALSE;
      wcscpy (pszText, plp->szPhoneLong);
      dwChars -= dwLen;
      pszText += wcslen(pszText);
   }

   // null terminate
   if (!dwChars)
      return FALSE;
   pszText[0] = 0;

   return TRUE;
}



/*************************************************************************************
CMLexicon::PronunciationFromText - Takes a test string with phonemes separated by
spaces and fills in a pronunciation buffer.

This also handles |WORD and :WORD to include the word as a sub-pronunciation.
| leaves the stress unchanged, : removes stresses.

inputs
   PWSTR          pszText - Fills in the text with a string of phonemes separated by spaces
   PBYTE          pbPron - To be filled with pronuncaition. Will be null terminated.
                     The phoneme number is the unsorted phone index + 1
   DWORD          dwChars - Number of characters avaialble in pbPron
   DWORD          *pdwBadPhone - If error then filled with index into pszText where
                  bad phone occurs.

returns
   BOOL - TRUE if sucess, FALSE if error... if error will fill in pdwBadPhone with
      the first character index (into pszText) of the bad phone
*/
BOOL CMLexicon::PronunciationFromText (PWSTR pszText, PBYTE pbPron, DWORD dwChars, DWORD *padwBadPhone)
{
   DWORD i;
   PWSTR pszOrig = pszText;
   *padwBadPhone = 0; // just to clear
   while (pszText[0]) {
      // skip space
      while (iswspace(pszText[0]))
         pszText++;
      if (!pszText[0])
         break;

      // loop until get another space
      PWSTR pszNext;
      for (pszNext = pszText+1; pszNext[0] && !iswspace(pszNext[0]); pszNext++);

      // fill in bad phone just in case
      *padwBadPhone = (DWORD)((PBYTE)pszText - (PBYTE)pszOrig) /sizeof(WCHAR);

      // find this phoneme
      WCHAR cTemp;
      cTemp = pszNext[0];
      pszNext[0] = 0;

      if ((pszText[0] == L'|') || (pszText[0] == L':')) {
         // make sure the word is real
         // And make sure will have enough space to store the pronuncation, taking
         // a max of 3 bytes per char, + 3 extras
         if (!WordExists (pszText+1) || (dwChars < 3 + wcslen(pszText+1)*3) ) {
            if (dwChars)
               pbPron[0] = 0; // null terminate
            pszNext[0] = cTemp;
            return FALSE;
         }

         // add it
         // add the indicator
         *(pbPron++) = (pszText[0] == L'|') ? LEXPHONE_REFSTRESS : LEXPHONE_REFUNSTRESS;
         dwChars--;

         // encode
         size_t dwBytesUsed = LexCharCompress (pszText+1, wcslen(pszText+1), pbPron, wcslen(pszText+1)*3+1);
         pbPron += dwBytesUsed;
         dwChars -= (DWORD)dwBytesUsed;
         
      }
      else {
         for (i = 0; i < PhonemeNum(); i++) {
            PLEXPHONE plp = PhonemeGetUnsort (i);
            if (!plp)
               continue;
            if (!_wcsicmp(plp->szPhoneLong, pszText))
               break;
         } // i
         if (i >= PhonemeNum()) {
            if (dwChars)
               pbPron[0] = 0; // null terminate
            pszNext[0] = cTemp;
            return FALSE;
         }


         // add this phoneme
         if (dwChars < 2) {
            pszNext[0] = cTemp;
            return FALSE;
         }
         pbPron[0] = (BYTE)i+1;
         pbPron++;
         dwChars--;
      }

      pszNext[0] = cTemp;
      pszText = pszNext;
   }

   // null terminate
   if (dwChars == 0)
      return FALSE;  // cant finish
   pbPron[0] = 0;

   return TRUE;
}


/****************************************************************************
CMLexicon::LexUIShowTTS - This function sets the edit control "ttsfile" with
the current tts engine.

inputs
   PCEscPage      pPage - Page to use
returns
   none
*/
void CMLexicon::LexUIShowTTS (PCEscPage pPage)
{
   //PCEscControl pControl;

   TTSCacheDefaultGet (pPage, L"ttsfile");
   // fill in the file
   //pControl = pPage->ControlFind (L"ttsfile");
   //if (pControl)
   //   pControl->AttribSet (Text(), m_szTTSSpeak[0] ? m_szTTSSpeak : L"(No TTS file)");
}

/****************************************************************************
CMLexicon::LexUIChangeTTS - This function pulls up a dialog allowin the selection
of a new tts voice, and then calls LexUIShowTTS() to display it

inputs
   PCEscPage      pPage - Page to use
returns
   none
*/
void CMLexicon::LexUIChangeTTS (PCEscPage pPage)
{
   if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
      TTSCacheDefaultGet (pPage, L"ttsfile");

   //WCHAR szw[256];
   //wcscpy (szw, m_szTTSSpeak);
   //if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szw,
   //   sizeof(szw)/sizeof(WCHAR), FALSE))
   //   return;

   // save the file
   //wcscpy (m_szTTSSpeak, szw);
   //m_fDirty = TRUE;

   //LexUIShowTTS (pPage);
}


/****************************************************************************
CMLexicon::LexUIPronFromEdit - This reads in an edit control and parse it.
If it finds that it cannot parse the entire edit control it will do a sounds-like.

inputs
   PCEscPage      pPage - Page to use
   PWSTR          pszEdit - edit control
   BOOL           fRewrite - if TRUE and a sound-like is typed, this will reset
                  the contents of the edit field to the phonemes

   ... use the following to get the prounciation
   PBYTE          pbPron - Fill the pronunciation in here when done. Can be NULL.
                     Proncunaition is null-terminate, unsorted phoneme number+1
   DWORD          dwPronSize - Number of bytes avaialbe for pbPron

   ... use the following to modify the pronunciation of a word automatically
   PWSTR          pszWord - If not NULL then LexUIPronFromEdit() will set the
                     word's pronunciation to this. Note: If the pron ends up
                     being blank then the sense will be deleted
   DWORD          dwWordForm - Form of word, to use for pszWord

   ... use the following to speak
   PWSTR          pszWordToSpeak - String of the word to speak. Used to can choose
                  the pronunciation based upon the word being spoken
   BOOL           fSpeak - TRUE TRUE then will try to speak
returns
   BOOL - TRUE if success, FALSE if failed (because cant find edit)
*/
BOOL CMLexicon::LexUIPronFromEdit (PCEscPage pPage, PWSTR pszEdit, BOOL fRewrite,
                                   PBYTE pbPron, DWORD dwPronSize,
                                   PWSTR pszWord, DWORD dwWordForm,
                                   PWSTR pszWordToSpeak, BOOL fSpeak)
{
   WCHAR szPron[256];
   PCEscControl pControl = pPage->ControlFind (pszEdit);
   if (!pControl)
      return FALSE;
   DWORD dwNeed;
   szPron[0] = 0;
   if (!pControl->AttribGet (Text(), szPron, sizeof(szPron), &dwNeed))
      return FALSE;

   // parse...
   BYTE abPron[PRONCHARS];
   DWORD dwBad;
   abPron[0] = 0; // for POS
   if (!PronunciationFromText (szPron, abPron+1, sizeof(abPron)-1, &dwBad)) {
      // found bad phoneme, so do sonunds like...
      CTextParse TextParse;
      if (!TextParse.Init (LangIDGet(), this))
         return FALSE;

      // convert to text
      PCMMLNode2 pNode = TextParse.ParseFromText (szPron, TRUE, FALSE);
      if (!pNode)
         return FALSE;

      // loop through all the words and get their pronunciations...
      size_t dwCur = 0;
      DWORD i;
      PWSTR psz;
      PCMMLNode2 pSub;
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;
         psz = pSub->NameGet();
         if (!psz || _wcsicmp(psz, TextParse.Word()))
            continue;

         // get the pronunciation
         psz = pSub->AttribGetString (TextParse.Pronunciation());
         if (!psz)
            continue;
         size_t dwLen = wcslen(psz);
         if (!dwLen)
            continue;
         if (dwCur + dwLen/2 + 1 >= sizeof(abPron)-1)
            continue;   // too long
         dwLen = MMLBinaryFromString (psz, abPron + dwCur+1, sizeof(abPron)-dwCur-2);

         dwCur += dwLen;
      }//i
      // add one to each
      DWORD j;
      for (j = 0; j < dwCur; j++)
         abPron[j+1]++;
      // null  terminate
      abPron[dwCur+1] = 0;


      // set the text back?
      if (fRewrite) {
         szPron[0] = 0;
         PronunciationToText (abPron+1, szPron, sizeof(szPron)/sizeof(WCHAR));
         pControl->AttribSet (Text(), szPron);
      }

      // delete node
      delete pNode;
   }

   // length of pronunciation?
   DWORD dwLen = (DWORD)strlen((char*)abPron+1);

   // copy pronuncaiton over
   if (pbPron) {
      if (dwLen+1 >= dwPronSize)
         return FALSE;
      strcpy ((char*)pbPron, (char*)abPron+1);
   }

   // fill in proncuniation
   if (pszWord) {
      CListVariable lForm;
      WordGet (pszWord, &lForm);

      if (lForm.Num() > dwWordForm) {
         if (!dwLen) {
            // remove...
            lForm.Remove (dwWordForm);
            if (lForm.Num())
               WordSet (pszWord, &lForm);
            else
               WordRemove (WordFind(pszWord));
         }
         else {   // add
            PBYTE pb = (PBYTE) lForm.Get(dwWordForm);
            // only bother setting if there's a change in the prounciation
            if (strcmp((char*)pb+1, (char*)&abPron[1])) {
               abPron[0] = pb[0];   // so keep same pos
               lForm.Set (dwWordForm, abPron, 2 + strlen((char*)abPron+1));
               WordSet (pszWord, &lForm);
            }
         }
      }
      else if (dwLen) {
         lForm.Add (abPron, 2 + strlen((char*)abPron+1));
         WordSet (pszWord, &lForm);
      }
   }

   // speak it
   if (fSpeak) {
      // in case has embedded pronunciations in here, deconscruct
      CListVariable lPron, lDontRecurse;
      if (!LexExpand (abPron, this, FALSE, &lDontRecurse, &lPron))
         return FALSE;
      if (!lPron.Num())
         return FALSE;

      // convert text to phoneme
      if (!PronunciationToText (((PBYTE)lPron.Get(0)) + 1, szPron, sizeof(szPron)/sizeof(WCHAR)))
         return FALSE;

      // make up string to speak
      CMem memSpeak;
      MemZero (&memSpeak);
      MemCat (&memSpeak, L"<phoneme ph=\"");
      MemCatSanitize (&memSpeak, szPron);
      MemCat (&memSpeak, L"\">");
      if (pszWordToSpeak)
         MemCatSanitize (&memSpeak, pszWordToSpeak);
      MemCat (&memSpeak, L"</phoneme>");

      TTSCacheSpeak ((PWSTR)memSpeak.p, TRUE, FALSE, pPage->m_pWindow->m_hWnd);
   } // if speak

   return TRUE;
}


/****************************************************************************
LexPhoneList - This fills in a table with a list of phonemes

inputs
   PCMLexicon     pLex - Lexicon
returns
   PWSTR - String (from gMemTemp) with table MML
*/
PWSTR LexPhoneList (PCMLexicon pLex)
{

   MemZero (&gMemTemp);
#define PHONECOL           4     // number of columns of phonemes

   // loop
   DWORD i, j;
   DWORD dwRows = (pLex->PhonemeNum() + PHONECOL-1)/PHONECOL;
   for (i = 0; i < dwRows; i++) {
      MemCat (&gMemTemp, L"<tr>");

      for (j = 0; j < PHONECOL; j++) {
         MemCat (&gMemTemp, L"<td>");
         
         PLEXPHONE plp = pLex->PhonemeGet (i + j*dwRows);
         if (plp) {
            MemCat (&gMemTemp, L"<bold>");
            MemCatSanitize (&gMemTemp, plp->szPhoneLong);
            MemCat (&gMemTemp, L"</bold> - ");
            MemCatSanitize (&gMemTemp, plp->szSampleWord);
         }

         MemCat (&gMemTemp, L"</td>");
      } // j
      
      MemCat (&gMemTemp, L"</tr>");
   } // i

   return (PWSTR)gMemTemp.p;
}

/****************************************************************************
LexPronEditPage
*/
static BOOL LexPronEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPEDI pedi = (PPEDI)pPage->m_pUserData;
   PCMLexicon pLex = (PCMLexicon) pedi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set up the fields
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pedi->pszWord);

         DWORD i;
         WCHAR szTemp[128];
         WCHAR szControl[64], szControl2[64];
         for (i = 0; i < min(4, pedi->plForms->Num()); i++) {
            swprintf (szControl, L"pron%d", (int)i);
            pControl = pPage->ControlFind (szControl);
            if (!pControl)
               continue;

            PBYTE pb = (PBYTE)pedi->plForms->Get(i)+1;
            if (!pLex->PronunciationToText (pb, szTemp, sizeof(szTemp)/sizeof(WCHAR)))
               continue;

            pControl->AttribSet (Text(), szTemp);

            // POS
            swprintf (szControl2, L"pos%d", (int)i);
            swprintf (szControl, L"posstat%d", (int)i);
            pLex->POSToStatus (pPage, szControl, szControl2, pb[-1]);
         } // i

         // set rest to unknown
         for (; i < 4; i++) {
            // POS
            swprintf (szControl2, L"pos%d", (int)i);
            swprintf (szControl, L"posstat%d", (int)i);
            pLex->POSToStatus (pPage, szControl, szControl2, POS_MAJOR_UNKNOWN);
         }

         // show tts
         pLex->LexUIShowTTS(pPage);
      }
      break;

   case ESCN_SCROLLING:
   case ESCN_SCROLL:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;
         if (!p || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         // must start with pos
         if ((psz[0] != 'p') || (psz[1] != 'o') || (psz[2] != L's'))
            break; // dont care

         // number follows
         DWORD dwNum = _wtoi(psz+3);
         if (dwNum >= 4)
            break;   // dont know this number

         // get the POS
         BYTE bPOS;
         bPOS = POS_MAJOR_MAKE(p->iPos);

         // set the status, dont bother writing because do that late
         WCHAR szControl[64];
         swprintf (szControl, L"posstat%d", (int)dwNum);
         pLex->POSToStatus (pPage, szControl, NULL, bPOS);
         return TRUE;
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         PWSTR pszSpeak = L"speak";
         DWORD dwLenSpeak = (DWORD)wcslen(pszSpeak);

         if (!_wcsicmp(psz, L"opentts")) {
            pLex->LexUIChangeTTS(pPage);
            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszSpeak, dwLenSpeak)) {
            DWORD dwNum = _wtoi(psz + dwLenSpeak);
            WCHAR szTemp[32];
            swprintf (szTemp, L"pron%d", (int)dwNum);
            pLex->LexUIPronFromEdit (pPage, szTemp, TRUE, NULL, 0, NULL, 0,
               pedi->pszWord, TRUE);
            return TRUE;
         }
      }
      return TRUE;


   case ESCM_USER+82:   // get the values.. This fills in (BOOL*)pParam with TRUE if success, FALSE if error
      {
         BOOL *pfOK = (BOOL*) pParam;
         *pfOK = FALSE;

         // get the name
         WCHAR szName[64];
         DWORD dwNeed, i;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"name");
         szName[0] = 0;
         if (pControl)
            pControl->AttribGet (Text(), szName, pedi->dwWordSize, &dwNeed);
         if (!szName[0]) {
            pPage->MBWarning (L"You must type in a name.");
            return TRUE;
         }

         // see if name is already in the lexicon
         DWORD dwIndex = pLex->WordFind (szName);
         if ((dwIndex != -1) && (dwIndex != pedi->dwWord)) {
            pPage->MBWarning (L"A word with that name already exists.",
               L"Every word must be unique.");
            return TRUE;
         }

         // pronunciations
         BYTE abForms[4][PRONCHARS];
         WCHAR szTemp[128];
         WCHAR szControl[64];
         memset (abForms, 0, sizeof(abForms));
         BOOL fFoundPron;
         fFoundPron = FALSE;
         for (i = 0; i < 4; i++) {
            swprintf (szControl, L"pron%d", (int)i);
            pControl = pPage->ControlFind (szControl);
            if (!pControl)
               continue;
            szTemp[0] = 0;
            pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeed);
            if (!szTemp[0])
               continue;

            if (!pLex->PronunciationFromText (szTemp, &abForms[i][1], sizeof(abForms[i])-1, &dwNeed)) {

               // set the selection to the start of the bad phoneme
               pControl->AttribSetInt (L"selstart", (int)dwNeed);
               pControl->AttribSetInt (L"selend", (int)dwNeed);

               // error
               PWSTR psz;
               switch (i) {
               case 0:
                  psz = L"main";
                  break;
               case 1:
                  psz = L"1st alternative";
                  break;
               case 2:
                  psz = L"2nd alternative";
                  break;
               default:
               case 3:
                  psz = L"3rd alternative";
                  break;
               }
               swprintf (szTemp, L"A phoneme in the %s pronunciation doesn't exist.", psz);
               pPage->MBWarning (szTemp);
               return TRUE;
            }

            // get pos
            swprintf (szControl, L"pos%d", (int)i);
            pControl = pPage->ControlFind (szControl);
            if (pControl) 
               abForms[i][0] = POS_MAJOR_MAKE(pControl->AttribGetInt (Pos()));

            if (abForms[i][1])
               fFoundPron = TRUE;
         } // i


         // copy over
         wcscpy (pedi->pszWord, szName);
         pedi->plForms->Clear();
         for (i = 0; i < 4; i++)
            if (abForms[i][1])
               pedi->plForms->Add (&abForms[i][0], 2 + strlen((char*)&abForms[i][1]));
         *pfOK = TRUE;
         return TRUE;
      }
      return TRUE;


   case ESCM_CLOSE:
      {
         // make sure to save entries
         BOOL fRet;
         pPage->Message (ESCM_USER+82, &fRet);
         if (!fRet)
            return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         // make sure to save entries
         // only really need to do this for back, but do for all links
         BOOL fRet;
         pPage->Message (ESCM_USER+82, &fRet);
         if (!fRet)
            return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify pronunciation";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONELIST")) {
            p->pszSubString = LexPhoneList(pLex);
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CMLexicon::DialogPronEdit - Brings up a dialog to modify the pronunciation of a word

inputs
   PCEscWindow          pWindow - window to use
   DWORD                dwWord - Word being edited, or -1 if adding new one
   PWSTR                pszWord - Pointer to the string continaing the word, modified in place
   DWORD                dwWordSize - Number of bytes available in pszWord
   PCListVariable       plForms - List of pronunciation forms. Should contain valid info intially, modified in place
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPronEdit (PCEscWindow pWindow, DWORD dwWord, PWSTR pszWord,
                                DWORD dwWordSize, PCListVariable plForms)
{
   PWSTR pszRet;
   PEDI pedi;
   //CM3DWave Wave;
   memset (&pedi, 0, sizeof(pedi));
   pedi.dwWord = dwWord;
   pedi.dwWordSize = dwWordSize;
   pedi.pLex = this;
   pedi.plForms = plForms;
   pedi.pszWord = pszWord;
   //pedi.pWave = &Wave;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPRONEDIT, LexPronEditPage, &pedi);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}


/*************************************************************************************
CMLexicon::DialogAddWord - Brings up UI to add a word.

inputs
   PCEscWindow          pWindow - window to use
returns
   DWORD - Index of the added word, or -1 if none added.
*/
DWORD CMLexicon::DialogAddWord (PCEscWindow pWindow)
{
   WCHAR szWord[64];
   CListVariable lForms;
   szWord[0] = 0;

   if (!DialogPronEdit (pWindow, -1, szWord, sizeof(szWord), &lForms))
      return -1;

   // if no text fail
   if (!szWord[0])
      return -1;

   // add it
   if (!WordSet (szWord, &lForms))
      return -1;

   // find it
   return WordFind (szWord);
}


/*************************************************************************************
CMLexicon::WordsFromFile - Reads a CMU text file (which is one word and its pronunciation
per line). New words are added to the lexicon.

inputs
   char              *pszFile - File to read in
   PCEscPage         pPage - Page loading in. Message boxes to ask questions are
                        loaded from this page.
returns
   BOOL - TRUE if success, FALSE if cant find file
*/
BOOL CMLexicon::WordsFromFile (char *pszFile, PCEscPage pPage)
{
   FILE *f;
   OUTPUTDEBUGFILE (pszFile);
   f = fopen (pszFile, "rt");
   if (!f)
      return FALSE;   // no info

   char szTemp[1024];
   DWORD dwCount, j;
   char *pEnd;
   CListVariable lPron;

   while (fgets(szTemp, sizeof(szTemp), f)) {
      if (szTemp[0] == '#')
         continue;   // comment

#ifdef _DEBUG
      // HACK - Lowercase CMU dictionary
      CharLower (szTemp);
#endif

      // find the first space, indicating the phoneme
      char *pChars;
      for (pChars = szTemp; pChars[0] && !iswspace (pChars[0]); pChars++);
      if (!pChars[0])
         continue;   // empty line
      pChars[0] = 0;
      pChars++;

      // the name ends with a bracket and a number
      pEnd = szTemp + (strlen(szTemp) - 1);
      if ((pChars >= szTemp+3) && (pEnd[0] == ')') && (pEnd[-2] == '(') &&
         (pEnd[-1] >= '1') && (pEnd[-1] <= '9')) {
            pEnd[-2] = 0;  // so have multiple pronunciations
         }

      // convert to unicode
      WCHAR szw[256];
      MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/2);
      if (!szw[0])
         continue;

      // if there's already a pronduncaiton get it
      DWORD dwIndex = WordFind (szw);
      lPron.Clear();
      if (dwIndex != -1)
         WordGet (dwIndex, NULL, 0, &lPron);

      BYTE abPron[PRONCHARS];
      abPron[0] = 0; // part of spech
      dwCount = 1;

#ifdef _DEBUG
      // HACK - This converts all secondary stress phonemes to primary stress
      for (j = 0; pChars[j]; j++)
         if (pChars[j] == '2')
            pChars[j] = '1';
#endif

      while (pChars[0]) {
         // skip white space
         while (pChars[0] && iswspace(pChars[0]) )
            pChars++;
         if (!pChars[0])
            break;   // EOF

         // find the end
         for (pEnd = pChars + 1; !iswspace(pEnd[0]); pEnd++);

         // phoneme match
         char cTemp;
         char *pszUse;
         cTemp = *pEnd;
         *pEnd = 0;
         // convert
         pszUse = pChars;
         WCHAR szwTemp[128];
         MultiByteToWideChar (CP_ACP, 0, pszUse, -1, szwTemp, sizeof(szwTemp)/sizeof(WCHAR));
         *pEnd = cTemp;

lookforphone:
         // see if it maches an phoneme in the list
         for (j = 0; j < PhonemeNum(); j++) {
            PLEXPHONE plp = PhonemeGetUnsort (j);
            if (!plp)
               continue;

            if (!_wcsicmp(plp->szPhoneLong, szwTemp))
               break;
         }
         if (j < PhonemeNum()) {
            // use this
            if (dwCount < sizeof(abPron)-2) {
               abPron[dwCount] = (BYTE)j+1;
               dwCount++;
            }
         }
         else {
            // as the user if they want to add it
            WCHAR szAsk[256];
            swprintf (szAsk, L"The file uses the \"%s\" phoneme but it isn't supported by your lexicon. Do you wish to add it?",
               szwTemp);
            int iRet = pPage->MBYesNo (szAsk,
               L"If you press Yes then the phoneme will be added; you'll need to provide an example word and stress information later. "
               L"If you press No the word will be skipped. "
               L"Pressing Cancel will stop the file loading.", TRUE);
            if (iRet != IDYES) {
               if (iRet == IDCANCEL)
                  goto done;
               else
                  goto nextword;
            }

            // add it
            szwTemp[4] = 0;   // make sure not too long
            PhonemeAddBlank (szwTemp);
            goto lookforphone;
         }

         // continue
         pChars = pEnd;
      } // while pChars

      // finish pronunciation
      abPron[dwCount] = 0;
      dwCount++;

      // if it already exists in the word then do nothing
      for (j = 0; j < lPron.Num(); j++) {
         if (lPron.Size(j) != dwCount)
            continue;

         if (!memcmp(lPron.Get(0), abPron, dwCount))
            break;
      }
      if (j < lPron.Num())
         continue;   // already there so dont bother

      // else, add
      if (abPron[1]) {
         lPron.Add (abPron, dwCount);
         WordSet (szw, &lPron);
      }
nextword:
      continue;
   } // while fgets()

done:
   fclose (f);

   return TRUE;
}

static PWSTR gpszScan = L"Scan";
static PWSTR gpszSentence = L"Sentence";

/*************************************************************************************
CMLexicon::TextScan - This reads in a text file and 1) produces a list of sentences
in the text file, and 2) fills in a tree of unique words and their word count.

inputs
   char                 *pszText - Text File. If this is NULL a common file dilaog
                           will pop up asking for the file.
   HWND                 hWnd - Window to show progress from.
   PCBTree              pTree - Filled in (or added to) with new words. Each
                           element is a DWORD that contains the word count
returns
   PCMMLNode2 - Root node containing "Sentence" nodes. Each setnence node contains "Word"
      nodes.
*/
PCMMLNode2 CMLexicon::TextScan (char *pszText, HWND hWnd, PCBTree pTree)
{
   char szFile[256];
   if (!pszText) {
      // open...
      OPENFILENAME   ofn;
      szFile[0] = 0;
      memset (&ofn, 0, sizeof(ofn));
      
      // BUGFIX - Set directory
      char szInitial[256];
      strcpy (szInitial, gszAppDir);
      GetLastDirectory(szInitial, sizeof(szInitial)); // BUGFIX - get last dir
      ofn.lpstrInitialDir = szInitial;

      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0\0\0";
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrTitle = "Look through a text file for words";
      ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = "txt";
      // nFileExtension 

      if (!GetOpenFileName(&ofn))
         return NULL;
      pszText = szFile;
   } // ask file name

   // read in the binary
   CMem memFile, memTemp;
   FILE *f;
   OUTPUTDEBUGFILE (pszText);
   f = fopen (pszText, "rb");
   if (!f)
      return NULL;

   // is it uncode
   WORD w;
   w = 0;
   fread (&w, sizeof(w), 1, f);
   BOOL fUnicode = (w == 0xfeff);

   // whole memoory
   fseek (f, 0, SEEK_END);
   int iLen = ftell (f);
   fseek (f, fUnicode ? sizeof(WORD) : 0, SEEK_SET);
   if (fUnicode)
      iLen -= sizeof(WORD);

   if (!memFile.Required ((DWORD)iLen+4))
      return NULL;
   memset ((PBYTE)memFile.p + iLen, 0, 4);   // just to make sure is null terminated
   fread (memFile.p, 1, iLen, f);
   fclose (f);

   // text parse
   CTextParse TP;
   TP.Init (LangIDGet(), this);

   // master
   PCMMLNode2 pMaster = new CMMLNode2;
   if (!pMaster)
      return NULL;
   pMaster->NameSet (gpszScan);

   // make sure tree set up
   if (pTree)
      pTree->m_fIgnoreCase = TRUE;

   // progress bar
   CProgress Progress;
   Progress.Start (hWnd, "Scanning...", TRUE);

   // repeat, looking for cr/lf
   WCHAR *pwCur = (WCHAR*)memFile.p;
   char *paCur = (char*)memFile.p;
   while (TRUE) {
      PCMMLNode2 pNode;
      WCHAR *pwNext;
      char *paNext;

      // update the progress bar
      if (fUnicode)
         Progress.Update ((fp)(DWORD)((PBYTE)pwCur - (PBYTE)memFile.p) / (fp)memFile.m_dwAllocated);
      else
         Progress.Update ((fp)(DWORD)((PBYTE)paCur - (PBYTE)memFile.p) / (fp)memFile.m_dwAllocated);

      // find next cr
      if (fUnicode) {
         if (!pwCur[0])
            break;
         for (pwNext = pwCur+1; pwNext[0] && !((pwNext[0] == L'\r') || (pwNext[0] == L'\n')); pwNext++);

         // convert to NULL
         // BUGFIX - Increase psznext
         if (pwNext[0]) {
            pwNext[0] = 0; // BUGFIX - was [-1] = 0
            pwNext++;
         }

         // parse
         pNode = TP.ParseFromText (pwCur, FALSE, FALSE);
         pwCur = pwNext;
      }
      else { // ansi
         if (!paCur[0])
            break;
         for (paNext = paCur+1; paNext[0] && !((paNext[0] == '\r') || (paNext[0] == '\n')); paNext++);

         // convert to NULL
         // BUGFIX - Increase psznext
         if (paNext[0]) {
            paNext[0] = 0; // BUGFIX - was [-1] = 0
            paNext++;
         }
         // paNext[-1] = 0;

         // allocate enough space for converting to unicode
         DWORD dwLen = (DWORD)strlen(paCur)+1;
         if (!memTemp.Required (dwLen * 2))
            continue;
         MultiByteToWideChar (CP_ACP, 0, paCur, -1, (PWSTR)memTemp.p, (DWORD)memTemp.m_dwAllocated / sizeof(WCHAR));

         // parse
         pNode = TP.ParseFromText ((PWSTR)memTemp.p, FALSE, FALSE);
         paCur = paNext;
      } // ansi

      // if nothing there then continue
      if (!pNode)
         continue;

      while (pNode->ContentNum()) {
         // create senetence
         PCMMLNode2 pSentence = new CMMLNode2;
         if (!pSentence) {
            delete pNode;
            delete pMaster;
            return NULL;
         }
         pSentence->NameSet (gpszSentence);
         pMaster->ContentAdd (pSentence);

         // look from the start of the node until find a sentence break
         while (pNode->ContentNum()) {
            PWSTR psz;
            PCMMLNode2 pElem;
            pElem = NULL;
            if (!pNode->ContentEnum (0, &psz, &pElem))
               break;
            if (psz) {
               pSentence->ContentAdd (psz);
               pNode->ContentRemove (0);
               continue;
            }

            // only need to stop if find a punctuation that's an end-of-sentence markter
            BOOL fEnd = FALSE;
            psz = pElem->NameGet();
            if (psz && !_wcsicmp(psz, TP.Punctuation())) {
               psz = pElem->AttribGetString(TP.Text());
               if (psz && psz[0] && !psz[1]) switch (psz[0]) {
                  case L'.':
                  case L'!':
                  case L'?':
                  case L';':
                     fEnd= TRUE;
                     break;
               } // switch
            }

            // transfer this over
            pNode->ContentRemove (0, FALSE);
            pSentence->ContentAdd (pElem);

            // add word to tree
            psz = pElem->NameGet();
            if (pTree && !_wcsicmp(psz, TP.Word())) {
               psz = pElem->AttribGetString(TP.Text());
               if (psz) {
                  DWORD *pdw = (DWORD*)pTree->Find (psz);
                  if (pdw)
                     pdw[0] = pdw[0] + 1;
                  else {
                     DWORD dw = 1;
                     pTree->Add (psz, &dw, sizeof(dw));
                  }
               }
            } // keep track of words

            if (fEnd)
               break; // done with this sentence
         } // while content num

      } // while pNode->ContentNum()
      delete pNode;

   } // while read in


   // done
   return pMaster;
}




/****************************************************************************
LexPronOOVPage
*/
static int __cdecl PWSTRCompare (const void *p1, const void *p2)
{
   PWSTR pp1 = *((PWSTR*) p1);
   PWSTR pp2 = *((PWSTR*) p2);
   int iRet = _wcsicmp(pp1, pp2);
   return iRet;
}
static BOOL LexPronOOVPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPMI pmi = (PPMI)pPage->m_pUserData;
   PCMLexicon pLex = (PCMLexicon) pmi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // leave all the pronunciations blank since they're new words

         // show tts
         pLex->LexUIShowTTS(pPage);
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"opentts")) {
            pLex->LexUIChangeTTS(pPage);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearreview")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear the review list?"))
               return TRUE;

            PCMLexicon pReview = pLex->ReviewListGet();
            if (pReview) {
               pReview->Clear();
               MLexiconCacheClose (pReview);
            }

            pPage->Message (ESCM_USER+82);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"markreview")) {
            // save all prons away
            pPage->Message (ESCM_USER+82);

            PCMLexicon pReview = pLex->ReviewListGet();
            if (pReview) {
               DWORD i;
               CListVariable lForm;
               for (i = 0; i < pmi->plOOV->Num(); i++) {
                  PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
                  if (!psz)
                     continue;
                  pReview->WordSet (psz, &lForm);
               }
               MLexiconCacheClose (pReview);
            }

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'p') && (psz[1] == L':')) {
            WCHAR szTemp[64];
            wcscpy (szTemp, psz);
            szTemp[0] = L'e';

            DWORD dwNum = _wtoi(psz + 2);

            pLex->LexUIPronFromEdit (pPage, szTemp, TRUE, NULL, 0, NULL, 0,
               (PWSTR)pmi->plOOV->Get(dwNum), TRUE);
            return TRUE;
         }
      }
      return TRUE;


   case ESCM_USER+82:   // get the values..
      {
         // go through all words and set pron
         // set pronunciation text
         DWORD i;
         for (i = 0; i < pmi->plOOV->Num(); i++) {
            PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
            if (!psz)
               continue;
         
            WCHAR szTemp[64];
            swprintf (szTemp, L"e:%d", (int)i);

            pLex->LexUIPronFromEdit (pPage, szTemp, FALSE, NULL, 0, psz, 0, NULL, NULL);
         } // i

      }
      return TRUE;


   case ESCM_CLOSE:
      {
         // make sure to save entries
         BOOL fRet;
         pPage->Message (ESCM_USER+82, &fRet);
         if (!fRet)
            return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         // make sure to save entries
         // only really need to do this for back, but do for all links
         BOOL fRet = TRUE;
         pPage->Message (ESCM_USER+82, &fRet);
         if (!fRet)
            return TRUE;
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Not in the lexicon";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDLIST")) {
            if (!pmi->plOOV->Num()) {
               p->pszSubString = L"<tr><td>There aren't any more words to review.</td></tr>";
               return TRUE;
            }

            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pmi->plOOV->Num(); i++) {
               PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
               MemCat (&gMemTemp, L"<tr>");

               // name
               MemCat (&gMemTemp, L"<td width=33%%><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold></td>");

               // edit box with play
               MemCat (&gMemTemp, L"<td align=center width=66%%>");
               MemCat (&gMemTemp, L"<bold><edit width=80%% maxchars=64 name=e:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/></bold><button style=teapot name=p:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/>");
               MemCat (&gMemTemp, L"</td>");

               MemCat (&gMemTemp, L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONELIST")) {
            p->pszSubString = LexPhoneList(pLex);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CMLexicon::DialogPronOOV - Brings up the main pronunciation editing dialog

inputs
   PCEscWindow          pWindow - window to use
   PCListVariable       plOOV - List of words from ReviewProduceNext()
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPronOOV (PCEscWindow pWindow, PCBTree pTreeReview)
{
   PWSTR pszRet;
   PMI pmi;
   CListVariable lOOV;
   //CM3DWave Wave;
   memset (&pmi, 0, sizeof(pmi));
   pmi.dwFirstWord = 0;
   pmi.pLex = this;
   pmi.plOOV = &lOOV;
   //pmi.pWave = &Wave;

redo:
   Save();  // just to save changes every go around in case crashes
   ReviewProduceNext (pTreeReview,  1, NUMREVIEW, NULL, NULL, 0, &lOOV);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPRONOOV, LexPronOOVPage, &pmi);
   if (!pszRet)
      return FALSE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}



/*************************************************************************************
CMLexicon::EnumEntireLexicon - This function is used by speech recognition training
to recommend words. It ensures that the lexicon is searched through in a random
order but that every word is accounted for.

What it does is fill the pListPWSTR with a list of pointers to strings of words
appearing within the lexicon and its parents. The list is then randomized so
that the order won't be the same every time.

inputs
   PCListFixed       pListPWSTR - Initialized to size of PWSTR and filled with pointers
                     to the word strings for the lexicon. Do NOT modify the strings.
   BOOL              fRandomize - If TRUE then randomize the words, else the words will
                     be in whatever order they're gotten from the lexicons.
returns
   none
*/
void CMLexicon::EnumEntireLexicon (PCListFixed pListPWSTR, BOOL fRandomize)
{
   pListPWSTR->Init (sizeof(PWSTR));

   // parent lex?
   if (m_pMasterLex)
      m_pMasterLex->EnumEntireLexicon (pListPWSTR, FALSE);  // dont randimze


   // enumerate this
   DWORD i;
   pListPWSTR->Required (pListPWSTR->Num() + m_lWords.Num());
   for (i = 0; i < m_lWords.Num(); i++) {
      DWORD *pdwWord = (DWORD*)m_lWords.Get(i);
      if (!pdwWord)
         return;
      PBYTE pbWord = (PBYTE)m_memWords.p + pdwWord[0];

      // length of string
      PWSTR pszWord = (PWSTR)pbWord;
      pListPWSTR->Add (&pszWord);
   } // i

   // randomize?
   DWORD dwNum = pListPWSTR->Num();
   if (fRandomize && dwNum) {
      srand (GetTickCount());

      PWSTR *ppsz = (PWSTR*)pListPWSTR->Get(0);
      for (i = 0; i < dwNum; i++) {
         DWORD j = rand() % dwNum;
         PWSTR pszTemp = ppsz[i];
         ppsz[i] = ppsz[j];
         ppsz[j] = pszTemp;
      } // i
   } // randomize
}

/*************************************************************************************
CMLexicon::ReviewListGet - This returns a lexicon object that is just used
to keep track of what words have already have their pronunciations reviewed.
No pronunciations are stored in the lexicon, just words.
The lexicon object is really a file with the same name as the current lexicon
except that it has its extension changed.

returns
   PCMLexicon - Review lexicon. This must be uncached using MLexiconCacheClose,
   and NOT deleted
*/
CMLexicon *CMLexicon::ReviewListGet (void)
{
   WCHAR szTemp[256];
   wcscpy (szTemp, m_szFile);
   DWORD dwLen = (DWORD)wcslen(szTemp);
   if (dwLen < 4)
      return NULL;
   szTemp[dwLen-1] = L'r';  // to indicate review

   return MLexiconCacheOpen (szTemp, TRUE);
}


/*************************************************************************************
CMLexicon::ReviewProduceNext - This produces a list of words to be reviewed next.
If the pTreeWords is not NULL then this is used to limit the words to review.
Otherwise, the entire lexicon is used.

inputs
   PCBTree        pTreeWords - If valid then limit words to review, else use
                                 the entire lexicon
   DWORD          dwMode - if 0 then review only words in the lexicon (or in the tree and also in the lexicon)
                              but which have not been reviewed,
                           if 1 then review only words in the tree but not in the lexicon, but
                              which have not bee reviewed
   DWORD          dwNumMax - Number of words to review
   PCMLexicon     pLexTemp - If not NULL, then use this lexicon for a list of words, otherwise
                              call ReviewListGet()
   PWSTR          pszFind - If not NULL, then this string must be contained within the word's
                              string for a match. For example, this could be "ing" to look
                              for all words ending in "ing"
   int            iFindLoc - If -1 pszFind must be at the start of a word, 0 anywhere,
                              1 at the end of a word
   PCListVariable plReview - Filled with a list of words that need to be reviewed next.
                  These are WCHAR strings
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMLexicon::ReviewProduceNext (PCBTree pTreeWords, DWORD dwMode, DWORD dwNumMax,
                                   PCMLexicon pLexTemp, PWSTR pszFind, int iFindLoc, PCListVariable plReview)
{
   // if have pTreeWords need to create a list and sort...
   CListFixed lSort;
   PWSTR *ppszSort = NULL;
   DWORD i;
   if (pTreeWords) {
      lSort.Init (sizeof(PWSTR));
      lSort.Required (pTreeWords->Num());
      for (i = 0; i < pTreeWords->Num(); i++) {
         PWSTR psz = pTreeWords->Enum(i);
         lSort.Add (&psz);
      }
      qsort (lSort.Get(0), lSort.Num(), sizeof(PWSTR), PWSTRCompare);

      ppszSort = (PWSTR*)lSort.Get(0);
   }

   // clear review list
   plReview->Clear();

   // get the review lexicon
   PCMLexicon pReview = pLexTemp ? pLexTemp : ReviewListGet ();

   DWORD dwFindLen = pszFind ? (DWORD)wcslen(pszFind) : 0;

   // loop through all the words
   DWORD dwNum = pTreeWords ? lSort.Num() : WordNum();
   WCHAR szBuf[128];
   for (i = 0; (i < dwNum) && (plReview->Num() < dwNumMax); i++) {
      // get the word...
      PWSTR pszWord;
      if (ppszSort)
         pszWord = ppszSort[i];
      else {
         if (!WordGet (i, szBuf, sizeof(szBuf), NULL))
            continue;
         pszWord = szBuf;
      }

      // if it's on the reviewed list continue
      if (pReview && (-1 != pReview->WordFind (pszWord)))
         continue;

      // if have something want to find then try to do so
      if (dwFindLen) switch (iFindLoc) {
      case -1: // look before
         if (_wcsnicmp(pszWord, pszFind, dwFindLen))
            continue;
         break;
      case 0:  // anywhere
         if (!MyStrIStr(pszWord, pszFind))
            continue;
         break;
      case 1:  // look at end
         DWORD dwWordLen = (DWORD)wcslen(pszWord);
         if (dwWordLen < dwFindLen)
            continue;
         if (_wcsicmp (pszWord + (dwWordLen-dwFindLen), pszFind))
            continue;
         break;
      }

      // depending upon the mode
      switch (dwMode) {
      default:
      case 0:  // word must be in master lex
         if (-1 == WordFind (pszWord))
            continue;
         break;
      case 1:  // word must not be in master lex
         if (-1 != WordFind (pszWord))
            continue;
         break;
      }

      // new word so add it
      plReview->Add (pszWord, (wcslen(pszWord)+1)*sizeof(WCHAR));
   } // i

   // close review lexicon
   if (!pLexTemp && pReview)
      MLexiconCacheClose (pReview);

   return TRUE;
}





/****************************************************************************
LexPronReviewPage
*/
static BOOL LexPronReviewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPMI pmi = (PPMI)pPage->m_pUserData;
   PCMLexicon pLex = (PCMLexicon) pmi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set pronunciation text
         DWORD i, j;
         CListVariable lForm;
         PCEscControl pControl;
         for (i = 0; i < pmi->plOOV->Num(); i++) {
            PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
            if (!psz)
               continue;
         
            lForm.Clear();
            pLex->WordGet (psz, &lForm);

            for (j = 0; j < lForm.Num(); j++) {
               WCHAR szTemp[64], szPhone[256];
               swprintf (szTemp, L"e:%d:%d", (int)i, (int)j);
               pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;

               PBYTE pb = (PBYTE)lForm.Get(j);
               if (!pLex->PronunciationToText (pb+1, szPhone, sizeof(szPhone)/sizeof(WCHAR)))
                  continue;
               pControl->AttribSet (Text(), szPhone);
            } // j
         } // i

         // show tts
         pLex->LexUIShowTTS(pPage);
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"opentts")) {
            pLex->LexUIChangeTTS(pPage);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearreview")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear the review list?"))
               return TRUE;

            PCMLexicon pReview = pmi->pReviewLex ? pmi->pReviewLex : pLex->ReviewListGet();
            if (pReview) {
               pReview->Clear();
               if (!pmi->pReviewLex)
                  MLexiconCacheClose (pReview);
            }

            pPage->Message (ESCM_USER+82);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"markreview")) {
            // save all prons away
            pPage->Message (ESCM_USER+82);

            PCMLexicon pReview = pmi->pReviewLex ? pmi->pReviewLex : pLex->ReviewListGet();
            if (pReview) {
               DWORD i;
               CListVariable lForm;
               for (i = 0; i < pmi->plOOV->Num(); i++) {
                  PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
                  if (!psz)
                     continue;
                  pReview->WordSet (psz, &lForm);
               }
               if (!pmi->pReviewLex)
                  MLexiconCacheClose (pReview);
            }

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'p') && (psz[1] == L':')) {
            WCHAR szTemp[64];
            wcscpy (szTemp, psz);
            szTemp[0] = L'e';

            DWORD dwNum = _wtoi(psz + 2);

            pLex->LexUIPronFromEdit (pPage, szTemp, TRUE, NULL, 0, NULL, 0, 
               (PWSTR)pmi->plOOV->Get(dwNum), TRUE);
            return TRUE;
         }
      }
      return TRUE;


   case ESCM_USER+82:   // get the values..
      {
         // go through all words and set pron
         // set pronunciation text
         DWORD i, j;
         CListVariable lForm;
         for (i = 0; i < pmi->plOOV->Num(); i++) {
            PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
            if (!psz)
               continue;
         
            lForm.Clear();
            pLex->WordGet (psz, &lForm);

            // BUGFIX - Go backwards so can delete
            for (j = lForm.Num()-1; j < lForm.Num() ; j--) {
               WCHAR szTemp[64];
               swprintf (szTemp, L"e:%d:%d", (int)i, (int)j);

               pLex->LexUIPronFromEdit (pPage, szTemp, FALSE, NULL, 0, psz, j, NULL, NULL);
            } // j
         } // i

      }
      return TRUE;


   case ESCM_CLOSE:
      {
         // make sure to save entries
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_LINK:
      {
         // make sure to save entries
         // only really need to do this for back, but do for all links
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Pronunciation review";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDLIST")) {
            if (!pmi->plOOV->Num()) {
               p->pszSubString = L"<tr><td>There aren't any more words to review.</td></tr>";
               return TRUE;
            }

            MemZero (&gMemTemp);

            DWORD i, j;
            CListVariable lForm;
            for (i = 0; i < pmi->plOOV->Num(); i++) {
               PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
               MemCat (&gMemTemp, L"<tr>");

               // get the pronunciations
               lForm.Clear();
               pLex->WordGet (psz, &lForm);

               // name
               MemCat (&gMemTemp, L"<td width=33%%><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold></td>");

               // edit box with play
               MemCat (&gMemTemp, L"<td align=center width=66%%>");
               for (j = 0; j < max(1,lForm.Num()); j++) {
                  if (j)
                     MemCat (&gMemTemp, L"<br/>");
                  MemCat (&gMemTemp, L"<bold><edit width=80%% maxchars=64 name=e:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L":");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L"/></bold><button style=teapot name=p:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L":");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L"/>");
               }
               MemCat (&gMemTemp, L"</td>");

               MemCat (&gMemTemp, L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONELIST")) {
            p->pszSubString = LexPhoneList(pLex);
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogPronReview - Brings up the main pronunciation editing dialog

inputs
   PCEscWindow          pWindow - window to use
   PCBTree              pTreeReview - Words to limit review to
   BOOL                 fTempLex - If TRUE store reviewed words in tempoary lexicon,
                              else write them to disk
   PWSTR          pszFind - If not NULL, then this string must be contained within the word's
                              string for a match. For example, this could be "ing" to look
                              for all words ending in "ing"
   int            iFindLoc - If -1 pszFind must be at the start of a word, 0 anywhere,
                              1 at the end of a word
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPronReview (PCEscWindow pWindow, PCBTree pTreeReview, BOOL fTempLex,
                                  PWSTR pszFind, int iFindLoc)
{
   PWSTR pszRet;
   PMI pmi;
   CListVariable lOOV;
   //CM3DWave Wave;
   CMLexicon LexTemp;
   memset (&pmi, 0, sizeof(pmi));
   pmi.dwFirstWord = 0;
   pmi.pLex = this;
   pmi.plOOV = &lOOV;
   //pmi.pWave = &Wave;
   pmi.pReviewLex = fTempLex ? &LexTemp : NULL;

redo:
   Save();  // just to save changes every go around in case crashes
   ReviewProduceNext (pTreeReview,  0, NUMREVIEW, pmi.pReviewLex, pszFind, iFindLoc, &lOOV);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPRONREVIEW, LexPronReviewPage, &pmi);
   if (!pszRet)
      return FALSE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}

/*************************************************************************************
CMLexicon::BulkConvert - Convert prefix/suffix in bulk.

inputs
   PWSTR          pszText - Text looking for
   PBYTE          pbPhone - Phonemes looking for. (Unsort + 1)
   BOOL           fEnd - If TRUE look at end of word, else beginning
   PBYTE          pbTo - Convert the phonemes to this. (Unsort + 1)
   PCProgressSocket pProgress - Progress
returns
   DWORD - Number that converted
*/
DWORD CMLexicon::BulkConvert (PWSTR pszText, PBYTE pbPhone, BOOL fEnd,
                             PBYTE pbTo, PCProgressSocket pProgress)
{
   DWORD dwCount = 0;
   DWORD dwLenText = (DWORD)wcslen(pszText);
   if (!dwLenText)
      return 0;
   DWORD dwLenPhone = (DWORD)strlen((char*)pbPhone);
   DWORD dwLenTo = (DWORD)strlen((char*)pbTo);

   DWORD i;
   CListVariable lForm;
   WCHAR szWord[256];
   for (i = 0; i < WordNum(); i++) {
      if (pProgress && ((i%100) == 0))
         pProgress->Update ((fp)i / (fp)WordNum());

      lForm.Clear();
      if (!WordGet (i, szWord, sizeof(szWord), &lForm))
         continue;

      // if no match then skip
      if (fEnd) {
         DWORD dwWordLen = (DWORD)wcslen(szWord);
         if (dwWordLen < dwLenText)
            continue;   // too short
         if (_wcsicmp(szWord + (dwWordLen-dwLenText), pszText))
            continue;
      }
      else {
         if (_wcsnicmp(szWord, pszText, dwLenText))
            continue;
      }

      // see if should change
      BYTE abTemp[256];
      BOOL fChanged = FALSE;
      DWORD j;
      for (j = 0; j < lForm.Num(); j++) {
         PBYTE pbCur = (PBYTE) lForm.Get(j);
         if (!pbCur)
            continue;

         DWORD dwLen = (DWORD)strlen((char*)(pbCur+1));

         // see if match
         if (fEnd) {
            if (dwLen < dwLenPhone)
               continue;   // too short
            if (strcmp((char*)pbCur + (1 + dwLen - dwLenPhone), (char*)pbPhone))
               continue;
         }
         else {
            if (strncmp((char*)pbCur+1, (char*)pbPhone, dwLenPhone))
               continue;
         }

         // if would be too long then skip
         if (dwLen + 2 - dwLenPhone + dwLenTo >= sizeof(abTemp))
            continue;

         // copy over
         memcpy (abTemp, pbCur, 1 + dwLen + 1);
         DWORD dwCurLen = 2 + dwLen;

         // move after the insert point
         DWORD dwInsertPoint;
         if (fEnd)
            dwInsertPoint = 1 + dwLen - dwLenPhone;
         else
            dwInsertPoint = 1;

         // move
         memmove (abTemp + (dwInsertPoint + dwLenTo),
            abTemp + (dwInsertPoint + dwLenPhone),
            dwCurLen - (dwInsertPoint + dwLenPhone));
         memcpy (abTemp + dwInsertPoint, pbTo, dwLenTo);

         // write it
         lForm.Set (j, abTemp, 1 + strlen((char*)abTemp+1) + 1);
         fChanged = TRUE;
      } // j
      if (!fChanged)
         continue;

      // if changed then save it
      WordSet (szWord, &lForm);
      dwCount++;
   } // i

   return dwCount;
}

/*************************************************************************************
CMLexicon::POSToString - Takes a part of speech number and writes out a string
describing it.

inputs
   BYTE           bPOS - part of speech
   BOOL           fShort - If TRUE return shortened version
returns
   PCWSTR - string
*/
PCWSTR CMLexicon::POSToString (BYTE bPOS, BOOL fShort)
{
   switch (POS_MAJOR_ISOLATE(bPOS)) {
   default:
   case POS_MAJOR_UNKNOWN:
      return fShort ? L"Unk" : L"Unknown";
   case POS_MAJOR_NOUN:
      return fShort ? L"N" : L"Noun";
   case POS_MAJOR_PRONOUN:
      return fShort ? L"Pron" : L"Pronoun";
   case POS_MAJOR_ADJECTIVE:
      return fShort ? L"Adj" : L"Adjective";
   case POS_MAJOR_PREPOSITION:
      return fShort ? L"Prep" : L"Preposition";
   case POS_MAJOR_ARTICLE:
      return fShort ? L"Art" : L"Article";
   case POS_MAJOR_VERB:
      return fShort ? L"V" : L"Verb";
   case POS_MAJOR_ADVERB:
      return fShort ? L"Adv" : L"Adverb";
   case POS_MAJOR_AUXVERB:
      return fShort ? L"Aux v" : L"Auxiliary verb";
   case POS_MAJOR_CONJUNCTION:
      return fShort ? L"Conj" : L"Conjunction";
   case POS_MAJOR_INTERJECTION:
      return fShort ? L"Interj" : L"Interjection";
   case POS_MAJOR_PUNCTUATION:
      return fShort ? L"Punct" : L"Punctuation";
   }
}

/*************************************************************************************
CMLexicon::POSToStatus - Fills in a status control based on the part of
speech.

inputs
   PCEscPage   pPage - Page
   PWSTR       pszStatus - Status control name
   PWSTR       pszScroll - Scrollbar name. Can be NULL
   BYTE        bPOS - Part of speech
returns
   BOOl - TRUE if found
*/
BOOL CMLexicon::POSToStatus (PCEscPage pPage, PWSTR pszStatus, PWSTR pszScroll, BYTE bPOS)
{
   PCEscControl pControl = pPage->ControlFind (pszStatus);
   if (!pControl)
      return FALSE;

   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<colorblend color=#");
   PWSTR psz;
   switch (POS_MAJOR_ISOLATE(bPOS)) {
   default:
   case POS_MAJOR_UNKNOWN:
      psz = L"808080";
      break;
   case POS_MAJOR_NOUN:
      psz = L"4040ff";
      break;
   case POS_MAJOR_PRONOUN:
      psz = L"4040c0";
      break;
   case POS_MAJOR_ADJECTIVE:
      psz = L"4080c0";
      break;
   case POS_MAJOR_PREPOSITION:
      psz = L"8040c0";
      break;
   case POS_MAJOR_ARTICLE:
      psz = L"404080";
      break;
   case POS_MAJOR_VERB:
      psz = L"ff0000";
      break;
   case POS_MAJOR_ADVERB:
      psz = L"c04000";
      break;
   case POS_MAJOR_AUXVERB:
      psz = L"804040";
      break;
   case POS_MAJOR_CONJUNCTION:
      psz = L"00ff00";
      break;
   case POS_MAJOR_INTERJECTION:
      psz = L"ffff00";
      break;
   }
   MemCat (&gMemTemp, psz);
   MemCat (&gMemTemp, L" posn=background/><null>");
   MemCatSanitize (&gMemTemp, (PWSTR) POSToString(bPOS));
   MemCat (&gMemTemp, L"</null>");

   ESCMSTATUSTEXT st;
   memset (&st, 0, sizeof(st));
   st.pszMML = (PWSTR)gMemTemp.p;
   pControl->Message (ESCM_STATUSTEXT, &st);

   // set scrollbar
   if (!pszScroll)
      return TRUE;
   pControl = pPage->ControlFind (pszScroll);
   if (!pControl)
      return FALSE;
   pControl->AttribSetInt (Pos(), (int)POS_MAJOR_EXTRACT(bPOS));

   return TRUE;
}



/*************************************************************************************
CMLexicon::POSReviewProduceNext - This produces a list of words to be reviewed next.
If the pTreeWords is not NULL then this is used to limit the words to review.
Otherwise, the entire lexicon is used.

inputs
   PCBTree        pTreeWords - If valid then limit words to review, else use
                                 the entire lexicon
   DWORD          dwNumMax - Number of words to review
   PCMLexicon     pLexTemp - If NULL, then review will show only those words whith a POS = unknown.
                              If not NULL then review will show only those words NOT in pLexTemp,
                              and will add to pLexTemp as they're reviewed
   PWSTR          pszFind - If not NULL, then this string must be contained within the word's
                              string for a match. For example, this could be "ing" to look
                              for all words ending in "ing"
   int            iFindLoc - If -1 pszFind must be at the start of a word, 0 anywhere,
                              1 at the end of a word
   PCListVariable plReview - Filled with a list of words that need to be reviewed next.
                  These are WCHAR strings
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMLexicon::POSReviewProduceNext (PCBTree pTreeWords, DWORD dwNumMax,
                                   PCMLexicon pLexTemp, PWSTR pszFind, int iFindLoc, PCListVariable plReview)
{
   // if have pTreeWords need to create a list and sort...
   CListFixed lSort;
   PWSTR *ppszSort = NULL;
   DWORD i;
   if (pTreeWords) {
      lSort.Init (sizeof(PWSTR));
      for (i = 0; i < pTreeWords->Num(); i++) {
         PWSTR psz = pTreeWords->Enum(i);
         lSort.Add (&psz);
      }
      qsort (lSort.Get(0), lSort.Num(), sizeof(PWSTR), PWSTRCompare);

      ppszSort = (PWSTR*)lSort.Get(0);
   }

   // clear review list
   plReview->Clear();

   DWORD dwFindLen = pszFind ? (DWORD)wcslen(pszFind) : 0;

   // loop through all the words
   DWORD dwNum = pTreeWords ? lSort.Num() : WordNum();
   WCHAR szBuf[128];
   CListVariable lForm;
   DWORD j;
   for (i = 0; (i < dwNum) && (plReview->Num() < dwNumMax); i++) {
      // get the word...
      PWSTR pszWord;
      if (ppszSort)
         pszWord = ppszSort[i];
      else {
         if (!WordGet (i, szBuf, sizeof(szBuf), NULL))
            continue;
         pszWord = szBuf;
      }

      // if have something want to find then try to do so
      if (dwFindLen) switch (iFindLoc) {
      case -1: // look before
         if (_wcsnicmp(pszWord, pszFind, dwFindLen))
            continue;
         break;
      case 0:  // anywhere
         if (!MyStrIStr(pszWord, pszFind))
            continue;
         break;
      case 1:  // look at end
         DWORD dwWordLen = (DWORD)wcslen(pszWord);
         if (dwWordLen < dwFindLen)
            continue;
         if (_wcsicmp (pszWord + (dwWordLen-dwFindLen), pszFind))
            continue;
         break;
      }

      // if cant get word then fail
      lForm.Clear();
      if (!WordGet (pszWord, &lForm))
         continue;

      if (pLexTemp) {
         // if have lexicon, make sure the word is not on the reviewed list
         if (-1 != pLexTemp->WordFind(pszWord))
            continue;
      }
      else {
         // else, make sure it has a vacant POS
         for (j = 0; j < lForm.Num(); j++) {
            PBYTE pb = (PBYTE)lForm.Get(j);
            if (!pb[0])
               break;
         }
         if (j >= lForm.Num())
            continue;   // all POS filled
      }
      
      // new word so add it
      plReview->Add (pszWord, (wcslen(pszWord)+1)*sizeof(WCHAR));
   } // i

   return TRUE;
}




/****************************************************************************
LexPOSReviewPage
*/
static BOOL LexPOSReviewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PPMI pmi = (PPMI)pPage->m_pUserData;
   PCMLexicon pLex = (PCMLexicon) pmi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set POS
         DWORD i, j;
         CListVariable lForm;
         for (i = 0; i < pmi->plOOV->Num(); i++) {
            PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
            if (!psz)
               continue;
         
            lForm.Clear();
            pLex->WordGet (psz, &lForm);

            for (j = 0; j < lForm.Num(); j++) {
               WCHAR szTemp[64], szTemp2[256];
               swprintf (szTemp, L"s:%d:%d", (int)i, (int)j);
               wcscpy (szTemp2, szTemp);
               szTemp2[0] = L't';

               PBYTE pb = (PBYTE)lForm.Get(j);

               pLex->POSToStatus (pPage, szTemp2, szTemp, pb[0]);
            } // j
         } // i

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"markreview")) {
            // set unknown POS?
            DWORD dwToNoun = pmi->pReviewLex ? FALSE : TRUE;

            // save all prons away
            pPage->Message (ESCM_USER+82, &dwToNoun);

            PCMLexicon pReview = pmi->pReviewLex;
            if (pReview) {
               DWORD i;
               CListVariable lForm;
               for (i = 0; i < pmi->plOOV->Num(); i++) {
                  PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
                  if (!psz)
                     continue;
                  pReview->WordSet (psz, &lForm);
               }
            }

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      return TRUE;

   case ESCN_SCROLLING:
   case ESCN_SCROLL:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;
         if (!p || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         // must start with pos
         if ((psz[0] != 's') || (psz[1] != ':'))
            break; // dont care

         // get the POS
         BYTE bPOS;
         bPOS = POS_MAJOR_MAKE(p->iPos);

         // set the status, dont bother writing because do that late
         WCHAR szControl[64];
         wcscpy (szControl, psz);
         szControl[0] = L't';
         pLex->POSToStatus (pPage, szControl, NULL, bPOS);
         return TRUE;
      }
      break;


   case ESCM_USER+82:   // get the values..
      {
         DWORD *pdwToNoun = (DWORD*)pParam;

         // go through all words and set pron
         // set pronunciation text
         DWORD i, j;
         CListVariable lForm;
         for (i = 0; i < pmi->plOOV->Num(); i++) {
            PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
            if (!psz)
               continue;
         
            lForm.Clear();
            pLex->WordGet (psz, &lForm);

            BOOL fChanged = FALSE;
            for (j = 0; j < lForm.Num() ; j++) {
               WCHAR szTemp[64];
               swprintf (szTemp, L"s:%d:%d", (int)i, (int)j);
               PCEscControl pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;

               PBYTE pb = (PBYTE)lForm.Get(j);
               int iPos = pControl->AttribGetInt (Pos());
               BYTE bNew = POS_MAJOR_MAKE(iPos);

               // if want to convert all remaining to noun, and it is still unknown
               // then convert it to a noun
               if (pdwToNoun && *pdwToNoun && !bNew)
                  bNew = POS_MAJOR_NOUN;

               if (pb[0] != bNew) {
                  pb[0] = bNew;
                  fChanged = TRUE;
               }
            } // j
            if (fChanged)
               pLex->WordSet (psz, &lForm);
         } // i

      }
      return TRUE;


   case ESCM_CLOSE:
      {
         // make sure to save entries
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_LINK:
      {
         // make sure to save entries
         // only really need to do this for back, but do for all links
         pPage->Message (ESCM_USER+82);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Review part-of-speech";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFSTORELEX")) {
            p->pszSubString = pmi->pReviewLex ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFSTORELEX")) {
            p->pszSubString = pmi->pReviewLex ? L"" : L"</comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFNSTORELEX")) {
            p->pszSubString = pmi->pReviewLex ? L"<comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFNSTORELEX")) {
            p->pszSubString = pmi->pReviewLex ? L"</comment>" : L"";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDLIST")) {
            if (!pmi->plOOV->Num()) {
               p->pszSubString = L"<tr><td>There aren't any more words to review.</td></tr>";
               return TRUE;
            }

            MemZero (&gMemTemp);

            DWORD i, j;
            CListVariable lForm;
            for (i = 0; i < pmi->plOOV->Num(); i++) {
               PWSTR psz = (PWSTR)pmi->plOOV->Get(i);
               MemCat (&gMemTemp, L"<tr>");

               // get the pronunciations
               lForm.Clear();
               pLex->WordGet (psz, &lForm);

               // name
               MemCat (&gMemTemp, L"<td width=33%%><bold>");
               MemCatSanitize (&gMemTemp, psz);
               MemCat (&gMemTemp, L"</bold></td>");

               // edit box with play
               MemCat (&gMemTemp, L"<td align=center width=66%%>");
               for (j = 0; j < max(1,lForm.Num()); j++) {
                  if (j)
                     MemCat (&gMemTemp, L"<br/>");
                  MemCat (&gMemTemp, L"<status width=33%% height=30 name=t:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L":");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L"/>");
                  MemCat (&gMemTemp, L"<scrollbar width=66%% orient=horz min=0 max=10 name=s:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L":");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L"/>");
               }
               MemCat (&gMemTemp, L"</td>");

               MemCat (&gMemTemp, L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogPOSReview - Brings up the main POS review dialog

inputs
   PCEscWindow          pWindow - window to use
   PCBTree              pTreeReview - Words to limit review to
   BOOL                 fTempLex - If TRUE store reviewed words in tempoary lexicon and
                           review words even if have POS. If FALSE then only review words
                           without POS
   PWSTR          pszFind - If not NULL, then this string must be contained within the word's
                              string for a match. For example, this could be "ing" to look
                              for all words ending in "ing"
   int            iFindLoc - If -1 pszFind must be at the start of a word, 0 anywhere,
                              1 at the end of a word
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogPOSReview (PCEscWindow pWindow, PCBTree pTreeReview, BOOL fTempLex,
                                  PWSTR pszFind, int iFindLoc)
{
   PWSTR pszRet;
   PMI pmi;
   CListVariable lOOV;
   //CM3DWave Wave;
   CMLexicon LexTemp;
   memset (&pmi, 0, sizeof(pmi));
   pmi.dwFirstWord = 0;
   pmi.pLex = this;
   pmi.plOOV = &lOOV;
   //pmi.pWave = &Wave;
   pmi.pReviewLex = fTempLex ? &LexTemp : NULL;

redo:
   Save();  // just to save changes every go around in case crashes
   POSReviewProduceNext (pTreeReview,  NUMREVIEW, pmi.pReviewLex, pszFind, iFindLoc, &lOOV);

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXPOSREVIEW, LexPOSReviewPage, &pmi);
   if (!pszRet)
      return FALSE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;
   else if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   return FALSE;
}


/*************************************************************************************
CMLexicon::POSBulkConvert - Convert prefix/suffix in bulk.

inputs
   PWSTR          pszText - Text looking for
   BOOL           fEnd - If TRUE look at end of word, else beginning
   BOOL           fMustBeUnknown - If TRUE the existing POS must be unknown before will
                  change. If FALSE will change no matter what
   BYTE           bPOS - POS to convert to
   PCProgressSocket pProgress - Progress
returns
   DWORD - Number that converted
*/
DWORD CMLexicon::POSBulkConvert (PWSTR pszText, BOOL fEnd, BOOL fMustBeUnknown,
                             BYTE bPOS, PCProgressSocket pProgress)
{
   DWORD dwCount = 0;
   DWORD dwLenText = (DWORD)wcslen(pszText);
   if (!dwLenText)
      return 0;

   DWORD i;
   CListVariable lForm;
   WCHAR szWord[256];
   for (i = 0; i < WordNum(); i++) {
      if (pProgress && ((i%100) == 0))
         pProgress->Update ((fp)i / (fp)WordNum());

      lForm.Clear();
      if (!WordGet (i, szWord, sizeof(szWord), &lForm))
         continue;

      // if no match then skip
      if (fEnd) {
         DWORD dwWordLen = (DWORD)wcslen(szWord);
         if (dwWordLen < dwLenText)
            continue;   // too short
         if (_wcsicmp(szWord + (dwWordLen-dwLenText), pszText))
            continue;
      }
      else {
         if (_wcsnicmp(szWord, pszText, dwLenText))
            continue;
      }

      // see if should change
      BOOL fChanged = FALSE;
      DWORD j;
      for (j = 0; j < lForm.Num(); j++) {
         PBYTE pbCur = (PBYTE) lForm.Get(j);
         if (!pbCur)
            continue;

         // if POS is known, but can only change unknown then continue
         if (fMustBeUnknown && pbCur[0])
            continue;

         // if same as what want ignore
         if (pbCur[0] == bPOS)
            continue;

         // else change
         pbCur[0] = bPOS;
         fChanged = TRUE;
      } // j
      if (!fChanged)
         continue;

      // if changed then save it
      WordSet (szWord, &lForm);
      dwCount++;
   } // i

   return dwCount;
}


/*************************************************************************************
NGramIndex - Returns the index for the N-gram given the POS.

inputs
   PBYTE       pabPOS - Pointer to an array of LEXPOSNGRAM POS. Words are in temporal order
returns
   DWORD - Index for ngram.
*/
__inline DWORD NGramIndex (PBYTE pabPOS)
{
   // what's the number
   DWORD dwBin = 0;
   DWORD i;
   for (i = 0; i < LEXPOSNGRAM; i++) {
      dwBin *= (POS_MAJOR_NUM+1);
      dwBin += pabPOS[i];
   }
   return dwBin;
}


/*************************************************************************************
CMLexicon::POSGuess - This guesses the POS for a sentence.

inputs
   PLEXPOSGUESS      paLPG - Pointer to an arrary of LEXPAUSEGUSS that have their
                        words initially filled in. Punctuation (such as commas) should
                        also be put in, with pszWord = ",", etc.
                        When the function returns bPOS will be filled with their guessed POS.
                        This is POS_MAJOR_XXX.

                        The wPOSBitField elements are important and MUST be filled in.

   DWORD             dwNum - Number of elements in paLPG
returns
   none
*/
void CMLexicon::POSGuess (PLEXPOSGUESS paLPG, DWORD dwNum)
{
#ifdef _DEBUG
   CMem memTemp;
#endif
   m_LexParse.POSGuess (paLPG, dwNum,
#ifdef _DEBUG
      &memTemp,
#else
      NULL,
#endif
      NULL, this);
}



/*************************************************************************************
CMLexicon::WordToPOSBitField - Internal method used to convert a word string
into a part-of-speech bit field, indicating the possible parts of speech
that it could be.

inputs
   PWSTR       psz - Word string. COuld be NULL
returns
   WORD - Bit field, with a bit set fot the major parts of speech.
            Thus 0x0c means bits 2 and 3 are turned on, which corresponds
            to major parts of speech 0x20 and 0x30.
*/
WORD CMLexicon::WordToPOSBitField (PWSTR psz)
{
   CListVariable lForm;
   if (psz)
      WordPronunciation (psz, &lForm, TRUE, NULL, NULL);

   // determine if this is punctuation
   BOOL fIsPunct = TRUE;
   DWORD j;
   if (psz) for (j = 0; psz[j]; j++)
      if (!iswpunct(psz[j])) {
         fIsPunct = FALSE;
         break;
      }

   // if it's a punctuation then mark as such
   if (fIsPunct)
      return (1 << POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION));

   // if there are no pronuncaitions (shouldnt happen) then assume none
   if (!lForm.Num())
      return (1 << POS_MAJOR_EXTRACT(POS_MAJOR_NOUN));

   // else, loop through all the forms
   WORD wAdd = 0;
   for (j = 0; j < lForm.Num(); j++) {
      WORD wTemp;
      PBYTE pb = (PBYTE)lForm.Get(j);
      wTemp = POS_MAJOR_EXTRACT(*pb);
      if (wTemp == POS_MAJOR_EXTRACT(POS_MAJOR_UNKNOWN))
         wTemp = POS_MAJOR_EXTRACT(POS_MAJOR_NOUN); // if unknown convert to noun
      
      wTemp = 1 << wTemp;
      wAdd |= wTemp;
   } // j

   return wAdd;
}

/*************************************************************************************
CMLexicon::POSMMLParseToLEXPOSGUESS - This accepts a PCMMLNode2 containg words
and punctuation (such as from CTextParse::ParseFromMMLText()), and fills in
a list with all the relevent words and punctuation. The POS is filled into the list
too. The pvUserData of the LEXPOSGUESS structure is a pointer to the MML node.

inputs
   PCMMLNode2         pNode - Node contiaing words and punctuation
   PCListFixed       plLEXPOSGUESS - Initialized to sizeof(LEXPOSGUESS) and filled with entries
   BOOL              fFillPOS - If TRUE then writes the POS information back into the
                     pNode words
   PCTextParse       pParse - Text parser to use for definition of Word() and Punctuation()
   BOOL              *pfBackedOff - Will fill this in with TRUE if backed off any of
                        the words. When doing UI to analyze sentence use this to detect
                        if need to ask user for more info.
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::POSMMLParseToLEXPOSGUESS (PCMMLNode2 pNode, PCListFixed plLEXPOSGUESS,
                                          BOOL fFillPOS, PCTextParse pParse,
                                          BOOL *pfBackedOff)
{
   plLEXPOSGUESS->Init (sizeof(LEXPOSGUESS));
   if (pfBackedOff)
      *pfBackedOff = FALSE;

   // loop through all the element in the node
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   LEXPOSGUESS lpg;
   memset (&lpg, 0, sizeof(lpg));
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub =NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, pParse->Word()) || !_wcsicmp(psz, pParse->Punctuation())) {
         psz = pSub->AttribGetString (pParse->Text());
         if (!psz)
            continue;

         lpg.pszWord = psz;
         lpg.pvUserData = pSub;
         lpg.wPOSBitField = WordToPOSBitField (lpg.pszWord);
         plLEXPOSGUESS->Add (&lpg);
         continue;
      }
   } // i

   // figure out POS
   PLEXPOSGUESS pLPG = (PLEXPOSGUESS) plLEXPOSGUESS->Get(0);
   DWORD dwNum = plLEXPOSGUESS->Num();

   // guess the POS
   POSGuess (pLPG, dwNum);

#if 0 // old code
   // BUGFIX - break on sentence boundaries or max 30 words
      // BUGFIX - reduced sentence size to 30 words
   DWORD dwSentStart, dwSentEnd, dwLastPunct;
   for (dwSentStart = 0; dwSentStart < dwNum; dwSentStart = dwSentEnd) {
      dwLastPunct = -1;
      for (dwSentEnd = dwSentStart+1; dwSentEnd < dwNum; dwSentEnd++) {
         WCHAR c = pLPG[dwSentEnd].pszWord[0];
         if (iswpunct (c))
            dwLastPunct = dwSentEnd+1;
         if ((c == L'.') || (c == L'?') || (c == L'!')) {
            dwSentEnd++;
            break;
         }

         // BUGFIX - If have to break sentence break at punctuation
         // BUGFIX - Upped from 30 to 100, since can now handle this
         // without getting very slow, due to new search for best POS
         if (dwSentEnd - dwSentStart >= MAXSENTENCELENGTH) {
            // sentence has gotten too long, so break
            if (dwLastPunct != -1)
               dwSentEnd = dwLastPunct;
            break;
         }
      }

      BOOL fBack;
      POSGuess (pLPG + dwSentStart, dwSentEnd - dwSentStart, &fBack);
      if (pfBackedOff && fBack)
         *pfBackedOff = TRUE;
   } // dwSentStart
#endif // 0


   // write POS in?
   WCHAR szTemp[32];
   if (fFillPOS) for (i = 0; i < dwNum; i++) {
      PCMMLNode2 pSub = (PCMMLNode2)pLPG[i].pvUserData;
      _itow ((int)pLPG[i].bPOS, szTemp, 10);
      if (!pSub->AttribGetString(pParse->POS()))   // BUGFIX - Dont set POS if already have pre-set
         pSub->AttribSetString (pParse->POS(), szTemp);
#ifdef _DEBUG
      else {
         OutputDebugString ("POS already exists\r\n");
         szTemp[0] = 0;
      }
#endif

      // rule depth
      _itow ((int)pLPG[i].bRuleDepthLowDetail, szTemp, 10);
      if (!pSub->AttribGetString(pParse->RuleDepthLowDetail()))   // BUGFIX - Dont set POS if already have pre-set
         pSub->AttribSetString (pParse->RuleDepthLowDetail(), szTemp);
#ifdef _DEBUG
      else {
         OutputDebugString ("RuleDepthLowDetail already exists\r\n");
         szTemp[0] = 0;
      }
#endif

      // parseruledepth
      _itow ((int)*((DWORD*)&pLPG[i].ParseRuleDepth), szTemp, 10);
      _ASSERTE (sizeof(DWORD) == sizeof(pLPG[i].ParseRuleDepth));
      if (!pSub->AttribGetString(pParse->ParseRuleDepth()))   // BUGFIX - Dont set POS if already have pre-set
         pSub->AttribSetString (pParse->ParseRuleDepth(), szTemp);
#ifdef _DEBUG
      else {
         OutputDebugString ("ParseRuleDepth already exists\r\n");
         szTemp[0] = 0;
      }
#endif

   }

   // done
   return TRUE;
}



/*************************************************************************************
CMLexicon::POSWaveToLEXPOSGUESS - This accepts a PCM3DWave containg words
and punctuation, and fills in
a list with all the relevent words and punctuation. The POS is filled into the list
too. The pvUserData of the LEXPOSGUESS structure is the word number (index into pWave->m_lWVWORD).

inputs
   PCM3DWave         pWave - Wave containing words and punctuation
   PCListFixed       plLEXPOSGUESS - Initialized to sizeof(LEXPOSGUESS) and filled with entries
   PCTextParse       pParse - Text parser to use for definition of Word() and Punctuation()
   BOOL              *pfBackedOff - Will fill this in with TRUE if backed off any of
                        the words. When doing UI to analyze sentence use this to detect
                        if need to ask user for more info.
returns
   BOOL - TRUE if success
*/
BOOL CMLexicon::POSWaveToLEXPOSGUESS (PCM3DWave pWave, PCListFixed plLEXPOSGUESS,
                                          PCTextParse pParse,
                                          BOOL *pfBackedOff)
{
   plLEXPOSGUESS->Init (sizeof(LEXPOSGUESS));
   if (pfBackedOff)
      *pfBackedOff = FALSE;

   // loop through all the element in the wave
   DWORD i;
   PWSTR psz;
   LEXPOSGUESS lpg;
   memset (&lpg, 0, sizeof(lpg));
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      psz = (PWSTR)(pw+1);
      if (!psz[0] || (psz[0] == L' '))
         continue;

      lpg.pszWord = psz;
      lpg.pvUserData = (PVOID)(size_t)i;
      lpg.wPOSBitField = WordToPOSBitField (lpg.pszWord);
      plLEXPOSGUESS->Add (&lpg);
   } // i

   // figure out POS
   PLEXPOSGUESS pLPG = (PLEXPOSGUESS) plLEXPOSGUESS->Get(0);
   DWORD dwNum = plLEXPOSGUESS->Num();

   // guess the POS
   POSGuess (pLPG, dwNum);

#if 0 // old code
   // BUGFIX - break on sentence boundaries or max 30 words
      // BUGFIX - reduced sentence size to 30 words
   DWORD dwSentStart, dwSentEnd, dwLastPunct;
   for (dwSentStart = 0; dwSentStart < dwNum; dwSentStart = dwSentEnd) {
      dwLastPunct = -1;
      for (dwSentEnd = dwSentStart+1; dwSentEnd < dwNum; dwSentEnd++) {
         WCHAR c = pLPG[dwSentEnd].pszWord[0];
         if (iswpunct (c))
            dwLastPunct = dwSentEnd+1;
         if ((c == L'.') || (c == L'?') || (c == L'!')) {
            dwSentEnd++;
            break;
         }

         // BUGFIX - If have to break sentence break at punctuation
         if (dwSentEnd - dwSentStart >= MAXSENTENCELENGTH) {
            // sentence has gotten too long, so break
            if (dwLastPunct != -1)
               dwSentEnd = dwLastPunct;
            break;
         }
      }

      BOOL fBack;
      POSGuess (pLPG + dwSentStart, dwSentEnd - dwSentStart, &fBack);
      if (pfBackedOff && fBack)
         *pfBackedOff = TRUE;
   } // dwSentStart
#endif // 0

   // done
   return TRUE;
}



// DGI - Information for dialog grammar
typedef struct {
   PLEXPOSGUESS         pLPG;    // pointer to words to modify
   DWORD                dwNum;   // number of words in here
   PCMLexicon           pLex;    // lexicon
   DWORD                dwCur;   // sentence number
   DWORD                dwTotal; // total sentence numbers
} DGI, *PDGI;

#if 0 // old code
/****************************************************************************
LexGrammarPage
*/
static BOOL LexGrammarPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PDGI pgdi = (PDGI)pPage->m_pUserData;
   PCMLexicon pLex = pgdi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // set POS
         DWORD i;
         for (i = 0; i < pgdi->dwNum; i++) {
            PLEXPOSGUESS pLPG = pgdi->pLPG + i;

            WCHAR szTemp[64], szTemp2[256];
            swprintf (szTemp, L"s:%d", (int)i);
            wcscpy (szTemp2, szTemp);
            szTemp2[0] = L't';

            pLex->POSToStatus (pPage, szTemp2, szTemp, pLPG->bPOS);
         } // i

      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"train")) {
            // loop through all the words and get their POS
            DWORD i, j;
            WCHAR szTemp[256], szTemp2[256];
            PCEscControl pControl;
            CListVariable lForm;
            for (i = 0; i < pgdi->dwNum; i++) {
               PLEXPOSGUESS pLPG = pgdi->pLPG + i;
               if (pLPG->bPOS == POS_MAJOR_PUNCTUATION)
                  continue; // dont change
               if (!pLPG->pszWord)
                  continue;

               // get it from scrollbar
               swprintf (szTemp, L"s:%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;
               pLPG->bPOS = POS_MAJOR_MAKE(pControl->AttribGetInt(Pos()));
               if (!pLPG->bPOS)
                  pLPG->bPOS = POS_MAJOR_NOUN;  // shouldnt happen

               // look in the lexicon and see if this exists...
               lForm.Clear();
               BOOL fRet;
               fRet = pLex->WordGet (pLPG->pszWord, &lForm);
               if (!fRet || !lForm.Num()) {
                  swprintf (szTemp, L"For you information, the word \"%s\" doesn't exist in the lexicon.",
                     pLPG->pszWord);
                  pPage->MBInformation (szTemp);
                  continue;
               }

               // try to find a matching POS
               BOOL fFoundMatch = FALSE;
               DWORD dwUnkPOS = -1;
               for (j = 0; j < lForm.Num(); j++) {
                  PBYTE pb = (PBYTE)lForm.Get(j);
                  if (!pb[0])
                     dwUnkPOS = j;
                  if (pb[0] == pLPG->bPOS)
                     fFoundMatch = TRUE;
               } // j

               // if found a match then no problem
               if (fFoundMatch)
                  continue;

               // if get here, the word doesnt support the POS
               int iRet;
               swprintf (szTemp, L"The word \"%s\" does not support the %s part-of-speech.",
                  pLPG->pszWord, pLex->POSToString(pLPG->bPOS, FALSE));

               // if found an unknown POS then ask if want to set the POS
               if (dwUnkPOS != -1) {
                  wcscat (szTemp, L" Do you wish to set it's part-of-speech to this?");
                  iRet = pPage->MBYesNo (szTemp,
                     L"The word currently has no part-of-speech defined.", TRUE);
                  if (iRet == IDCANCEL)
                     return TRUE;   // stop this
                  if (iRet == IDYES) {
                     for (j = 0; j < lForm.Num(); j++) {
                        PBYTE pb = (PBYTE)lForm.Get(j);
                        if (pb[0])
                           continue;   // has pos
                        pb[0] = pLPG->bPOS;
                     }
                     pLex->WordSet (pLPG->pszWord, &lForm);
                  }
                  continue;
               }

               // else, already has POS defined
               wcscpy (szTemp2, L"The words current parts-of-speech is/are:");
               for (j = 0; j < lForm.Num(); j++) {
                  PBYTE pb = (PBYTE)lForm.Get(j);
                  wcscat (szTemp2, L" ");
                  wcscat (szTemp2, pLex->POSToString (pb[0], FALSE));
               }
               wcscat (szTemp, L" Do you wish to add a new part-of-speech?");
               iRet = pPage->MBYesNo (szTemp, szTemp2, TRUE);
               if (iRet == IDCANCEL)
                  return TRUE;
               if (iRet == IDYES) {
                  PBYTE pb = (PBYTE)lForm.Get(0);
                  lForm.Add (pb, lForm.Size(0));
                  pb = (PBYTE)lForm.Get(lForm.Num()-1);
                  pb[0] = pLPG->bPOS;
                  pLex->WordSet (pLPG->pszWord, &lForm);
                  continue;
               }

               // else, ask if wants to set new POS
               wcscpy (szTemp, L"Do you want to set the word's part of speech to ");
               wcscat (szTemp, pLex->POSToString (pLPG->bPOS, FALSE));
               wcscat (szTemp, L"?");
               iRet = pPage->MBYesNo (szTemp,
                  L"If you press \"yes\" then all the old parts-of-speech will be discarded and replace by the new one.", TRUE);
               if (iRet == IDCANCEL)
                  return TRUE;
               if (iRet == IDYES) {
                  for (j = 0; j < lForm.Num(); j++) {
                     PBYTE pb = (PBYTE)lForm.Get(j);
                     pb[0] = pLPG->bPOS;
                  }
                  pLex->WordSet (pLPG->pszWord, &lForm);
               }
            } // i

            // if get here then train on these POS
            pLex->POSTrain (pgdi->pLPG, pgdi->dwNum, NULL);

            // just save in case crash
            pLex->Save();

            // done
            pPage->Exit (L"next");
            return TRUE;
         }
      }
      return TRUE;

   case ESCN_SCROLLING:
   case ESCN_SCROLL:
      {
         PESCNSCROLL p = (PESCNSCROLL) pParam;
         if (!p || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         // must start with pos
         if ((psz[0] != 's') || (psz[1] != ':'))
            break; // dont care

         // get the POS
         BYTE bPOS;
         bPOS = POS_MAJOR_MAKE(p->iPos);

         // set the status, dont bother writing because do that late
         WCHAR szControl[64];
         wcscpy (szControl, psz);
         szControl[0] = L't';
         pLex->POSToStatus (pPage, szControl, NULL, bPOS);
         return TRUE;
      }
      break;



   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Learn grammar rules";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PERDONE")) {
            MemZero(&gMemTemp);
            MemCat (&gMemTemp, (int)pgdi->dwCur * 100 / (int)pgdi->dwTotal);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"WORDLIST")) {
            MemZero (&gMemTemp);

            DWORD i;
            for (i = 0; i < pgdi->dwNum; i++) {
               PLEXPOSGUESS pLPG = pgdi->pLPG + i;

               MemCat (&gMemTemp, L"<tr>");

               // name
               MemCat (&gMemTemp, L"<td width=33%%><bold>");
               MemCatSanitize (&gMemTemp, pLPG->pszWord);
               MemCat (&gMemTemp, L"</bold></td>");

               // edit box with play
               MemCat (&gMemTemp, L"<td width=66%%>");
               if (pLPG->bPOS != POS_MAJOR_PUNCTUATION) {
                  MemCat (&gMemTemp, L"<status width=33%% height=30 name=t:");
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");

                  MemCat (&gMemTemp, L"<scrollbar width=66%% orient=horz min=1 max=10 name=s:");
                     // NOTE: Dont allow scrollbar to be in unk position
                  MemCat (&gMemTemp, (int)i);
                  MemCat (&gMemTemp, L"/>");
               }
               MemCat (&gMemTemp, L"</td>");

               MemCat (&gMemTemp, L"</tr>");
            } // i

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}
#endif // 0, old code

/*************************************************************************************
CMLexicon::DialogGrammar - Brings up UI to analyze the grammar and fill in the NGram
database.

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CMLexicon::DialogGrammar (PCEscWindow pWindow)
{
#ifdef OLDNGRAMPOS
   // set random
   srand (GetTickCount());

   // scan the file
   PCMMLNode2 pNode = TextScan (NULL, pWindow->m_hWnd, NULL);
   if (!pNode)
      return TRUE;

   //PWSTR pszRet;

   // make text parse
   CTextParse TextParse;
   if (!TextParse.Init (LangIDGet(), this))
      return FALSE;


   // wipe out the current training
   m_NGramSingle.Init (NGRAMSINGLE_VALUES, NGRAMSINGLE_HISTORY, NGRAM_EOD);
   m_NGramRules.Init (NGRAMRULES_VALUES, NGRAMRULES_HISTORY, NGRAM_EOD);
   m_lGTRULE.Init (sizeof(GTRULE), 0, 0);

   // train using the CGramTrain object
   CGramTrain TrainOrig;
   DWORD i;
   CListFixed lLEXPOSGUESS;
   LEXPOSGUESS lpg;
   PCMMLNode2 pSub;
   PWSTR psz;
   memset (&lpg, 0, sizeof(lpg));
   {
      CProgress Progress;
      Progress.Start (pWindow->m_hWnd, "Analyzing", TRUE);

      Progress.Push (0, 0.2);
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;

         Progress.Update ((fp)i / (fp)pNode->ContentNum());

         // if find any words not in lexicon then skip
         BOOL fNotIn = FALSE;
         PCMMLNode2 pWord;
         DWORD j;
         lLEXPOSGUESS.Init (sizeof(LEXPOSGUESS));
         for (j = 0; j < pSub->ContentNum(); j++) {
            pWord = NULL;
            pSub->ContentEnum (j, &psz, &pWord);
            if (!pWord)
               continue;
            psz = pWord->NameGet();
            if (!psz)
               continue;   // skip

            // if it's marked as a word, make sure that it's in the
            // dictionary. if it isn't, then skip the entire sentence
            if (!_wcsicmp(psz, TextParse.Word())) {
               PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
               if (!psz2)
                  continue;

               if (-1 == WordFind(psz2)) {
                  fNotIn = TRUE;
                  break;
               }
            }

            // keep track of all the words found
            if (!_wcsicmp(psz, TextParse.Word()) || !_wcsicmp(psz, TextParse.Punctuation())) {
               PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
               if (psz2) {
                  lpg.pszWord = psz2;
                  lpg.pvUserData = NULL;
                  lpg.wPOSBitField = WordToPOSBitField (lpg.pszWord);
                  lLEXPOSGUESS.Add (&lpg);
               }
            }

         } // j
         // if any word isn't in lexicon continue
         if (fNotIn)
            continue;


         // add this
         TrainOrig.SentenceAdd (lLEXPOSGUESS.Num(), (PLEXPOSGUESS)lLEXPOSGUESS.Get(0), this);

         // train the Ngram
         m_NGramSingle.TrainStreamLEXPOSGUESS (lLEXPOSGUESS.Num(), (PLEXPOSGUESS)lLEXPOSGUESS.Get(0), 1.0);
      } // i
      Progress.Pop ();


      // go through and figure out the optimium set of rules to reduce the size
      PCGramTrain pBest;
      Progress.Push (0.2, 0.9);
#define RULEBLOCKSIZE         16       // number of rules to calculate in each block
      pBest = TrainOrig.DiscoverNewRules (RULEBLOCKSIZE,
         (NGRAMRULES_VALUES - 16 /*POS*/) / RULEBLOCKSIZE,
         this, &m_lGTRULE, &Progress);
      Progress.Pop ();

      Progress.Push (0.9, 1);


      // train all the new rules into NGram
      Progress.Update (0);
      DWORD dwIndex = 0;
      do {
         DWORD dwWords;
         double fScore;
         PBYTE pabPOS = pBest->SentenceGet (dwIndex, &dwWords, &fScore, &dwIndex);
         if (!pabPOS)
            break;

         // train this
         m_NGramRules.TrainStream (dwWords, pabPOS, fScore);
      } while (dwIndex);
      delete pBest;

      // train n-grams
      Progress.Update (.3);
      m_NGramSingle.TrainingApply ();

      Progress.Update (.6);
      m_NGramRules.TrainingApply ();
      Progress.Pop ();
   } // for analysis UI
   delete pNode;  // clear out since dont need

   // if have less than 10000 sentences (1000 words) then ask for more
   m_fDirty = TRUE;
   if (m_NGramSingle.m_fTrainingCount < 300000)
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Grammar training is finished for the file, but you should train more.",
         L"You don't have enough training yet. Try analyzing different text files.",
         MB_ICONINFORMATION | MB_OK);
   else
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Grammar training is finished for the file.",
         L"You appear to have enough training for text-to-speech to work well.",
         MB_ICONINFORMATION | MB_OK);

   return TRUE;

#else // !OLDNGRAMPOS
   m_fDirty = TRUE;  // assume chanted
   return m_LexParse.Dialog (pWindow, this);
#endif
}

/*************************************************************************************
CMLexicon::ShardSomeTraining - Returns TRUE if the shards already have some training.
*/
BOOL CMLexicon::ShardSomeTraining (void)
{
   if (m_lPCLexShard.Num())
      return TRUE;
   else
      return FALSE;
}

/*************************************************************************************
CMLexicon::ShardClear - Clears the shard trainig
*/
void CMLexicon::ShardClear (void)
{
   m_lLEXSHARDNGRAM.Clear();
   DWORD i;
   PCLexShard *ppls = (PCLexShard*) m_lPCLexShard.Get(0);
   for (i = 0; i < m_lPCLexShard.Num(); i++) {
      if (ppls[i])
         delete ppls[i];
   } // i
   m_lPCLexShard.Clear();
}

/*************************************************************************************
CMLexicon::ShardAdd - Adds a shard if it doesn't already exist.

inputs
   PWSTR             pszWord - Word
   PBYTE             pabPron - Pron
returns
   BOOL - TRUE if added, FALSE if already exists
*/
BOOL CMLexicon::ShardAdd (PWSTR pszWord, PBYTE pabPron)
{
   DWORD i;
   PCLexShard *ppls = (PCLexShard*)m_lPCLexShard.Get(0);
   for (i = 0; i < m_lPCLexShard.Num(); i++) {
      PCLexShard pls = ppls[i];
      if (!pls->m_pszText || !pls->m_pbPhone)
         continue;

      if (_wcsicmp(pls->m_pszText, pszWord))
         continue;
      if (strcmp((char*)pls->m_pbPhone, (char*)pabPron))
         continue;

      // else match
      return FALSE;
   } // i

   // if get here must add
   PCLexShard pls = new CLexShard;
   if (!pls)
      return FALSE;
   pls->TextAndPhoneSet (pszWord, pabPron);
   m_lPCLexShard.Add (&pls);
   m_fDirty = TRUE;

#ifdef _DEBUG
   WCHAR szTemp[256];
   char szaTemp[256];
   wcscpy (szTemp, pszWord);
   wcscat (szTemp, L"=");
   PronunciationToText (pabPron, szTemp+wcslen(szTemp), 100);
   wcscat (szTemp,L"\r\n");
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0,0);
   OutputDebugString (szaTemp);
#endif

   return TRUE;
}

/*************************************************************************************
CMLexicon::ShardParseWord - This accepts a word and parses it into shards.

inputs
   PWSTR             pszWord - Word string, starting at point where will analyze for
                     shard. Generall the start of the string, except this function
                     recurses, so will call itself with later value.
   PBYTE             pbMustMatch - If not-NULL then this is an array of phonemes
                     that the shard must match. If NULL then no specific match is required
   WORD              *pawScratch - Scratch memory where shards are written
   DWORD             dwScratchNum - Number of elements in pawScratch. Should be at least 50
   PCListVariable    plShard - Will be filled in with a list possible derivations
                     of the word using different shards. It will automatically end
                     up with the word with the lowest shard count.
   DWORD             *padwSkipped - Will be filled in with the number of characters
                     of the word skipped by the best shard matches.
   DWORD             dwScratchOffset - Offset to write into scratch. Pass in 0.
                     Only when it recurses is a larger number passed in
   DWORD             dwSkipCur - Current skip rate. Pass in 0. Only when it recurses
                     will a higher number be used
returns
   none
*/
void CMLexicon::ShardParseWord (PWSTR pszWord, PBYTE pbMustMatch, WORD *pawScratch, DWORD dwScratchNum,
                                PCListVariable plShard, DWORD *padwSkipped,
                                DWORD dwScratchOffset, DWORD dwSkipCur)
{
   // intiialize
   if (!dwScratchOffset && !dwSkipCur) {
      plShard->Clear();
      *padwSkipped = 0;
   }

   // some error checks
   if (dwScratchOffset >= dwScratchNum)
      return;
   if (plShard->Num()) {
      if (dwSkipCur > *padwSkipped)
         return;
      // BUGFIX - Only exit if must match
      // BUGFIX - Take pbMustMatch out because too many possibilities
      if ((dwScratchOffset > plShard->Size(0)/sizeof(WORD)))
         return;  // already have data there and cant expect to do better
   }

   // if done with word then add what have
   if (!pszWord[0]) {
      // if there's anything left in the must-match then we haven't matched, so
      // dont bother
      if (pbMustMatch && pbMustMatch[0])
         return;

      // clear list if have something better
      if (plShard->Num()) {
         // BUGFIX - Only do this if must match
         // BUGFIX - Take pbMustMatch out because too many possibilities
         if ((dwScratchOffset < plShard->Size(0)/sizeof(WORD))) {
            plShard->Clear();
            *padwSkipped = dwSkipCur;
         }
         if (dwSkipCur < *padwSkipped) {
            plShard->Clear();
            *padwSkipped = dwSkipCur;
         }
      }

      // add it
      plShard->Add (pawScratch, dwScratchOffset * sizeof(WORD));
      return;
   }

   // try all the shards
   DWORD i, j;
   PCLexShard *ppls = (PCLexShard*) m_lPCLexShard.Get(0);
   for (i = 0; i < m_lPCLexShard.Num(); i++) {
      PCLexShard pls = ppls[i];
      if (!pls->m_pszText)
         continue;
      DWORD dwLen = (DWORD)wcslen(pls->m_pszText);
      if (!dwLen)
         continue;

      // compare...
      for (j = 0; j < dwLen; j++)
         if (towlower(pls->m_pszText[j]) != towlower(pszWord[j]))
            break;
      if (j < dwLen)
         continue;   // doesnt match

      // else match...
      if (pbMustMatch) {
         if (!pls->m_pbPhone) {
            if (pbMustMatch[0])
               continue;   // doesnt match this
         }
         else if (strncmp((char*)pbMustMatch, (char*)pls->m_pbPhone, strlen((char*)pls->m_pbPhone)))
            continue;   // doesnt match this
      }

      // else, match
      pawScratch[dwScratchOffset] = (WORD)i;
      ShardParseWord (pszWord + dwLen,
         pbMustMatch ? (pbMustMatch + (pls->m_pbPhone ? strlen((char*)pls->m_pbPhone) : 0)) : NULL,
         pawScratch, dwScratchNum, plShard, padwSkipped, dwScratchOffset + 1, dwSkipCur);
   } // i

   // also hypothesize skipping this one
   ShardParseWord (pszWord + 1, pbMustMatch,
      pawScratch, dwScratchNum, plShard, padwSkipped, dwScratchOffset, dwSkipCur+1);

   // done
}


/*************************************************************************************
CMLexicon::ShardCalcScoreOnNGram - Given a list of shards, this calculates the score
for the given word, looking up its ngram.

inputs
   WORD              *pawShard - List of shards
   DWORD             dwNum - Number of shards
   DWORD             dwIndex - From 0..dwNum-1 to include based on the given word,
                        or >= dwNum if will calculate for ending of word
returns
   fp - score
*/
fp CMLexicon::ShardCalcScoreOnNGram (WORD *pawShard, DWORD dwNum, DWORD dwIndex)
{
   PLEXSHARDNGRAM pNGram = (PLEXSHARDNGRAM)m_lLEXSHARDNGRAM.Get(0);
   DWORD dwNumNGram = m_lLEXSHARDNGRAM.Num();
   WORD awLook[NUMSHARDNGRAM];

   PLEXSHARDNGRAM apFind[NUMSHARDNGRAM];
   DWORD i, j, k;
   fp fTotalScore = 0;
   i = dwIndex;
   for (j = 0; j < NUMSHARDNGRAM; j++) {
      int iLoc = (int)i + (int)j - NUMSHARDNGRAM + 1;
      if ((iLoc < 0) || (iLoc >= (int)dwNum))
         awLook[j] = -1;
      else
         awLook[j] = pawShard[iLoc];
   } // j

   // find a match
   memset (apFind, 0, sizeof(apFind));
   for (j = 0; j < dwNumNGram; j++) {
      PLEXSHARDNGRAM png = pNGram + j;

      // match from the right end
      for (k = 0; k < NUMSHARDNGRAM; k++)
         if (png->awShard[NUMSHARDNGRAM-1-k] != awLook[NUMSHARDNGRAM-1-k])
            break;
      if (!k)
         continue;   // no match at all
      if ((k < NUMSHARDNGRAM) && (png->awShard[NUMSHARDNGRAM-1-k] != (WORD)-2))
         continue;   // different word, so skip

      // else, store
      apFind[k-1] = png;
   } //j, dwNumNGram

   // find the best match
   fp fScore = 1;
   for (j = NUMSHARDNGRAM-1; j < NUMSHARDNGRAM; j--, fScore /= 10000.0) { // decrease the score each time
      if (apFind[j] && apFind[j]->wCount) {
         fScore *= (fp)apFind[j]->wCount;
         break;
      }
   }

   // total score
   return log(fScore);
}

/*************************************************************************************
CMLexicon::ShardCalcScore - Given a list of shards, this uses the Ngram to
calculate the scores.

inputs
   WORD              *pawShard - List of shards
   DWORD             dwNum - Number of shards
returns
   fp - score
*/
fp CMLexicon::ShardCalcScore (WORD *pawShard, DWORD dwNum)
{
   DWORD i;
   fp fTotalScore = 0;
   for (i = 0; i < dwNum + NUMSHARDNGRAM - 1; i++) {
      fTotalScore += ShardCalcScoreOnNGram (pawShard, dwNum, i);
   } // i

   // bugfix - divide score by # shards, otherwise will enocurage long lengths
   // of shards intead of shorter
   fTotalScore /= (fp) i;

   return fTotalScore;
}


/*************************************************************************************
CMLexicon::ShardAdvanceHyp - Advance a single hypothesis.

inputs
   PLTSHYP        pHyp - Hypthesis to advance
   PCListFixed    plAddTo - Add all avances to this list
returns
   BOOL - TRUE if the hypthesis is actually finished and shouldnt be advanced anymore
*/
BOOL CMLexicon::ShardAdvanceHyp (PLTSHYP pHyp, PCListFixed plAddTo)
{
   // if no more then all done
   DWORD i;
   if (!pHyp->pszCur[0]) {
      // calculate the final scores for finishing word
      for (i = 0; i < NUMSHARDNGRAM; i++)
         pHyp->fScore += ShardCalcScoreOnNGram (pHyp->awShard, pHyp->dwCurShard, pHyp->dwCurShard + i);
      return TRUE;
   }
   if (pHyp->dwCurShard >= sizeof(pHyp->awShard) / sizeof(WORD))
      return FALSE;  // too long so just exit

   // look through all possible shards
   // try all the shards
   DWORD j;
   PCLexShard *ppls = (PCLexShard*) m_lPCLexShard.Get(0);
   LTSHYP lth;
   for (i = 0; i < m_lPCLexShard.Num(); i++) {
      PCLexShard pls = ppls[i];
      if (!pls->m_pszText)
         continue;
      DWORD dwLen = (DWORD)wcslen(pls->m_pszText);
      if (!dwLen)
         continue;

      // compare...
      for (j = 0; j < dwLen; j++)
         if (towlower(pls->m_pszText[j]) != towlower(pHyp->pszCur[j]))
            break;
      if (j < dwLen)
         continue;   // doesnt match


      // else, match
      lth = *pHyp;
      lth.awShard[pHyp->dwCurShard] = (WORD)i;
      lth.dwCurShard++;
      lth.pszCur += dwLen;
      lth.fScore += ShardCalcScoreOnNGram (lth.awShard, lth.dwCurShard, lth.dwCurShard -1) *
         (fp)max(dwLen,1);
         // BUGFIX - Score affected by length, so longer segments will have higher score
      lth.fScore -= 3;  // BUFIX - Penalty so likes fewer shards

      plAddTo->Add (&lth);
   } // i

   // also hypothesize skipping this one
   lth = *pHyp;
   lth.pszCur += 1;
   lth.fScore -= 100;   // severe penalty
   plAddTo->Add (&lth);

   return FALSE;
}


/*************************************************************************************
CMLexicon::ChineseLTS - ChineseLTS useing pinyin rules.

inputs
   PWSTR             pszWord - Word
   PCListVariable    plForm - Filled in with the letter to sound
returns
   BOOL - TRUE if succede
*/
BOOL CMLexicon::ChineseLTS (PWSTR pszWord, PCListVariable plForm, PCListVariable plDontRecurse)
{
   // don't recurse
   DWORD i;
   if (plDontRecurse)
      for (i = 0; i < plDontRecurse->Num(); i++)
         if (!_wcsicmp ((PWSTR)plDontRecurse->Get(i), pszWord))
            return FALSE;

   plForm->Clear();

   char acTemp[256];
   acTemp[0] = 0; // unknown POS
   acTemp[1] = 0; // null string

   // loop
   PWSTR pszCur;
   DWORD dwUsed;
   CListVariable lFormTemp;
   for (pszCur = pszWord; pszCur[0]; ) {
      while (iswspace(pszCur[0]) || (pszCur[0] == L'-'))
         pszCur++;
      if (!pszCur[0])
         break;

      dwUsed = ChineseFindPinyinSyllableLength (this, pszCur, TRUE);
      if (!dwUsed)
         return FALSE;  // no matches

      // get
      lFormTemp.Clear();
      WCHAR cTemp = pszCur[dwUsed];
      pszCur[dwUsed] = 0;
      WordGet (pszCur, &lFormTemp);
      if (plDontRecurse && lFormTemp.Num())
         plDontRecurse->Add (pszCur, (wcslen(pszCur)+1)*sizeof(WCHAR));  // so don't recurse
      pszCur[dwUsed] = cTemp;

      if (!lFormTemp.Num())
         return FALSE;  // error

      char *pszTemp = (char*)lFormTemp.Get(0) + 1;
      if (strlen(pszTemp) + strlen(acTemp+1) + 4 >= sizeof(acTemp))
         return FALSE;  // error, too long

      // else, concatnate
      strcat (acTemp + 1, pszTemp);

      pszCur += dwUsed;
   }

   // if silent then error
   if (!strlen(acTemp + 1))
      return FALSE;
   
   // just one form... I'm being lazy
   plForm->Add (acTemp, strlen(acTemp+1)+2);

   return TRUE;

}


/*************************************************************************************
CMLexicon::ShardLTS - Given a word string, this converts it to phonemes.

inputs
   PWSTR             pszWord - word
   PCListVariable    plForm - Fill in with letter to sound
returns
   BOOL - TRUE if succede
*/
static int __cdecl LTSHYPCompare (const void *p1, const void *p2)
{
   PLTSHYP pp1 = (PLTSHYP) p1;
   PLTSHYP pp2 = (PLTSHYP) p2;
   if (pp1->fScore > pp2->fScore)
      return -1;
   else if (pp1->fScore < pp2->fScore)
      return 1;
   else
      return 0;
}
BOOL CMLexicon::ShardLTS (PWSTR pszWord, PCListVariable plForm)
{
   plForm->Clear();

   // two lists of hyptohesis
   CListFixed alHyp[2];
   alHyp[0].Init (sizeof (LTSHYP));
   alHyp[1].Init (sizeof (LTSHYP));

   // best score
   LTSHYP lthBest;
   memset (&lthBest, 0, sizeof(lthBest));

   // fill in first one
   LTSHYP lth;
   DWORD dwCurSource = 0;
   memset (&lth, 0, sizeof(lth));
   lth.pszCur = pszWord;
   alHyp[dwCurSource].Add (&lth);

   while (alHyp[dwCurSource].Num()) {
      DWORD dwCurDest = !dwCurSource;

      // clear the other list
      alHyp[dwCurDest].Clear();

      // loop and fill it in
      DWORD i, dwLoopSize;
      PLTSHYP pSrc = (PLTSHYP) alHyp[dwCurSource].Get(0);
      dwLoopSize = alHyp[dwCurSource].Num();
      dwLoopSize = min(dwLoopSize, 25);   // don't ever consider more than 100 hyps
      for (i = 0; i < dwLoopSize; i++) {
         if (!ShardAdvanceHyp(pSrc + i, &alHyp[dwCurDest]))
            continue;   // nothing left to consider

         // else, this one is complete, check its score
         if (!lthBest.pszCur) {
            // no current best so use this one
            lthBest = pSrc[i];
            continue;
         }

         // if its score is more than the best then use that
         if (pSrc[i].fScore > lthBest.fScore) {
            lthBest = pSrc[i];
            continue;
         }
      } // i

      // sort the destination so the highest scores are first
      // this way, when check them, will end up looking at the highest scoring
      // ones first
      qsort (alHyp[dwCurDest].Get(0), alHyp[dwCurDest].Num(), sizeof(LTSHYP), LTSHYPCompare);

      // repeat
      dwCurSource = dwCurDest;
   } // while hypthesis

   // if there is no best one error
   if (!lthBest.pszCur || !lthBest.dwCurShard)
      return FALSE;

   // convert these to phonemes
   WORD *pawShards = lthBest.awShard;
   DWORD dwNumShards = lthBest.dwCurShard;
   PCLexShard *ppls = (PCLexShard*)m_lPCLexShard.Get(0);
   DWORD dwNum = m_lPCLexShard.Num();
   BYTE abPron[128];
   DWORD dwCur = 1;

   fp afPOSScore[POS_MAJOR_NUM];
   memset (afPOSScore, 0, sizeof(afPOSScore));

   DWORD i;
   for (i = 0; i < dwNumShards; i++) {
      if (pawShards[i] >= dwNum)
         continue;   // shouldnt happen
      PCLexShard pls = ppls[pawShards[i]];

      if (!pls->m_pbPhone)
         continue;
      DWORD dwLen = (DWORD)strlen((char*)pls->m_pbPhone);
      if (dwLen + dwCur >= sizeof(abPron))
         continue;

      // copy over
      memcpy (abPron + dwCur, pls->m_pbPhone, dwLen);
      dwCur += dwLen;

      // score for POS
      DWORD j;
      for (j = 0; j < POS_MAJOR_NUM; j++)
         afPOSScore[j] += log((fp)max(pls->m_adwPOSCount[j],1));
   } // i
   abPron[dwCur] = 0;

   // find the best POS
   DWORD dwBestPOS = POS_MAJOR_EXTRACT(POS_MAJOR_NOUN);
   for (i = 1; i < POS_MAJOR_NUM; i++) // note: skipping unknown
      if (afPOSScore[i] > afPOSScore[dwBestPOS])
         dwBestPOS = i;
   abPron[0] = (BYTE)POS_MAJOR_MAKE (dwBestPOS);

   plForm->Add (abPron, dwCur+1);

   return TRUE;
}


// LDI - Letter to sound info
typedef struct {
   PCMLexicon           pLex;    // lexicon
   PWSTR                pszWord; // word to show
   PBYTE                pabPron; // pronunciation for the word
   DWORD                dwCur;   // sentence number
   DWORD                dwTotal; // total sentence numbers
} LDI, *PLDI;

/****************************************************************************
LexLTSPage
*/
static BOOL LexLTSPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PLDI pldi = (PLDI)pPage->m_pUserData;
   PCMLexicon pLex = pldi->pLex;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in intiial control
         PCEscControl pControl = pPage->ControlFind (L"divide");
         if (pControl)
            pControl->AttribSet (Text(), L"/");
         pControl = pPage->ControlFind (L"text");
         if (pControl)
            pControl->AttribSet (Text(), pldi->pszWord);

         // convert to phonemes
         WCHAR szTemp[128];
         pLex->PronunciationToText (pldi->pabPron, szTemp, sizeof(szTemp)/sizeof(WCHAR));
         pControl = pPage->ControlFind (L"pron");
         if (pControl)
            pControl->AttribSet (Text(), szTemp);
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         // only care about train
         if (_wcsicmp(psz, L"train"))
            break;

         // get the new word and the phonemes
         WCHAR szWord[64], szPhone[128], szSep[8];
         PCEscControl pControl;
         DWORD dwNeed;
         szSep[0] = szWord[0] = szPhone[0] = 0;
         pControl = pPage->ControlFind (L"divide");
         if (pControl)
            pControl->AttribGet (Text(), szSep, sizeof(szSep), &dwNeed);
         if (!szSep[0]) {
            pPage->MBWarning (L"You must type in a character that will separate the word and phonemes.");
            return TRUE;
         }
         pControl = pPage->ControlFind (L"text");
         if (pControl)
            pControl->AttribGet (Text(), szWord, sizeof(szWord), &dwNeed);

         pControl = pPage->ControlFind (L"pron");
         if (pControl)
            pControl->AttribGet (Text(), szPhone, sizeof(szPhone), &dwNeed);

         // count the number of dividers...
         DWORD i;
         DWORD dwDivWord = 0, dwDivPhone = 0;
         for (i = 0; szWord[i]; i++)
            if (szWord[i] == szSep[0])
               dwDivWord++;
         for (i = 0; szPhone[i]; i++)
            if (szPhone[i] == szSep[0])
               dwDivPhone++;
         if (dwDivWord != dwDivPhone) {
            pPage->MBWarning (L"You must segment the word and prounciation into the same number of groups.");
            return TRUE;
         }

         if (dwDivWord < strlen((char*)pldi->pabPron)/2) {
            int iRet = pPage->MBYesNo (L"You haven't put many dividers in. Do you want to continue with this segmentation?");
            if (iRet != IDYES)
               return TRUE;
         }

         // keep track of what should be covering in the original word vs. the
         // new word, to make sure haven't changed anything
         DWORD dwOrigWord = 0, dwOrigPhone = 0;
         DWORD dwNewWord = 0, dwNewPhone = 0;
         DWORD dwNextPhone, dwNextWord;
         while (TRUE) {
            // find the next slash
            BOOL fNext = FALSE;  // set to TRUE if there's a next word
            for (dwNextWord = dwNewWord; szWord[dwNextWord]; dwNextWord++)
               if (szWord[dwNextWord] == szSep[0]) {
                  fNext = TRUE;
                  break;
               }
            for (dwNextPhone = dwNewPhone; szPhone[dwNextPhone]; dwNextPhone++)
               if (szPhone[dwNextPhone] == szSep[0])
                  break;   // assume there will be a next phone since have same number

            // put in null terminations
            szWord[dwNextWord] = szPhone[dwNextPhone] = 0;

            // get the pronunciation of this segment
            BYTE abPron[PRONCHARS];
            DWORD dwBad;
            abPron[0] = 0;
            pLex->PronunciationFromText (szPhone + dwNewPhone, abPron, sizeof(abPron), &dwBad);

            // if all done then exit
            if (!fNext && (dwNextWord == dwNewWord) && !abPron[0])
               break;

            // if came up with two adjacent slashes in word then error
            if (dwNextWord == dwNewWord) {
               pPage->MBWarning (L"You must have at least one letter from the word between the slashes.");
               return TRUE;
            }

            // if text different then error
            if (wcsncmp(pldi->pszWord + dwOrigWord, szWord + dwNewWord,
               dwNextWord - dwNewWord)) {

               pPage->MBWarning (L"You cannot change the word; just insert slashes.");
               pControl = pPage->ControlFind (L"text");
               if (pControl)
                  pControl->AttribSet (Text(), pldi->pszWord);
               return TRUE;
            }

            // if phonemes different then error
            if (strncmp((char*)pldi->pabPron + dwOrigPhone, (char*)abPron, strlen((char*)abPron))) {
               pPage->MBWarning (L"You cannot change the pronunciation; just insert slashes.");
               WCHAR szTemp[128];
               pLex->PronunciationToText (pldi->pabPron, szTemp, sizeof(szTemp)/sizeof(WCHAR));
               pControl = pPage->ControlFind (L"pron");
               if (pControl)
                  pControl->AttribSet (Text(), szTemp);
               return TRUE;
            }

            // add it
            pLex->ShardAdd (szWord + dwNewWord, abPron);


            // advance
            if (!fNext)
               break;
            dwOrigWord += (dwNextWord - dwNewWord);
            dwOrigPhone += (DWORD)strlen((char*)abPron);
            dwNewWord = dwNextWord+1;
            dwNewPhone = dwNextPhone+1;
         } // while true


         // just save in case crash
         pLex->m_fDirty = TRUE;
         pLex->Save();

         // done
         pPage->Exit (L"next");
         return TRUE;
      }
      return TRUE;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Learn pronunciation rules";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"LEXFILE")) {
            p->pszSubString = pLex->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PERDONE")) {
            MemZero(&gMemTemp);
            MemCat (&gMemTemp, (int)pldi->dwCur * 100 / (int)pldi->dwTotal);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CMLexicon::DialogLTS - Brings up UI to analyze the LTS and fill in the NGram
database.

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
static int __cdecl PCLexShardCompare (const void *p1, const void *p2)
{
   PCLexShard *pp1 = (PCLexShard*) p1;
   PCLexShard *pp2 = (PCLexShard*) p2;
   int iLen1 = (DWORD)wcslen(pp1[0]->m_pszText);
   int iLen2 = (DWORD)wcslen(pp2[0]->m_pszText);
   return iLen2 - iLen1;
}
BOOL CMLexicon::DialogLTS (PCEscWindow pWindow)
{
   PWSTR pszRet;
   BOOL fAlreadySecondPass = FALSE;

   // clear ngram
   m_lLEXSHARDNGRAM.Clear();

   // loop through all the words
   DWORD i, j;
   CListVariable lForm, lShards, lDontRecurse;
   WCHAR szWord[64];
   WORD awShardTemp[64];
   DWORD dwSkipped;
   BOOL fRet = TRUE;
   BOOL fAbort = FALSE;
   PCProgress pProgress = NULL;
   for (i = 0; i < WordNum(); i++) {
      if (!pProgress) {
         pProgress = new CProgress;
         pProgress->Start (pWindow->m_hWnd, "Looking...", TRUE);
      }
      if (pProgress && ((i%100)==0))
         pProgress->Update ((fp)i / (fp)WordNum());

      // BUGFIX - Was just word get, but really ned get pronunciation
      if (!WordGet(i, szWord, sizeof(szWord), NULL))
         continue;
      lForm.Clear();
      lDontRecurse.Clear();
      if (!WordPronunciation (szWord, &lForm, FALSE, NULL, &lDontRecurse))
         continue;

      for (j = 0; j < lForm.Num(); j++) {
         PBYTE pabPron = ((PBYTE)lForm.Get(0)) + 1;

         // see if this parses ok
         ShardParseWord (szWord, pabPron, awShardTemp, sizeof(awShardTemp)/sizeof(WORD),
            &lShards, &dwSkipped);

         if (lShards.Num() && !dwSkipped)
            continue;   // no problem since parsed fully

         if (pProgress)
            delete pProgress;
         pProgress = NULL;

         // else, wouldnt parse
         LDI ldi;
         memset (&ldi, 0, sizeof(ldi));
         ldi.dwCur = i;
         ldi.dwTotal = WordNum();
         ldi.pabPron = pabPron;
         ldi.pLex = this;
         ldi.pszWord = szWord;

         pszRet = pWindow->PageDialog (ghInstance, IDR_MMLLEXLTS, LexLTSPage, &ldi);
         if (!pszRet) {
            fRet = FALSE;
            fAbort = TRUE;
            goto build;
         }
         else if (!_wcsicmp(pszRet, Back())) {
            fRet = TRUE;
            fAbort = TRUE;
            goto build;
         }
         else if (!_wcsicmp(pszRet, L"next"))
            continue;
         else {
            fRet = FALSE;
            fAbort = TRUE;
            goto build;
         }
      }  // j, forms
   } // i

build:

   if (pProgress)
      delete pProgress;
   pProgress = NULL;

#ifdef _DEBUG
   if (GetKeyState (VK_CONTROL) < 0)
      return fRet;
#endif
   // loop through all the shards and clear out POS count
   PCLexShard *ppls = (PCLexShard*)m_lPCLexShard.Get(0);
   DWORD dwNumShard = m_lPCLexShard.Num();
   for (i = 0; i < dwNumShard; i++)
      memset (ppls[i]->m_adwPOSCount, 0, sizeof(ppls[i]->m_adwPOSCount));

   // sort all the shards so the largest ones in front
   qsort (m_lPCLexShard.Get(0), m_lPCLexShard.Num(), sizeof(PCLexShard), PCLexShardCompare);

   // allocate memory large enough for ngrams and backoffs
   CMem  amem[NUMSHARDNGRAM];
   PDWORD padwNGram[NUMSHARDNGRAM];
   dwNumShard++;  // so include highest one as not part of word
   DWORD dwNum = 1;
   for (i = 0; i < NUMSHARDNGRAM; i++) {
      dwNum *= dwNumShard;
      if (!amem[i].Required (dwNum * sizeof(DWORD)))
         return FALSE;
      padwNGram[i] = (DWORD*) amem[i].p;
      memset (padwNGram[i], 0, sizeof(DWORD)*dwNum);
   }
   m_lLEXSHARDNGRAM.Clear();

   // loop through all the words again
   CProgress Progress;
   Progress.Start (pWindow->m_hWnd, "Analyzing", TRUE);
   for (i = 0; i < WordNum(); i++) {
      if ((i%100) == 0)
         Progress.Update ((fp)i / (fp)WordNum());

      // BUGFIX - Cant use word get for this, so change to ponrunciations
      if (!WordGet(i, szWord, sizeof(szWord), NULL))
         continue;
      lDontRecurse.Clear();
      if (!WordPronunciation (szWord, &lForm, FALSE, NULL, &lDontRecurse))
         continue;

      for (j = 0; j < lForm.Num(); j++) {
         PBYTE pabPron = ((PBYTE)lForm.Get(0)) + 1;

         // see if this parses ok
         ShardParseWord (szWord, pabPron, awShardTemp, sizeof(awShardTemp)/sizeof(WORD),
            &lShards, &dwSkipped);

         if (!lShards.Num() || dwSkipped)
            continue;   // shouldn't only happen if user cancelled out too soon

         // take the first one and assume the prounciation is done with it
         WORD *pawShard = (WORD*)lShards.Get(0);
         DWORD dwNum = (DWORD)lShards.Size(0) / sizeof(WORD);

         // loop over all the shards and create ngram
         DWORD k, m;
         int iLoc;
         WORD awNGram[NUMSHARDNGRAM];
         for (k = 0; k < dwNum + NUMSHARDNGRAM-1; k++) {
            for (m = 0; m < NUMSHARDNGRAM; m++) {
               iLoc = (int)m + (int)k - NUMSHARDNGRAM+1;
               if ((iLoc < 0) || (iLoc >= (int)dwNum))
                  awNGram[m] = dwNumShard-1;
               else
                  awNGram[m] = pawShard[iLoc];
            } // m

            // fill in the ngram tables
            DWORD dwOffset, n;
            dwOffset = 0;
            for (m = 0; m < NUMSHARDNGRAM; m++) {
               dwOffset *= dwNumShard;
               dwOffset += (DWORD)awNGram[NUMSHARDNGRAM-m-1];

               DWORD *padw = padwNGram[m] + dwOffset;

               // if there's already an element in here then get that and add one
               if (*padw) {
                  PLEXSHARDNGRAM plsng = (PLEXSHARDNGRAM)m_lLEXSHARDNGRAM.Get(*padw - 1);
                  if (plsng->wCount < 0xffff)
                     plsng->wCount++;
               }
               else {
                  LEXSHARDNGRAM lsng;
                  for (n = 0; n < NUMSHARDNGRAM; n++) {  // fill in
                     lsng.awShard[n] = awNGram[n];
                     if (lsng.awShard[n] == dwNumShard-1)
                        lsng.awShard[n] = -1;   // indicate before start/end of word
                     if (NUMSHARDNGRAM-n-1 > m)
                        lsng.awShard[n] = -2;   // unknown
                  } // n
                  lsng.wCount = 1;
                  m_lLEXSHARDNGRAM.Add (&lsng);
                  *padw = m_lLEXSHARDNGRAM.Num();
               }
            } // m

            // keep track of POS
            BYTE bPOS = POS_MAJOR_EXTRACT(pabPron[-1]);
            if ((k < dwNum) && (bPOS < POS_MAJOR_NUM))
               ppls[pawShard[k]]->m_adwPOSCount[bPOS]++;
         } // k

      } //j
   } // i

   // clear out dead shards
   BOOL fDeleted;
   fDeleted = FALSE;
   if (!fAlreadySecondPass) for (i = m_lPCLexShard.Num()-1; i < m_lPCLexShard.Num(); i--) {
      PCLexShard pls = *((PCLexShard*)m_lPCLexShard.Get(i));

      DWORD dwSum = 0;
      for (j = 0; j < POS_MAJOR_NUM; j++)
         dwSum += pls->m_adwPOSCount[j];
      if (!dwSum) {
         delete pls;
         fDeleted = TRUE;
         m_lPCLexShard.Remove (i);
      }
   }
   fAlreadySecondPass = TRUE;
   if (fDeleted)
      goto build;


   // go through the tri-grams and remove those with a low count
   for (i = m_lLEXSHARDNGRAM.Num()-1; i < m_lLEXSHARDNGRAM.Num(); i--) {
      PLEXSHARDNGRAM plg = (PLEXSHARDNGRAM)m_lLEXSHARDNGRAM.Get(i);
      if (plg->wCount < 10)
         m_lLEXSHARDNGRAM.Remove (i);
   } // i

   m_fDirty = TRUE;  // so will save


   // if have less than 100 sentences (1000 words) then ask for more
   if (WordNum() < 10000)
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Pronunciation training is finished for the lexicon, but you should train more as you add more words.",
         NULL,
         MB_ICONINFORMATION | MB_OK);
   else
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Pronunciation training is finished for the lexicon.",
         NULL,
         MB_ICONINFORMATION | MB_OK);

   return fRet;
}

/*************************************************************************************
CMLexicon::LexUnisynParseToToken - Take strings for list of phonemes, etc. and parse to a token.

inputs
   PWSTR          psz - String from the dialog. This may  include newlines for new tokens.
   DWORD          dwMajorID - Major token ID to use.
                        LUPTMAJOR_XXX. if this is LUPTMAJOR_PHONESTRESS or LUPTMAJOR_PHONENOSTRESS or LUPTMAJOR_PHONEFINAL
                        then phonemes will be added, as well as unstressed and possibly secondary stress.
                        Fill in dwMinorID with the unsorted phoneme number with unstressed. Next is primary (if exsits). Next
                        is secondary stress (if there is).
   PCListFixed    plLUPT - List of LUPT that is added to.
   BOOL           fStressRemap - If TRUE then only primary and no stress. Ignore secondary stress.
                     Only used if adding phoneme
   DWORD          dwNumTones - If LUMPTMAJOR_PHONEFINAL, then number of tones
returns
   none
*/
void CMLexicon::LexUnisynParseToToken (PWSTR psz, DWORD dwMajorID, PCListFixed plLUPT, BOOL fStressRemap, DWORD dwNumTones)
{
   LUPT lupt;

   PWSTR pszEnd = psz + wcslen(psz);
   DWORD i;
   while (psz[0]) {
      // see how far to newline
      PWSTR pszR = wcschr (psz, L'\r');
      PWSTR pszN = wcschr (psz, L'\n');

      if (!pszR)
         pszR = pszEnd;
      if (!pszN)
         pszN = pszEnd;

      // take the first one
      pszN = min(pszN, pszR);

      // what's the length?
      DWORD dwLength = (DWORD)((PBYTE)pszN - (PBYTE)psz) / sizeof(WCHAR);
      if (!dwLength) {
         // empty
         psz++;
         continue;
      }

      // if length is too long then skip anyway
      if (dwLength >= (sizeof(lupt.szToken) / sizeof(WCHAR) - 1)) {
         psz = pszN;
         continue;
      }

      // fill in
      memset (&lupt, 0, sizeof(lupt));
      memcpy (lupt.szToken, psz, dwLength * sizeof(WCHAR)); // will automatically be null terminated
      lupt.dwLen = (DWORD) wcslen(lupt.szToken);
      lupt.dwMajorID = dwMajorID;

      // if phoneme
      if ((dwMajorID == LUPTMAJOR_PHONESTRESS) || (dwMajorID == LUPTMAJOR_PHONENOSTRESS) || (dwMajorID == LUPTMAJOR_PHONEFINAL) ) {
         DWORD dwPhonesToAdd = 1;
         if (dwMajorID == LUPTMAJOR_PHONESTRESS)
            dwPhonesToAdd = fStressRemap ? 2 : 3;
         else if (dwMajorID == LUPTMAJOR_PHONEFINAL)
            dwPhonesToAdd = dwNumTones;

         // add
         WORD wPhoneOtherStress = 255;
         for (i = 0; i < dwPhonesToAdd; i++) {
            LEXPHONE lp;
            memset (&lp, 0, sizeof(lp));
            lp.bEnglishPhone = 0;
            lp.bStress = (BYTE)i;

            size_t dwLenNeeded = wcslen(lupt.szToken) + 1;
            if (dwPhonesToAdd > 1)
               dwLenNeeded++;
            if (dwLenNeeded > sizeof(lp.szPhoneLong) / sizeof(WCHAR))
               break;   // too big, so dont add
            wcscpy (lp.szPhoneLong, lupt.szToken);

            if (!i)
               lupt.dwMinorID = m_lLEXPHONE.Num();

            if (dwPhonesToAdd > 1) {
               DWORD dwLen = (DWORD) wcslen(lp.szPhoneLong);
               lp.szPhoneLong[dwLen] = ((dwMajorID == LUPTMAJOR_PHONEFINAL) ? L'1' : L'0') + (WCHAR)i;
               lp.szPhoneLong[dwLen+1] = 0;

               switch (i) {
               case 0:
                  wPhoneOtherStress = (WORD)m_lLEXPHONE.Num();
                  break;
               case 1:
               case 2:
               case 3:
               case 4:
               case 5:
               default:
                  // do nothing specail
                  break;
               } // switch
            } // append stress mark
            wcscpy (lp.szSampleWord, L"sample");
            lp.wPhoneOtherStress = i ? wPhoneOtherStress : 255;

            // if already exists then ignore
            if (PhonemeFind (lp.szPhoneLong) != (DWORD)-1)
               break;   // dont add

            m_lLEXPHONE.Add (&lp);
            PhonemeSort();
            m_fDirty = TRUE;
         } // i
         if (i < dwPhonesToAdd) {
            psz += dwLength;
            continue;   // error
         }
      } // if phoneme

      // add and done
      plLUPT->Add (&lupt);
      psz += dwLength;


   } // while psz[0]
}



/*************************************************************************************
LexLUPTParse - Finds the longest string.

inputs
   PWSTR          psz - String. Must NOT include spaces
   BOOL           fCaseSensative - TRUE if case sensative
   PCListFixed    plLUPT - List of LUPT
   DWORD          dwMajorIDStart - Start at this major ID for lupt search
   DWORD          dwMajorIDStop - Stop at this major ID for lupt search (exclusive)
   BOOL           fSpaces - TRUE if must be followed by a space, or a non-phone symbol
                     ONLY if it's a phoneme
returns
   PLUPT - LUPT with the longest/best match. NULL if no match
*/
PLUPT LexLUPTParse (PWSTR psz, BOOL fCaseSensative, PCListFixed plLUPT,
                    DWORD dwMajorIDStart, DWORD dwMajorIDStop, BOOL fSpaces)
{
   PLUPT pBest = NULL;
   DWORD dwBestLen = 0;

   // loop
   DWORD i;
   PLUPT pCur = (PLUPT) plLUPT->Get(0);
   for (i = 0; i < plLUPT->Num(); i++, pCur++) {
      if ((pCur->dwMajorID < dwMajorIDStart) || (pCur->dwMajorID >= dwMajorIDStop))
         continue;   // out of range

      // make sure longer than best match
      if (pCur->dwLen <= dwBestLen)
         continue;   // too short, so skip

      // see if match
      int iRet;
      if (fCaseSensative)
         iRet = wcsncmp (psz, pCur->szToken, pCur->dwLen);
      else
         iRet = _wcsnicmp (psz, pCur->szToken, pCur->dwLen);
      if (iRet)
         continue;   // not a match

      // else, matched. Make sure it ends properly
      if (fSpaces &&
            ((pCur->dwMajorID == LUPTMAJOR_PHONESTRESS) || (pCur->dwMajorID == LUPTMAJOR_PHONENOSTRESS) || (pCur->dwMajorID == LUPTMAJOR_PHONEFINAL)) )
         {
         if (psz[pCur->dwLen] && !iswspace(psz[pCur->dwLen]) ) {
            // look for a next token
            PLUPT pFind = LexLUPTParse (psz + pCur->dwLen, fCaseSensative, plLUPT,
               max(dwMajorIDStart, LUPTMAJOR_STRESS0), dwMajorIDStop, FALSE);
            if (!pFind)
               continue;   // not a valid follow-on symbol
         }  // check to see end character
      }

      // else, match. Since already longer than what have, keep
      pBest = pCur;
      dwBestLen = pBest->dwLen;
   } // i

   return pBest;
}


/*************************************************************************************
CMLexicon::LexImportUnisynParsePron - Parses the pronunciation of the word.

inputs
   PCEscPage      pPage - Page to display errors on
   PWSTR          psz - Line to parse
   PCListFixed    plLUPT - List of LUPT
   PSTR           pszPron - Filled with the pronuncaiton, and NULL terminated
   DWORD          dwLen - Numbre of characters in pszPron
   PWSTR          pszWord - Word so can display errror
   BOOL           fStressSyllable - If TRUE stress stays for entire syllable, else only for next stressed phoneme
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportUnisynParsePron (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
                                      PBYTE pszPron, DWORD dwLen, PWSTR pszWord,
                                      BOOL fStressSyllable)
{
   DWORD dwStress = 0;  // default to not in stress
   PLUPT plupt;
   WCHAR szTemp[256];

   // repeat
   while (psz[0] && (dwLen > 1)) {
      // skip whtiespace
      if (iswspace(psz[0])) {
         psz++;
         continue;
      }

      // find the token
      plupt = LexLUPTParse (psz, FALSE, plLUPT, LUPTMAJOR_PHONESTART, LUPTMAJOR_PHONESTOP, TRUE);

      if (!plupt) {
         swprintf (szTemp, L"Can't parse phoneme symbol, \"%s\" in word", psz);
         pPage->MBError (szTemp, pszWord);

         return FALSE;
      }

      switch (plupt->dwMajorID) {
      case LUPTMAJOR_PHONESTRESS:
         pszPron[0] = (BYTE)(plupt->dwMinorID + dwStress + 1);
         if (!fStressSyllable)
            dwStress = 0;
         pszPron++;
         dwLen--;
         break;
      case LUPTMAJOR_PHONENOSTRESS:
      case LUPTMAJOR_PHONEFINAL:
         pszPron[0] = (BYTE)(plupt->dwMinorID + 1);
         pszPron++;
         dwLen--;
         break;
      case LUPTMAJOR_STRESS0:
         // primary stress
         dwStress = 1;
         break;
      case LUPTMAJOR_STRESS1:
         // secondary stress, but only if not remapped
         dwStress = 2;
         break;
      case LUPTMAJOR_SYLLABLE:
         // no stress
         dwStress = 0;
         break;
      default:
      case LUPTMAJOR_IGNORE:
         // do nothing
         break;
      } // switch

      psz += plupt->dwLen;
   }

   // NULL terminate
   pszPron[0] = 0;
   return TRUE;
}

/*************************************************************************************
CMLexicon::LexImportUnisynParsePOS - Parses out the parts of speech.

inputs
   PCEscPage      pPage - Page to display errors on
   PWSTR          psz - Line to parse
   PCListFixed    plLUPT - List of LUPT
   DWORD          *pdwPOS - Filled with bit fields for which parts of speech were identified.
                     (1 << POS_MAJOR_EXTRACT(POS_MAJOR_NOUN)), etc.
   PWSTR          pszWord - Word so can display errror
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportUnisynParsePOS (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
                                      DWORD *pdwPOS, PWSTR pszWord)
{
   *pdwPOS = 0;
   PLUPT plupt;
   WCHAR szTemp[256];

   // find match
   while (psz[0]) {
      // skip spaces
      if (iswspace(psz[0])) {
         psz++;
         continue;
      }

      // skip slash
      if (psz[0] == L'/') {
         psz++;
         continue;
      }

      // find longest symbol
      plupt = LexLUPTParse (psz, FALSE, plLUPT, LUPTMAJOR_POSSTART, LUPTMAJOR_POSSTTOP, FALSE);

      // make sure it ends properly
      if (plupt && psz[plupt->dwLen] && !iswspace(psz[plupt->dwLen]) && (psz[plupt->dwLen] != '/') )
         plupt = NULL;  // parse error

      if (!plupt) {
         swprintf (szTemp, L"Can't parse part-of-speech symbol, \"%s\" in word", psz);
         pPage->MBError (szTemp, pszWord);

         return FALSE;
      }

      // set the bit
      DWORD dwPOS = POS_MAJOR_NOUN;
      switch (plupt->dwMajorID) {
      case LUPTMAJOR_POSNOUN:
         dwPOS = POS_MAJOR_NOUN;
         break;
      case LUPTMAJOR_POSPRONOUN:
         dwPOS = POS_MAJOR_PRONOUN;
         break;
      case LUPTMAJOR_POSADJECTIVE:
         dwPOS = POS_MAJOR_ADJECTIVE;
         break;
      case LUPTMAJOR_POSPREPOSITION:
         dwPOS = POS_MAJOR_PREPOSITION;
         break;
      case LUPTMAJOR_POSARTICLE:
         dwPOS = POS_MAJOR_ARTICLE;
         break;
      case LUPTMAJOR_POSVERB:
         dwPOS = POS_MAJOR_VERB;
         break;
      case LUPTMAJOR_POSADVERB:
         dwPOS = POS_MAJOR_ADVERB;
         break;
      case LUPTMAJOR_POSAUXVERB:
         dwPOS = POS_MAJOR_AUXVERB;
         break;
      case LUPTMAJOR_POSCONJUNCTION:
         dwPOS = POS_MAJOR_CONJUNCTION;
         break;
      case LUPTMAJOR_POSINTERJECTION:
         dwPOS = POS_MAJOR_INTERJECTION;
         break;
      } // switch
      *pdwPOS = *pdwPOS | (1 << POS_MAJOR_EXTRACT(dwPOS));

      // advance
      psz += plupt->dwLen;
   } // while

   // if no POS then default
   if (!*pdwPOS)
      *pdwPOS = 1 << POS_MAJOR_EXTRACT(POS_MAJOR_UNKNOWN);

   return TRUE;
}


/*************************************************************************************
CMLexicon::LexImportUnisynParse - Parses a single line

inputs
   PCEscPage      pPage - Page to display errors on
   PWSTR          psz - Line to parse
   PCListFixed    plLUPT - List of LUPT
   BOOL           fStressRemap - Remapping secondary stress to primary
   BOOL           fPhoneSpaces - Phonemes must have spaces after them
   BOOL           fStressSyllable - If TRUE then stress affects entire syllabe, else just next phoneme
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportUnisynParse (PCEscPage pPage, PWSTR psz, PCListFixed plLUPT,
                                      BOOL fStressRemap, BOOL fPhoneSpaces, BOOL fStressSyllable)
{   // get rid of /r or /n
   PWSTR pszR = wcschr (psz, L'\r');
   if (pszR)
      *pszR = 0;
   pszR = wcschr (psz, L'\n');
   if (pszR)
      *pszR = 0;

   // find the colon after the name
   PWSTR pszWord = psz;
   PWSTR pszColon = wcschr (psz, L':');
   if (!pszColon) {
      pPage->MBError (L"Can't find the colon after the word.", pszWord);
      return FALSE;
   }
   psz = pszColon + 1;
   *pszColon = 0; // so isolate the word

   // skip to the colon after the word number
   pszColon = wcschr (psz, L':');
   if (!pszColon) {
      pPage->MBError (L"Can't find the colon after the word-version number.", pszWord);
      return FALSE;
   }
   psz = pszColon + 1;
   *pszColon = 0; // so isolate the word

   // part of speech
   PWSTR pszPOS = psz;
   pszColon = wcschr (psz, L':');
   if (!pszColon) {
      pPage->MBError (L"Can't find the colon after the part of speech.", pszWord);
      return FALSE;
   }
   psz = pszColon + 1;
   *pszColon = 0; // so isolate the word

   // pronunciation
   PWSTR pszPron = psz;
   pszColon = wcschr (psz, L':');
   if (pszColon)
      *pszColon = 0; // so ignore everything after pronunciation
   // NOTE: In future might want to store as set of morphs, but not now

   // parse the parts of speech
   DWORD dwPOS = 0;
   if (!LexImportUnisynParsePOS (pPage, pszPOS, plLUPT, &dwPOS, pszWord))
      return FALSE;

   // parse the pronunciation
   BYTE abPron[32];
   if (!LexImportUnisynParsePron (pPage, pszPron, plLUPT, &abPron[1], sizeof(abPron)-1, pszWord, fStressSyllable))
      return FALSE;

   // if 0 length then error
   size_t dwLen = strlen((PSTR)&abPron[1]);
   if (!dwLen) {
      pPage->MBError (L"The word doesn't have a pronunciation", pszWord);
      return FALSE;
   }

   // see if already have a pronuncation
   CListVariable lForm;
   WordGet (pszWord, &lForm);

   // add new forms
   DWORD i;
   for (i = 0; i < POS_MAJOR_NUM; i++) {
      if (!(dwPOS & (1 << i)))
         continue;   // bit not set

      abPron[0] = POS_MAJOR_MAKE(i);

      lForm.Add (abPron, dwLen + 1 /*POS*/ + 1 /*NULL*/);
   } // i

   // write it
   WordSet (pszWord, &lForm);
   
   return TRUE;
}

/*************************************************************************************
CMLexicon::LexImportUnisyn - Imports a Unisyn lexicon.

inputs
   PCEscPage      pPage - Page to display errors on
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportUnisyn (PCEscPage pPage)
{
   // make sure there are no words
   if (m_lWords.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the words in the existing lexicon?",
         L"You can't import the new lexicon while there are words. Do you wish to clear them?"))
         return FALSE;

      WordClearAll();
   }

   // make sure there are no phonemes
   if (m_lLEXPHONE.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the phones in the existing lexicon?",
         L"You can't import the new lexicon while there are phones. Do you wish to clear them?"))
         return FALSE;

      m_lLEXPHONE.Clear();
      PhonemeSort();
      m_fDirty = TRUE;
   }

   // clear shards for pronuncation
   ShardClear();
   m_fDirty = TRUE;

   // read in strings
   CListFixed lLUPT;
   lLUPT.Init (sizeof(LUPT));
   CMem mem;
   DWORD fStressRemap, fPhoneSpaces, fStressSyllable;
   LexRegGet (L"ImportUnisynStressRemap", NULL, NULL, &fStressRemap, 1);
   LexRegGet (L"ImportUnisynPhoneSpaces", NULL, NULL, &fPhoneSpaces, 1);
   LexRegGet (L"ImportUnisynStressSyllable", NULL, NULL, &fStressSyllable, 0);

   LexRegGet (L"ImportUnisynPhoneStress", &mem, gpszImportUnisynPhoneStress); // also adds phonemes
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_PHONESTRESS, &lLUPT, fStressRemap);

   LexRegGet (L"ImportUnisynPhoneNoStress", &mem, gpszImportUnisynPhoneNoStress);   // also adds phonemes
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_PHONENOSTRESS, &lLUPT);

   LexRegGet (L"ImportUnisynStress0", &mem, gpszImportUnisynStress0);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_STRESS0, &lLUPT);

   LexRegGet (L"ImportUnisynStress1", &mem, gpszImportUnisynStress1);
   LexUnisynParseToToken ((PWSTR)mem.p, fStressRemap ? LUPTMAJOR_STRESS0 : LUPTMAJOR_STRESS1, &lLUPT);

   LexRegGet (L"ImportUnisynStress2", &mem, gpszImportUnisynStress2);
   LexUnisynParseToToken ((PWSTR)mem.p, fStressRemap ? LUPTMAJOR_STRESS0 : LUPTMAJOR_STRESS1, &lLUPT);

   LexRegGet (L"ImportUnisynStress3", &mem, gpszImportUnisynStress3);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_SYLLABLE, &lLUPT);

   LexRegGet (L"ImportUnisynSymbolIgnore", &mem, gpszImportUnisynSymbolIgnore);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_IGNORE, &lLUPT);

   LexRegGet (L"ImportUnisynPOSNoun", &mem, gpszImportUnisynPOSNoun);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSNOUN, &lLUPT);

   LexRegGet (L"ImportUnisynPOSPronoun", &mem, gpszImportUnisynPOSPronoun);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSPRONOUN, &lLUPT);

   LexRegGet (L"ImportUnisynPOSAdjective", &mem, gpszImportUnisynPOSAdjective);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSADJECTIVE, &lLUPT);

   LexRegGet (L"ImportUnisynPOSPreposition", &mem, gpszImportUnisynPOSPreposition);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSPREPOSITION, &lLUPT);

   LexRegGet (L"ImportUnisynPOSArticle", &mem, gpszImportUnisynPOSArticle);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSARTICLE, &lLUPT);

   LexRegGet (L"ImportUnisynPOSVerb", &mem, gpszImportUnisynPOSVerb);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSVERB, &lLUPT);

   LexRegGet (L"ImportUnisynPOSAdverb", &mem, gpszImportUnisynPOSAdverb);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSADVERB, &lLUPT);

   LexRegGet (L"ImportUnisynPOSAuxVerb", &mem, gpszImportUnisynPOSAuxVerb);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSAUXVERB, &lLUPT);

   LexRegGet (L"ImportUnisynPOSConjunction", &mem, gpszImportUnisynPOSConjunction);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSCONJUNCTION, &lLUPT);

   LexRegGet (L"ImportUnisynPOSInterjection", &mem, gpszImportUnisynPOSInterjection);
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_POSINTERJECTION, &lLUPT);

   // open file
   LexRegGet (L"ImportUnisynFile", &mem, L"");
   FILE *pf = _wfopen ((PWSTR)mem.p, L"rt");
   WCHAR szHuge[1000];
   if (!pf) {
      pPage->MBError (L"Can't open the file.", (PWSTR)mem.p);
      return FALSE;
   }


   // read in line at a time (as ascii), convert to unicode, and parse line
   while (fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf)) {
      if (!LexImportUnisynParse (pPage, szHuge, &lLUPT, fStressRemap, fPhoneSpaces, fStressSyllable)) {
         fclose (pf);
         return FALSE;
      }
   } // get string

   // close file
   fclose (pf);

   return TRUE;
}



/*************************************************************************************
fgetsNoCR - get string without CR

inputs
   PWSTR          psz - Filled in
   int            n - Number of chars
   FILE           *pf - File
returns
   PWSTR - String, or NULL if nothing
*/
PWSTR fgetsNoCR (PWSTR psz, int n, FILE *pf)
{
   PWSTR pszRet = fgetws (psz, n, pf);
   if (!pszRet)
      return pszRet;

   // eliminate /r/n
   PWSTR pszN = wcschr (pszRet, L'\n');
   if (pszN)
      pszN[0] = 0;
   pszN = wcschr (pszRet, L'\r');
   if (pszN)
      pszN[0] = 0;

   return pszRet;
}


/*************************************************************************************
CMLexicon::LexImportMandarinLblFile - Read in label file and add words to lex.

inputs
   PCEscPage         pPage - Page
   PWSTR             pszFileNameOnly - Only the file name
   PWSTR             pszFile - File with path
   FILE              *pfData - Where to write the utt.data to
returns
   BOOL - TRUE if success. FALSE if error (and will have displayed error message)
*/
BOOL CMLexicon::LexImportMandarinLblFile (PCEscPage pPage, PWSTR pszFileNameOnly,
                                          PWSTR pszFile, FILE *pfData)
{
   // read in file
   FILE *pf = _wfopen (pszFile, L"rt");
   if (!pf) {
      pPage->MBError (L"Can't open a .lbl file.", pszFile);
      return FALSE;
   }

   CListVariable lPinYin;
   CListVariable lPhones;
   CMem memSentence;
   MemZero (&memSentence);
   
   // read to pinyin stream
   WCHAR szHuge[5000];
   while (TRUE) {
      if (!fgetsNoCR (szHuge, sizeof(szHuge)/sizeof(WCHAR), pf)) {
         fclose (pf);
         pPage->MBError (L"Can't find the text, \"PinYin stream:\" in the file.", pszFile);
         return FALSE;
      }

      if (!_wcsicmp(szHuge, L"PinYin stream:"))
         break;
   }

   // read to end of pinyin stream
   PWSTR pszFind, pszSemi;
   PWSTR pszNameEquals = L"name = ";
   DWORD dwNameEqualsLen = (DWORD)wcslen(pszNameEquals);
   while (TRUE) {
      if (!fgetsNoCR (szHuge, sizeof(szHuge)/sizeof(WCHAR), pf)) {
         fclose (pf);
         pPage->MBError (L"Can't find the text, \"End of PinYin stream.\" in the file.", pszFile);
         return FALSE;
      }

      if (!_wcsicmp(szHuge, L"End of PinYin stream."))
         break;

      // find "name = "
      pszFind = (PWSTR) MyStrIStr (szHuge, pszNameEquals);
      if (!pszFind) {
         fclose (pf);
         pPage->MBError (L"Can't find \"name = \" in the file.", pszFile);
         return FALSE;
      }
      pszFind += dwNameEqualsLen;

      // find semicolon
      pszSemi = wcschr (pszFind, L';');
      if (!pszSemi) {
         fclose (pf);
         pPage->MBError (L"Can't find ending semi-colon in the file.", pszFile);
         return FALSE;
      }
      pszSemi[0] = 0;

      // add this
      lPinYin.Add (pszFind, (wcslen(pszFind)+1)*sizeof(WCHAR));
      

      // add to sentence string
      if (((PWSTR)memSentence.p)[0])
         MemCat (&memSentence, L" ");
      MemCat (&memSentence, pszFind);
   }

   while (TRUE) {
      if (!fgetsNoCR (szHuge, sizeof(szHuge)/sizeof(WCHAR), pf)) {
         fclose (pf);
         pPage->MBError (L"Can't find the text, \"Initial\\Final stream:\" in the file.", pszFile);
         return FALSE;
      }

      if (!_wcsicmp(szHuge, L"Initial\\Final stream:"))
         break;
   }

   // read to end of pinyin stream
   while (TRUE) {
      if (!fgetsNoCR (szHuge, sizeof(szHuge)/sizeof(WCHAR), pf)) {
         fclose (pf);
         pPage->MBError (L"Can't find the text, \"End of Initial\\Final stream.\" in the file.", pszFile);
         return FALSE;
      }

      if (!_wcsicmp(szHuge, L"End of Initial\\Final stream."))
         break;

      // find "name = "
      pszFind = (PWSTR) MyStrIStr (szHuge, pszNameEquals);
      if (!pszFind) {
         fclose (pf);
         pPage->MBError (L"Can't find \"name = \" in the file.", pszFile);
         return FALSE;
      }
      pszFind += dwNameEqualsLen;

      // find semicolon
      pszSemi = wcschr (pszFind, L';');
      if (!pszSemi) {
         fclose (pf);
         pPage->MBError (L"Can't find ending semi-colon in the file.", pszFile);
         return FALSE;
      }
      pszSemi[0] = 0;

      // add this
      lPhones.Add (pszFind, (wcslen(pszFind)+1)*sizeof(WCHAR));
   }

   // save sentence to utt.data
   WCHAR szTemp[256];
   wcscpy (szTemp, pszFileNameOnly);
   DWORD dwLen = (DWORD)wcslen(szTemp);
   if ((dwLen >= 4) && !_wcsicmp(szTemp + (dwLen - 4), L".lbl"))
      szTemp[dwLen-4] = 0; // so no extra

   fwprintf (pfData, L"(%s \"%s\")\n", szTemp, (PWSTR)memSentence.p);

   fclose (pf);


   // loop through all the pinying and get pronunciation for each
   char szPron[16];
   DWORD dwFind, dwLoc;
   WCHAR szTemp2[256];
   CListVariable lForm;
   DWORD i;
   while (lPinYin.Num()) {
      PWSTR pszPinYin = (PWSTR) lPinYin.Get(0);

      // find the tone number
      dwLen = (DWORD) wcslen(pszPinYin);
      PWSTR pszTone = dwLen ? (pszPinYin + (dwLen-1)) : NULL;

      dwLoc = 0;
      szPron[dwLoc++] = 0; // no known POS

      if (!lPhones.Num()) {
         pPage->MBError (L"Mismatch between pinyin and phonemes, no initial.", pszFile);
         return FALSE;
      }

      // get the first phoneme and try to find it
      PWSTR pszPhone = (PWSTR) lPhones.Get(0);
      dwFind = PhonemeFindUnsort (pszPhone);
      if (dwFind != (DWORD)-1) {
         // found the initial
         szPron[dwLoc++] = (BYTE)dwFind+1;
         lPhones.Remove (0);
      }

      // must have a final
      if (!lPhones.Num()) {
         pPage->MBError (L"Mismatch between pinyin and phonemes, no final.", pszFile);
         return FALSE;
      }
      pszPhone = (PWSTR) lPhones.Get(0);
      wcscpy (szTemp, pszPhone);
      lPhones.Remove (0);  // since will either use or fail
      wcscat (szTemp, pszTone);

      dwFind = PhonemeFindUnsort (szTemp);
      if (dwFind == (DWORD)-1) {
         swprintf (szTemp2, L"Can't find phoneme %s (tagged as final) in the lexicon.", szTemp);
         pPage->MBError (szTemp2, pszFile);
         return FALSE;
      }
      szPron[dwLoc++] = (BYTE)dwFind+1;
      szPron[dwLoc++] = 0; // NULL terminate


      // search for prounciations
      if (!WordExists (pszPinYin)) {
         // add it
         lForm.Clear();
         lForm.Add (szPron, dwLoc * sizeof(BYTE));
         WordSet (pszPinYin, &lForm);
      }
      else {
         // exists
         lForm.Clear();
         WordGet (pszPinYin, &lForm);

         // find match
         for (i = 0; i < lForm.Num(); i++)
            if (!strcmp ((char*)lForm.Get(i)+1, szPron + 1))
               break;
         if (i >= lForm.Num()) {
            // add it
            lForm.Add (szPron, dwLoc * sizeof(BYTE));
            WordSet (pszPinYin, &lForm);
         }
         // else, already matched, so do nothing
      }


      // remove from list
      lPinYin.Remove (0);
   } // while

   // too many phones
   if (lPhones.Num()) {
      pPage->MBError (L"Too many phonemes in file.", pszFile);
      return FALSE;
   }

   return TRUE;
}


/*************************************************************************************
CMLexicon::LexImportMandarin - Imports a Unisyn lexicon.

inputs
   PCEscPage      pPage - Page to display errors on
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportMandarin (PCEscPage pPage)
{
   // make sure there are no words
   if (m_lWords.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the words in the existing lexicon?",
         L"You can't import the new lexicon while there are words. Do you wish to clear them?"))
         return FALSE;

      WordClearAll();
   }

   // make sure there are no phonemes
   if (m_lLEXPHONE.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the phones in the existing lexicon?",
         L"You can't import the new lexicon while there are phones. Do you wish to clear them?"))
         return FALSE;

      m_lLEXPHONE.Clear();
      PhonemeSort();
      m_fDirty = TRUE;
   }

   // clear shards for pronuncation
   ShardClear();
   m_fDirty = TRUE;

   // read in strings
   CListFixed lLUPT;
   lLUPT.Init (sizeof(LUPT));
   CMem mem;
   DWORD dwNumTones;
   LexRegGet (L"ImportMandarinNumTones", NULL, NULL, &dwNumTones, 5);
   dwNumTones = max(dwNumTones, 1);
   dwNumTones = min(dwNumTones, 6);

   LexRegGet (L"ImportMandarinPhoneFinal", &mem, gpszImportMandarinPhoneFinal); // also adds phonemes
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_PHONEFINAL, &lLUPT, FALSE, dwNumTones);

   LexRegGet (L"ImportMandarinPhoneInitial", &mem, gpszImportMandarinPhoneInitial);   // also adds phonemes
   LexUnisynParseToToken ((PWSTR)mem.p, LUPTMAJOR_PHONENOSTRESS, &lLUPT);


   // open file
   LexRegGet (L"ImportMandarinUttData", &mem, L"c:\\temp\\Utts.Data");
   FILE *pfUttData = _wfopen ((PWSTR)mem.p, L"wt");
   if (!pfUttData) {
      pPage->MBError (L"Can't open the file.", (PWSTR)mem.p);
      return FALSE;
   }

   // get the directory
   LexRegGet (L"ImportMandarinDir", &mem, L"");
   PWSTR psz = (PWSTR) mem.p;
   DWORD dwLen = (DWORD) wcslen(psz);
   mem.Required ((dwLen+2)*sizeof(WCHAR));
   psz = (PWSTR) mem.p;
   if (dwLen && (psz[dwLen-1] != L'\\'))
      wcscat (psz, L"\\");


   // find all .lbl files
   WCHAR szDir[256];
   WCHAR szTemp[256];
   wcscpy (szDir, psz);
   wcscat (szDir, L"*.lbl");

   WIN32_FIND_DATAW FindFileData;
   HANDLE hFind;

   DWORD dwFilesFound = 0;

   hFind = FindFirstFileW(szDir, &FindFileData);
   if (hFind != INVALID_HANDLE_VALUE) {
      while (TRUE) {
         dwFilesFound++;

         // analyze this file
         wcscpy (szTemp, psz);
         wcscat (szTemp, FindFileData.cFileName);
         if (!LexImportMandarinLblFile (pPage, FindFileData.cFileName, szTemp, pfUttData))
            break;

         if (!FindNextFileW (hFind, &FindFileData))
            break;
      }

      FindClose(hFind);
   }

   // close file
   fclose (pfUttData);

   swprintf (szTemp, L"Found %d .lbl files.", (int)dwFilesFound);
   pPage->MBInformation (szTemp);

   return TRUE;
}


// MANDARIN2PHONEME
typedef struct {
   PWSTR             pszPhone;      // phoneme
   PWSTR             pszSoundsLike; // sounds like
   BOOL              fTonal;        // true if tonal, FALSE if not
   PWSTR             pszEnglishPhone;  // english-equivalent phoneme
} MANDARIN2PHONEME, *PMANDARIN2PHONEME;

static MANDARIN2PHONEME gaMandarin2Phoneme[] = {
   // initials
   L"b", L"Bin", FALSE, L"b",
   L"p", L"Pin", FALSE, L"p",
   L"m", L"My", FALSE, L"m",
   L"f", L"Food", FALSE, L"f",
   L"d", L"Dog", FALSE, L"t",
   L"t", L"Tin", FALSE, L"t",
   L"n", L"Nice", FALSE, L"n",
   L"l", L"Learn", FALSE, L"l",
   L"g", L"Gift", FALSE, L"k",
   L"k", L"Kick", FALSE, L"k",
   L"h", L"Hi", FALSE, L"h",
   L"j", L"John", FALSE, L"jh",
   L"q", L"Q", FALSE, L"ch",
   L"x", L"X", FALSE, L"sh",
   L"zh", L"ZH", FALSE, L"sh",
   L"ch", L"CH", FALSE, L"ch",
   L"sh", L"SHe", FALSE, L"sh",
   L"r", L"R", FALSE, L"z",
   L"z", L"Z", FALSE, L"k",
   L"c", L"C", FALSE, L"k",
   L"s", L"Shoe", FALSE, L"s",
   L"w", L"Wood", FALSE, L"w",
   L"y", L"You", FALSE, L"y",

   // finals (without consonants)
   // -i - buzzed - not sure
   L"a", L"A", TRUE, L"aa",
   L"o", L"O", TRUE, L"uw",
   L"e", L"E", TRUE, L"ah",
   L"ex", L"E hat", TRUE, L"eh",
   L"ai", L"AI", TRUE, L"ay",
   L"ei", L"EI", TRUE, L"ey",
   L"ao", L"AO", TRUE, L"ao",
   L"ou", L"OU", TRUE, L"ow",
   // an - combination
   // en - combination
   // ang - combination
   // eng - combination
   L"ong", L"ONG", TRUE, L"uh",   // need to add ng though
   L"er", L"ER", TRUE, L"er",

   L"i", L"I", TRUE, L"iy",
   L"yi", L"I prefix", FALSE, L"y",  // kind-of consonant
   // ia - yi + a
   // io - combination
   // ie - imbination
   // iao - combination
   // iu - combination
   // in - combination
   // yang - combination
   // ying - combination
   // yong - combination

   L"u", L"U", TRUE, L"uw",
   L"yu", L"U prefix", FALSE, L"w",  // kind-of consonant
   // wa - yu + a = combination
   // wo - combination
   // wai - combination
   // wei - combination
   // wan - combination
   // wen - combination
   // wang - combination
   // weng - combination

   L"v", L"U umlout", TRUE, L"uw",
   L"yv", L"U umlout prefix", FALSE, L"y",  // kind-of consonant
   // yue = yv + e
   // yuan = yv + e + n
   // yun = v + n

   // lots of finals ending in r

   // final consonants (extra)
   L"ng", L"waNG", FALSE, L"nx",
   L"rr", L"aRE", FALSE, L"r"

};

// finals
// MANDARIN2FINAL
typedef struct {
   PWSTR          pszFinal;      // final name, can be NULL
   PWSTR          pszPhonemes;   // phonemes (using 1 where a stress should be placed)
   PWSTR          pszFormZeroInitial;  // what form to use with a zero initial. Can be NULL
} MANDARIN2FINAL, *PMANDARIN2FINAL;

static PWSTR gapszInitial[] = {
   L"b",
   L"p",
   L"m",
   L"f",
   L"d",
   L"t",
   L"n",
   L"l",
   L"g",
   L"k",
   L"h",
   L"j",
   L"q",
   L"x",
   L"zh",
   L"ch",
   L"sh",
   L"r",
   L"z",
   L"c",
   L"s",
   L"w",
   L"y"
};

static MANDARIN2FINAL gaMandarin2Final[] = {
   L"_a", L"a1", L"a",
   L"_o", L"o1", L"o",
   L"_e", L"e1", L"e",
   L"_ehat", L"ex1", NULL,
   L"_ai", L"ai1", L"ai",
   L"_ei", L"ei1", L"ei",
   L"_ao", L"ao1", L"ao",
   L"_ou", L"ou1", L"ou",
   L"_an", L"a1 n", L"an",
   L"_en", L"e1 n", L"en",
   L"_ang", L"a1 ng", L"ang",
   L"_eng", L"e1 ng", L"eng",
   NULL, L"ong1 ng", L"weng",
   NULL, L"er1", L"er",

   L"_i", L"i1", L"yi",
   L"_ia", L"yi |_a1", L"ya",
   L"_io", L"yi |_o1", L"yo",
   L"_ie", L"yi |_ehat1", L"ye",
   L"_iao", L"yi |_ao1", L"yao",
   L"_iu", L"yi |_ou1", L"you",
   L"_ian", L"yi |_ehat1 n", L"yan",
   L"_in", L"|_i1 n", L"yin",
   L"_iang", L"yi |_ang1", L"yang",
   L"_ing", L"|_i1 ng", L"ying",
   L"_iong", L"yi ong1 ng", L"yong",   // BUGFIX - Had |_ong1, but not adding that yet

   L"_u", L"u1", L"wu",
   L"_ua", L"yu |_a1", L"wa",
   L"_uo", L"yu |_o1", L"wo",
   L"_uai", L"yu |_ai1", L"wai",
   L"_ui", L"yu |_ei1", L"wei",
   L"_uan", L"yu |_an1", L"wan",
   L"_un", L"yu |_en1", L"wen",
   L"_uang", L"yu |_ang1", L"wang",
   L"_ong", L"yu |_eng1", L"weng",  // BUGFIX - Had |_eng1, but not addeding that yet
   //L"_ong", L"yu e1 ng", L"weng",  // BUGFIX - Had |_eng1, but not addeding that yet

   L"_v", L"v1", L"yu",
   L"_ue", L"yv |_ehat1", L"yue",
   L"_ve", L"yv |_ehat1", L"yue",
   L"_van", L"yv |_ehat1 n", L"yuen",
   L"_vn", L"|_v1 n", L"yun",

   L"_ar", L"er1", NULL,
   L"_er", L"e1 rr", NULL,
   L"_or", L"|_o1 rr", NULL,
   L"_air", L"|_ar1", NULL,
   L"_eir", L"ex1 rr", NULL,
   L"_aor", L"|_ao1 rr", NULL,
   L"_our", L"|_ou1 rr", NULL,
   L"_anr", L"|_ar1", NULL,
   L"_enr", L"ex1 rr", NULL,
   L"_angr", L"a1 rr", NULL,
   L"_engr", L"e1 rr", NULL,
   L"_ongr", L"ong1 rr", NULL,
   L"_ir", L"yi |_eir1", NULL,
   L"_ir", L"e1 rr", NULL,
   L"_iar", L"yi |_ar1", NULL,
   L"_ier", L"|_ie1 rr", NULL,
   L"_iaor", L"|_iao1 rr", NULL,
   L"_iur", L"yi |_ou1 rr", NULL,
   L"_ianr", L"|_i1 rr", NULL,
   L"_inr", L"|_ir1", NULL,
   L"_iangr", L"yi |_ang1 rr", NULL,
   L"_ingr", L"yi |_eng1 rr", NULL,
   L"_iongr", L"yi |_ong1 rr", NULL,
   L"_ur", L"|_u1 rr", NULL,
   L"_uar", L"yu |_ar1", NULL,
   L"_uor", L"|_uo1 rr", NULL,
   L"_uair", L"yu |_ar1", NULL,
   L"_uir", L"yu |_eir1", NULL,
   L"_uanr", L"yu |_ar1", NULL,
   L"_unr", L"yu |_eir1", NULL,
   L"_uangr", L"yu |_ang1 rr", NULL,
   L"_vr", L"yv |_eir1", NULL,
   L"_ver", L"|_ue1 rr", NULL,
   L"_vanr", L"yv |_ar1", NULL,
   L"_vnr", L"yv |_eir1", NULL,

   // added - not sure if right
   NULL, L"er1", L"r"

};

#define NUMMANDARINTONES         5     // 5 tones in mandarin



/***************************************************************************
CMLexicon::ChineseUse - Returns true if should use the chinese parsing rules
for this lexicon
*/
BOOL CMLexicon::ChineseUse (void)
{
   if (PRIMARYLANGID(m_LangID) == 0x04)
      return TRUE;
   else
      return FALSE;
}

/*************************************************************************************
CMLexicon::LexImportMandarin2ParsePOS - Guess the part-of-speech from the text.

inputs
   PCEscPage      pPage - To display errors off of
   PWSTR          pszLine - Full line
   PWSTR          pszDef - Definition to try and understand
   DWORD          *padwPOS - Array of POS_MAJOR_NUM to fill in. Add 1 if come up with a conclusive POS
   PCMLexicon     pLexEnglish - English lexicon
   PCTextParse    pTextParseEnglish - Text parser for english
returns
   BOOL - If TRUE then continue, FALSE then stop processing
*/
BOOL CMLexicon::LexImportMandarin2ParsePOS (PCEscPage pPage, PWSTR pszLine, PWSTR pszDef, DWORD *padwPOS, PCMLexicon pLexEnglish, PCTextParse pTextParseEnglish)
{
   while (iswspace(pszDef[0]))
      pszDef++;

   // if this starts with "\to " then it's definitely a verb
   if (!wcsncmp (pszDef, L"to ", 3) || !wcsncmp (pszDef, L"To ", 3)) {
      padwPOS[POS_MAJOR_EXTRACT(POS_MAJOR_VERB)] ++;
      return TRUE;
   }
   
   // abbreviation
   if (!wcsncmp (pszDef, L"abbr. ", 6)) {
      padwPOS[POS_MAJOR_EXTRACT(POS_MAJOR_NOUN)] ++;
      return TRUE;
   }

   if (!pszDef[0])
      return TRUE;   // blank line. do nothing

#ifdef _DEBUG  // BUGBUG - hack so can build quickly
   padwPOS[POS_MAJOR_EXTRACT(POS_MAJOR_NOUN)]++;
   return TRUE;
#endif

   // parse this and guess
   DWORD adwPOSTemp[POS_MAJOR_NUM];
   memset (adwPOSTemp, 0, sizeof(adwPOSTemp));
   PCMMLNode2 pNode = pTextParseEnglish->ParseFromText (pszDef, TRUE, FALSE);

   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, pTextParseEnglish->Word()) )
         continue;   // not a word

      BYTE bPOS;
      psz = pSub->AttribGetString(pTextParseEnglish->POS());
      if (psz)
         bPOS = POS_MAJOR_EXTRACT ((BYTE)_wtoi(psz));
      else
         bPOS = POS_MAJOR_EXTRACT (POS_MAJOR_UNKNOWN);

      if (bPOS < POS_MAJOR_NUM)
         adwPOSTemp[bPOS]++;
#ifdef _DEBUG
      else {
         OutputDebugStringW (L"\r\nBad POS in: ");
         OutputDebugStringW (pszLine);
      }
#endif
   } // i

   // find the one with the highest value
   DWORD dwBest = (DWORD)-1;
   DWORD dwBestValue = 0;
   DWORD dwValue;
   for (i = 0; i < POS_MAJOR_NUM; i++) {
      dwValue = adwPOSTemp[i];
      if (!dwValue)
         continue;

      // if this ISN'T a function word then increase the value
      switch (i) {
         case POS_MAJOR_EXTRACT (POS_MAJOR_PREPOSITION):
         case POS_MAJOR_EXTRACT (POS_MAJOR_ARTICLE):
         case POS_MAJOR_EXTRACT (POS_MAJOR_AUXVERB):
         case POS_MAJOR_EXTRACT (POS_MAJOR_CONJUNCTION):
            break;

         case POS_MAJOR_EXTRACT (POS_MAJOR_NOUN):
         case POS_MAJOR_EXTRACT (POS_MAJOR_VERB):
            dwValue *= 4;
            break;

         default:
            dwValue *= 2;
            break;
      } // swtcih

      if ((dwBest == (DWORD)-1) || (dwValue > dwBestValue)) {
         dwBest = i;
         dwBestValue = dwValue;
      }
   } // i

   delete pNode;

   // if found a best value then assume that's the meaning
   if ((dwBest != (DWORD)-1) && (dwBest != POS_MAJOR_EXTRACT (POS_MAJOR_UNKNOWN))  )
      padwPOS[dwBest]++;

   // else, unknown
   return TRUE;
}


/*************************************************************************************
CMLexicon::LexImportMandarin2Parse - Reads in a line of cedict_ts.u8 at a time
and adds words if necessary

inputs
   PCEscPage      pPage - Where to bring up errors
   PWSTR          pszLine - Line of text
   PCMLexicon     pLexEnglish - English lexicon
   PCTextParse    pTextParseEnglish - Text parser for english
returns
   BOOL - TRUE if success, FALSE if error and should stop
*/
BOOL CMLexicon::LexImportMandarin2Parse (PCEscPage pPage, PWSTR pszLine, PCMLexicon pLexEnglish, PCTextParse pTextParseEnglish)
{
   // if the line starts with '#' then is a comment, and ignore
   if (pszLine[0] == L'#')
      return TRUE;

   // look for the first left bracket
   PWSTR pszLeftBracket = wcschr (pszLine, L'[');
   if (!pszLeftBracket)
      return TRUE;   // unknown line

   // look for first right bracket
   PWSTR pszRightBracket = wcschr (pszLeftBracket, L']');
   if (!pszRightBracket) {
      pPage->MBError (L"Can't find ] in the line", pszLine);
      return FALSE;
   }

   // copy what's in teh brackets
   WCHAR szInBrackets[256];
   WCHAR cChar = pszRightBracket[0];
   pszRightBracket[0] = 0;
   wcscpy (szInBrackets, pszLeftBracket + 1);
   pszRightBracket[0] = cChar;

   // make a word out of this
   WCHAR szWordName[256], szPronText[256], szSyllable[256];
   szWordName[0] = 0;
   szPronText[0] = 0;
   WCHAR *pszFrom, *pszEnd;
   DWORD dwSyllables = 0;
   CListVariable lWordNames, lPronNames;
   CListFixed lSyllables;
   lSyllables.Init (sizeof(DWORD));
   for (pszFrom = szInBrackets; *pszFrom; pszFrom++) {
      // skip white space
      if (iswspace (*pszFrom))
         continue;   // skip

      // BUGFIX - An A with a hat sometimes appears. ignore it
      if (pszFrom[0] == 194)
         continue;

      // BUGFIX - A special ellipses character ... sometimes appears, treat it as a whitespace
      if ((pszFrom[0] == 226) && (pszFrom[1] == 128) && (pszFrom[2] == 166)) {
         pszFrom += 2;
         continue;
      }

      // if there's a comma here, then it's a phrase. Divide what have
      if ((pszFrom[0] == L',') || (pszFrom[0] == L'·')) {
         lWordNames.Add (szWordName, (wcslen(szWordName)+1)*sizeof(WCHAR));
         lPronNames.Add (szPronText, (wcslen(szPronText)+1)*sizeof(WCHAR));
         lSyllables.Add (&dwSyllables);

         dwSyllables = 0;
         szWordName[0] = 0;
         szPronText[0] = 0;

         continue;
      }

      // look for whitespace or end of line
      for (pszEnd = pszFrom+1; *pszEnd && !iswspace(*pszEnd); pszEnd++)
         if ((pszEnd[0] >= L'0') && (pszEnd[0] <= L'9')) {
            // force to treat as whitespace
            pszEnd++;
            break;
         }
      cChar = *pszEnd;
      *pszEnd = 0;
      wcscpy (szSyllable, pszFrom);
      *pszEnd = cChar;
      pszFrom = pszEnd-1;

      // if this contains a ":" then it's a u-oomlout, so convert the same
      PWSTR pszColon = wcschr (szSyllable, L':');
      if (pszColon) {
         // convert u: to v
         memmove (pszColon, pszColon + 1, (wcslen(pszColon+1)+1)*sizeof(WCHAR));
         pszColon[-1] = 'v';
      }
      // add to word name
      if (szWordName[0])
         wcscat (szWordName, L"-"); // hypen
      wcscat (szWordName, szSyllable);

      // add to word pron
      if (szPronText[0])
         wcscat (szPronText, L" "); // space between prons
      wcscat (szPronText, L"|"); // so know to use the pronunciation of the word
      wcscat (szPronText, szSyllable);

      
      dwSyllables++;
   } // pszFrom
   lWordNames.Add (szWordName, (wcslen(szWordName)+1)*sizeof(WCHAR));
   lPronNames.Add (szPronText, (wcslen(szPronText)+1)*sizeof(WCHAR));
   lSyllables.Add (&dwSyllables);

   // guess the parts of speech
   DWORD adwPOS[POS_MAJOR_NUM];
   memset (adwPOS, 0, sizeof(adwPOS));
   pszFrom = wcschr (pszRightBracket+1, L'/');
   for (; pszFrom; pszFrom = wcschr (pszFrom+1, L'/')) {
      PWSTR pszNext = wcschr (pszFrom+1, L'/');
      if (!pszNext)
         pszNext = pszFrom + wcslen(pszFrom);

      cChar = pszNext[0];
      pszNext[0] = 0;
      if (!LexImportMandarin2ParsePOS (pPage, pszLine, pszFrom + 1, &adwPOS[0], pLexEnglish, pTextParseEnglish))
         return FALSE;
      pszNext[0] = cChar;
   }

   // if nothing then assume it's a noun
   DWORD i;
   for (i = 0; i < POS_MAJOR_NUM; i++)
      if (adwPOS[i])
         break;
   if (i >= POS_MAJOR_NUM)
      adwPOS[POS_MAJOR_EXTRACT(POS_MAJOR_NOUN)] = 1;



   DWORD dwVersion;
   for (dwVersion = 0; dwVersion < lWordNames.Num(); dwVersion++) {
      wcscpy (szWordName, (PWSTR)lWordNames.Get(dwVersion));
      wcscpy (szPronText, (PWSTR)lPronNames.Get(dwVersion));
      dwSyllables = *((DWORD*)lSyllables.Get(dwVersion));

      // must have at least one syllable
      if (!dwSyllables) {
         pPage->MBError (L"The word doesn't contain any syllables.", pszLine);
         return FALSE;
      }
      // make sure that can create a pronunciation
      BYTE     abPron[256];
      DWORD dwBadPhone;
      abPron[0] = 0;
      if (!PronunciationFromText (szPronText, abPron+1, sizeof(abPron)-1, &dwBadPhone)) {

#ifdef _DEBUG
         OutputDebugStringW (L"\r\nCan't find the pronunciation for one of these syllables: ");
         OutputDebugStringW (szPronText);
         continue;
#endif

         pPage->MBWarning (L"Can't find the pronunciation for one of these syllables.", szPronText);

         continue;
      }

      // see if the word exists
      CListVariable lPron;
      lPron.Clear();
      WordGet (szWordName, &lPron);
      BOOL fHasPron = FALSE;
      BOOL fHasPronWithPOS = FALSE;
      BYTE *pabPron;
      DWORD i;
      for (i = 0; i < lPron.Num(); i++) {
         fHasPron = TRUE;

         pabPron = (PBYTE)lPron.Get(i);
         if (pabPron[0])
            fHasPronWithPOS = TRUE;
      }

      // if it exists and there's more than one syllable then complain,
#ifdef _DEBUG
      if ((fHasPron && (dwSyllables > 1)) || (fHasPronWithPOS && (dwSyllables == 1)) ) {
         OutputDebugStringW (L"\r\nThe word already exists: ");
         OutputDebugStringW (szWordName);
         // pPage->MBError (L"The word already exists.", pszLine);
         // return FALSE;
      }
#endif

      CListVariable lPronNew;
      lPronNew.Clear();
      DWORD j;
      // if this is only one syllable...
      if (dwSyllables == 1) {
         // if there's no pronunciation then error
         if (!lPron.Num()) {
            pPage->MBError (L"One of the syllables doesn't have a pronunciation.", pszLine);
            return FALSE;
         }

         for (i = 0; i < lPron.Num(); i++) {
            // copy over the existing pronunciation (in 0) and use that,
            // because otherwise this just refers back to itself
            pabPron = (PBYTE)lPron.Get(i);
            abPron[0] = 0;
            memcpy (abPron+1, pabPron+1, strlen((char*)pabPron+1)+1);

            // make sure not there alredy
            for (j = 0; j < lPronNew.Num(); j++) {
               pabPron = (PBYTE)lPronNew.Get(j);
               if (!memcmp (pabPron, abPron, strlen((char*)abPron+1)+2))
                  break;   // the same
            } // j
            if (j >= lPronNew.Num())
               // else, add
               lPronNew.Add (abPron, strlen((char*)abPron+1)+2);
         } // i

      }
      else
         // only one new pronunciation
         lPronNew.Add (abPron, strlen((char*)abPron+1)+2);

      // add all potential pronunciations
      PBYTE pabPronNew;
      DWORD dwPOS;
      for (dwPOS = 0; dwPOS < POS_MAJOR_NUM; dwPOS++) {
         // add for POS that found
         if (!adwPOS[dwPOS])
            continue;

         for (i = 0; i < lPronNew.Num(); i++) {
            pabPronNew = (PBYTE)lPronNew.Get(i);
            pabPronNew[0] = (BYTE)POS_MAJOR_MAKE(dwPOS);

            for (j = 0; j < lPron.Num(); j++) {
               pabPron = (PBYTE)lPron.Get(j);

               // if the pronunciation strings are different then skip over
               if (strcmp((char*)pabPronNew+1, (char*)pabPron+1))
                  continue;

               // else, they're the same

               // if the existing pronunciation is a 0 POS, then just overwrite
               if (pabPron[0] == 0)
                  break;

               // if this is an exact match, then overwrite
               if (pabPron[0] == pabPronNew[0])
                  break;
            } // j

            // if get here and haven't found a match then add
            if (j >= lPron.Num())
               lPron.Add (pabPronNew, strlen((char*)pabPronNew+1) + 2);
            else // copy over
               lPron.Set (j, pabPronNew, strlen((char*)pabPronNew+1) + 2);
         } // i
      } // dwPOS

      // write the word
      if (!WordSet (szWordName, &lPron)) {
         pPage->MBError (L"WordSet() error.");
         return FALSE;
      }
   } // dwVersion

   return TRUE;
}


/*************************************************************************************
CMLexicon::LexImportMandarin2 - Imports a Unisyn lexicon.

inputs
   PCEscPage      pPage - Page to display errors on
returns
   BOOL - TRUE if success, FALSE if failure (will have displayed error)
*/
BOOL CMLexicon::LexImportMandarin2 (PCEscPage pPage)
{
   // make sure there are no words
   if (m_lWords.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the words in the existing lexicon?",
         L"You can't import the new lexicon while there are words. Do you wish to clear them?"))
         return FALSE;

      WordClearAll();
   }

   // make sure there are no phonemes
   if (m_lLEXPHONE.Num()) {
      if (IDYES != pPage->MBYesNo (L"Do you wish to erase all the phones in the existing lexicon?",
         L"You can't import the new lexicon while there are phones. Do you wish to clear them?"))
         return FALSE;

      m_lLEXPHONE.Clear();
      PhonemeSort();
      m_fDirty = TRUE;
   }

   // clear shards for pronuncation
   ShardClear();
   m_fDirty = TRUE;


   // create all the phonemes
   DWORD i;
   PMANDARIN2PHONEME pmp = &gaMandarin2Phoneme[0];
   // DWORD dwIndex;
   PLEXPHONE plp;
   DWORD dwTones, dwTone;
   WCHAR szPhoneTone[16], szPhoneToneUnstressed[16];
   for (i = 0; i < sizeof(gaMandarin2Phoneme) / sizeof(gaMandarin2Phoneme[0]); i++, pmp++) {
      dwTones = pmp->fTonal ? NUMMANDARINTONES : 1;
      szPhoneToneUnstressed[0] = 0;

      for (dwTone = 0; dwTone < dwTones; dwTone++) {
         if (pmp->fTonal)
            swprintf (szPhoneTone, L"%s%d", pmp->pszPhone, (int)dwTone+1);
         else
            wcscpy (szPhoneTone, pmp->pszPhone);

         // remember the unstressed one
         if (!dwTone)
            wcscpy (szPhoneToneUnstressed, szPhoneTone);

         // add the phoneme
         PhonemeAddBlank (szPhoneTone);

         // sort so can find
         PhonemeSort ();

         // fill it in
         plp = PhonemeGet (szPhoneTone);
         if (!plp)
            return FALSE;  // unexpected error

         plp->bStress = (BYTE)dwTone;
         wcscpy (plp->szSampleWord, pmp->pszSoundsLike);
         plp->wPhoneOtherStress = dwTone ? PhonemeFindUnsort (szPhoneToneUnstressed) : 255;
         _ASSERTE (!dwTone || (plp->wPhoneOtherStress < 100));
         plp->bEnglishPhone = (BYTE) MLexiconEnglishPhoneFind (pmp->pszEnglishPhone);
         _ASSERTE (plp->bEnglishPhone < 100);   // so that found phone
      } // dwToens
   } // i

   // create all syllables
   DWORD dwFinal, dwInitial;
   PMANDARIN2FINAL pfin = &gaMandarin2Final[0];
   WCHAR szPronString[256];
   BYTE abPron[256];
   WCHAR szFinalName[256], szWordName[256];
   CListVariable lForms;
   for (dwFinal = 0; dwFinal < sizeof(gaMandarin2Final) /sizeof(gaMandarin2Final[0]); dwFinal++, pfin++) {
      for (dwTone = 0; dwTone < NUMMANDARINTONES; dwTone++) {
         // copy over the pronuncation string and replace 1's with the tone
         wcscpy (szPronString, pfin->pszPhonemes);
         for (i = 0; szPronString[i]; i++)
            if (szPronString[i] == L'1')
               szPronString[i] = L'1' + (WCHAR)dwTone;

         // convert this to phonemes
         abPron[0] = 0; // unknown
         DWORD dwBadPhone;
         if (!PronunciationFromText (szPronString, abPron+1, sizeof(abPron)-1, &dwBadPhone)) {
            _ASSERTE(FALSE);
            continue;   // error, shouldnt get
         }

         // what's the name if this final
         swprintf (szFinalName, L"%s%d", pfin->pszFinal ? pfin->pszFinal : pfin->pszFormZeroInitial, (int)dwTone+1);
 

         // pronunciatons
         lForms.Clear ();

         // if it exists already then there's a problem
         if (WordExists (szFinalName)) {
#ifdef _DEBUG
            OutputDebugStringW (L"\r\nLexImportMandarin2 WordExists() add final: ");
            OutputDebugStringW (szFinalName);
#endif
            WordGet (szFinalName, &lForms);
         }

         // fill in the forms
         DWORD dwLen = (DWORD)strlen((char*)abPron+1) + 2;
         for (i = 0; i < lForms.Num(); i++)
            if (!memcmp (lForms.Get(0), abPron, dwLen))
               break;   // matching pronunciation
         if (i >= lForms.Num())
            lForms.Add (abPron, dwLen);   // don't add since matching pronunciation

         // create it
         WordSet (szFinalName, &lForms);

         // if !pfin->pszFinal, then used zero intial for the word, so just stop here
         if (!pfin->pszFinal)
            continue;

         // make up all the word versions
         _ASSERTE (pfin->pszFinal[0] == L'_');
         for (dwInitial = 0; dwInitial <= sizeof(gapszInitial)/sizeof(gapszInitial[0]); dwInitial++) {
            PWSTR psz;
            if (dwInitial)
               psz = gapszInitial[dwInitial-1];
            else
               psz = pfin->pszFormZeroInitial;
            if (!psz)
               continue;

            // fill in the pronunciation with this
            if (dwInitial)
               swprintf (szPronString, L"%s |%s", psz, szFinalName);
            else
               swprintf (szPronString, L"|%s", szFinalName);
            abPron[0] = 0; // unknown
            if (!PronunciationFromText (szPronString, abPron+1, sizeof(abPron)-1, &dwBadPhone)) {
               _ASSERTE (FALSE);
               continue;   // error, shouldnt get
            }

            // full name
            if (dwInitial)
               swprintf (szWordName, L"%s%s%d", psz, pfin->pszFinal + 1, (int)dwTone+1);
            else
               swprintf (szWordName, L"%s%d", psz, (int)dwTone+1); // default final

            // pronunciatons
            lForms.Clear ();

            // if it exists already then there's a problem
            if (WordExists (szWordName)) {
#ifdef _DEBUG
               OutputDebugStringW (L"\r\nLexImportMandarin2 WordExists() add word: ");
               OutputDebugStringW (szWordName);
#endif

               WordGet (szWordName, &lForms);
            }

            // fill in the forms
            DWORD dwLen = (DWORD)strlen((char*)abPron+1) + 2;
            for (i = 0; i < lForms.Num(); i++)
               if (!memcmp (lForms.Get(0), abPron, dwLen))
                  break;   // matching pronunciation
            if (i >= lForms.Num())
               lForms.Add (abPron, dwLen);   // don't add since matching pronunciation

            // create it
            WordSet (szWordName, &lForms);
         } // i
      } // dwTone
   } // i

   // read in strings
   CMem mem;
   LexRegGet (L"ImportMandarin2EnglishLex", &mem, L"c:\\program files\\mxac\\3D Outside the box\\EnglishInstalled.mlx");   // also adds phonemes
   PCMLexicon pLexEnglish = MLexiconCacheOpen ((PWSTR)mem.p, FALSE);
   if (!pLexEnglish) {
      pPage->MBError (L"The english lexicon can't be found.", (PWSTR)mem.p);
      return FALSE;
   }

   CTextParse TextParse;
   TextParse.Init (pLexEnglish->LangIDGet(), pLexEnglish);

   LexRegGet (L"ImportMandarin2U8File", &mem, L""); // also adds phonemes

   FILE *pf = _wfopen ((PWSTR)mem.p, L"rt");
   WCHAR szHuge[1000];
   if (!pf) {
      pPage->MBError (L"Can't open the file.", (PWSTR)mem.p);
      MLexiconCacheClose (pLexEnglish);
      return FALSE;
   }


   fseek (pf, 0, SEEK_END);
   int iSize = ftell (pf);
   fseek (pf, 0, SEEK_SET);

   {
      CProgress Progress;
      Progress.Start (pPage->m_pWindow->m_hWnd, "Loading...", TRUE);

      // read in line at a time (as ascii), convert to unicode, and parse line
      while (fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf)) {

         Progress.Update ((fp)ftell(pf) / (fp)iSize);

         if (!LexImportMandarin2Parse (pPage, szHuge, pLexEnglish, &TextParse)) {
            fclose (pf);
            MLexiconCacheClose (pLexEnglish);
            return FALSE;
         }
      } // get string
   }

   // close file
   fclose (pf);


   MLexiconCacheClose (pLexEnglish);
   return TRUE;
}


/*************************************************************************************
GGramTrain::Constructor
*/
CGramTrain::CGramTrain (void)
{
   m_dwLastSentence = (DWORD)-1;
}


/*************************************************************************************
GGramTrain::Destructor
*/
CGramTrain::~CGramTrain (void)
{
   // do nothing for now
}


/*************************************************************************************
GGramTrain::SentenceGet - Gets a sentence from the list.

inputs
   DWORD          dwIndex - Sentence number. Start at 0. The next acceptable sentence
                  number is filled into *pdwNext.
   DWORD          *pdwNumWords  - Filled with the number of words in the sentence.
                  Can be NULL.
   double         *pfScore - Filled in with the score. Can be NULL.
   DWORD          *pdwNext - Filled in with the sentence that follows this. Set to
                  0 if the next one doesn't exist
returns
   PBYTE - Array of bytes for word POS. The array is *pdwNumWords long
*/
PBYTE CGramTrain::SentenceGet (DWORD dwIndex, DWORD *pdwNumWords, double *pfScore, DWORD *pdwNext)
{
   if (m_dwLastSentence == (DWORD)-1) {
      if (pdwNumWords)
         *pdwNumWords = 0;
      if (pfScore)
         *pfScore = 0;
      if (pdwNext)
         *pdwNext = 0;
      return NULL;
   }

   PGTSENT pSent = (PGTSENT) ((PBYTE)m_memSentences.p + dwIndex);
   if (pdwNumWords)
      *pdwNumWords = pSent->dwWords;
   if (pfScore)
      *pfScore = pSent->fScore;
   if (pdwNext)
      *pdwNext = pSent->dwNext;
   return (PBYTE)(pSent+1);
}


/*************************************************************************************
GGramTrain::SentenceAdd - Adds a sentence to the list.

inputs
   DWORD       dwNum - Number of words
   BYTE        *pabPOS - Array of dwNUM parts of speech... 0..15 are the POS values,
               might also contain higher numbers which are rules.
   double      fScore - Score for the sentence.
returns
   DWORD - Index of new sentence, or -1 if error
*/
DWORD CGramTrain::SentenceAdd (DWORD dwNum, BYTE *pabPOS, double fScore)
{
   // figure out where will start adding
   DWORD dwAddTo, dwSize;
   PGTSENT pPreviousLast;
   if (m_dwLastSentence == (DWORD)-1) {
      dwAddTo = 0;
      pPreviousLast = NULL;
   }
   else {
      pPreviousLast = (PGTSENT) ((PBYTE)m_memSentences.p + m_dwLastSentence);
      dwSize = ((pPreviousLast->dwWords + 3) & ~0x03) + sizeof(GTSENT);
      dwAddTo = m_dwLastSentence + dwSize;
   }

   // determine how much will need
   dwSize = ((dwNum + 3) & ~0x03) + sizeof(GTSENT);
   if (!m_memSentences.Required (dwAddTo + dwSize))
      return (DWORD)-1; // error
   if (pPreviousLast)
      pPreviousLast = (PGTSENT) ((PBYTE)m_memSentences.p + m_dwLastSentence); // since might have been moved

   // copy over
   PGTSENT pNew = (PGTSENT) ((PBYTE)m_memSentences.p + dwAddTo);
   pNew->dwNext = 0;
   pNew->dwWords = dwNum;
   pNew->fScore = fScore;
   memcpy (pNew+1, pabPOS, dwNum);

   // link
   if (pPreviousLast)
      pPreviousLast->dwNext = dwAddTo;
   m_dwLastSentence = dwAddTo;

   // done
   return dwAddTo;
}


/*************************************************************************************
GGramTrain::SentenceAdd - Adds a sentnce to the sentence list, using an array
of LEXPOSGUESS for the words. This automatically determines words with ambiguous
parts-of-speech and adds multiple copies of the sentence with alternative POS.
Each alternative will have a fractional score, summing to 1.0.

inputs
   DWORD             dwNum - Number of entries
   PLEXPOSGUESS      pLPG - Array of word into. NOTE: wPOSBitField MUST be filled in.
   PCMLexicon        pLex - Lexicon to use
returns
   DWORD - Sentence index added, or -1 if error
*/
DWORD CGramTrain::SentenceAdd (DWORD dwNum, PLEXPOSGUESS paLPG, PCMLexicon pLex)
{
   // create a bit-field for all the possible parses
   CListFixed lBits;
   if (!lBits.Init (sizeof(WORD)))
      return (DWORD)-1;

   DWORD i, j;
   // fill in bit field
   lBits.Required (dwNum);
   for (i = 0; i < dwNum; i++, paLPG++)
      lBits.Add (&paLPG->wPOSBitField);

   // now that have bits, create another list containing the POS BYTE to use, if only
   // one POS
   CListFixed lPOS;
   lPOS.Init (sizeof(BYTE));
   WORD *pawBits = (WORD*)lBits.Get(0);
   BYTE bAdd;
   DWORD dwUnkWords = 0;
   lPOS.Required (lBits.Num());
   for (i = 0; i < lBits.Num(); i++) {
      WORD awBit = pawBits[i];

      if (awBit & (awBit-1)) {
         // can have multiple POS's
         dwUnkWords++;
         bAdd = 0;
         lPOS.Add (&bAdd);
         continue;
      }

      // this code assumes that POS is 0
      _ASSERTE (POS_MAJOR_UNKNOWN == 0);

      // else, figure out bit
      for (j = 0; j < 16; j++)
         if (awBit & (WORD)(1 << j))
            break;
      bAdd = (BYTE)j;
      lPOS.Add (&bAdd);
   } // i

   // if there are too many unknown words then error, because will result in too many sentences
   if (dwUnkWords > 8)
      return (DWORD)-1;

   // do a recursive sentence add
   return SentenceAddRecurse (lBits.Num(), pawBits, (PBYTE)lPOS.Get(0), 1.0, 0);
}


/*************************************************************************************
GGramTrain::SentenceAddRecurse - Internal method called by SentenceAdd() with
arbitrary POS. This creates all possible forms, reducing the score each time.

inputs
   DWORD       dwNum - Number of words in the sentence
   WORD        *pawBits - Array of dwNum entries, with bit fields indicating possible
               POS's.
   BYTE        *pabPOS - Array of dwNum entries for POS. If the POS is 0 then there is more
               than one possible POS.
   double      fScore - Current score.
   DWORD       dwCur - Current word to look at. If >= dwNum then add the existing setnce
returns
   DWORD - Sentence number for one of the sentences added, or -1 if error
*/
DWORD CGramTrain::SentenceAddRecurse (DWORD dwNum, WORD *pawBits, BYTE *pabPOS,
                                      double fScore, DWORD dwCur)
{
   if (dwCur >= dwNum) {
      // if 0 length then error
      if (!dwNum)
         return (DWORD)-1;

      // all the pabPOS should be filled in, so add
      return SentenceAdd (dwNum, pabPOS, fScore);
   }

   // if there aren't any choices to make then easy
   if (pabPOS[dwCur])
      return SentenceAddRecurse (dwNum, pawBits, pabPOS, fScore, dwCur+1);

   // if there are choices then figure out alternatives

   // count the number of alternatives
   DWORD dwCount = 0;
   WORD wTemp = pawBits[dwCur];
   for (; wTemp; wTemp = wTemp & (wTemp-1))
      dwCount++;

   // evenly divide the score
   fScore /= (double)dwCount;

   // produce all the alternatives
   DWORD dwRet, i;
   wTemp = pawBits[dwCur];
   for (i = 0; i < 16; i++) {
      if (!(wTemp & (WORD)(1 << i)))
         continue; // nothing here

      // else, chose this and call in
      pabPOS[dwCur] = (BYTE)i;
      dwRet = SentenceAddRecurse (dwNum, pawBits, pabPOS, fScore, dwCur+1);
      if (dwRet == (DWORD)-1)
         return dwRet;

      // if no more POS then quit now
      dwCount--;
      if (!dwCount)
         break;
   } // i

   // restore
   pabPOS[dwCur] = 0;

   // done
   return dwRet;  // return the last one added
}


/*************************************************************************************
GGramTrain::IdenitfyRules - This identifies the top N rules, where a rule converts
part's of speech A followed by B into C. The most common conversion are returned.

inputs
   DWORD             dwNum - Number of rules that want to get
   PCMLexicon        pLex - Lexicon (used for debug strings only)
   PCListFixed       plGTRULE - Will be initialized to sizeof(GTRULE), and filled
                     with the top N rules in order from most common to least.
                     NOTE: The bConvert won't be filled in.
returns
   BOOL - TRUE if succes
*/

// GTRULESCORE - Internal structure for storing a rule with a score
typedef struct {
   double         fScore;     // score
   GTRULE         Rule;       // rule
} GTRULESCORE, *PGTRULESCORE;

static int _cdecl GTRULESCORESort (const void *elem1, const void *elem2)
{
   GTRULESCORE *pdw1, *pdw2;
   pdw1 = (GTRULESCORE*) elem1;
   pdw2 = (GTRULESCORE*) elem2;

   if (pdw1->fScore < pdw2->fScore)
      return 1;
   else if (pdw1->fScore > pdw2->fScore)
      return -1;
   else
      return 0;
}

BOOL CGramTrain::IdentifyRules (DWORD dwNum, PCMLexicon pLex, PCListFixed plGTRULE)
{
   plGTRULE->Init (sizeof(GTRULE));

   // if not sentences no rules
   if (m_dwLastSentence == (DWORD)-1)
      return TRUE;

   // create enough memory to store 256 x 256 doubles, so can count the scores
   CMem memCount;
   DWORD dwSize = 256 * 256 * sizeof(double);
   if (!memCount.Required (dwSize))
      return FALSE;
   double *pafCount = (double*)memCount.p;
   memset (pafCount, 0, dwSize);

   // loop through all the sentences
   DWORD dwCur, i;
   PGTSENT pSent;
   PBYTE pbPOS;
   for (dwCur = 0; ; ) {
      pSent = (PGTSENT)((PBYTE)m_memSentences.p + dwCur);
      pbPOS = (PBYTE)(pSent+1);

      // loop through all the parts of speech, and get POS pair
      for (i = 1; i < pSent->dwWords; i++)
         pafCount[ (DWORD)pbPOS[i-1]*256 + (DWORD)pbPOS[i] ] += pSent->fScore;

      // next
      dwCur = pSent->dwNext;
      if (!dwCur)
         break;   // end of sentences
   } // for dwCur

   // create a list of all pairs
   CListFixed lGTRULESCORE;
   lGTRULESCORE.Init (sizeof(GTRULESCORE));
   GTRULESCORE rs;
   memset (&rs, 0, sizeof(rs));
   for (i = 0; i < 256 * 256; i++, pafCount++) {
      if (!*pafCount)
         continue;   // no score

      // else, add
      rs.fScore = *pafCount;
      rs.Rule.bFirst = (BYTE) (i >> 8);
      rs.Rule.bSecond = (BYTE) (i & 0xff);
      // NOTE: Not filling in convert-to at this point
      lGTRULESCORE.Add (&rs);
   } // i

   // sort this list
   PGTRULESCORE pRS = (PGTRULESCORE) lGTRULESCORE.Get(0);
   qsort (pRS, lGTRULESCORE.Num(), sizeof(GTRULESCORE), GTRULESCORESort);

   // adjust num down if more than total counds
   dwNum = min(dwNum, lGTRULESCORE.Num());
   plGTRULE->Required (plGTRULE->Num() + dwNum);
   for (i = 0; i < dwNum; i++, pRS++) {
      plGTRULE->Add (&pRS->Rule);

#ifdef _DEBUG
      WCHAR szFirst[64], szSecond[64], szTemp[128];
      if (pRS->Rule.bFirst < 16)
         wcscpy (szFirst, pLex->POSToString (POS_MAJOR_MAKE(pRS->Rule.bFirst)));
      else
         swprintf (szFirst, L"%d", (int)(DWORD)pRS->Rule.bFirst);

      if (pRS->Rule.bSecond < 16)
         wcscpy (szSecond, pLex->POSToString (POS_MAJOR_MAKE(pRS->Rule.bSecond)));
      else
         swprintf (szSecond, L"%d", (int)(DWORD)pRS->Rule.bSecond);
      
      swprintf (szTemp, L"\r\nGrammar rule: %s %s -> %d (score=%g)", szFirst, szSecond, (int)(DWORD)pRS->Rule.bConvert,
         pRS->fScore);

      OutputDebugStringW (szTemp);
#endif
   } // i

   return TRUE;
}


/*************************************************************************************
POSApplyRules - This applies a set of GTRULEs to a sentence, reducing the number
of words in the sentence.

inputs
   DWORD       *pdwNum - Originally filled with the number of words in the sentence.
                           This will be modified with the new number of words
                           after all the rules have been applied.
   BYTE        *pabPOS - *pdwNum word parts of speech. This will be modified in place
                           as rules are applied
   DWORD       dwNumRules - Number of rules
   PGTRULE     *paGTRULE - Array of rules. The first rule is executed first, and
                           so on down the line. If the rule succede, the bFirst,bSecond
                           pair is replaced with bConvert.
returns
   none
*/
void POSApplyRules (DWORD *pdwNum, BYTE *pabPOS, DWORD dwNumRules, PGTRULE paGTRULE)
{
   // loop through all the rules
   DWORD i;
   DWORD dwNum = *pdwNum;
   for (; dwNumRules; dwNumRules--, paGTRULE++) {
      BYTE bFirst = paGTRULE->bFirst;
      BYTE bSecond = paGTRULE->bSecond;
      PBYTE pab;
      for (i = 1, pab = pabPOS+1; i < dwNum; i++, pab++)
         if ((pab[0] == bSecond) && (pab[-1] == bFirst)) {
            // replace
            pab[-1] = paGTRULE->bConvert;
            memmove (pab, pab+1, dwNum-i-1);
            dwNum--;

            // NOTE: had i-- and pab--, but not point since wont affect the outcome
            //i--;  // to undo next increase
            //pab--;  // to undo next increase
         }
   } // dwNumRules

   *pdwNum = dwNum;
}



/*************************************************************************************
CGramTrain::CloneSmaller - Clones the training, but uses one with a limit of
so many megabytes, just to make processing faster.

inputs
   DWORD       dwLimit - Maximum number of megabytes
returns
   PCGramTrain - Cloned version, with the rules applied
*/
CGramTrain *CGramTrain::CloneSmaller (DWORD dwLimit)
{
   if (m_dwLastSentence == (DWORD)-1)
      return new CGramTrain;  // since was empty

   // see how far can go until exceed limit
   DWORD dwCur;
   PGTSENT pSent;
   for (dwCur = 0; dwCur < dwLimit; ) {
      pSent = (PGTSENT) ((PBYTE)m_memSentences.p + dwCur);
      if (!pSent->dwNext)
         break;

      dwCur = pSent->dwNext;
   } // dwCur

   // extra size
   pSent = (PGTSENT) ((PBYTE)m_memSentences.p + dwCur);
   DWORD dwNeed = ((pSent->dwWords + 3) & ~0x03) + sizeof(GTSENT) + dwCur;

   PCGramTrain pNew = new CGramTrain;
   if (!pNew)
      return NULL;

   // clone and clip off
   if (!pNew->m_memSentences.Required (dwNeed)) {
      delete pNew;
      return NULL;
   }
   memcpy (pNew->m_memSentences.p, m_memSentences.p, dwNeed);

   // trim it off there
   pNew->m_dwLastSentence = dwCur;
   pSent = (PGTSENT) ((PBYTE)pNew->m_memSentences.p + pNew->m_dwLastSentence);
   pSent->dwNext = 0;

   return pNew;
}


/*************************************************************************************
CGramTrain::CloneAndApply - Clones the current list of sentences and applies rules.

inputs
   DWORD       dwNumRules - Number of rules
   PGTRULE     paGTRULE - Array of rules. The first rule is executed first, and
                           so on down the line. If the rule succede, the bFirst,bSecond
                           pair is replaced with bConvert.
returns
   PCGramTrain - Cloned version, with the rules applied
*/
CGramTrain *CGramTrain::CloneAndApply (DWORD dwNumRules, PGTRULE paGTRULE)
{
   // create a clone
   PCGramTrain pNew = new CGramTrain;
   if (!pNew)
      return NULL;

   // figure out approx how much memory will need by looking at this one
   DWORD dwNeed = 0;
   PGTSENT pSent = NULL;
   if (m_dwLastSentence != (DWORD)-1) {
      pSent = (PGTSENT) ((PBYTE)m_memSentences.p + m_dwLastSentence);
      dwNeed = ((pSent->dwWords + 3) & ~0x3) + sizeof(GTSENT);
      dwNeed += m_dwLastSentence;
      pNew->m_memSentences.Required (dwNeed);
   }

   // loop through all the sentences
   pSent = (m_dwLastSentence != (DWORD)-1) ? (PGTSENT) m_memSentences.p : NULL;
   while (TRUE) {
      // add a new one cloning this
      DWORD dwNewSent = pNew->SentenceAdd (pSent->dwWords, (PBYTE)(pSent+1), pSent->fScore);
      if (dwNewSent == (DWORD)-1) {
         delete pNew;
         return NULL;
      }

      // apply the rules
      PGTSENT pSentNew = (PGTSENT) ((PBYTE)pNew->m_memSentences.p + dwNewSent);
      POSApplyRules (&pSentNew->dwWords, (PBYTE)(pSentNew+1), dwNumRules, paGTRULE);


      // go to the next one
      if (!pSent->dwNext)
         break;
      pSent = (PGTSENT) ((PBYTE)m_memSentences.p + pSent->dwNext);
   } // while pSent

   // done
   return pNew;
}


/*************************************************************************************
CGramTrain::Score - Determines the "efficieny" score of all the sentences. This
is the sum of # words in a sentece x sentence's score. The lower this value for
a collection of sentences, the better the rules are working at recuding the
complexity.

returns
   double - Total score
*/
double CGramTrain::Score (void)
{
   if (m_dwLastSentence == (DWORD)-1)
      return 0;

   double fTotal = 0;
   PGTSENT pSent = (PGTSENT) m_memSentences.p;
   while (TRUE) {
      fTotal += (double)pSent->dwWords * pSent->fScore;

      // go to the next one
      if (!pSent->dwNext)
         break;
      pSent = (PGTSENT) ((PBYTE)m_memSentences.p + pSent->dwNext);
   } // while pSent

   return fTotal;
}


/*************************************************************************************
CGramTrain::JitterRule - This does a random manipulation to one of the rules.

inputs
   DWORD       dwNum - Number of rules
   PGTRULE     paGTRULE - List of dwNumRules. This is jittered. The rule
                  numbers are reordered to start at 16 and continue on.
                  So, pConvert # = index + 16.
   DWORD       dwModAfter - Can modify rules whose number is >= this one.
                  This way, rules at the top wont be jittered.
   DWORD       dwIgnoreIfChangeAfter - If the rules that were changed were all >=
                  this value, then return FALSE. Otherwise return TRUE.
   DWORD       dwForceEffect - If this is 0, an effect will be randomly
                  chosen. Otherwise:
                        1 = completely randomize after dwMofAfter(never randomly selected)
                        2 = swap one rule with another (after dwModAfter)
                        3 = move one rule up/down (after dwMofAfter most common randomly selected)
returns
   BOOL - TRUE if changed any rules < dwIgnoreIfChangeafter. FALSE if dind't
*/
BOOL CGramTrain::JitterRule (DWORD dwNum, PGTRULE paGTRULE, DWORD dwModAfter,
                             DWORD dwIgnoreIfChangeAfter, DWORD dwForceEffect)
{
   if (!dwForceEffect) {
      DWORD dwRand = (DWORD)rand() % 100;
      if (dwRand < 10)
         dwForceEffect = 2;
      else
         dwForceEffect = 3;
   }

   // figure out how much have to work with
   DWORD i, dwTo, dwFrom;
   DWORD dwRange = (dwModAfter < dwNum) ? (dwNum - dwModAfter) : 0;
   if (dwRange <= 1) {
      // rewrite all the rules, just in case
      for (i = 0; i < dwNum; i++)
         paGTRULE[i].bConvert = (BYTE)(i+16);

      return FALSE;  // nothing to change
   }

   // do the effect
   GTRULE rTemp;
   BOOL fRet = FALSE;
   switch (dwForceEffect) {
   case 1:  // completely randmize, swapping
      for (i = 0; i < dwRange; i++) {
         dwTo = (DWORD)rand() % dwRange;

         rTemp = paGTRULE[dwModAfter + dwTo];
         paGTRULE[dwModAfter + dwTo] = paGTRULE[dwModAfter + i];
         paGTRULE[dwModAfter + i] = rTemp;
      } // i
      fRet = TRUE;
      break;

   case 2:  // swap and two
      dwFrom = ((DWORD)rand() % dwRange) + dwModAfter;
      dwTo = ((DWORD)rand() % dwRange) + dwModAfter;

      rTemp = paGTRULE[dwTo];
      paGTRULE[dwTo] = paGTRULE[dwFrom];
      paGTRULE[dwFrom] = rTemp;

      if ((dwFrom < dwIgnoreIfChangeAfter) || (dwTo < dwIgnoreIfChangeAfter))
         fRet = TRUE;
      break;

   default: // move up/down
      dwFrom = (DWORD)rand() % dwRange;
      if (!dwFrom)
         dwTo = 1;   // always move down
      else if (dwFrom+1 >= dwRange)
         dwTo = dwFrom-1;  // always move up
      else
         dwTo = (rand()%2) ? (dwFrom+1) : (dwFrom-1);
      dwFrom += dwModAfter;
      dwTo += dwModAfter;

      rTemp = paGTRULE[dwTo];
      paGTRULE[dwTo] = paGTRULE[dwFrom];
      paGTRULE[dwFrom] = rTemp;

      if ((dwFrom < dwIgnoreIfChangeAfter) || (dwTo < dwIgnoreIfChangeAfter))
         fRet = TRUE;
      break;

   } // switch

   // rewrite the rule numbers
   for (i = 0; i < dwNum; i++)
      paGTRULE[i].bConvert = (BYTE)(i+16);

   return fRet;
}



/*************************************************************************************
CGramTrain::DiscoverNewRules - This method takes the current training set
and tries to disover the best rules for minimizing its size.

inputs
   DWORD       dwNumRules - Number of existing rules.
   PGTRULE     paGTRULE - List of dwNumRules existing rules. These rules must
               already have been applied to this CGramTrain.
   DWORD       dwToDiscover - Number of rules to discover. dwToDicover + dwNumRules cant
               be >= 256.
   PCMLexicon  pLex - Lexicon. For test purposes.
   PCListFixed *plNewGTRULE - Filled in with the new GTRULEs to use
   PCProgressSocket pProgress - Progress bar, updated from 0 to 1 over the course of discovery.
returns
   PCGramTrain - Returns a new training set with the rules in plNewGTRULE already applied.
*/
CGramTrain *CGramTrain::DiscoverNewRules (DWORD dwNumRules, PGTRULE paGTRULE, DWORD dwToDiscover,
                              PCMLexicon pLex, PCListFixed plNewGTRULE, PCProgressSocket pProgress)
{
#define SURPLUSRULES       2     // create 2x as many rules as want to discover, so can discard some
#define TOTALRANDOMIZES    4     // number of total ranomizations to do
#define SMALLTESTSIZE      1000000  // max bytes in small size

#ifdef _DEBUG
#define NUMBERJITTERS      1000   // how many jitters to do for each
#else
#define NUMBERJITTERS      10   // how many jitters to do for each
#endif


   // determine the top N rules
   CListFixed lGTRULETop;
   if (!IdentifyRules (dwToDiscover*SURPLUSRULES, pLex, &lGTRULETop))
      return NULL;

   // if haven't discovered as many rules as expected, then reduce the number of
   // rules to look for
   dwToDiscover = min (dwToDiscover, lGTRULETop.Num() / SURPLUSRULES);

   // create a combined rule set, with existing and new rules
   CListFixed lCombined;
   DWORD i;
   lCombined.Init (sizeof(GTRULE), paGTRULE, dwNumRules);
   PGTRULE pGR = (PGTRULE)lGTRULETop.Get(0);
   lCombined.Required (lGTRULETop.Num());
   for (i = 0; i < lGTRULETop.Num(); i++, pGR++)
      lCombined.Add (pGR);

   DWORD dwModAfter = dwNumRules;
   DWORD dwIgnoreIfChangeAfter = dwModAfter + dwToDiscover;

   // create a small version of this
   PCGramTrain pSmall = CloneSmaller (SMALLTESTSIZE);
   if (!pSmall)
      return NULL;

   // loop through several passes where randomize everything and then jitter
   DWORD dwTotalRand;
   CListFixed lBest, lTry;
   PCGramTrain pBestOfAll = NULL, pBestSmall = NULL, pBest = NULL, pTry, pTryLarge;
   fp fBestOfAllScore, fBestSmallScore, fBestScore, fTryScore, fTryLargeScore;
   for (dwTotalRand = 0; dwTotalRand < TOTALRANDOMIZES; dwTotalRand++) {
      if (pProgress)
         pProgress->Push ((fp)dwTotalRand / (fp)TOTALRANDOMIZES, (fp)(dwTotalRand+1) / (fp)TOTALRANDOMIZES);

      // start out with a copy of the combined and randomize the bottom bit
      // however, if this is the last time arond, and ther's a best, then use that
      if ((dwTotalRand+1 == TOTALRANDOMIZES) && pBestOfAll) {
         lBest.Init (sizeof(GTRULE), plNewGTRULE->Get(0), plNewGTRULE->Num());
      }
      else {
         lBest.Init (sizeof(GTRULE), lCombined.Get(0), lCombined.Num());
         JitterRule (lBest.Num(), (PGTRULE)lBest.Get(0), dwModAfter, dwIgnoreIfChangeAfter,
            dwTotalRand ? 1 : 3);
            // BUGFIX - For one attempt, start with optimum sort and see if get lower from there
      }
      pBest = CloneAndApply (dwToDiscover, (PGTRULE)lBest.Get(dwModAfter));
         // NOTE: only applying rules AFTER dwModAfter, since the ones before should
         // already be applied
      if (!pBest) {
         if (pProgress)
            pProgress->Pop();
         break;   // error
      }
      fBestScore = pBest->Score ();

      // make a small version of best
      pBestSmall = pSmall->CloneAndApply (dwToDiscover, (PGTRULE)lBest.Get(dwModAfter));
      if (!pBestSmall) {
         delete pBest;
         if (pProgress)
            pProgress->Pop();
         break;   // error
      }
      fBestSmallScore = pBestSmall->Score ();


      // loop through all the jitters, trying to see if makes it better than the best
      if (lCombined.Num() > dwModAfter+1) for (i = 0; i < NUMBERJITTERS; i++) {
         // update progress
         if (pProgress && !(i%16))
            pProgress->Update ((fp)i / (fp)NUMBERJITTERS);

         // copy the best and apply a jitter. Repeat the jitter until
         // something has changed
         lTry.Init (sizeof(GTRULE), lBest.Get(0), lBest.Num());
         do {
            while (!JitterRule (lTry.Num(), (PGTRULE)lTry.Get(0), dwModAfter, dwIgnoreIfChangeAfter, 0));
         } while (rand() % 2);   // so may jitter more than one meaningful item

         // see how well this does
         pTry = pSmall->CloneAndApply (dwToDiscover, (PGTRULE)lTry.Get(dwModAfter));
         if (!pTry)
            continue;   // ignore this. shouldnt happen
         fTryScore = pTry->Score();

#if 0 // def _DEBUG
         WCHAR szTemp[128];
         swprintf (szTemp, L"\r\nBest SMALL = %g, try = %g", fBestSmallScore, fTryScore);
         OutputDebugStringW (szTemp);
#endif
         // keep this
         if (fTryScore < fBestSmallScore) {
            // try a larger version
            pTryLarge = CloneAndApply (dwToDiscover, (PGTRULE)lTry.Get(dwModAfter));
            if (!pTryLarge) {
               delete pTry;
               continue;   // ignore this. shouldnt happen
            }
            fTryLargeScore = pTryLarge->Score();

#ifdef _DEBUG
         WCHAR szTemp[128];
         swprintf (szTemp, L"\r\nBest LARGE = %g, try = %g", fBestScore, fTryLargeScore);
         OutputDebugStringW (szTemp);
#endif
         // keep this
            if (fTryLargeScore < fBestScore) {
               delete pBest;
               delete pBestSmall;
               pBest = pTryLarge;
               fBestScore = fTryLargeScore;
               pBestSmall = pTry;
               fBestSmallScore = fTryScore;
               lBest.Init (sizeof(GTRULE), lTry.Get(0), lTry.Num());
            }
            else {
               delete pTry;
               delete pTryLarge;
            }
         }
         else
            // discard this
            delete pTry;

      } // i

      // if best is better than total best, then keep best
      if (!pBestOfAll || (fBestScore < fBestOfAllScore)) {
         if (pBestOfAll)
            delete pBestOfAll;
         pBestOfAll = pBest;
         pBest = NULL;
         fBestOfAllScore = fBestScore;
         plNewGTRULE->Init (sizeof(GTRULE), lBest.Get(0), lBest.Num());
      }

      // clean up
      if (pBest)
         delete pBest;
      if (pBestSmall)
         delete pBestSmall;
      if (pProgress)
         pProgress->Pop();
   } // dwTotalRan

   // delete end rules
   if (dwIgnoreIfChangeAfter < plNewGTRULE->Num())
      plNewGTRULE->Truncate (dwIgnoreIfChangeAfter);

#ifdef _DEBUG
   // output all the rules
   PGTRULE pRS = (PGTRULE)plNewGTRULE->Get(0);
   DWORD dwNum = plNewGTRULE->Num();
   OutputDebugString ("\r\n");
   for (i = 0; i < dwNum; i++, pRS++) {
      WCHAR szFirst[64], szSecond[64], szTemp[128];
      if (pRS->bFirst < 16)
         wcscpy (szFirst, pLex->POSToString (POS_MAJOR_MAKE(pRS->bFirst)));
      else
         swprintf (szFirst, L"%d", (int)(DWORD)pRS->bFirst);

      if (pRS->bSecond < 16)
         wcscpy (szSecond, pLex->POSToString (POS_MAJOR_MAKE(pRS->bSecond)));
      else
         swprintf (szSecond, L"%d", (int)(DWORD)pRS->bSecond);
      
      swprintf (szTemp, L"\r\nGrammar rule: %s %s -> %d", szFirst, szSecond, (int)(DWORD)pRS->bConvert);

      OutputDebugStringW (szTemp);
   } // i
#endif

   // return total best
   delete pSmall;
   return pBestOfAll;
}



/*************************************************************************************
CGramTrain::DiscoverNewRules - This method takes the current training set
and tries to disover the best rules for minimizing its size.

inputs
   DWORD       dwBlockSize - Look for rules in this block size, usually 16.
   DWORD       dwBlocks - Number of blocks to use. dwBlockSize * dwBlocks + 16 < 256
   PCMLexicon  pLex - Lexicon. For test purposes.
   PCListFixed *plNewGTRULE - Filled in with the new GTRULEs to use
   PCProgressSocket pProgress - Progress bar, updated from 0 to 1 over the course of discovery.
returns
   PCGramTrain - Returns a new training set with the rules in plNewGTRULE already applied.
*/
CGramTrain *CGramTrain::DiscoverNewRules (DWORD dwBlockSize, DWORD dwBlocks,
                              PCMLexicon pLex, PCListFixed plNewGTRULE, PCProgressSocket pProgress)
{
   // initialize existing rules to nothing
   plNewGTRULE->Init (sizeof(GTRULE), NULL, 0);

   // figure out the current best
   PCGramTrain pCur = this;
   PCGramTrain pRet;
   CListFixed lRuleCopy;

   // loop through all the blocks
   DWORD i;
   for (i = 0; i < dwBlocks; i++) {
      if (pProgress)
         pProgress->Push ((fp)i / (fp)dwBlocks, (fp)(i+1) / (fp)dwBlocks);

      // make a copy of the existing rules
      lRuleCopy.Init (sizeof(GTRULE), plNewGTRULE->Get(0), plNewGTRULE->Num());

      // minimize
      pRet = pCur->DiscoverNewRules (lRuleCopy.Num(), (PGTRULE)lRuleCopy.Get(0),
         dwBlockSize, pLex, plNewGTRULE, pProgress);
      if (pCur != this)
         delete pCur;
      pCur = pRet;

      if (pProgress)
         pProgress->Pop ();
      if (!pCur)
         break;   // error

#ifdef _DEBUG
   WCHAR szTemp[128];
   swprintf (szTemp, L"\r\nCur score transition:%g to %g", Score(), pCur->Score());
   OutputDebugStringW (szTemp);
#endif
   } // i

#ifdef _DEBUG
   WCHAR szTemp[128];
   swprintf (szTemp, L"\r\nScore transition:%g to %g", Score(), pCur->Score());
   OutputDebugStringW (szTemp);
#endif

   return (pCur != this) ? pCur : NULL;
}


/*************************************************************************************
CNGramDatabase::Constructor
*/
CNGramDatabase::CNGramDatabase (void)
{
   Init (2, 2, 0);   // hack init
}

/*************************************************************************************
CNGramDatabase::Constructor
*/
CNGramDatabase::~CNGramDatabase (void)
{
   // do nothinf for now
}


/*************************************************************************************
CNGramDatabase::Init - Call this to initialize the values.

inputs
   DWORD          dwValues - Number of values that each enty can store, from 0..dwValues-1.
                     This has to be 255 or less (not 256)
   DWORD          dwHistory - Number of dimensions for history. Max MAXNGRAMHISTORY
   BYTE           bEOD - Data value used when beyoond the edge of a stream.
*/
BOOL CNGramDatabase::Init (DWORD dwValues, DWORD dwHistory, BYTE bEOD)
{
   m_dwValues = dwValues;
   m_dwHistory = dwHistory;
   m_bEOD = bEOD;

   m_dwValuesPlusOne = m_dwValues+1;
   DWORD i;
   for (i = 0; i <= m_dwHistory; i++)
      m_adwHistoryScale[i] = (i ? (m_adwHistoryScale[i-1]*m_dwValuesPlusOne) : 1);
   m_memTraining.m_dwCurPosn = 0;   // so know its empty

   // allocate enough for NGram and backoff
   DWORD dwNeed  = m_adwHistoryScale[m_dwHistory] /*NGram*/ + m_adwHistoryScale[m_dwHistory-1] /*backoff*/;
   if (!m_memNGram.Required(dwNeed))
      return FALSE;
   memset (m_memNGram.p, 0, dwNeed);
   m_memNGram.m_dwCurPosn = dwNeed;
   m_pabNGram = (PBYTE) m_memNGram.p;
   m_pabBackoff = m_pabNGram + m_adwHistoryScale[m_dwHistory];
   m_fTrainingCount = 0;

   return TRUE;
}


/*************************************************************************************
CNGramDatabase::Clone - Clones the database
*/
CNGramDatabase * CNGramDatabase::Clone (void)
{
   PCNGramDatabase pNew = new CNGramDatabase;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/*************************************************************************************
CNGramDatabase::CloneTo - Clones the database
*/
BOOL CNGramDatabase::CloneTo (CNGramDatabase *pTo)
{
   if (!pTo->Init (m_dwValues, m_dwHistory, m_bEOD))
      return NULL;

   memcpy (pTo->m_memNGram.p, m_memNGram.p, m_memNGram.m_dwCurPosn);
   pTo->m_fTrainingCount = m_fTrainingCount;

   return TRUE;
}


/*************************************************************************************
CNGramDatabase::IndexNGram - Returns the index into the Ngram.

inputs
   PBYTE          pabHistory - Array of m_dwHistory bytes, [0] being the oldest in time,
                     [m_dwHistory-1] being the new hypothesis
returns
   DWORD - index
*/
DWORD CNGramDatabase::IndexNGram (PBYTE pabHistory)
{
   DWORD i, dwRet = 0;
   for (i = 0; i < m_dwHistory; i++)
      dwRet += m_adwHistoryScale[i] * (DWORD) pabHistory[i];
   return dwRet;
}


/*************************************************************************************
CNGramDatabase::IndexBackoff - Returns the index into the backoff.

inputs
   PBYTE          pabHistory - Array of m_dwHistory bytes, [0] being the oldest in time,
                     [m_dwHistory-1] being the new hypothesis (which is ignored)
returns
   DWORD - index
*/
DWORD CNGramDatabase::IndexBackoff (PBYTE pabHistory)
{
   DWORD i, dwRet = 0;
   for (i = 0; i < m_dwHistory-1; i++)
      dwRet += m_adwHistoryScale[i] * (DWORD) pabHistory[i];
   return dwRet;
}



/*************************************************************************************
CNGramDatabase::TrainFixedBlock - This trains an NGram for a fixed block of
elements.

inputs
   PBYTE          pabHistory - Array of m_dwHistory bytes. Values are 0..m_dwValues-1 for
                  the real data.
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainFixedBlock (PBYTE pabHistory, double fCount)
{
   // make sure have allocated
   if (!m_memTraining.m_dwCurPosn) {
      DWORD dwNeed = m_adwHistoryScale[m_dwHistory] * sizeof(double);
      if (!m_memTraining.Required (dwNeed))
         return;  // error
      memset (m_memTraining.p, 0, dwNeed);
      m_memTraining.m_dwCurPosn = dwNeed;
      m_fTrainingCount = 0;
   }

   double *paf = (double*)m_memTraining.p;

   // copy the history to internal memory so can backoff
   BYTE abTemp[MAXNGRAMHISTORY];
   memcpy (abTemp, pabHistory, sizeof(BYTE)*m_dwHistory);
   DWORD i, dwIndex;
   for (i = 0; i < m_dwHistory; i++) {
      dwIndex = IndexNGram (abTemp);
      paf[dwIndex] += fCount;
      abTemp[i] = m_dwValues; // to set to backoff value
   } // i

   m_fTrainingCount += fCount;
}




/*************************************************************************************
CNGramDatabase::TrainStreamIndex - This accepts an array of historical values, and
an index into them. It trains up to the index.

inputs
   DWORD          dwNum - Number of elements in the stream.
   PBYTE          pabStream - Stream. dwNum elements. Values from 0..m_dwValues-1
   int            iIndex - Current index into the stream to train on. This can be
                  before 0 or after dwNum-1. The m_dwHistory slots including and
                  before iIndex will be trained.
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainStreamIndex (DWORD dwNum, PBYTE pabStream, int iIndex, double fCount)
{
   BYTE abTemp[MAXNGRAMHISTORY];
   DWORD i;
   for (i = 0, iIndex -= (m_dwHistory-1); i < m_dwHistory; i++, iIndex++) {
      if ((iIndex < 0) || (iIndex >= (int)dwNum))
         abTemp[i] = m_bEOD;  // beyond end, so use end of data
      else
         abTemp[i] = pabStream[iIndex];
   } // i

   TrainFixedBlock (abTemp, fCount);
}




/*************************************************************************************
CNGramDatabase::TrainStream - Trains all of the elements in the stream.

inputs
   DWORD          dwNum - Number of elements in the stream.
   PBYTE          pabStream - Stream. dwNum elements. Values from 0..m_dwValues-1
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainStream (DWORD dwNum, PBYTE pabStream, double fCount)
{
   // end up training slightly before and after
   DWORD i;
   for (i = 0; i < dwNum + m_dwHistory-1; i++)
      TrainStreamIndex (dwNum, pabStream, (int)i, fCount);
}




/*************************************************************************************
CNGramDatabase::TrainFixedBlockBitField - This trains an N-gram where the value
isn't necessarily determined. (For example: A word might be a noun or verb.)

inputs
   WORD           *pawHistory - Array of m_dwStream values. Each bit in the word
                  represents a possible value. Thus 0x0c (with bits 0x08 and 0x04) means
                  that the value could be 3 (1 << 3 == 0x08) or 2 (1 << 2 == 0x08).
                  This also means that values higher than 15 cannot be trained...
                  which is fine, since it's used for training parts-of-speech.
                  At least 1 bit must be set.
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainFixedBlockBitField (WORD *pawHistory, double fCount)
{
   // loop through each of the history elements
   DWORD i, j, dwCount;
   WORD wTemp;
   for (i = 0; i < m_dwHistory; i++) {
      // count the number of bits set
      wTemp = pawHistory[i];
      for (dwCount = 0; wTemp; wTemp = wTemp & (wTemp-1))
         dwCount++;

      // dwCount should at least be 1
      _ASSERTE (dwCount);

      // if there is more than one bit set, then recurse, doing only 1 bit
      if (dwCount > 1) {
         wTemp = pawHistory[i];
         fCount /= (double)dwCount;

         for (j = 0; j < 16; j++) {
            if (!((WORD)(1 << j) & wTemp))
               continue; // not set

            pawHistory[i] = (WORD)(1 << j);
            TrainFixedBlockBitField (pawHistory, fCount);

            dwCount--;
            if (!dwCount)
               break;   // all done
         } // j
         pawHistory[i] = wTemp;  // restore
         return;
      }
   } // i

   // if got here, then all have exactly one item, so
   // go through and figure out what the one item is
   BYTE abTemp[MAXNGRAMHISTORY];
   for (i = 0; i < m_dwHistory; i++) {
      wTemp = pawHistory[i];
      for (j = 0; j < 16; j++)
         if (wTemp & (WORD)(1 << j))
            break;
      abTemp[i] = (BYTE)j;
   } // i

   // train this
   TrainFixedBlock (abTemp, fCount);
}



/*************************************************************************************
CNGramDatabase::TrainStreamIndexBitField - This accepts an array of historical values, and
an index into them. It trains up to the index.

inputs
   DWORD          dwNum - Number of elements in the stream.
   WORD           *pawStream - Stream. dwNum elements. These are bit fields
                  like those used in TrainFixedBlockBitField().
   int            iIndex - Current index into the stream to train on. This can be
                  before 0 or after dwNum-1. The m_dwHistory slots including and
                  before iIndex will be trained.
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainStreamIndexBitField (DWORD dwNum, WORD *pawStream, int iIndex, double fCount)
{
   WORD awTemp[MAXNGRAMHISTORY];
   DWORD i;
   for (i = 0, iIndex -= (m_dwHistory-1); i < m_dwHistory; i++, iIndex++) {
      if ((iIndex < 0) || (iIndex >= (int)dwNum))
         awTemp[i] = (WORD) (1 << m_bEOD);  // beyond end, so use end of data
      else
         awTemp[i] = pawStream[iIndex];
   } // i

   TrainFixedBlockBitField (awTemp, fCount);
}





/*************************************************************************************
CNGramDatabase::TrainStreamBitField - Trains all of the elements in the stream.

inputs
   DWORD          dwNum - Number of elements in the stream.
   WORD           *pawStream - Stream. dwNum elements. These are bit fields
                  like those used in TrainFixedBlockBitField().
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainStreamBitField (DWORD dwNum, WORD *pawStream, double fCount)
{
   // end up training slightly before and after
   DWORD i;
   for (i = 0; i < dwNum + m_dwHistory-1; i++)
      TrainStreamIndexBitField (dwNum, pawStream, (int)i, fCount);
}


/*************************************************************************************
CNGramDatabase::TrainStreamLEXPOSGUESS - Trains on an array of LEXPOSGUESS.
The LEXPOSGUESS.wPOSBitField MUST be filled in.

inputs
   DWORD          dwNum - Number of elements.
   PLEXPOSGUESS   paStream - Strem of LEXPOSGUESS. Only the wPOSBitField is used
   double         fCoun
   double         fCount - Count to use for training. Usually this is 1.0, but not always
returns
   none
*/
void CNGramDatabase::TrainStreamLEXPOSGUESS (DWORD dwNum, PLEXPOSGUESS paStream, double fCount)
{
   CListFixed lStream;
   lStream.Init (sizeof(WORD));
   DWORD i;
   lStream.Required (dwNum);
   for (i = 0; i < dwNum; i++)
      lStream.Add (&paStream[i].wPOSBitField);

   TrainStreamBitField (lStream.Num(), (WORD*)lStream.Get(0), fCount);
}


/*************************************************************************************
CNGramDatabase::TrainingApply - Copies all of the training into the main database.
You must call this after doing training. This erases all data that currently
exists in the main database.
*/
void CNGramDatabase::TrainingApply (void)
{
   // wipe out the existing database
   memset (m_memNGram.p, 0, m_memNGram.m_dwCurPosn);

   // loop through all levels of backoff and train them
   DWORD i, j;
   BYTE abTemp[MAXNGRAMHISTORY];
   for (i = 0; i < m_dwHistory; i++) {
      // clear
      memset (abTemp, 0, sizeof(abTemp));

      // set the backoff bits
      for (j = 0; j < i; j++)
         abTemp[j] = (BYTE)m_dwValues;

      // repeat, increasing the counter to go through all possibilities
      while (TRUE) {
         // train this level of backoff
         TrainingApplySpecific (abTemp);

         // increase a value
         DWORD dwIncIndex = i;
         while (dwIncIndex < m_dwHistory-1) {
            // increase the counter one
            abTemp[dwIncIndex]++;

            // if still within range then do nothing
            if (abTemp[dwIncIndex] < (BYTE)m_dwValues)
               break;

            // if overflow, push counter to the next one
            abTemp[dwIncIndex] = 0;
            dwIncIndex++;
         } // while increasing ok

         // if just rolled over then done
         if (dwIncIndex >= m_dwHistory-1)
            break;   // all done
      } // while TRUE
   } // i
}


/*************************************************************************************
NGramProbToByte - Converts a ngram probability to a byte.

inputs
   double         fProb - Probability. This can be 0.
returns
   BYTE - Value
*/
#define NGRAMPROBRESOLUTION         10000.0     // how small a probability can handle

__inline BYTE NGramProbToByte (double fProb)
{
   if (fProb <= 0)
      return 0;   // backoff

   fProb = (log(fProb) / log(NGRAMPROBRESOLUTION) + 1) * 256.0;
   if (fProb >= 255)
      return 255;
   else if (fProb <= 1)
      return 1;   // dont allow to go to 0
   else
      return (BYTE)fProb;
}


/*************************************************************************************
NGramByteToProb - Converts a byte-value score in the NGram table to
a probability.

inputs
   BYTE           bValue - Value. 255 = 1.0 prob, 1 = 1/NGRAMPROBRESOLUTION prob, 0 = 0 prob
returns
   double - probability
*/
__inline double NGramByteToProb (BYTE bValue)
{
   if (!bValue)
      return 0;

   double fProb = ((double)bValue / 256.0) - 1.0;
   fProb *= log(NGRAMPROBRESOLUTION);
   fProb = exp (fProb);

   return fProb;
}


/*************************************************************************************
CNGramDatabase::TrainingApplySpecific - Internal method that applies the training
for a specific scenario, at a give level of history.

This fills in the byte field for the N-gram as well as the backoff zero's information.

inputs
   PBYTE       pabHistory - Filled with Undefined (m_dwValues) part way, and a
               then valid values after that. This can be modified in place AT [m_dwHistory-1] only.
returns
   none
*/
void CNGramDatabase::TrainingApplySpecific (PBYTE pabHistory)
{
#if 0 // def _DEBUG
   double fTemp;
   fTemp = NGramByteToProb (NGramProbToByte (1.0));
   fTemp = NGramByteToProb (NGramProbToByte (0.5));
   fTemp = NGramByteToProb (NGramProbToByte (0.1));
   fTemp = NGramByteToProb (NGramProbToByte (0.001));
   fTemp = NGramByteToProb (NGramProbToByte (0.00001));
   fTemp = NGramByteToProb (NGramProbToByte (0.00000001));
#endif

   // if no training then done
   if (!m_memTraining.m_dwCurPosn)
      return;

   // first, need to figure out starting index and index delta
   pabHistory[m_dwHistory-1] = 0;
   DWORD dwStartIndex = IndexNGram (pabHistory);
   pabHistory[m_dwHistory-1] = 1;
   DWORD dwIndexDelta = IndexNGram (pabHistory) - dwStartIndex;

   // next, loop through all possible outcomes of the history and...
   // if the number of occurances < MINNGRAMCOUNT then set to 0, but pretend its MINNGRAMCOUNT
   // remember number of zeros to calulcate the zeros
   // sum up all values (including 0's treated as MINNGRAMCOUNT) to figure out the zeros
#define MINNGRAMCOUNT         3     // must have 3 occurances to count
   double *paf = (double*)m_memTraining.p;
   double fSum = 0, fZeros = 0;
   DWORD i, dwIndex;
   for (i = 0, dwIndex = dwStartIndex; i < m_dwValues; i++, dwIndex += dwIndexDelta) {
      double fValue = paf[dwIndex];
      if (!fValue) {
         // this is a zero
         fValue = MINNGRAMCOUNT;
         fZeros += MINNGRAMCOUNT;
      }
      else if (fValue <= MINNGRAMCOUNT) {
         // this is a zero
         fValue = MINNGRAMCOUNT;
         fZeros += MINNGRAMCOUNT;
         paf[dwIndex] = 0; // for later
      }
#ifdef _DEBUG
      else
         fZeros += 0;
#endif
      fSum += fValue;
   } // i
   fp fSumInv = 1.0 / fSum;   // wont be 0 since always have some backoff

   // go through convert all the values to bytes to store away, first
   // normalizing all the values so they sum to 1.0, (including the backoff)
   for (i = 0, dwIndex = dwStartIndex; i < m_dwValues; i++, dwIndex += dwIndexDelta) {
      double fValue = paf[dwIndex];
      
      if (fValue)
         m_pabNGram[dwIndex] = NGramProbToByte(fValue * fSumInv);
      else
         m_pabNGram[dwIndex] = 0;
   }

   // fill in the backoff
   if (fZeros != fSum)
      m_pabBackoff[IndexBackoff(pabHistory)] = NGramProbToByte(fZeros * fSumInv);
   else
      m_pabBackoff[IndexBackoff(pabHistory)] = 255;
}


static PWSTR gpszNGramDatabase = L"NGramDatabase";
static PWSTR gpszHistory = L"History";
static PWSTR gpszValues = L"Values";
static PWSTR gpszEOD = L"EOD";
static PWSTR gpszTrainingCount = L"TrainingCount";

/*************************************************************************************
CNGramDatabase::MMLTo - Standard API
*/
PCMMLNode2 CNGramDatabase::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszNGramDatabase);

   // write out info
   MMLValueSet (pNode, gpszHistory, (int)m_dwHistory);
   MMLValueSet (pNode, gpszValues, (int)m_dwValues);
   MMLValueSet (pNode, gpszEOD, (int)(DWORD)m_bEOD);
   MMLValueSet (pNode, gpszTrainingCount, (fp) m_fTrainingCount);

   CMem memRLE;
   if (m_memNGram.m_dwCurPosn) {
      // convert to RLE
      memRLE.m_dwCurPosn = 0;
      if (RLEEncode ((PBYTE)m_memNGram.p, m_memNGram.m_dwCurPosn, 1, &memRLE))
         return FALSE;
      size_t dwRLESize = memRLE.m_dwCurPosn;
      if (!dwRLESize)
         return FALSE;

      // write to MML
      MMLValueSet (pNode, gpszNGram, (PBYTE)memRLE.p, memRLE.m_dwCurPosn);
   }

   return pNode;
}



/*************************************************************************************
CNGramDatabase::MMLFrom - Standard API
*/
BOOL CNGramDatabase::MMLFrom (PCMMLNode2 pNode)
{
   DWORD dwHistory, dwValues;
   BYTE bEOD;

   dwHistory = (DWORD) MMLValueGetInt (pNode, gpszHistory, (int)m_dwHistory);
   dwValues = (DWORD) MMLValueGetInt (pNode, gpszValues, (int)m_dwValues);
   bEOD = (BYTE) MMLValueGetInt (pNode, gpszEOD, (int)(DWORD)m_bEOD);

   if (!Init (dwValues, dwHistory, bEOD))
      return FALSE;


   m_fTrainingCount = MMLValueGetDouble (pNode, gpszTrainingCount, 0);



   CMem memRLE;
   MMLValueGetBinary (pNode, gpszNGram, &memRLE);
   //psz = MMLValueGet (pNode, gpszNGram);
   if (memRLE.m_dwCurPosn /*psz*/) {
      //if (!memRLE.Required (wcslen(psz)/2))
      //   return FALSE;
      PBYTE pb = (PBYTE)memRLE.p;
      size_t dwSize = memRLE.m_dwCurPosn;
      //DWORD dwSize = MMLBinaryFromString (psz, pb, memRLE.m_dwAllocated);

      // RLE decode
      size_t dwExpect = m_memNGram.m_dwCurPosn;
      m_memNGram.m_dwCurPosn = 0;
      size_t dwUsed;
      if (RLEDecode ((PBYTE)memRLE.p, dwSize, 1, &m_memNGram, &dwUsed))
         return FALSE;
      dwSize = m_memNGram.m_dwCurPosn;
      if (!dwSize)
         return FALSE;

      // make sure size is correct
      if (dwExpect != m_memNGram.m_dwCurPosn)
         m_memNGram.m_dwCurPosn = dwExpect;   // invalid, so reset to what originally was
   }

   return TRUE;
}


/*************************************************************************************
CNGramDatabase::ProbFixedBlock - Given a fixed block of parts of speech (values
0..m_dwValues-1), this determines its probability.

inputs
   PBYTE       pabHistory - History. [0] = oldest, [1] = recent addition
returns
   double - Probability
*/
double CNGramDatabase::ProbFixedBlock (PBYTE pabHistory)
{
   // copy over
   BYTE abTemp[MAXNGRAMHISTORY];
   memcpy (abTemp, pabHistory, m_dwHistory);

   // loop over all backoffs
   DWORD i;
   double fTotal = 1.0;
   for (i = 0; i < m_dwHistory; i++) {
      // get this score
      BYTE bVal = m_pabNGram[IndexNGram(abTemp)];

      // if found a value then done
      if (bVal) {
         fTotal *= NGramByteToProb(bVal);
         break;
      }

      // get the backoff probability and multiply that in
      // this accounts for residual
      bVal = m_pabBackoff[IndexBackoff(abTemp)];
      fTotal *= NGramByteToProb (bVal);

      // backoff for the next one
      abTemp[i] = (BYTE)m_dwValues;
   } // i

   // in order to keep comparisons normalized as much
   // as possible, take Nth root of this
   return pow (fTotal, 1.0 / (double)m_dwHistory);
}


/*************************************************************************************
CNGramDatabase::ProbStreamIndex - This accepts an array of historical values, and
an index into them. It returns a probability given the index.

inputs
   DWORD          dwNum - Number of elements in the stream.
   PBYTE          pabStream - Stream. dwNum elements. Values from 0..m_dwValues-1
   int            iIndex - Current index into the stream to train on. This can be
                  before 0 or after dwNum-1. The m_dwHistory slots including and
                  before iIndex will be trained.
returns
   double - probability
*/
double CNGramDatabase::ProbStreamIndex (DWORD dwNum, PBYTE pabStream, int iIndex)
{
   BYTE abTemp[MAXNGRAMHISTORY];
   DWORD i;
   for (i = 0, iIndex -= (m_dwHistory-1); i < m_dwHistory; i++, iIndex++) {
      if ((iIndex < 0) || (iIndex >= (int)dwNum))
         abTemp[i] = m_bEOD;  // beyond end, so use end of data
      else
         abTemp[i] = pabStream[iIndex];
   } // i

   return ProbFixedBlock (abTemp);
}




/*************************************************************************************
CNGramDatabase::ProbStream - Returns the probability of a stream.

inputs
   DWORD          dwNum - Number of elements in the stream.
   PBYTE          pabStream - Stream. dwNum elements. Values from 0..m_dwValues-1
returns
   double - Probability
*/
double CNGramDatabase::ProbStream (DWORD dwNum, PBYTE pabStream)
{
   // end up training slightly before and after
   DWORD i;
   double fProb = 1.0;
   for (i = 0; i < dwNum + m_dwHistory-1; i++)
      fProb *= ProbStreamIndex (dwNum, pabStream, (int)i);

   // in order to keep probabilities as normalizes as possible, take power of this
   return pow (fProb, 1.0 / (fp)i);
}



/*************************************************************************************
CNGramDatabase::HypExpand - This internal function expands the list of hypothesis into the new
list.

inputs
   PCListFixed       plFrom - Where the hypothesis came from, list of PCLexPOSHyp
   PCListFixed       plTo - Initialized to a list of PCLexPOSHyp
   WORD              *pawPOSBitField - An array of bit fields, like TrainFixedBlockBitField
   DWORD             dwNum - Number of words
   DWORD             dwCur - Current word index (in paLPG) trying to fill in POS for. Will fill
                        in words from dwNum to the end of the sentence, for the best result.
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CNGramDatabase::HypExpand (PCListFixed plFrom, PCListFixed plTo,
                              WORD *pawPOSBitField, DWORD dwNum, DWORD dwCur)
{
   // init
   plTo->Init (sizeof (PCLexPOSHyp));
   PCLexPOSHyp *pphFrom = (PCLexPOSHyp*) plFrom->Get(0);

   // figure out the part of speech available
   WORD wBits = (dwCur < dwNum) ? pawPOSBitField[dwCur] : (WORD)(1 << m_bEOD);

   DWORD i;
   BYTE abPOSAvail[16];
   DWORD dwNumPOSAvail = 0;
   for (i = 0; i < 16; i++) {
      // make sure have bit set
      if (!(wBits & (WORD)(1 << i)))
         continue;

      abPOSAvail[dwNumPOSAvail] = (BYTE)i;
      dwNumPOSAvail++;
   } // i
   if (!dwNumPOSAvail)
      return FALSE;  // shouldnt happen

   // loop through all hypothesis
   DWORD dwFrom;
   for (dwFrom = 0; dwFrom < plFrom->Num(); dwFrom++) {
      PCLexPOSHyp pFrom = pphFrom[dwFrom];
      BYTE *pabCur = (BYTE*) pFrom->m_lPOS.Get(0);
      DWORD dwCurNum = pFrom->m_lPOS.Num();

      // fill in the parts of speech, except for last in entry
      BYTE abPOS[MAXNGRAMHISTORY];
      int iOffset = (int)dwCurNum - (int)(m_dwHistory-1);
      for (i = 0; i < m_dwHistory-1; i++, iOffset++) {
         if (iOffset < 0)
            abPOS[i] = m_bEOD;
         else
            abPOS[i] = pabCur[iOffset];
      } // i

      // loop through all the POS available to the word
      plTo->Required (plTo->Num() + dwNumPOSAvail);
      for (i = 0; i < dwNumPOSAvail; i++) {
         // clone this object
         PCLexPOSHyp pNew = pFrom->Clone();
         if (!pNew)
            continue;
         plTo->Add (&pNew);

         // add the new POS
         pNew->m_lPOS.Add (&abPOSAvail[i]);

         // fill in the last POS into list
         abPOS[m_dwHistory-1] = abPOSAvail[i];

         pNew->m_fScore *= ProbFixedBlock (abPOS);
      } // i
   } // dwFrom

   return TRUE;
}


/*************************************************************************************
CNGramDatabase::HypCleanup - This eliminates duplicates and low scores from the list

It does this by:
- Eliminating any hypothesis that end up being the same, taking the highest score
- Keeping only the top 100 hypothesis

inputs
   PCListFixed       plHyp - List of PCLexPOSHyp objects
returns
   BOOL - TRUE if success
*/

BOOL CNGramDatabase::HypCleanup (PCListFixed plHyp)
{
   // sort by the last N elements in the hypothesis
   qsort (plHyp->Get(0), plHyp->Num(), sizeof(PCLexPOSHyp), PCLexPOSHypCompare1);

   // eliminate duplicates
   if (!plHyp->Num())
      return TRUE;   // nothing to elim
   PCLexPOSHyp *pph = (PCLexPOSHyp*) plHyp->Get(0);
   DWORD dwNum = pph[0]->m_lPOS.Num(); // all will have the same number
   DWORD dwCompare = dwNum; // min(dwNum, m_dwHistory);
      // BUGFIX - Was eliminaming anything that hadn't changed recently, but
      // this breaks the second pass, so have to keep
   DWORD i;
   for (i = 1; i < plHyp->Num(); i++) {
      if (memcmp ( (PBYTE)pph[i-1]->m_lPOS.Get(dwNum-dwCompare),
         (PBYTE)pph[i]->m_lPOS.Get(dwNum-dwCompare), dwCompare))
         continue;   // they're different

      // else, they're the same, so eliminate the one at i (since it will
      // have a lower score)
      delete pph[i];
      plHyp->Remove (i);
      i--;  // to repeat
      pph = (PCLexPOSHyp*) plHyp->Get(0); // just in case realloced
   } // i

   // sort by score
   qsort (plHyp->Get(0), plHyp->Num(), sizeof(PCLexPOSHyp), PCLexPOSHypCompare2);

   // while too many hypothesis, remove
   while (plHyp->Num() > 100) {
      delete pph[plHyp->Num()-1];
      plHyp->Remove(plHyp->Num()-1);
      pph = (PCLexPOSHyp*) plHyp->Get(0); // just in case realloced
   } // while

   // done
   return TRUE;
}



/*************************************************************************************
CNGramDatabase::ProbStreamBitField - Returns the top hypothesis for a stream.

inputs
   DWORD          dwNum - Number of elements in the stream.
   WORD           *pawStream - Stream. dwNum elements. These are bit fields
                  like those used in TrainFixedBlockBitField().
   PCListFixed    plPCLexPOSHyp - Filled with a list of the top hypothesis obhects, PCLexPOSHyp.
                  They are sorted according to highst score. You must free them all.
returns
   BOOL - TRUE if succss
*/
BOOL CNGramDatabase::ProbStreamBitField (DWORD dwNum, WORD *pawStream, PCListFixed plPCLexPOSHyp)
{
   plPCLexPOSHyp->Init (sizeof(PCLexPOSHyp));

   // set up a default hypothesis
   CListFixed alPCLexPOSHyp[2];
   alPCLexPOSHyp[0].Init (sizeof(PCLexPOSHyp));
   PCLexPOSHyp pNew = new CLexPOSHyp;
   if (!pNew)
      return FALSE;  // shouldnt happen
   pNew->m_fScore = 1.0;
   alPCLexPOSHyp[0].Add (&pNew);

   // loop through all the hypothesis, creating
   PCLexPOSHyp *pphFrom, *pphTo;
   DWORD dwFrom = 0, dwTo = 1;
   DWORD dwCount;
   BOOL fRet = TRUE;
   DWORD i, j;
   for (i = 0; i < dwNum + m_dwHistory - 1; i++) {
      dwFrom = (i%2);
      dwTo = (dwFrom+1)%2;
      
      // expand this
      fRet = HypExpand (&alPCLexPOSHyp[dwFrom], &alPCLexPOSHyp[dwTo], pawStream, dwNum, i);

      // free up all the from
      pphFrom = (PCLexPOSHyp*)alPCLexPOSHyp[dwFrom].Get(0);
      dwCount = alPCLexPOSHyp[dwFrom].Num();
      for (j = 0; j < dwCount; j++)
         delete pphFrom[j];
      alPCLexPOSHyp[dwFrom].Clear();

      // potentially break if error
      if (!fRet)
         break;

      // pair down the to-list
      if (!HypCleanup (&alPCLexPOSHyp[dwTo]))
         break;

   } // i
   double fPower = 1.0 / (double)i; // to normalize probability

   // transfer the to-results to plPCLexPOSHyp
   plPCLexPOSHyp->Init (sizeof(PCLexPOSHyp), alPCLexPOSHyp[dwTo].Get(0), alPCLexPOSHyp[dwTo].Num());
   alPCLexPOSHyp[dwTo].Clear();
   pphTo = (PCLexPOSHyp*)plPCLexPOSHyp->Get(0);
   for (i = 0; i < plPCLexPOSHyp->Num(); i++) {
      pNew = pphTo[i];
      pNew->m_fScore = pow(pNew->m_fScore, fPower);

      // remove the last few results since they're filler punctuations
      pNew->m_lPOS.Truncate (dwNum);
   }

   // output the best POS
#if 0 //def _DEBUG
   WCHAR szwTemp[16];
   PWSTR paszPOS[POS_MAJOR_NUM+1] = {L"UNK", L"noun", L"pron", L"adj", L"prep", L"art",
      L"verb", L"adv", L"aux v", L"conj", L"inter", L"PUNCT"};

   OutputDebugString ("\r\n");

   pphTo = (PCLexPOSHyp*)plPCLexPOSHyp->Get(0);
   for (i = 0; i < min(plPCLexPOSHyp->Num(), 16); i++) {
      OutputDebugString ("\r\n");
      pNew = pphTo[i];

      PBYTE pabPOS = (PBYTE) pNew->m_lPOS.Get(0);
      dwCount = pNew->m_lPOS.Num();

      swprintf (szwTemp, L"%g: ", (double) pNew->m_fScore);
      OutputDebugStringW (szwTemp);

      for (j = 0; j < dwCount; j++) {
         //if (paLPG[j].pszWord)
         //   OutputDebugStringW (paLPG[j].pszWord);
         OutputDebugString ("(");
         OutputDebugStringW (paszPOS[pabPOS[j]]);
         OutputDebugString (") ");
      } // j

      OutputDebugString ("\r\n");
   
   } // i

   OutputDebugString ("\r\n");
#endif

   // free up the hypthesis
   for (i = 0; i < 2; i++) {
      pphFrom = (PCLexPOSHyp*)alPCLexPOSHyp[i].Get(0);
      dwCount = alPCLexPOSHyp[i].Num();
      for (j = 0; j < dwCount; j++)
         delete pphFrom[j];
   } // i

   return TRUE;
}




/*************************************************************************************
CLexParseGroup:: Constructor and destructor
*/
CLexParseGroup::CLexParseGroup (void)
{
   MemZero (&m_memName);
   MemZero (&m_memDesc);
   m_ldwGroupCompare.Init (sizeof(DWORD));
   m_dwPOS = (DWORD)-1;
}

CLexParseGroup::~CLexParseGroup (void)
{
   // do nothing for now
}


/*************************************************************************************
CLexParseGroup::WordPartOfGroup - Tests to see if a word is part of the group.

inputs
   PWSTR          psz - Word
   DWORD          dwPOSFlags - Flags for the part of speech that the word can be,
                     with bit for each POS
returns
   DWORD - Flags for the part of speech that the word can be (if it is part of the group).
            If this is 0 then the word can't be part of the group
*/
DWORD CLexParseGroup::WordPartOfGroup (PWSTR psz, DWORD dwPOSFlags)
{
   // if there's a specific part of speech then limit dwPOSFlags
   if (m_dwPOS != (DWORD)-1) {
      dwPOSFlags &= (1 << m_dwPOS);
      if (!dwPOSFlags)
         return 0;   // cant possibly be this since wrong POS
   }

   // else, searcht he strings
   DWORD dwLen = (DWORD)wcslen(psz);
   DWORD *pdwCompare = (DWORD*)m_ldwGroupCompare.Get(0);
   DWORD i, dwLenCompare;
   for (i = 0; i < m_lGroupString.Num(); i++, pdwCompare++) {
      PWSTR pszCompare = (PWSTR)m_lGroupString.Get(i);

      switch (*pdwCompare) {
      case 0:  // exact match
      default:
         if (!_wcsicmp(psz, pszCompare))
            return dwPOSFlags;   // match
         break;
      case 1:  // start
         dwLenCompare = (DWORD)wcslen(pszCompare);
         if ((dwLenCompare <= dwLen) && !_wcsnicmp(psz, pszCompare, dwLenCompare))
            return dwPOSFlags;   // match
         break;
      case 2:  // end
         dwLenCompare = (DWORD)wcslen(pszCompare);
         if ((dwLenCompare <= dwLen) && !_wcsnicmp(psz + (dwLen-dwLenCompare), pszCompare, dwLenCompare))
            return dwPOSFlags;   // match
         break;
      } // switch
   } // i

   // if get here no match
   return 0;
}



/****************************************************************************
LexGrammarGroupPage
*/
static BOOL LexGrammarGroupPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCLexParseGroup plp = (PCLexParseGroup)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the name and stuff
         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"name"))
            pControl->AttribSet (Text(), (PWSTR)plp->m_memName.p);
         if (pControl = pPage->ControlFind (L"desc"))
            pControl->AttribSet (Text(), (PWSTR)plp->m_memDesc.p);
         ComboBoxSet (pPage, L"pos", (int)plp->m_dwPOS);

         pPage->Message (ESCM_USER+198);
      }
      break;

   case ESCM_USER+198:  // set list box
      {
         DWORD i;
         PCEscControl pControl;
         ESCMLISTBOXADD lba;

         // fill in the groups
         MemZero (&gMemTemp);
         DWORD *padwCompare = (DWORD*)plp->m_ldwGroupCompare.Get(0);
         for (i = 0; i < plp->m_lGroupString.Num(); i++, padwCompare++) {
            PWSTR psz = (PWSTR)plp->m_lGroupString.Get(i);

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L">\"<bold>");
            MemCatSanitize (&gMemTemp, psz);
            MemCat (&gMemTemp, L"</bold>\"");
            switch (*padwCompare) {
            case 1:
               MemCat (&gMemTemp, L" (Words that start with)");
               break;
            case 2:
               MemCat (&gMemTemp, L" (Words that end with)");
               break;
            } // switch
            MemCat (&gMemTemp, L"</elem>");
         } // i

         if (pControl = pPage->ControlFind (L"groups")) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            // add this to the list box
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set the selection
            pControl->AttribSetInt (CurSel(), plp->m_lGroupString.Num() ? (plp->m_lGroupString.Num()-1) : 0);
         }

      }
      return TRUE;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"name") || !_wcsicmp(psz, L"desc")) {
            WCHAR szTemp[256];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            PCMem pMem = (!_wcsicmp(psz, L"name")) ? &plp->m_memName : &plp->m_memDesc;
            MemZero (pMem);
            MemCat (pMem, szTemp);
            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"pos")) {
            DWORD dwVal = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dwVal == plp->m_dwPOS)
               break;   // no change

            // else change
            plp->m_dwPOS = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"deletegroup")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this word group?"))
               return TRUE;

            pPage->Exit (L"delete");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupadd")) {
            WCHAR szName[64];
            DWORD dwNeeded;
            szName[0] = 0;
            PCEscControl pControl = pPage->ControlFind (L"wordname");
            if (pControl)
               pControl->AttribGet (Text(), szName ,sizeof(szName), &dwNeeded);

            // combo
            pControl = pPage->ControlFind (L"wordcompare");
            DWORD dwCompare = 0;
            if (pControl)
               dwCompare = (DWORD)pControl->AttribGetInt (CurSel());

            // add it
            plp->m_lGroupString.Add (szName, (wcslen(szName)+1)*sizeof(WCHAR));
            plp->m_ldwGroupCompare.Add (&dwCompare);

            pPage->Message (ESCM_USER+198);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupremove")) {
            PCEscControl pControl = pPage->ControlFind (L"groups");
            DWORD dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : (DWORD)-1;
            if (dwSel >= plp->m_lGroupString.Num()) {
               pPage->MBWarning (L"You must first select a word to remove.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the word?"))
               return TRUE;

            // delete it
            plp->m_lGroupString.Remove (dwSel);
            plp->m_ldwGroupCompare.Remove (dwSel);

            pPage->Message (ESCM_USER+198);
            return TRUE;
         }
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Edit a word group";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CLexParseGroup::Dialog - Brings up a dialog for editing the specific group.

inputs
   PCEscWindow          pWindow - Window to display from
   BOOL                 *pfWantToDelete - If the user selected delete then this will
                        be set to TRUE
returns
   BOOL - TRUE if user pressed back. FALSE if they closed
*/
BOOL CLexParseGroup::Dialog (PCEscWindow pWindow, BOOL *pfWantToDelete)
{
   *pfWantToDelete = FALSE;

   PWSTR psz = pWindow->PageDialog (ghInstance, IDR_MMLLEXGRAMMARGROUP, LexGrammarGroupPage, this);
   if (!psz)
      return FALSE;
   else if (!_wcsicmp(psz, Back()))
      return TRUE;
   else if (!_wcsicmp(psz, L"delete")) {
      *pfWantToDelete = TRUE;
      return TRUE;
   }

   return FALSE;
}


static PWSTR gpszLexParseGroup = L"LexParseGroup";
static PWSTR gpszName = L"Name";
static PWSTR gpszDesc = L"Desc";
static PWSTR gpszPOS = L"POS";
static PWSTR gpszItem = L"Item";
static PWSTR gpszCompare = L"Compare";

/*************************************************************************************
CLexParseGroup::MMLTo - Standard API
*/
PCMMLNode2 CLexParseGroup::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLexParseGroup);

   if (((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   if (((PWSTR)m_memDesc.p)[0])
      MMLValueSet (pNode, gpszDesc, (PWSTR)m_memDesc.p);
   MMLValueSet (pNode, gpszPOS, (int)m_dwPOS);

   // comparison
   DWORD i;
   DWORD *pdwCompare = (DWORD*)m_ldwGroupCompare.Get(0);
   for (i = 0; i < m_lGroupString.Num(); i++, pdwCompare++) {
      PWSTR psz = (PWSTR) m_lGroupString.Get(i);
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszItem);

      if (psz)
         MMLValueSet (pSub, gpszName, psz);
      MMLValueSet (pSub, gpszCompare, (int)*pdwCompare);
   } // i

   return pNode;
}


/*************************************************************************************
CLexParseGroup::MMLFrom - Standard API
*/
BOOL CLexParseGroup::MMLFrom (PCMMLNode2 pNode)
{
   // clear
   MemZero (&m_memName);
   MemZero (&m_memDesc);
   m_lGroupString.Clear();
   m_ldwGroupCompare.Clear();
   m_dwPOS = (DWORD)-1;

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   psz = MMLValueGet (pNode, gpszDesc);
   if (psz)
      MemCat (&m_memDesc, psz);
   m_dwPOS = (DWORD) MMLValueGetInt (pNode, gpszPOS, -1);

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

      if (!_wcsicmp(psz, gpszItem)) {
         // found an item
         psz = MMLValueGet (pSub, gpszName);
         if (!psz)
            psz = L"";
         m_lGroupString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         DWORD dwCompare = (DWORD)MMLValueGetInt (pSub, gpszCompare, 0);
         m_ldwGroupCompare.Add (&dwCompare);
         continue;
      }
   } /// i

   return TRUE;

}



/*************************************************************************************
CLexParseGroup::CloneTo - Standard API
*/
BOOL CLexParseGroup::CloneTo (CLexParseGroup *pTo)
{
   // NOTE: not tested
   MemZero (&pTo->m_memName);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   MemZero (&pTo->m_memDesc);
   MemCat (&pTo->m_memDesc, (PWSTR)m_memDesc.p);

   DWORD i;
   pTo->m_lGroupString.Clear();
   pTo->m_lGroupString.Required (m_lGroupString.Num());
   for (i = 0; i < m_lGroupString.Num(); i++)
      pTo->m_lGroupString.Add (m_lGroupString.Get(i), m_lGroupString.Size(i));
   pTo->m_ldwGroupCompare.Init (sizeof(DWORD), m_ldwGroupCompare.Get(0), m_ldwGroupCompare.Num());
   pTo->m_dwPOS = m_dwPOS;

   return TRUE;
}



/*************************************************************************************
CLexParseGroup::Clone - Standard API
*/
CLexParseGroup *CLexParseGroup::Clone (void)
{
   // NOTE: not tested
   PCLexParseGroup pNew = new CLexParseGroup;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}


/*************************************************************************************
CLexParseRule:: Constructor and destructor
*/
CLexParseRule::CLexParseRule (void)
{
   MemZero (&m_memName);
   MemZero (&m_memDesc);
   m_fScore = 0;
   m_pLexParse = NULL;
}

CLexParseRule::~CLexParseRule (void)
{
   // do nothign for now
}


/*************************************************************************************
CLexParseRule::Sorted - Inform the rule that the groups and rules have been sorted.

inputs
   DWORD          *padwGroup - Indexing this by the old group number results in
                              the new group number.
   DWORD          *padwRule - Like padwGroup
*/
void CLexParseRule::Sorted (DWORD *padwGroup, DWORD *padwRule)
{
   DWORD i, j;
   for (i = 0; i < m_lRules.Num(); i++) {
      PLEXPARSERULE plpr = (PLEXPARSERULE)m_lRules.Get(i);
      DWORD dwCount = (DWORD)m_lRules.Size(i) / sizeof(LEXPARSERULE);

      for (j = 0; j < dwCount; j++, plpr++) {
         if (plpr->wType == 1) // group
            plpr->wNumber = (WORD)padwGroup[plpr->wNumber];
         else if (plpr->wType == 2) // rule
            plpr->wNumber = (WORD)padwRule[plpr->wNumber];
      } // i
   } // i
}


/*************************************************************************************
CLexParseRule::GroupDeleted - Call this whenever a group has been deleted. This
will readjust all group references >= down by one. If the group was found in the
rule then the specific rule will be deleted.

inputs
   DWORD          dwGroup - Group index number, 0+
*/
void CLexParseRule::GroupDeleted (DWORD dwGroup)
{
   DWORD i, j;
   for (i = 0; i < m_lRules.Num(); i++) {
      PLEXPARSERULE plpr = (PLEXPARSERULE)m_lRules.Get(i);
      DWORD dwCount = (DWORD)m_lRules.Size(i) / sizeof(LEXPARSERULE);
      BOOL fWantToDelete = FALSE;

      for (j = 0; j < dwCount; j++, plpr++) {
         if (plpr->wType != 1)
            continue;   // only care about groups
         if (plpr->wNumber == (WORD)dwGroup) {
            // this rule references a group that no longer exists
            fWantToDelete = TRUE;
            break;
         }
         if (plpr->wNumber >= (WORD)dwGroup)
            plpr->wNumber--; // occurred after, so reduce number
      } // i

      if (fWantToDelete) {
         m_lRules.Remove (i);
         i--;
         continue;
      }
   } // i
}

/*************************************************************************************
CLexParseRule::RuleDeleted - Call this whenever a rule has been deleted. This
will readjust all group references >= down by one. If the rule was found in the
rule then the specific rule will be deleted.

inputs
   DWORD          dwGroup - Group index number, 0+
*/
void CLexParseRule::RuleDeleted (DWORD dwRule)
{
   DWORD i, j;
   for (i = 0; i < m_lRules.Num(); i++) {
      PLEXPARSERULE plpr = (PLEXPARSERULE)m_lRules.Get(i);
      DWORD dwCount = (DWORD)m_lRules.Size(i) / sizeof(LEXPARSERULE);
      BOOL fWantToDelete = FALSE;

      for (j = 0; j < dwCount; j++, plpr++) {
         if (plpr->wType != 2)
            continue;   // only care about rules
         if (plpr->wNumber == (WORD)dwRule) {
            // this rule references a group that no longer exists
            fWantToDelete = TRUE;
            break;
         }
         if (plpr->wNumber >= (WORD)dwRule)
            plpr->wNumber--; // occurred after, so reduce number
      } // i

      if (fWantToDelete) {
         m_lRules.Remove (i);
         i--;
         continue;
      }
   } // i
}





/****************************************************************************
LexGrammarRulePage
*/
static BOOL LexGrammarRulePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCLexParseRule plp = (PCLexParseRule)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the name and stuff
         PCEscControl pControl;
         if (pControl = pPage->ControlFind (L"name"))
            pControl->AttribSet (Text(), (PWSTR)plp->m_memName.p);
         if (pControl = pPage->ControlFind (L"desc"))
            pControl->AttribSet (Text(), (PWSTR)plp->m_memDesc.p);
         DoubleToControl (pPage, L"score", plp->m_fScore);

         pPage->Message (ESCM_USER+198);
      }
      break;

   case ESCM_USER+198:  // set list box
      {
         DWORD i;
         PCEscControl pControl;
         ESCMLISTBOXADD lba;

         // fill in the rule cases
         MemZero (&gMemTemp);
         for (i = 0; i < plp->m_lRules.Num(); i++) {
            PLEXPARSERULE plpr = (PLEXPARSERULE) plp->m_lRules.Get(i);
            DWORD dwCount = (DWORD)plp->m_lRules.Size(i) / sizeof(LEXPARSERULE);

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L">");

            DWORD j;
            for (j = 0; j < dwCount; j++, plpr++) {
               if (j)
                  MemCat (&gMemTemp, L" + ");

               switch (plpr->wType) {
               case 0:  // POS
                  switch (plpr->wNumber) {
                  case POS_MAJOR_EXTRACT(POS_MAJOR_NOUN):
                     MemCat (&gMemTemp, L"POS:<bold>Noun</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_PRONOUN):
                     MemCat (&gMemTemp, L"POS:<bold>Pronoun</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_ADJECTIVE):
                     MemCat (&gMemTemp, L"POS:<bold>Adjective</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_PREPOSITION):
                     MemCat (&gMemTemp, L"POS:<bold>Preposition</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_ARTICLE):
                     MemCat (&gMemTemp, L"POS:<bold>Article</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_VERB):
                     MemCat (&gMemTemp, L"POS:<bold>Verb</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_ADVERB):
                     MemCat (&gMemTemp, L"POS:<bold>Adverb</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_AUXVERB):
                     MemCat (&gMemTemp, L"POS:<bold>Auxiliary verb</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_CONJUNCTION):
                     MemCat (&gMemTemp, L"POS:<bold>Conjunction</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_INTERJECTION):
                     MemCat (&gMemTemp, L"POS:<bold>Interjection</bold>");
                     break;
                  case POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION):
                     MemCat (&gMemTemp, L"POS:<bold>Punctuation</bold>");
                     break;
                  } // switch
                  break;

               case 1:  // Group
                  {
                     PCLexParseGroup *pplpg = (PCLexParseGroup*) plp->m_pLexParse->m_lPCLexParseGroup.Get(plpr->wNumber);
                     if (pplpg) {
                        MemCat (&gMemTemp, L"Group:<bold>");
                        MemCatSanitize (&gMemTemp, (PWSTR) pplpg[0]->m_memName.p);
                        MemCat (&gMemTemp, L"</bold>");
                     }
                  }
                  break;

               case 2:  // rule
                  {
                     PCLexParseRule *pplpr = (PCLexParseRule*) plp->m_pLexParse->m_lPCLexParseRule.Get(plpr->wNumber);
                     if (pplpr) {
                        MemCat (&gMemTemp, L"Rule:<bold>");
                        MemCatSanitize (&gMemTemp, (PWSTR) pplpr[0]->m_memName.p);
                        MemCat (&gMemTemp, L"</bold>");
                     }
                  }
                  break;
               } // switch

               switch (plpr->dwFlags) {
               case LEXPARSERULEFLAGS_REPEAT:
                  MemCat (&gMemTemp, L"<italic>(1+)</italic>");
                  break;

               case LEXPARSERULEFLAGS_REPEAT | LEXPARSERULEFLAGS_OPTIONAL:
                  MemCat (&gMemTemp, L"<italic>(0+)</italic>");
                  break;

               case LEXPARSERULEFLAGS_OPTIONAL:
                  MemCat (&gMemTemp, L"<italic>(0-1)</italic>");
                  break;
               } // flags
            } // j

            MemCat (&gMemTemp, L"</elem>");
         } // i

         if (pControl = pPage->ControlFind (L"groups")) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            // add this to the list box
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set the selection
            pControl->AttribSetInt (CurSel(), plp->m_lRules.Num() ? (plp->m_lRules.Num()-1) : 0);
         }

      }
      return TRUE;


   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"name") || !_wcsicmp(psz, L"desc")) {
            WCHAR szTemp[256];
            DWORD dwNeeded;
            szTemp[0] = 0;
            p->pControl->AttribGet (Text(), szTemp, sizeof(szTemp), &dwNeeded);

            PCMem pMem = (!_wcsicmp(psz, L"name")) ? &plp->m_memName : &plp->m_memDesc;
            MemZero (pMem);
            MemCat (pMem, szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"score")) {
            plp->m_fScore = DoubleFromControl (pPage, L"score");
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"deletegroup")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this rule?"))
               return TRUE;

            pPage->Exit (L"delete");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupadd")) {
#define MAXELEM      5  // maximum number of elemens in the case
            LEXPARSERULE alpr[MAXELEM];
            memset (alpr, 0, sizeof(alpr));
            DWORD dwNum = 0;

            DWORD i;
            WCHAR szTemp[64];
            PCEscControl pControl;
            for (i = 0; i < MAXELEM; i++) {
               swprintf (szTemp, L"caseword%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (!pControl)
                  continue;

               DWORD dwVal = (DWORD)pControl->AttribGetInt (CurSel());
               ESCMCOMBOBOXGETITEM cb;
               memset (&cb, 0, sizeof(cb));
               cb.dwIndex = dwVal;
               pControl->Message (ESCM_COMBOBOXGETITEM, &cb);
               if (!cb.pszName)
                  continue;   // eror. shouldnt happen
               dwVal = (DWORD)_wtoi(cb.pszName);
               if ((dwVal == (DWORD)-1) || !dwVal)
                  continue;   // empty

               if (dwVal < 16) {
                  // POS
                  alpr[dwNum].wType = 0;
                  alpr[dwNum].wNumber = (WORD)dwVal - 0;
               }
               else if (dwVal < 1000) {
                  // group
                  alpr[dwNum].wType = 1;
                  alpr[dwNum].wNumber = (WORD)(dwVal - 16);
               }
               else {
                  // rule
                  alpr[dwNum].wType = 2;
                  alpr[dwNum].wNumber = (WORD)(dwVal - 1000);
               }

               // get the repeat
               swprintf (szTemp, L"caserepeat%d", (int)i);
               pControl = pPage->ControlFind (szTemp);
               if (pControl)
                  dwVal = (DWORD)pControl->AttribGetInt (CurSel());
               else
                  dwVal = 0;
               alpr[dwNum].dwFlags = dwVal;

               // increment
               dwNum++;
            } // i

            // if there aren't any elements in the case then error
            if (!dwNum) {
               pPage->MBWarning (L"You must first select a sequence of words/groups/rules for the case.");
               return TRUE;
            }

            // add this
            plp->m_lRules.Add (alpr, dwNum * sizeof(LEXPARSERULE));
            pPage->Message (ESCM_USER+198);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupremove")) {
            PCEscControl pControl = pPage->ControlFind (L"groups");
            DWORD dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : (DWORD)-1;
            if (dwSel >= plp->m_lRules.Num()) {
               pPage->MBWarning (L"You must first select a rule case to remove.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the rule case?"))
               return TRUE;

            // delete it
            plp->m_lRules.Remove (dwSel);

            pPage->Message (ESCM_USER+198);
            return TRUE;
         }
      }
      return TRUE;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Edit a rule";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"EXTRAELEM")) {
            MemZero (&gMemTemp);
            DWORD i;

            PCLexParseGroup *pplpg = (PCLexParseGroup*)plp->m_pLexParse->m_lPCLexParseGroup.Get(0);
            for (i = 0; i < plp->m_pLexParse->m_lPCLexParseGroup.Num(); i++) {
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i + 16);
               MemCat (&gMemTemp, L">Group:<bold>");
               MemCatSanitize (&gMemTemp, (PWSTR)pplpg[i]->m_memName.p);
               MemCat (&gMemTemp, L"</bold></elem>");
            }

            PCLexParseRule *pplpr = (PCLexParseRule*)plp->m_pLexParse->m_lPCLexParseRule.Get(0);
            for (i = 0; i < plp->m_pLexParse->m_lPCLexParseRule.Num(); i++) {
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i + 1000);
               MemCat (&gMemTemp, L">Rule:<bold>");
               MemCatSanitize (&gMemTemp, (PWSTR)pplpr[i]->m_memName.p);
               MemCat (&gMemTemp, L"</bold></elem>");
            }

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/*************************************************************************************
CLexParseRule::Dialog - Brings up a dialog for editing the specific rule.

inputs
   PCEscWindow          pWindow - Window to display from
   BOOL                 *pfWantToDelete - If the user selected delete then this will
                        be set to TRUE
   PCLexParse           pLexParse - Main parser
returns
   BOOL - TRUE if user pressed back. FALSE if they closed
*/
BOOL CLexParseRule::Dialog (PCEscWindow pWindow, BOOL *pfWantToDelete, PCLexParse pLexParse)
{
   *pfWantToDelete = FALSE;
   m_pLexParse = pLexParse;

   PWSTR psz = pWindow->PageDialog (ghInstance, IDR_MMLLEXGRAMMARRULE, LexGrammarRulePage, this);
   if (!psz)
      return FALSE;
   else if (!_wcsicmp(psz, Back()))
      return TRUE;
   else if (!_wcsicmp(psz, L"delete")) {
      *pfWantToDelete = TRUE;
      return TRUE;
   }

   return FALSE;
}


static PWSTR gpszLexParseRule = L"LexParseRule";
static PWSTR gpszScore = L"Score";
static PWSTR gpszPhraseNum = L"PhraseNum";

/*************************************************************************************
CLexParseRule::MMLTo - Standard API
*/
PCMMLNode2 CLexParseRule::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLexParseRule);

   if (((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);
   if (((PWSTR)m_memDesc.p)[0])
      MMLValueSet (pNode, gpszDesc, (PWSTR)m_memDesc.p);
   MMLValueSet (pNode, gpszScore, m_fScore);

   // rules
   DWORD i;
   for (i = 0; i < m_lRules.Num(); i++) {
      PLEXPARSERULE plpr = (PLEXPARSERULE) m_lRules.Get(i);
      DWORD dwCount = (DWORD)m_lRules.Size(i) / sizeof(LEXPARSERULE);

      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszItem);

      MMLValueSet (pSub, gpszName, (PBYTE)plpr, dwCount * sizeof(LEXPARSERULE));
   } // i

   return pNode;
}


/*************************************************************************************
CLexParseRule::MMLFrom - Standard API
*/
BOOL CLexParseRule::MMLFrom (PCMMLNode2 pNode)
{
   // clear
   MemZero (&m_memName);
   MemZero (&m_memDesc);
   m_lRules.Clear();
   m_fScore = 0;

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);
   psz = MMLValueGet (pNode, gpszDesc);
   if (psz)
      MemCat (&m_memDesc, psz);
   m_fScore = MMLValueGetDouble (pNode, gpszScore, 0);

   DWORD i;
   PCMMLNode2 pSub;
   BYTE abTemp[sizeof(LEXPARSERULE)*32];
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszItem)) {
         // found an item
         size_t dwSize = MMLValueGetBinary (pSub, gpszName, abTemp, sizeof(abTemp));
         if (dwSize && (dwSize <= sizeof(abTemp)))
            m_lRules.Add (abTemp, dwSize);
         continue;
      }
   } /// i

   return TRUE;

}



/*************************************************************************************
CLexParseRule::CloneTo - Standard API
*/
BOOL CLexParseRule::CloneTo (CLexParseRule *pTo)
{
   // NOTE: not tested
   MemZero (&pTo->m_memName);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);
   MemZero (&pTo->m_memDesc);
   MemCat (&pTo->m_memDesc, (PWSTR)m_memDesc.p);
   pTo->m_fScore = m_fScore;
   pTo->m_pLexParse = m_pLexParse;

   DWORD i;
   pTo->m_lRules.Clear();
   pTo->m_lRules.Required (m_lRules.Num());
   for (i = 0; i < m_lRules.Num(); i++)
      pTo->m_lRules.Add (m_lRules.Get(i), m_lRules.Size(i));

   return TRUE;
}




/*************************************************************************************
CLexParseRule::Clone - Standard API
*/
CLexParseRule *CLexParseRule::Clone (void)
{
   // NOTE: not tested
   PCLexParseRule pNew = new CLexParseRule;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}





/*************************************************************************************
CLexParse::Constructor and destructor
*/
CLexParse::CLexParse (void)
{
   m_lPCLexParseGroup.Init (sizeof(PCLexParseGroup));
   m_lPCLexParseRule.Init (sizeof(PCLexParseRule));
   m_pLexDialog = NULL;
   m_pNGramSingle = NULL;
   m_pNGramRules = NULL;

   InitializeCriticalSection (&m_csMultiThreaded);
}

CLexParse::~CLexParse (void)
{
   Clear();

   DeleteCriticalSection (&m_csMultiThreaded);
}



#define NGRAMSINGLE_HISTORY         5        // keep track of 5 words
#define NGRAMRULES_HISTORY          3        // because so large, only 3 words woth
#define NGRAMSINGLE_VALUES          (POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION)+1)      // allow for 128 values
#define NGRAMRULES_VALUES           128      // allow for 128 values
#define NGRAM_EOD                   POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION)  // default for end of data

/*************************************************************************************
CLexParse::CreateNGramIfNecessary - Creates the N-gram objects if necessary.
Not usually created since take a lot of memory.
*/
void CLexParse::CreateNGramIfNecessary (void)
{
   if (!m_pNGramSingle) {
      m_pNGramSingle = new CNGramDatabase;
      if (m_pNGramSingle)
         m_pNGramSingle->Init (NGRAMSINGLE_VALUES, NGRAMSINGLE_HISTORY, NGRAM_EOD);
   }

   if (!m_pNGramRules) {
      m_pNGramRules = new CNGramDatabase;
      if (m_pNGramRules)
         m_pNGramRules->Init (NGRAMRULES_VALUES, NGRAMRULES_HISTORY, NGRAM_EOD);
   }
}


/*************************************************************************************
CLexParse::GroupDelete - Deletes a specific group index.

inputs
   DWORD       dwGroup - Group index ot delete
*/
void CLexParse::GroupDelete (DWORD dwGroup)
{
   if (dwGroup >= m_lPCLexParseGroup.Num())
      return;  // doesn't exist

   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get (0);
   DWORD i;
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      pplpr[i]->GroupDeleted (dwGroup);

   // delete it
   PCLexParseGroup *pplpg = (PCLexParseGroup*)m_lPCLexParseGroup.Get (0);
   delete pplpg[dwGroup];
   m_lPCLexParseGroup.Remove (dwGroup);
}



/*************************************************************************************
CLexParse::RuleDelete - Deletes a specific rule index.

inputs
   DWORD       dwRule - Rule index ot delete
*/
void CLexParse::RuleDelete (DWORD dwRule)
{
   if (dwRule >= m_lPCLexParseRule.Num())
      return;  // doesn't exist

   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get (0);
   DWORD i;
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      pplpr[i]->RuleDeleted (dwRule);

   // delete it
   delete pplpr[dwRule];
   m_lPCLexParseRule.Remove (dwRule);
}




static int __cdecl TESTSCORESORTCompare (const void *p1, const void *p2)
{
   PTESTSCORESORT pp1 = (PTESTSCORESORT) p1;
   PTESTSCORESORT pp2 = (PTESTSCORESORT) p2;
   if (pp1->fScore > pp2->fScore)
      return 1;
   else if (pp1->fScore < pp2->fScore)
      return -1;
   else
      return 0;
}

/****************************************************************************
LexGrammarPage
*/
static BOOL LexGrammarPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCLexParse plp = (PCLexParse)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         DWORD i;
         PCEscControl pControl;
         ESCMLISTBOXADD lba;

         // fill in the groups
         MemZero (&gMemTemp);
         PCLexParseGroup *pplpg = (PCLexParseGroup*) plp->m_lPCLexParseGroup.Get(0);
         for (i = 0; i < plp->m_lPCLexParseGroup.Num(); i++) {
            PCLexParseGroup plpg = pplpg[i];

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MemCatSanitize (&gMemTemp, (PWSTR) plpg->m_memName.p);
            MemCat (&gMemTemp, L"</bold> - ");
            MemCatSanitize (&gMemTemp, (PWSTR) plpg->m_memDesc.p);
            MemCat (&gMemTemp, L"</elem>");
         } // i

         if (pControl = pPage->ControlFind (L"groups")) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            // add this to the list box
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set the selection
            pControl->AttribSetInt (CurSel(), 0);
         }


         // fill in the rules
         MemZero (&gMemTemp);
         PCLexParseRule *pplpr = (PCLexParseRule*) plp->m_lPCLexParseRule.Get(0);
         for (i = 0; i < plp->m_lPCLexParseRule.Num(); i++) {
            PCLexParseRule plpr = pplpr[i];

            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MemCatSanitize (&gMemTemp, (PWSTR) plpr->m_memName.p);
            MemCat (&gMemTemp, L"</bold> - ");
            MemCatSanitize (&gMemTemp, (PWSTR) plpr->m_memDesc.p);
            MemCat (&gMemTemp, L"</elem>");
         } // i

         if (pControl = pPage->ControlFind (L"rules")) {
            pControl->Message (ESCM_LISTBOXRESETCONTENT);

            // add this to the list box
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set the selection
            pControl->AttribSetInt (CurSel(), 0);
         }
      }
      break;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"grammar")) {
            // if already has training then ask if want to add to
            plp->CreateNGramIfNecessary ();
            if (plp->m_pNGramSingle->m_fTrainingCount) {
               int iRet;
               iRet = pPage->MBYesNo (
                  L"You already have some grammar training. Do you want to start over again?");
               if (iRet != IDYES)
                  return TRUE;
               //if (iRet == IDCANCEL)
               //   return TRUE;
               //if (iRet == IDNO)
               //   pLex->m_memNGram.m_dwCurPosn = 0;   // clear out
            }
            plp->DialogGrammarProb (pPage->m_pWindow, plp->m_pLexDialog);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupadd")) {
            PCLexParseGroup pNew = new CLexParseGroup;
            if (!pNew)
               return TRUE;   // shouldnt happen
            MemCat (&pNew->m_memName, L"Type in a name");
            MemCat (&pNew->m_memDesc, L"Type in a description");
            plp->m_lPCLexParseGroup.Add (&pNew);

            WCHAR szTemp[64];
            swprintf (szTemp, L"group:%d", (int)plp->m_lPCLexParseGroup.Num()-1);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ruleadd")) {
            PCLexParseRule pNew = new CLexParseRule;
            if (!pNew)
               return TRUE;   // shouldnt happen
            MemCat (&pNew->m_memName, L"Type in a name");
            MemCat (&pNew->m_memDesc, L"Type in a description");
            plp->m_lPCLexParseRule.Add (&pNew);

            WCHAR szTemp[64];
            swprintf (szTemp, L"rule:%d", (int)plp->m_lPCLexParseRule.Num()-1);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"groupedit")) {
            PCEscControl pControl = pPage->ControlFind (L"groups");
            DWORD dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : (DWORD)-1;
            if (dwSel >= plp->m_lPCLexParseGroup.Num()) {
               pPage->MBWarning (L"You must first select a group to edit.");
               return TRUE;
            }

            WCHAR szTemp[64];
            swprintf (szTemp, L"group:%d", (int)dwSel);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ruleedit")) {
            PCEscControl pControl = pPage->ControlFind (L"rules");
            DWORD dwSel = pControl ? (DWORD) pControl->AttribGetInt (CurSel()) : (DWORD)-1;
            if (dwSel >= plp->m_lPCLexParseRule.Num()) {
               pPage->MBWarning (L"You must first select a rule to edit.");
               return TRUE;
            }

            WCHAR szTemp[64];
            swprintf (szTemp, L"rule:%d", (int)dwSel);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"textscan")) {
            // scan the file
            PCMMLNode2 pNode = plp->m_pLexDialog->TextScan (NULL, pPage->m_pWindow->m_hWnd, NULL);
            if (!pNode)
               return TRUE;

            //PWSTR pszRet;

            // make text parse
            CTextParse TextParse;
            if (!TextParse.Init (plp->m_pLexDialog->LangIDGet(), plp->m_pLexDialog)) {
               delete pNode;
               return FALSE;
            }


            // train using the CGramTrain object
            DWORD i;
            CListFixed lLEXPOSGUESS;
            CListFixed lTESTSCORESORT;
            lTESTSCORESORT.Init (sizeof(TESTSCORESORT));
            LEXPOSGUESS lpg;
            PCMMLNode2 pSub;
            PWSTR psz;
            memset (&lpg, 0, sizeof(lpg));
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Analyzing", TRUE);

               for (i = 0; i < pNode->ContentNum(); i++) {
                  pSub = NULL;
                  pNode->ContentEnum (i, &psz, &pSub);
                  if (!pSub)
                     continue;

                  Progress.Update ((fp)i / (fp)pNode->ContentNum());

                  // if find any words not in lexicon then skip
                  PCMMLNode2 pWord;
                  DWORD j;
                  lLEXPOSGUESS.Init (sizeof(LEXPOSGUESS));
                  for (j = 0; j < pSub->ContentNum(); j++) {
                     pWord = NULL;
                     pSub->ContentEnum (j, &psz, &pWord);
                     if (!pWord)
                        continue;
                     psz = pWord->NameGet();
                     if (!psz)
                        continue;   // skip

                     // if it's marked as a word, make sure that it's in the
                     // dictionary. if it isn't, then skip the entire sentence
                     if (!_wcsicmp(psz, TextParse.Word())) {
                        PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
                        if (!psz2)
                           continue;
                     }

                     // keep track of all the words found
                     if (!_wcsicmp(psz, TextParse.Word()) || !_wcsicmp(psz, TextParse.Punctuation())) {
                        PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
                        if (psz2) {
                           lpg.pszWord = psz2;
                           lpg.pvUserData = NULL;
                           lpg.wPOSBitField = plp->m_pLexDialog->WordToPOSBitField (lpg.pszWord);
                           lLEXPOSGUESS.Add (&lpg);
                        }
                     }

                  } // j

                  if (!lLEXPOSGUESS.Num())
                     continue;   // empty sentence

                  // guess the POS and fill in a new memory
                  TESTSCORESORT ts;
                  ts.pMem = new CMem;
                  if (!ts.pMem)
                     continue;
                  MemZero (ts.pMem);
                  plp->POSGuess ((PLEXPOSGUESS)lLEXPOSGUESS.Get(0), lLEXPOSGUESS.Num(),
                     ts.pMem, &ts.fScore, plp->m_pLexDialog);

                  // add to the list
                  lTESTSCORESORT.Add (&ts);
               } // i
            } // progress


            // clear the node
            delete pNode;  // clear out since dont need

            // sort the list
            qsort (lTESTSCORESORT.Get(0), lTESTSCORESORT.Num(), sizeof(TESTSCORESORT), TESTSCORESORTCompare);

            // fill in the list box
            CMem memList;
            PCEscControl pControl;
            ESCMLISTBOXADD lba;

            // fill in the groups
            MemZero (&gMemTemp);
            PTESTSCORESORT ptss = (PTESTSCORESORT) lTESTSCORESORT.Get(0);
            WCHAR szTemp[32];
            for (i = 0; i < lTESTSCORESORT.Num(); i++, ptss++) {
               PWSTR psz = (PWSTR)ptss->pMem->p;

               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"><align wrapindent=64 tab=64>");
               MemCat (&gMemTemp, L"<italic>");
               swprintf (szTemp, L"%g", (double)ptss->fScore);
               MemCat (&gMemTemp, szTemp);
               MemCat (&gMemTemp, L"</italic>&tab;");
               MemCat (&gMemTemp, psz);
               MemCat (&gMemTemp, L"<p/></align></elem>");

               // delete
               delete ptss->pMem;
            } // i

            if (pControl = pPage->ControlFind (L"textlist")) {
               pControl->Message (ESCM_LISTBOXRESETCONTENT);

               // add this to the list box
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszMML = (PWSTR) gMemTemp.p;
               pControl->Message (ESCM_LISTBOXADD, &lba);

               // set the selection
               pControl->AttribSetInt (CurSel(), 0);
            }
            return TRUE;
         } // if "textscan"
      }
      return TRUE;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Change grammar rules";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CLexParseRule::Dialog - Brings up a dialog for editing the specific rule.

inputs
   PCEscWindow          pWindow - Window to display from
   PCMLexicon           pLex - Lexicon to use
returns
   BOOL - TRUE if user pressed back. FALSE if they closed
*/
BOOL CLexParse::Dialog (PCEscWindow pWindow, PCMLexicon pLex)
{
   PWSTR pszGroup = L"group:", pszRule = L"rule:";
   DWORD dwGroupLen = (DWORD)wcslen(pszGroup), dwRuleLen = (DWORD)wcslen(pszRule);
   BOOL fRet, fDelete;
   m_pLexDialog = pLex;

redo:
   PWSTR psz = pWindow->PageDialog (ghInstance, IDR_MMLLEXGRAMMAR, LexGrammarPage, this);
   if (!psz)
      return FALSE;
   else if (!_wcsicmp(psz, Back()))
      return TRUE;
   else if (!wcsncmp(psz, pszGroup, dwGroupLen)) {
      DWORD dwIndex = (DWORD)_wtoi(psz + dwGroupLen);
      if (dwIndex >= m_lPCLexParseGroup.Num())
         goto redo;

      PCLexParseGroup plg = *((PCLexParseGroup*)m_lPCLexParseGroup.Get(dwIndex));
      fRet = plg->Dialog (pWindow, &fDelete);

      if (fDelete)
         GroupDelete (dwIndex);

      // sort since modified
      Sort ();
      
      if (fRet)
         goto redo;
      else
         return FALSE;
   }
   else if (!wcsncmp(psz, pszRule, dwRuleLen)) {
      DWORD dwIndex = (DWORD)_wtoi(psz + dwRuleLen);
      if (dwIndex >= m_lPCLexParseRule.Num())
         goto redo;

      PCLexParseRule plg = *((PCLexParseRule*)m_lPCLexParseRule.Get(dwIndex));
      fRet = plg->Dialog (pWindow, &fDelete, this);

      if (fDelete)
         RuleDelete (dwIndex);
      
      // sort since modified
      Sort ();
      
      if (fRet)
         goto redo;
      else
         return FALSE;
   }

   return TRUE;
}



/*************************************************************************************
CLexParse::POSGuess - Guesses the POS and fills in paLPG.

inputs
   PLEXPOSGUESS      paLPG - Pointer to an arrary of LEXPAUSEGUSS that have their
                        words initially filled in. Punctuation (such as commas) should
                        also be put in, with pszWord = ",", etc.
                        When the function returns bPOS will be filled with their guessed POS.
                        This is POS_MAJOR_XXX.

                        The wPOSBitField elements are important and MUST be filled in.

   DWORD             dwNum - Number of elements in paLPG
   PCMem             pMemText - If not NULL, this is filled in with a MML text version of the
                        guess. Make sure to call MemZero() on this before passing it in.
   fp                *pfScore - If not NULL, filled in with the score
   PCMLexicon        pLex - Lexicon to use
returns
   none
*/
// BUGFIX - Make really large since have new hypotheis stuff
#define MAXSENTLEN         300    // sentences more that this number of words are automatically cut
#define CUTSENTLENTO       200    // cut sentence length to this
//#define MAXSENTLEN         30    // sentences more that this number of words are automatically cut
//#define CUTSENTLENTO       20    // cut sentence length to this
void CLexParse::POSGuess (PLEXPOSGUESS paLPG, DWORD dwNum, PCMem pMemText, fp *pfScore, PCMLexicon pLex)
{
   // first off, guess using the probabilities
   POSGuessProb (paLPG, dwNum, pLex);

   if (pfScore)
      *pfScore = 0;

   CListFixed l;
   l.Init (sizeof(PCLexParseWordScratch));

   DWORD i;
   PCLexParseWordScratch pNew;
   l.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      pNew = new CLexParseWordScratch;
      if (!pNew)
         continue;

      if (!pNew->Init (paLPG[i].pszWord, paLPG[i].wPOSBitField, POS_MAJOR_EXTRACT(paLPG[i].bPOS), this, pLex)) {
         delete pNew;
         continue;
      }

      l.Add (&pNew);

      // BUGFIX - Moved after so dont clear out POS
      paLPG[i].bPOS = 0;
      paLPG[i].bRuleDepthLowDetail = 0;
      memset (&paLPG[i].ParseRuleDepth, 0, sizeof(paLPG[i].ParseRuleDepth));
   } // i

   DWORD dwCutSize = l.Num();
   if (dwCutSize >= MAXSENTLEN)
      dwCutSize = CUTSENTLENTO;
   DWORD dwStart;
   CMem memBest;
   for (dwStart = 0; dwStart < l.Num(); dwStart += dwCutSize) {
      DWORD dwThisPass = l.Num() - dwStart;
      dwThisPass = min(dwThisPass, dwCutSize);

      // parse this
      memBest.m_dwCurPosn = 0;
      ParseScratch ((PCLexParseWordScratch*)l.Get(dwStart), dwThisPass, &memBest);

      // output
      if (memBest.m_dwCurPosn) {
         PLEXPARSEWORDSCRATCHRESULT psr = (PLEXPARSEWORDSCRATCHRESULT) ((PLEXPARSEWORDSCRATCH)memBest.p + 1);
         ParseResultToString ((PCLexParseWordScratch*)l.Get(dwStart), dwThisPass,
            psr, ((DWORD)memBest.m_dwCurPosn - sizeof(LEXPARSEWORDSCRATCH)) / sizeof(LEXPARSEWORDSCRATCHRESULT),
            pMemText);

         if (pfScore)
            *pfScore += ((PLEXPARSEWORDSCRATCH)memBest.p)->fScore;

   #if 0 // def _DEBUG
         if (pMemText) {
            OutputDebugString ("\r\n");
            OutputDebugStringW ((PWSTR)pMemText->p);
         }
   #endif
      }
   } // dwStart

   // go through and eliminate depths until there are only 4
   CListFixed lDepthCount, lDepth;
   lDepthCount.Init (sizeof(DWORD));
   lDepth.Init (sizeof(DWORD));
   DWORD dwMax = 0;
   DWORD dw, j;
   DWORD dwZero = 0;
   DWORD *padwDepthCount, *padwDepth;
   for (i = 0; i < l.Num(); i++) {
      pNew = *((PCLexParseWordScratch*) l.Get(i));

      for (j = 0; j < 2; j++) {
         dw = j ? pNew->m_ParseRuleDepth.bAfter : pNew->m_ParseRuleDepth.bDuring;
         dwMax = max(dwMax, dw);
         lDepth.Add (&dw);

         lDepthCount.Required (dw+1);
         while (lDepthCount.Num() <= dw)
            lDepthCount.Add (&dwZero);
         padwDepthCount = (DWORD*)lDepthCount.Get(dw);
         *padwDepthCount += 1;  // to increment
      } // j
   } // i
   
   // repeat
   padwDepthCount = (DWORD*)lDepthCount.Get(0);
   padwDepth = (DWORD*)lDepth.Get(0);
   while (dwMax) {
      // find the lowest count
      DWORD dwLowest = 0;
      for (i = 1; i <= dwMax; i++)
         if (padwDepthCount[i] < padwDepthCount[dwLowest])
            dwLowest = i;

      // if the lowest has elements in it, and dwMax < 4, then just exit
      // otherwise, want to eliminate since the level isn't even used
      if (padwDepthCount[dwLowest] && (dwMax < 4))
         break;

      // see if should merge this with what's below or what's above
      if (dwLowest == 0)
         dwLowest++; // take this line and move down
      else if (dwLowest >= dwMax) {
         // do nothing. Lower this
      }
      else {
         if (padwDepthCount[dwLowest-1] > padwDepthCount[dwLowest+1])
            dwLowest++; // merge line above down
      }

      // move depthcount count
      padwDepthCount[dwLowest-1] += padwDepthCount[dwLowest];
      memmove (padwDepthCount + dwLowest, padwDepthCount + (dwLowest+1), (dwMax - dwLowest) * sizeof(DWORD));
      dwMax--;

      // adjust all values
      for (i = 0; i < lDepth.Num(); i++)
         if (padwDepth[i] >= dwLowest)
            padwDepth[i] -= 1;
   } // while dwMax >= 4

   // transfer over and free
   if (l.Num() == dwNum) for (i = 0; i < l.Num(); i++) {
      pNew = *((PCLexParseWordScratch*) l.Get(i));
      paLPG[i].bPOS = POS_MAJOR_MAKE(pNew->m_bFinalPOS);
      paLPG[i].bRuleDepthLowDetail = (BYTE)(padwDepth[i*2+0] | (padwDepth[i*2+1] << 2));
      paLPG[i].ParseRuleDepth = pNew->m_ParseRuleDepth;
   }

   for (i = 0; i < l.Num(); i++)
      delete *((PCLexParseWordScratch*) l.Get(i));
}



/*************************************************************************************
CLexParse::MMLTo - Standard API
*/
PCMMLNode2 CLexParse::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszLexParse);

   // groups
   DWORD i;
   PCLexParseGroup *pplpg = (PCLexParseGroup*)m_lPCLexParseGroup.Get (0);
   PCMMLNode2 pSub;
   for (i = 0; i < m_lPCLexParseGroup.Num(); i++) {
      pSub = pplpg[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   // rules
   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get (0);
   for (i = 0; i < m_lPCLexParseRule.Num(); i++) {
      pSub = pplpr[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   // need to save Ngrams and rules
   if (m_pNGramRules) {
      pSub = m_pNGramRules->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszNGramRules);
         pNode->ContentAdd (pSub);
      }
   }
   if (m_pNGramSingle) {
      pSub = m_pNGramSingle->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszNGramSingle);
         pNode->ContentAdd (pSub);
      }
   }
   if (m_lGTRULE.Num())
      MMLValueSet (pNode, gpszGTRULE, (PBYTE) m_lGTRULE.Get(0), m_lGTRULE.Num()*sizeof(GTRULE));

   return pNode;
}


/*************************************************************************************
CLexParse::MMLFrom - Standard API

inputs
   BOOL           fIncludeNGrams - If TRUE, will load in NGrams. If FALSE, won't.
                     Use FALSE when lexicon is for word history or whatever
*/
BOOL CLexParse::MMLFrom (PCMMLNode2 pNode, BOOL fIncludeNGrams)
{
   Clear();

   // need to init Ngrams and rules
   CMem memRLE;
   //m_NGramSingle.Init (NGRAMSINGLE_VALUES, NGRAMSINGLE_HISTORY, NGRAM_EOD);
   //m_NGramRules.Init (NGRAMRULES_VALUES, NGRAMRULES_HISTORY, NGRAM_EOD);
   m_lGTRULE.Init (sizeof(GTRULE), 0, 0);
   MMLValueGetBinary (pNode, gpszGTRULE, &memRLE);
   if (memRLE.m_dwCurPosn)
      m_lGTRULE.Init (sizeof(GTRULE), memRLE.p, (DWORD)memRLE.m_dwCurPosn / sizeof(GTRULE));


   // get elements
   PWSTR psz;
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszLexParseGroup)) {
         PCLexParseGroup pGroup = new CLexParseGroup;
         if (!pGroup)
            continue;
         if (!pGroup->MMLFrom (pSub)) {
            delete pGroup;
            continue;
         }
         m_lPCLexParseGroup.Add (&pGroup);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLexParseRule)) {
         PCLexParseRule pRule = new CLexParseRule;
         if (!pRule)
            continue;
         if (!pRule->MMLFrom (pSub)) {
            delete pRule;
            continue;
         }
         m_lPCLexParseRule.Add (&pRule);
         continue;
      }
      else if (!_wcsicmp(psz, gpszNGramRules) && fIncludeNGrams) {
         CreateNGramIfNecessary ();

         if (m_pNGramRules)
            m_pNGramRules->MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszNGramSingle) && fIncludeNGrams) {
         CreateNGramIfNecessary ();

         if (m_pNGramSingle)
            m_pNGramSingle->MMLFrom (pSub);
         continue;
      }
   } // i

   return TRUE;
}




/*************************************************************************************
CLexParse::CloneTo - Standard API
*/
BOOL CLexParse::CloneTo (CLexParse *pTo)
{
   // NOTE: not tested
   pTo->Clear();
   // free up parse group
   DWORD i;

   pTo->m_lPCLexParseGroup.Init (sizeof(PCLexParseGroup), m_lPCLexParseGroup.Get(0), m_lPCLexParseGroup.Num());
   PCLexParseGroup *pplpg = (PCLexParseGroup*)pTo->m_lPCLexParseGroup.Get (0);
   for (i = 0; i < pTo->m_lPCLexParseGroup.Num(); i++)
      pplpg[i] = pplpg[i]->Clone();

   pTo->m_lPCLexParseRule.Init (sizeof(PCLexParseRule), m_lPCLexParseRule.Get(0), m_lPCLexParseRule.Num());
   PCLexParseRule *pplpr = (PCLexParseRule*)pTo->m_lPCLexParseRule.Get (0);
   for (i = 0; i < pTo->m_lPCLexParseRule.Num(); i++)
      pplpr[i] = pplpr[i]->Clone();

   // copy over the Ngrams and rules
   if (m_pNGramRules)
      pTo->m_pNGramRules = m_pNGramRules->Clone();
   if (m_pNGramSingle)
      pTo->m_pNGramSingle = m_pNGramSingle->Clone();
   //m_NGramRules.CloneTo (&pTo->m_NGramRules);
   //m_NGramSingle.CloneTo (&pTo->m_NGramSingle);
   pTo->m_lGTRULE.Init (sizeof(GTRULE), m_lGTRULE.Get(0), m_lGTRULE.Num());

   return TRUE;
}





/*************************************************************************************
CLexParse::Clone - Standard API
*/
CLexParse *CLexParse::Clone (void)
{
   // NOTE: not tested
   PCLexParse pNew = new CLexParse;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



/*************************************************************************************
CLexParse::Clear - Stnadad PAI
*/
void CLexParse::Clear (void)
{
   // free up parse group
   DWORD i;
   PCLexParseGroup *pplpg = (PCLexParseGroup*)m_lPCLexParseGroup.Get (0);
   for (i = 0; i < m_lPCLexParseGroup.Num(); i++)
      delete pplpg[i];
   m_lPCLexParseGroup.Clear();

   // free up parse rule
   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get (0);
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      delete pplpr[i];
   m_lPCLexParseRule.Clear();

   // other values
   if (m_pNGramSingle) {
      delete m_pNGramSingle;
      m_pNGramSingle = NULL;
   }
   if (m_pNGramRules) {
      delete m_pNGramRules;
      m_pNGramRules = NULL;
   }
   //m_NGramSingle.Init (NGRAMSINGLE_VALUES, NGRAMSINGLE_HISTORY, NGRAM_EOD);
   //m_NGramRules.Init (NGRAMRULES_VALUES, NGRAMRULES_HISTORY, NGRAM_EOD);
   m_lGTRULE.Init (sizeof(GTRULE), 0, 0);

}



/*************************************************************************************
CLexParse::Sort - Sorts the groups and rules alphabetically.
*/
static int __cdecl PCLexParseGroupCompare (const void *p1, const void *p2)
{
   PCLexParseGroup pp1 = *((PCLexParseGroup*) p1);
   PCLexParseGroup pp2 = *((PCLexParseGroup*) p2);
   int iRet = _wcsicmp((PWSTR)pp1->m_memName.p, (PWSTR)pp2->m_memName.p);
   return iRet;
}
static int __cdecl PCLexParseRuleCompare (const void *p1, const void *p2)
{
   PCLexParseRule pp1 = *((PCLexParseRule*) p1);
   PCLexParseRule pp2 = *((PCLexParseRule*) p2);
   int iRet = _wcsicmp((PWSTR)pp1->m_memName.p, (PWSTR)pp2->m_memName.p);
   return iRet;
}
void CLexParse::Sort (void)
{
   CMem mem;
   if (!mem.Required ((m_lPCLexParseGroup.Num() + m_lPCLexParseRule.Num()) * sizeof(DWORD)))
      return;
   DWORD *padwGroup = (DWORD*)mem.p;
   DWORD *padwRule = padwGroup + m_lPCLexParseGroup.Num();

   DWORD i;
   PCLexParseGroup *pplpg = (PCLexParseGroup*)m_lPCLexParseGroup.Get (0);
   for (i = 0; i < m_lPCLexParseGroup.Num(); i++)
      pplpg[i]->m_dwIndexBeforeSort = i;
   qsort (m_lPCLexParseGroup.Get(0), m_lPCLexParseGroup.Num(), sizeof(PCLexParseGroup), PCLexParseGroupCompare);
   for (i = 0; i < m_lPCLexParseGroup.Num(); i++)
      padwGroup[pplpg[i]->m_dwIndexBeforeSort] = i;

   // free up parse rule
   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get (0);
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      pplpr[i]->m_dwIndexBeforeSort = i;
   qsort (m_lPCLexParseRule.Get(0), m_lPCLexParseRule.Num(), sizeof(PCLexParseRule), PCLexParseRuleCompare);
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      padwRule[pplpr[i]->m_dwIndexBeforeSort] = i;
   for (i = 0; i < m_lPCLexParseRule.Num(); i++)
      pplpr[i]->Sorted (padwGroup, padwRule);
}



/*************************************************************************************
CLexParse::ParseLEXPARSERULESingle - This just parses a single lexparserule, ignoring
repeats and optional.

inputs
   PLEXPARSERULE        pRule - Rule to look at. dwFlags (for repeat and optional are ignored)
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PCMem                pMemScratch - Scratch memory to use
returns
   PCListVariable - List of parse results, or NULL if it doesn't parse.
         Do NOT free this list or change it since it's permanently part of of pplpws.
*/
PCListVariable CLexParse::ParseLEXPARSERULESingle (PLEXPARSERULE pRule, PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                                   PCMem pMemScratch)
{
   if (!dwNumWord)
      return NULL;   // nothing left

   PCLexParseWordScratch pWord = pplpws[0];
   DWORD i, j;
   PCListVariable plRet;
   PCListVariable *pplRet;

   // look at the rule type
   switch (pRule->wType) {
   case 0:  // word
      // find a POS to use
      if (!(pWord->m_dwPOSFlags & (1 << pRule->wNumber)))
         return NULL;   // not supported
      return &pWord->m_alvWord[pRule->wNumber];  // guaranteed to have at least one entry

   case 1:  // group
      // see if it supports that group
      if (!(pWord->m_padwGroup[pRule->wNumber / 32] & (1 << (pRule->wNumber % 32))))
         return NULL;

      // get the group list
      pplRet = (PCListVariable*) pWord->m_hGroupPCListVariable.Find (pRule->wNumber);
      return pplRet ? pplRet[0] : NULL;

   case 2:  // rule
      // see if the rule has already been generated
      if (pWord->RuleAlreadyParsed (pRule->wNumber, &plRet))
         return plRet;

      // else, not parsed, so parse and add
      break;

   default:
      return NULL;   // error
   } // switch

   // if get here then need to parse the rule
   // set flag so that wont recurse on self
   pWord->RuleParsed (pRule->wNumber, NULL, 0);

   // loop through all the cases
   PCLexParseRule *pplpr = (PCLexParseRule*) m_lPCLexParseRule.Get (pRule->wNumber);
   if (!pplpr)
      return NULL;   // erorr that shouldnt happen
   PCLexParseRule plpr = pplpr[0];
   for (i = 0; i < plpr->m_lRules.Num(); i++) {
      PLEXPARSERULE pr = (PLEXPARSERULE) plpr->m_lRules.Get(i);
      DWORD dwCount = (DWORD)plpr->m_lRules.Size(i) / sizeof(LEXPARSERULE);

      PCListVariable plv = ParseLEXPARSERULEMultiple (pr, dwCount, dwCount ? pr[0].dwFlags : 0,
         pplpws, dwNumWord, pMemScratch);
      if (!plv)
         continue;   // couldnt parse this case

      // modify the headers
      for (j = 0; j < plv->Num(); j++) {
         PLEXPARSEWORDSCRATCH ps = (PLEXPARSEWORDSCRATCH)plv->Get(j);
         ps->fScore += plpr->m_fScore;
         ps->lpwsrThis.wCase = (WORD)i;
         ps->lpwsrThis.wNumber = pRule->wNumber;
         ps->lpwsrThis.wType = 2;   // rule
         // NOTE: ps->lpwsrThis.wPOS will be filled in by RuleParsed

         // add this
         pWord->RuleParsed (pRule->wNumber, ps,
            ((DWORD)plv->Size(j) - sizeof(LEXPARSEWORDSCRATCH)) / sizeof(LEXPARSEWORDSCRATCHRESULT));
      } // j

      // delete the return list
      delete plv;
   } // i, over cases

   // finally, go back and try again
   if (pWord->RuleAlreadyParsed (pRule->wNumber, &plRet))
      return plRet;
   else
      return NULL;
}




/*************************************************************************************
CLexParse::ParseLEXPARSERULEMultiple - Parses multiple LEXPARSERULEs in an array.

inputs
   PLEXPARSERULE        pRule - Rule to look at.
   DWORD                dwNumRule - Number of rules in the array
   DWORD                dwFlagsOverride - Override for dwFlags for the 1st rule.
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PCMem                pMemScratch - Scratch memory to use
returns
   PCListVariable - List of parse results, or NULL if it doesn't parse.
         You MUST FREE this list.
*/
PCListVariable CLexParse::ParseLEXPARSERULEMultiple (PLEXPARSERULE pRule, DWORD dwNumRule, DWORD dwFlagsOverride,
                                                     PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                                     PCMem pMemScratch)
{
   PCListVariable plv, plvRet, plvRet2;
   DWORD i, j;
   if (!dwNumRule) {
      // no more rules to look at, so return this
      // should only happen on an optional as the last element
      plv = new CListVariable;
      LEXPARSEWORDSCRATCH lpws;
      memset (&lpws, 0, sizeof(lpws));
      lpws.lpwsrThis.wType = (WORD)-1; // since empty
      // lpws.dwWords = 0; to indicate that have 0 words
      if (plv)
         plv->Add (&lpws, sizeof(lpws));
      return plv;
   }

   // BUGFIX - If there's an optional rule at the end, and no words, call down the list
   if (!dwNumWord && (dwFlagsOverride & LEXPARSERULEFLAGS_OPTIONAL))
      return ParseLEXPARSERULEMultiple (pRule+1, dwNumRule-1, (dwNumRule > 1) ? pRule[1].dwFlags : 0,
         pplpws, dwNumWord, pMemScratch);

   if (!dwNumWord)
      return NULL;   // nothing left

   // if optional, then try skipping this

   // blank return list
   plv = NULL;

   if (dwFlagsOverride & LEXPARSERULEFLAGS_OPTIONAL) {
      // try skipping this rule
      plvRet = ParseLEXPARSERULEMultiple (pRule+1, dwNumRule-1, (dwNumRule > 1) ? pRule[1].dwFlags : 0,
         pplpws, dwNumWord, pMemScratch);

      // add these all to the main return
      if (plvRet) {
         if (!plv)
            plv = new CListVariable;

         if (plv) {
            plv->Required (plv->Num() + plvRet->Num());
            for (i = 0; i < plvRet->Num(); i++)
               plv->Add (plvRet->Get(i), plvRet->Size(i));
         }
         delete plvRet;
      } // if plvRet
   }

   // do one copy of the rule
   PLEXPARSEWORDSCRATCH pws, pws2, pws3;
   PLEXPARSEWORDSCRATCHRESULT pwsr3;
   DWORD dwSize1, dwSize2, dwSize3;
   plvRet = ParseLEXPARSERULESingle (pRule, pplpws, dwNumWord, pMemScratch);
   if (plvRet && plvRet->Num()) {
      if (!plv)
         plv = new CListVariable;
      if (!plv)
         return NULL;   // error. shouldnt happen
      // try to parse next in the rules
      if (dwNumRule <= 1) { // no other rules
         // blank memory to write into
         dwSize3 = sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT);
         if (!pMemScratch->Required (dwSize3))
            return NULL;   // erorr. shouldnt happen
         pws3 = (PLEXPARSEWORDSCRATCH)pMemScratch->p;
         pwsr3 = (PLEXPARSEWORDSCRATCHRESULT) (pws3+1);
         memset (pws3, 0, dwSize3);

         for (i = 0; i < plvRet->Num(); i++) {
            // copy over the return info
            pws2 = (PLEXPARSEWORDSCRATCH)plvRet->Get(i);
            pws3->dwWords = pws2->dwWords;
            pws3->fScore = pws2->fScore;
            *pwsr3 = pws2->lpwsrThis;

            // add a copy
            plv->Add (pws3, dwSize3);
         } // i
      }
      else { // have other rules, so try to parse these
         for (i = 0; i < plvRet->Num(); i++) {
            pws = (PLEXPARSEWORDSCRATCH)plvRet->Get(i);
            dwSize1 = (DWORD)plvRet->Size(i);
            plvRet2 = ParseLEXPARSERULEMultiple (pRule+1, dwNumRule-1, pRule[1].dwFlags,
               pplpws + pws->dwWords, dwNumWord - pws->dwWords, pMemScratch);
            if (!plvRet2)
               continue;   // no further matches so entire rule is invalid
            
            // combine these two
            if (!plv) {
               plv = new CListVariable;
               if (!plv)
                  return NULL;
            }

            for (j = 0; j < plvRet2->Num(); j++) {
               pws2 = (PLEXPARSEWORDSCRATCH)plvRet2->Get(j);
               dwSize2 = (DWORD)plvRet2->Size(j);
               dwSize3 = dwSize2 + sizeof(LEXPARSEWORDSCRATCHRESULT);
               if (!pMemScratch->Required (dwSize3))
                  return NULL;   // shouldnt happen
         
               pws3 = (PLEXPARSEWORDSCRATCH)pMemScratch->p;
               pwsr3 = (PLEXPARSEWORDSCRATCHRESULT) (pws3+1);
               memset (pws3, 0, sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT));

               pws3->dwWords = pws->dwWords + pws2->dwWords;
               pws3->fScore = pws->fScore + pws2->fScore;
               *pwsr3 = pws->lpwsrThis; // since at the beginning
               memcpy (pwsr3 + 1, pws2 + 1, dwSize2 - sizeof(LEXPARSEWORDSCRATCH)); // copy end bits of result

               // add a copy
               plv->Add (pws3, dwSize3);
            } // j
            delete plvRet2;   // since came from multiple
         } // i
      }

      // try to parse repeats
      if (dwFlagsOverride & LEXPARSERULEFLAGS_REPEAT) {
         for (i = 0; i < plvRet->Num(); i++) {
            pws = (PLEXPARSEWORDSCRATCH)plvRet->Get(i);
            dwSize1 = (DWORD)plvRet->Size(i);
            plvRet2 = ParseLEXPARSERULEMultiple (pRule, dwNumRule, LEXPARSERULEFLAGS_REPEAT,
               pplpws + pws->dwWords, dwNumWord - pws->dwWords, pMemScratch);
            if (!plvRet2)
               continue;   // no further matches so entire rule is invalid
            
            // combine these two
            if (!plv) {
               plv = new CListVariable;
               if (!plv)
                  return NULL;
            }

            for (j = 0; j < plvRet2->Num(); j++) {
               pws2 = (PLEXPARSEWORDSCRATCH)plvRet2->Get(j);
               dwSize2 = (DWORD)plvRet2->Size(j);
               dwSize3 = dwSize2 + sizeof(LEXPARSEWORDSCRATCHRESULT);
               if (!pMemScratch->Required (dwSize3))
                  return NULL;   // shouldnt happen
         
               pws3 = (PLEXPARSEWORDSCRATCH)pMemScratch->p;
               pwsr3 = (PLEXPARSEWORDSCRATCHRESULT) (pws3+1);
               memset (pws3, 0, sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT));

               pws3->dwWords = pws->dwWords + pws2->dwWords;
               pws3->fScore = pws->fScore + pws2->fScore;
               *pwsr3 = pws->lpwsrThis; // since at the beginning
               memcpy (pwsr3 + 1, pws2 + 1, dwSize2 - sizeof(LEXPARSEWORDSCRATCH)); // copy end bits of result

               // add a copy
               plv->Add (pws3, dwSize3);
            } // j
            delete plvRet2;   // since came from multiple
         } // i
      }  // repeats
   } // if single parse worked

   return plv;
}





#if 0 // old code
/*************************************************************************************
CLexParse::ParseScratchRecurse - This is the recursive version for finding the
parse with the best score.

inputs
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PCMem                pMemCur - Filled with the current parse. m_dwCurPosn is pointing to the
                           memory location where the next LEXPARSEWORDSCRATCHRESULT goes.
                           The header (LEXPARSEWORDSCRATCH) is filled in with a current score, etc.
   PCMem                pMemScratch - Scratch memory to use
   PCMem                pMemBest - Should initially have m_dwCurPosn filled with 0, but
                           gradually filled with the best parse.
returns
   BOOL - TRUE if success
*/
#define SCOREFORGIVENESS      3     // allow 3 points of forgiveness
BOOL CLexParse::ParseScratchRecurse (PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                     PCMem pMemCur, PCMem pMemScratch, PCMem pMemBest)
{
   // if this score is much worse than the best score then stop right now
   PLEXPARSEWORDSCRATCH pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;
   PLEXPARSEWORDSCRATCH pBest = (PLEXPARSEWORDSCRATCH)pMemBest->p;
   if (pMemBest->m_dwCurPosn && (pCur->fScore + SCOREFORGIVENESS < pBest->fScore))
      return TRUE;
   
   // if there aren't any words left then consider setting as best
   if (!dwNumWord) {
      if (!pMemBest->m_dwCurPosn || (pCur->fScore > pBest->fScore)) {
         if (!pMemBest->Required (pMemCur->m_dwCurPosn))
            return FALSE;
         pMemBest->m_dwCurPosn = pMemCur->m_dwCurPosn;
         memcpy (pMemBest->p, pMemCur->p, pMemCur->m_dwCurPosn);
      }
      return TRUE;
   }

   // reduce the current score by one (temporarily) since all cases do that
   fp fOldScore = pCur->fScore;
   pCur->fScore -= 1;

   // loop through all the rules and try applying to this word
   DWORD i, j;
   LEXPARSERULE lpr;
   PCListVariable plv;
   PLEXPARSEWORDSCRATCHRESULT plpwsr;
   memset (&lpr, 0, sizeof(lpr));
   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get(0);
   DWORD dwApproach;
   for (dwApproach = 0; dwApproach < 2; dwApproach++) {
      // if dwApproach == 0 then go through all the rules
      // if dwApproach == 1 then hypothesize a POS
      DWORD dwNum = dwApproach ? 1 : m_lPCLexParseRule.Num();
      for (i = 0; i < dwNum; i++) {
         if (dwApproach) {
            lpr.wType = 0;
            lpr.wNumber = 0;
            for (j = 0; j < 16; j++)
               if (pplpws[0]->m_dwPOSFlags & (1 << j)) {
                  lpr.wNumber = (WORD)j;
                  break;
               }
         }
         else {
            lpr.wType = 2;
            lpr.wNumber = (WORD)i;
         }

         plv = ParseLEXPARSERULESingle (&lpr, pplpws, dwNumWord, pMemScratch);
         if (!plv)
            continue;   // didn't parse

         // else, this parsed, so add and then recurse
         if (!pMemCur->Required (pMemCur->m_dwCurPosn + sizeof(LEXPARSEWORDSCRATCHRESULT)))
            return FALSE;  // error. shouldnt happen

         for (j = 0; j < plv->Num(); j++) {
            PLEXPARSEWORDSCRATCH ps = (PLEXPARSEWORDSCRATCH)plv->Get(j);

            pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;
            DWORD dwOldPosn = pMemCur->m_dwCurPosn;
            pMemCur->m_dwCurPosn += sizeof(LEXPARSEWORDSCRATCHRESULT);
            DWORD dwOldWords = pCur->dwWords;

            // copy over new entry
            plpwsr = (PLEXPARSEWORDSCRATCHRESULT) ((PBYTE)pMemCur->p + dwOldPosn);
            *plpwsr = ps->lpwsrThis;
            pCur->dwWords += ps->dwWords;
            pCur->fScore += ps->fScore;

            // recurse
            BOOL fRet = ParseScratchRecurse (pplpws + ps->dwWords, dwNumWord - ps->dwWords, pMemCur, pMemScratch, pMemBest);

            // restore
            pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;
            pMemCur->m_dwCurPosn = dwOldPosn;
            pCur->dwWords = dwOldWords;
            pCur->fScore = fOldScore-1;

            if (!fRet)
               return FALSE;
         } // j, over all parse results

      } // i
   } // dwApproach

   // restore the score
   pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;
   pCur->fScore = fOldScore;
   return TRUE;
}
#endif // 0


// EMTPARSESCRATCHRECURSE
typedef struct {
   // on all EMTCxxx
   DWORD          dwStart;       // start count
   DWORD          dwEnd;         // end count
   DWORD          dwType;        // type

   // specific
   PCListFixed    plFrom;
   PCListFixed    plTo;
   DWORD          dwTime;
   PCLexParseWordScratch *pplpws;
   DWORD          dwNumWord;
   PCMem          paMemScratch;
} EMTPARSESCRATCHRECURSE, *PEMTPARSESCRATCHRECURSE;

/*************************************************************************************
CLexParse::EscMultiThreadedCallback - Standard call
*/
void CLexParse::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread)
{
   DWORD *padw = (DWORD*)pParams;
   DWORD dwStart = padw[0];
   DWORD dwEnd = padw[1];
   DWORD dwType = padw[2];
   DWORD i; //, j;

   switch (dwType) {
   case 20: // prosody beam search
      {
         PEMTPARSESCRATCHRECURSE pe = (PEMTPARSESCRATCHRECURSE) pParams;

         PCListFixed plFrom = pe->plFrom;
         PCListFixed plTo = pe->plTo;
         DWORD dwTime = pe->dwTime;
         PCLexParseWordScratch *pplpws = pe->pplpws;
         DWORD dwNumWord = pe->dwNumWord;
         PCMem pMemScratch = pe->paMemScratch + dwThread;

         DWORD dwHyp;
         PCMem pMemCur, pNew;
         PLEXPARSEWORDSCRATCH pCur;
         LEXPARSERULE lpr;
         DWORD j;
         PCListVariable plv;
         PLEXPARSEWORDSCRATCHRESULT plpwsr;

         // loop over all from
         for (dwHyp = dwStart; dwHyp < dwEnd; dwHyp++) {   // note: going backwards
            PCMem *ppMem = (PCMem*)plFrom->Get(dwHyp);
            pMemCur = *ppMem;
            pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;

            // if this doesn't have the right # of words then merely decrease the score
            // and copy it over
            if (pCur->dwWords != dwTime) {
               pCur->fScore -= 1;   // so that score is in line

               EnterCriticalSection (&m_csMultiThreaded);
               plTo->Add (&pMemCur);
               LeaveCriticalSection (&m_csMultiThreaded);

               // plFrom->Remove (dwHyp);
               *ppMem = NULL;
               continue;
            }

            // else, if get here, need to expand
            memset (&lpr, 0, sizeof(lpr));
            DWORD dwApproach;
            for (dwApproach = 0; dwApproach < 2; dwApproach++) {
               // if dwApproach == 0 then go through all the rules
               // if dwApproach == 1 then hypothesize a POS
               DWORD dwNum = dwApproach ? 1 : m_lPCLexParseRule.Num();
               for (i = 0; i < dwNum; i++) {
                  if (dwApproach) {
                     lpr.wType = 0;
                     lpr.wNumber = 0;
                     for (j = 0; j < 16; j++)
                        if (pplpws[dwTime]->m_dwPOSFlags & (1 << j)) {
                           lpr.wNumber = (WORD)j;
                           break;
                        }
                  }
                  else {
                     lpr.wType = 2;
                     lpr.wNumber = (WORD)i;
                  }

                  plv = ParseLEXPARSERULESingle (&lpr, pplpws + dwTime, dwNumWord - dwTime, pMemScratch);
                  if (!plv)
                     continue;   // didn't parse

                  for (j = 0; j < plv->Num(); j++) {
                     PLEXPARSEWORDSCRATCH ps = (PLEXPARSEWORDSCRATCH)plv->Get(j);

                     pNew = new CMem;
                     if (!pNew)
                        continue;
                     if (!pNew->Required (pMemCur->m_dwCurPosn + sizeof(LEXPARSEWORDSCRATCHRESULT))) {
                        delete pNew;
                        continue;
                     }
                     memcpy (pNew->p, pMemCur->p, pMemCur->m_dwCurPosn);
                     pNew->m_dwCurPosn = pMemCur->m_dwCurPosn + sizeof(LEXPARSEWORDSCRATCHRESULT);


                     // copy over new entry
                     pCur = (PLEXPARSEWORDSCRATCH)pNew->p;
                     plpwsr = (PLEXPARSEWORDSCRATCHRESULT) ((PBYTE)pNew->p + pMemCur->m_dwCurPosn);
                     *plpwsr = ps->lpwsrThis;
                     pCur->dwWords += ps->dwWords;
                     pCur->fScore += ps->fScore + (fp)ps->dwWords - 2;
                        // NOTE: Adding number of words so counteract fScore -= 1 if not at that time point
                        // NOTE: -2 means that if only added one word, score decreases by one for every "element"
                        // of CFG or individual word that's added

                     // add to list
                     EnterCriticalSection (&m_csMultiThreaded);
                     plTo->Add (&pNew);
                     LeaveCriticalSection (&m_csMultiThreaded);
                  } // j, over all parse results

               } // i
            } // dwApproach
         } // dwHyp, over plFrom
      }
      return;
   } // switch
}

/*************************************************************************************
CLexParse::ParseScratchRecurse - This is the recursive version for finding the
parse with the best score.

inputs
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PCMem                *paMemScratch - Scratch memory to use. Must have MAXRAYTHREAD memories
   PCMem                pMemBest - Should initially have m_dwCurPosn filled with 0, but
                           gradually filled with the best parse.
returns
   BOOL - TRUE if success
*/
static int __cdecl PCMemCompare (const void *p1, const void *p2)
{
   PCMem pp1 = *((PCMem*) p1);
   PCMem pp2 = *((PCMem*) p2);
   PLEXPARSEWORDSCRATCH ps1 = (PLEXPARSEWORDSCRATCH) pp1->p;
   PLEXPARSEWORDSCRATCH ps2 = (PLEXPARSEWORDSCRATCH) pp2->p;

   if (ps1->fScore > ps2->fScore)
      return -1;
   else if (ps1->fScore < ps2->fScore)
      return 1;
   else
      return 0;
}

BOOL CLexParse::ParseScratchRecurse (PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                     PCMem paMemScratch, PCMem pMemBest)
{
   CListFixed alHyp[2];
   alHyp[0].Init (sizeof(PCMem));
   alHyp[1].Init (sizeof(PCMem));

   // fill in initial hyptohesis
   PCMem pInitial = new CMem;
   if (!pInitial)
      return FALSE;
   if (!pInitial->Required (sizeof(LEXPARSEWORDSCRATCH))) {
      delete pInitial;
      return FALSE;
   }
   PLEXPARSEWORDSCRATCH pCur = (PLEXPARSEWORDSCRATCH)pInitial->p;
   pInitial->m_dwCurPosn = sizeof(LEXPARSEWORDSCRATCH);
   memset (pCur, 0, sizeof(*pCur));
   alHyp[0].Add (&pInitial);

   DWORD i, j; // , dwHyp;
   DWORD dwTime;
   PCLexParseRule *pplpr = (PCLexParseRule*)m_lPCLexParseRule.Get(0);
   PCListFixed plFrom = &alHyp[1];
   PCListFixed plTo = &alHyp[0]; // so that if no words will be empty list
   PCMem pMemCur; // , pNew;
   for (dwTime = 0; dwTime < dwNumWord; dwTime++) {
      plFrom = &alHyp[dwTime%2];
      plTo = &alHyp[1 - (dwTime%2)];

      // free up to
      for (j = 0; j < plTo->Num(); j++)
         delete *((PCMem*)plTo->Get(j));
      plTo->Clear();

#if 0 // disable this because seems too intricate for multithreaded code
      // multithreaded
      EMTPARSESCRATCHRECURSE em;
      memset (&em, 0, sizeof(em));
      em.dwType = 20;
      em.dwNumWord = dwNumWord;
      em.dwTime = dwTime;
      em.paMemScratch = paMemScratch;
      em.plFrom = plFrom;
      em.plTo = plTo;
      em.pplpws = pplpws;
      ThreadLoop (0, plFrom->Num(), 1, &em, sizeof(em), NULL);

      // eliminate anything in plFrom that's NULL
      // BUGFIX - Added this when went multitheaded
      for (i = plFrom->Num()-1; i < plFrom->Num(); i--) { // going backwards
         PCMem *ppMem = (PCMem*)plFrom->Get(i);
         if (!*ppMem)
            plFrom->Remove (i);
      } // i
#endif 0

#if 1 // BUGFIX - Re-enabled because multhreaded code seems to complex moved to multhread
      LEXPARSERULE lpr;
      PCListVariable plv;
      PLEXPARSEWORDSCRATCHRESULT plpwsr;
      DWORD dwHyp;
      PCMem pNew;

      // loop over all from
      for (dwHyp = plFrom->Num()-1; dwHyp < plFrom->Num(); dwHyp--) {   // note: going backwards
         pMemCur = *((PCMem*)plFrom->Get(dwHyp));
         pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;

         // if this doesn't have the right # of words then merely decrease the score
         // and copy it over
         if (pCur->dwWords != dwTime) {
            pCur->fScore -= 1;   // so that score is in line
            plTo->Add (&pMemCur);
            plFrom->Remove (dwHyp);
            continue;
         }

         // else, if get here, need to expand
         memset (&lpr, 0, sizeof(lpr));
         DWORD dwApproach;
         for (dwApproach = 0; dwApproach < 2; dwApproach++) {
            // if dwApproach == 0 then go through all the rules
            // if dwApproach == 1 then hypothesize a POS
            DWORD dwNum = dwApproach ? 1 : m_lPCLexParseRule.Num();
            for (i = 0; i < dwNum; i++) {
               if (dwApproach) {
                  lpr.wType = 0;
                  lpr.wNumber = 0;
                  for (j = 0; j < 16; j++)
                     if (pplpws[dwTime]->m_dwPOSFlags & (1 << j)) {
                        lpr.wNumber = (WORD)j;
                        break;
                     }
               }
               else {
                  lpr.wType = 2;
                  lpr.wNumber = (WORD)i;
               }

               plv = ParseLEXPARSERULESingle (&lpr, pplpws + dwTime, dwNumWord - dwTime, &paMemScratch[0]);
               if (!plv)
                  continue;   // didn't parse

               for (j = 0; j < plv->Num(); j++) {
                  PLEXPARSEWORDSCRATCH ps = (PLEXPARSEWORDSCRATCH)plv->Get(j);

                  pNew = new CMem;
                  if (!pNew)
                     continue;
                  if (!pNew->Required (pMemCur->m_dwCurPosn + sizeof(LEXPARSEWORDSCRATCHRESULT))) {
                     delete pNew;
                     continue;
                  }
                  memcpy (pNew->p, pMemCur->p, pMemCur->m_dwCurPosn);
                  pNew->m_dwCurPosn = pMemCur->m_dwCurPosn + sizeof(LEXPARSEWORDSCRATCHRESULT);


                  // copy over new entry
                  pCur = (PLEXPARSEWORDSCRATCH)pNew->p;
                  plpwsr = (PLEXPARSEWORDSCRATCHRESULT) ((PBYTE)pNew->p + pMemCur->m_dwCurPosn);
                  *plpwsr = ps->lpwsrThis;
                  pCur->dwWords += ps->dwWords;
                  pCur->fScore += ps->fScore + (fp)ps->dwWords - 2;
                     // NOTE: Adding number of words so counteract fScore -= 1 if not at that time point
                     // NOTE: -2 means that if only added one word, score decreases by one for every "element"
                     // of CFG or individual word that's added

                  // add to list
                  plTo->Add (&pNew);
               } // j, over all parse results

            } // i
         } // dwApproach
      } // dwHyp, over plFrom
#endif // 0

      // sort the hypothesis so the highest score is on top
      qsort (plTo->Get(0), plTo->Num(), sizeof(PCMem), PCMemCompare);

#if 0 // def _DEBUG
      WCHAR szTemp[64];
      OutputDebugStringW (L"\r\n");
      for (i = 0; i < plTo->Num(); i++) {
         pNew = *((PCMem*)plTo->Get(i));
         pCur = (PLEXPARSEWORDSCRATCH)pNew->p;
         plpwsr = (PLEXPARSEWORDSCRATCHRESULT) (pCur+1);
         MemZero (pMemScratch);
         ParseResultToString (pplpws, dwNumWord,plpwsr, 
            (pNew->m_dwCurPosn - sizeof(LEXPARSEWORDSCRATCH)) / sizeof(LEXPARSEWORDSCRATCHRESULT),
            0, pMemScratch);

         swprintf (szTemp, L"\r\n%d = ", (int)i);
         OutputDebugStringW (szTemp);
         OutputDebugStringW ((PWSTR)pMemScratch->p);
      }
#endif

      // eliminate if too many
#define MAXRULEHYP      50
      if (plTo->Num() > MAXRULEHYP) {
         for (i = MAXRULEHYP; i < plTo->Num(); i++) {
            pMemCur = *((PCMem*)plTo->Get(i));
            pCur = (PLEXPARSEWORDSCRATCH)pMemCur->p;
            delete pMemCur;
         }
         plTo->Truncate (MAXRULEHYP);
      }
   } // dwTime

   // copy over the best
   pMemBest->m_dwCurPosn = 0;
   if (plTo->Num()) {
      pMemCur = *((PCMem*)plTo->Get(0));
      if (pMemBest->Required(pMemCur->m_dwCurPosn)) {
         pMemBest->m_dwCurPosn = pMemCur->m_dwCurPosn;
         memcpy (pMemBest->p, pMemCur->p, pMemCur->m_dwCurPosn);
      }
   }

   // finally, free it all up
   for (i = 0; i < 2; i++)
      for (j = 0; j < alHyp[i].Num(); j++)
         delete *((PCMem*)alHyp[i].Get(j));

   return TRUE;
}



/*************************************************************************************
CLexParse::ParseScratch - This parses the sentence looking for the best-scoring result.

inputs
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PCMem                pMemBest - If success, will be filled with m_dwCurPosn != 0, and will
                           have a LEXPARSEWORDSCRATCH structure and subsequent LEXPARSEWORDSCRATCHRESULT
                           structures filled in.
returns
   BOOL - TRUE if success
*/
BOOL CLexParse::ParseScratch (PCLexParseWordScratch *pplpws, DWORD dwNumWord, PCMem pMemBest)
{
   pMemBest->m_dwCurPosn = 0;

   CMem amemScratch[MAXRAYTHREAD];

   //CMem memCur;
   //if (!memCur.Required (sizeof(LEXPARSEWORDSCRATCH)))
   //   return FALSE;
   //memCur.m_dwCurPosn = sizeof(LEXPARSEWORDSCRATCH);  // so have starting point
   //PLEXPARSEWORDSCRATCH ps = (PLEXPARSEWORDSCRATCH) memCur.p;
   //memset (ps, 0, sizeof(*ps));

   if (!ParseScratchRecurse (pplpws, dwNumWord, /*&memCur,*/ &amemScratch[0], pMemBest)) {
      pMemBest->m_dwCurPosn = 0;
      return FALSE;
   }

   if (!pMemBest->m_dwCurPosn)
      return FALSE;

   // modify score so it's always positive
   // NOTE: I dont think I want to modify score up
   //ps = (PLEXPARSEWORDSCRATCH) pMemBest->p;
   //ps->fScore += (fp)dwNumWord;
   return TRUE;
}



/*************************************************************************************
CLexParse::ParseResultToString - Work way backwards and convert the parse
result to a MML string indicating how things were parsed.

This also sets the pplpws's m_bFinalPOS and m_ParseRuleDepth.

inputs
   PCLexParseWordScratch *pplpws - Pointer to an array of PCLexParseWordScratch for upcoming words.
   DWORD                dwNumWord - Number of entries left in pplpws
   PLEXPARSEWORDSCRATCHRESULT pResult - Results, from ParseScratch()'s pMemBEst
   DWORD                dwNumResult - Number left in pResult
   DWORD                dwPhrase - Phrase number to use. Start this off with 0, but may change as recurse
   PCMem                pMemMML - Filled with MML. This should already have been MemZero()
                           call since will just use MemCat(). This can be NULL.
returns
   BOOL - TRUE if success
*/
BOOL CLexParse::ParseResultToString (PCLexParseWordScratch *pplpws, DWORD dwNumWord,
                                     PLEXPARSEWORDSCRATCHRESULT pResult, DWORD dwNumResult,
                                     PCMem pMemMML)
{
   // loop over all these
   DWORD i, j, dwCount;
   PCLexParseGroup *ppg = (PCLexParseGroup*) m_lPCLexParseGroup.Get(0);
   PCLexParseRule *ppr = (PCLexParseRule*) m_lPCLexParseRule.Get(0);
   PCListVariable plv;
   PLEXPARSEWORDSCRATCH pws;
   PWSTR psz;
   for (i = 0; i < dwNumResult; i++, pResult++) {
      // space
      if (pMemMML && i)
         MemCat (pMemMML, L" ");

      switch (pResult->wType) {
      case 0:  // POS
      case 1:  // group
         if (pMemMML) {
            if (pResult->wType == 1) {
               MemCatSanitize (pMemMML, (PWSTR)ppg[pResult->wNumber]->m_memName.p);
               MemCat (pMemMML, L":");
            } // if group

            // show
            psz = L"808080";
            switch (pResult->wPOS) {
            case POS_MAJOR_EXTRACT(POS_MAJOR_NOUN):
               //MemCat (pMemMML, L"Noun:");
               psz = L"4040ff";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_PRONOUN):
               //MemCat (pMemMML, L"Pronoun:");
               psz = L"4040c0";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_ADJECTIVE):
               //MemCat (pMemMML, L"Adjective:");
               psz = L"4080c0";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_PREPOSITION):
               //MemCat (pMemMML, L"Preposition:");
               psz = L"8040c0";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_ARTICLE):
               //MemCat (pMemMML, L"Article:");
               psz = L"404080";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_VERB):
               //MemCat (pMemMML, L"Verb:");
               psz = L"ff0000";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_ADVERB):
               //MemCat (pMemMML, L"Adverb:");
               psz = L"c04000";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_AUXVERB):
               //MemCat (pMemMML, L"Auxiliary verb:");
               psz = L"804040";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_CONJUNCTION):
               //MemCat (pMemMML, L"Conjunction:");
               psz = L"00ff00";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_INTERJECTION):
               //MemCat (pMemMML, L"Interjection:");
               psz = L"ffff00";
               break;
            case POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION):
               //MemCat (pMemMML, L"Punctuation:");
               psz = L"808080";
               break;
            } // switch POS

            // the word
            MemCat (pMemMML, L"<font color=#");
            MemCat (pMemMML, psz);
            MemCat (pMemMML, L"><bold>");
            MemCatSanitize (pMemMML, pplpws[0]->m_pszWord);
            MemCat (pMemMML, L"</bold></font>");
         } // if pMemMML

         // set
         pplpws[0]->m_bFinalPOS = (BYTE) pResult->wPOS;

         // incrememnt pointers
         pplpws++;
         dwNumWord--;
         break;

      case 2:  // rule
         // get the rule results
         if (!pplpws[0]->RuleAlreadyParsed (pResult->wNumber, &plv))
            continue;   // shouldnt happen
         if (!plv)
            continue;   // shoudlnt happen

         // get the specific index
         pws = (PLEXPARSEWORDSCRATCH) plv->Get(pResult->wPOS);
         if (!pws)
            continue;   // shouldnt happen
         dwCount = ((DWORD)plv->Size(pResult->wPOS) - sizeof(LEXPARSEWORDSCRATCH))/sizeof(LEXPARSEWORDSCRATCHRESULT);

         // note the rule
         if (pMemMML) {
            MemCatSanitize (pMemMML, (PWSTR)ppr[pResult->wNumber]->m_memName.p);
            MemCat (pMemMML, L":(");
         }

         if (!ParseResultToString(pplpws, dwNumWord, (PLEXPARSEWORDSCRATCHRESULT) (pws+1),
            dwCount,
            pMemMML))
            return FALSE;  // error

         if (pMemMML) {
            MemCat (pMemMML, L")");
         }

         // increase the rule depth if we end up producing more than one rule, and
         // end up with multiple words
         if (dwCount && (pws->dwWords >= 2)) // only do rule depth if multiple words
            for (j = 0; j < pws->dwWords; j++) {
               // increase primary depth
               pplpws[j]->m_ParseRuleDepth.bDuring++;

               // only increase before depth if j
               if (j)
                  pplpws[j]->m_ParseRuleDepth.bBefore++;

               // only increase the subequent depth if not coming out of the rule
               if (j + 1 < pws->dwWords)
                  pplpws[j]->m_ParseRuleDepth.bAfter++;
            } // j

         // increment pointers
         pplpws += pws->dwWords;
         dwNumWord -= pws->dwWords;

         break;

      default:
         continue;   // shouldnt get
      } // switch
   } // i

   return TRUE;
}





/*************************************************************************************
CLexParse::POSGuessProp - This guesses the POS for a sentence using proabilities.

inputs
   PLEXPOSGUESS      paLPG - Pointer to an arrary of LEXPAUSEGUSS that have their
                        words initially filled in. Punctuation (such as commas) should
                        also be put in, with pszWord = ",", etc.
                        When the function returns bPOS will be filled with their guessed POS.
                        This is POS_MAJOR_XXX.

                        The wPOSBitField elements are important and MUST be filled in.

   DWORD             dwNum - Number of elements in paLPG
   PCMLexicon        pLex - Lexicon to use for pronunciation
returns
   none
*/
void CLexParse::POSGuessProb (PLEXPOSGUESS paLPG, DWORD dwNum, PCMLexicon pLex)
{
   // if nothing here then no prob
   if (!dwNum)
      return;

   // fill in all the forms
   DWORD i;
   CListFixed lBitField;
   lBitField.Init (sizeof(WORD));
   lBitField.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      lBitField.Add (&paLPG[i].wPOSBitField);

      paLPG[i].plForm = new CListVariable;
      if (!paLPG[i].plForm)
         return;
      pLex->WordPronunciation (paLPG[i].pszWord, paLPG[i].plForm, TRUE, NULL, NULL);
   }

   // get the top probabilities
   CListFixed lPCLexPOSHyp;
   CreateNGramIfNecessary ();
   m_pNGramSingle->ProbStreamBitField (lBitField.Num(), (WORD*)lBitField.Get(0), &lPCLexPOSHyp);
   PCLexPOSHyp *pphTo = (PCLexPOSHyp*)lPCLexPOSHyp.Get(0);
   PCLexPOSHyp pNew, pBest;

#if 0 // def _DEBUG
   DWORD j;
   WCHAR szwTemp[16];
   PWSTR paszPOS[POS_MAJOR_NUM+1] = {L"UNK", L"noun", L"pron", L"adj", L"prep", L"art",
      L"verb", L"adv", L"aux v", L"conj", L"inter", L"PUNCT"};

   OutputDebugString ("\r\n");
#endif

   // loop through all the hypothesis and calculate the score based on simplified POSs.
   CListFixed lTemp;
   pBest = NULL;
   for (i = 0; i < lPCLexPOSHyp.Num(); i++) {
      pNew = pphTo[i];

      // clone and apply the rules to this
      lTemp.Init (sizeof(BYTE), pNew->m_lPOS.Get(0), pNew->m_lPOS.Num());
      DWORD dwNum = lTemp.Num();
      PBYTE pPOS = (PBYTE) lTemp.Get(0);
      POSApplyRules (&dwNum, pPOS, m_lGTRULE.Num(), (PGTRULE)m_lGTRULE.Get(0));

      // get the score for this
      double fScore = m_pNGramRules->ProbStream (dwNum, pPOS);
      
      // write this in
      pNew->m_fScore = sqrt(pNew->m_fScore * fScore);

      // if this is the best one then remember
      if (!pBest || (pNew->m_fScore > pBest->m_fScore))
         pBest = pNew;

#if 0 // def _DEBUG
      if ((i < 16) || (pNew == pBest)) {
         OutputDebugString ("\r\n");

         PBYTE pabPOS = (PBYTE) pNew->m_lPOS.Get(0);
         DWORD dwCount = pNew->m_lPOS.Num();

         swprintf (szwTemp, L"%g: ", (double) pNew->m_fScore);
         OutputDebugStringW (szwTemp);

         for (j = 0; j < dwCount; j++) {
            if ((j < dwCount) && paLPG[j].pszWord)
               OutputDebugStringW (paLPG[j].pszWord);
            OutputDebugString ("(");
            OutputDebugStringW (paszPOS[pabPOS[j]]);
            OutputDebugString (") ");
         } // j

         OutputDebugString ("\r\n");
         for (j = 0; j < dwNum; j++) {
            swprintf (szwTemp, L" %d", (int)(DWORD)pPOS[j]);
            OutputDebugStringW (szwTemp);
         }

         OutputDebugString ("\r\n");
      
      }
#endif // _DEBUG
   } // i

#if 0 // def _DEBUG
   OutputDebugString ("\r\n");
#endif // _DEBUG



   // loop through the too list and fill the POS into the result
   if (pBest) {
      PBYTE pabPOS = (PBYTE) pBest->m_lPOS.Get(0);
      DWORD dwCount = pBest->m_lPOS.Num();

      for (i = 0; i < dwNum; i++) {
         if (i < dwCount)
            paLPG[i].bPOS = POS_MAJOR_MAKE(pabPOS[i]);
         else
            paLPG[i].bPOS = POS_MAJOR_PUNCTUATION; // shouldnt happen
      } // i
   } // if can write


   // free up all the hypothesis
   for (i = 0; i < lPCLexPOSHyp.Num(); i++)
      delete pphTo[i];

   // free up lists
   for (i = 0; i < dwNum; i++) {
      delete paLPG[i].plForm;
      paLPG[i].plForm = NULL;
   }
}


/*************************************************************************************
CLexParse::DialogGrammarProb - Brings up UI to analyze the grammar and fill in the NGram
database.

inputs
   PCEscWindow          pWindow - window to use
   PCMLexicon           pLex - Lexicon to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CLexParse::DialogGrammarProb (PCEscWindow pWindow, PCMLexicon pLex)
{
   // set random
   srand (GetTickCount());

   // scan the file
   PCMMLNode2 pNode = pLex->TextScan (NULL, pWindow->m_hWnd, NULL);
   if (!pNode)
      return TRUE;

   //PWSTR pszRet;

   // make text parse
   CTextParse TextParse;
   if (!TextParse.Init (pLex->LangIDGet(), pLex))
      return FALSE;


   // wipe out the current training
   CreateNGramIfNecessary ();
   m_pNGramSingle->Init (NGRAMSINGLE_VALUES, NGRAMSINGLE_HISTORY, NGRAM_EOD);
   m_pNGramRules->Init (NGRAMRULES_VALUES, NGRAMRULES_HISTORY, NGRAM_EOD);
   m_lGTRULE.Init (sizeof(GTRULE), 0, 0);

   // train using the CGramTrain object
   CGramTrain TrainOrig;
   DWORD i;
   CListFixed lLEXPOSGUESS;
   LEXPOSGUESS lpg;
   PCMMLNode2 pSub;
   PWSTR psz;
   memset (&lpg, 0, sizeof(lpg));
   {
      CProgress Progress;
      Progress.Start (pWindow->m_hWnd, "Analyzing", TRUE);

      Progress.Push (0, 0.2);
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;

         Progress.Update ((fp)i / (fp)pNode->ContentNum());

         // if find any words not in lexicon then skip
         BOOL fNotIn = FALSE;
         PCMMLNode2 pWord;
         DWORD j;
         lLEXPOSGUESS.Init (sizeof(LEXPOSGUESS));
         for (j = 0; j < pSub->ContentNum(); j++) {
            pWord = NULL;
            pSub->ContentEnum (j, &psz, &pWord);
            if (!pWord)
               continue;
            psz = pWord->NameGet();
            if (!psz)
               continue;   // skip

            // if it's marked as a word, make sure that it's in the
            // dictionary. if it isn't, then skip the entire sentence
            if (!_wcsicmp(psz, TextParse.Word())) {
               PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
               if (!psz2)
                  continue;

               if (-1 == pLex->WordFind(psz2)) {
                  fNotIn = TRUE;
                  break;
               }
            }

            // keep track of all the words found
            if (!_wcsicmp(psz, TextParse.Word()) || !_wcsicmp(psz, TextParse.Punctuation())) {
               PWSTR psz2 = pWord->AttribGetString (TextParse.Text());
               if (psz2) {
                  lpg.pszWord = psz2;
                  lpg.pvUserData = NULL;
                  lpg.wPOSBitField = pLex->WordToPOSBitField (lpg.pszWord);
                  lLEXPOSGUESS.Add (&lpg);
               }
            }

         } // j
         // if any word isn't in lexicon continue
         if (fNotIn)
            continue;


         // add this
         TrainOrig.SentenceAdd (lLEXPOSGUESS.Num(), (PLEXPOSGUESS)lLEXPOSGUESS.Get(0), pLex);

         // train the Ngram
         m_pNGramSingle->TrainStreamLEXPOSGUESS (lLEXPOSGUESS.Num(), (PLEXPOSGUESS)lLEXPOSGUESS.Get(0), 1.0);
      } // i
      Progress.Pop ();


      // go through and figure out the optimium set of rules to reduce the size
      PCGramTrain pBest;
      Progress.Push (0.2, 0.9);
#define RULEBLOCKSIZE         16       // number of rules to calculate in each block
      pBest = TrainOrig.DiscoverNewRules (RULEBLOCKSIZE,
         (NGRAMRULES_VALUES - 16 /*POS*/) / RULEBLOCKSIZE,
         pLex, &m_lGTRULE, &Progress);
      Progress.Pop ();

      Progress.Push (0.9, 1);


      // train all the new rules into NGram
      Progress.Update (0);
      DWORD dwIndex = 0;
      do {
         DWORD dwWords;
         double fScore;
         PBYTE pabPOS = pBest->SentenceGet (dwIndex, &dwWords, &fScore, &dwIndex);
         if (!pabPOS)
            break;

         // train this
         m_pNGramRules->TrainStream (dwWords, pabPOS, fScore);
      } while (dwIndex);
      delete pBest;

      // train n-grams
      Progress.Update (.3);
      m_pNGramSingle->TrainingApply ();

      Progress.Update (.6);
      m_pNGramRules->TrainingApply ();
      Progress.Pop ();
   } // for analysis UI
   delete pNode;  // clear out since dont need

   // if have less than 10000 sentences (1000 words) then ask for more
   // m_fDirty = TRUE;
   if (m_pNGramSingle->m_fTrainingCount < 300000)
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Grammar training is finished for the file, but you should train more.",
         L"You don't have enough training yet. Try analyzing different text files.",
         MB_ICONINFORMATION | MB_OK);
   else
      EscMessageBox (pWindow->m_hWnd, ASPString(),
         L"Grammar training is finished for the file.",
         L"You appear to have enough training for text-to-speech to work well.",
         MB_ICONINFORMATION | MB_OK);

   return TRUE;
}



#if 0
/*************************************************************************************
CLexParse::Test - test function
*/
void CLexParse::Test (PCMLexicon pLex)
{
   CListFixed l;
   l.Init (sizeof(PCLexParseWordScratch));

   PWSTR apsz[] = {L"mike", L",", L"silly", L"this", L"is", L"a", L"sickly", L",", L"friendly", L",", L"happy", L"test", L"."};
   DWORD i;
   PCLexParseWordScratch pNew;
   for (i = 0; i < sizeof(apsz)/sizeof(PWSTR); i++) {
      pNew = new CLexParseWordScratch;
      pNew->Init (apsz[i], iswpunct (apsz[i][0]) ? (1 << 0x0b) : 0, this, pLex);
      l.Add (&pNew);
   } // i

   // parse this
   CMem memBest;
   ParseScratch ((PCLexParseWordScratch*)l.Get(0), l.Num(), &memBest);

   // output
   CMem memOut;
   if (memBest.m_dwCurPosn) {
      MemZero (&memOut);
      PLEXPARSEWORDSCRATCHRESULT psr = (PLEXPARSEWORDSCRATCHRESULT) ((PLEXPARSEWORDSCRATCH)memBest.p + 1);
      ParseResultToString ((PCLexParseWordScratch*)l.Get(0), l.Num(),
         psr, (memBest.m_dwCurPosn - sizeof(LEXPARSEWORDSCRATCH)) / sizeof(LEXPARSEWORDSCRATCHRESULT),
         0, &memOut);
      OutputDebugString ("\r\n");
      OutputDebugStringW ((PWSTR)memOut.p);
   }


   // free
   for (i = 0; i < l.Num(); i++)
      delete *((PCLexParseWordScratch*) l.Get(i));
}
#endif

/*************************************************************************************
CLexParseWordScratch::Constructor and destructor
*/
CLexParseWordScratch::CLexParseWordScratch (void)
{
   m_pLexParse = NULL;
   m_pLex = NULL;
   m_pszWord = NULL;
   m_dwPOSFlags = 0;
   m_dwPOSPreferred = (DWORD)-1;
   m_padwGroup = NULL;
   m_padwRule = NULL;
   m_hRulePCListVariable.Init (sizeof(PCListVariable));
   m_hGroupPCListVariable.Init (sizeof(PCListVariable));
   m_bFinalPOS = 0;
   memset (&m_ParseRuleDepth, 0, sizeof(m_ParseRuleDepth));
}

CLexParseWordScratch::~CLexParseWordScratch (void)
{
   // free up the variable lists
   DWORD i;
   for (i = 0; i < m_hRulePCListVariable.Num(); i++) {
      PCListVariable pl = *((PCListVariable*)m_hRulePCListVariable.Get (i));
      delete pl;
   } // i
   m_hRulePCListVariable.Clear();

   // free up the group lists
   for (i = 0; i < m_hGroupPCListVariable.Num(); i++) {
      PCListVariable pl = *((PCListVariable*)m_hGroupPCListVariable.Get (i));
      delete pl;
   } // i
   m_hGroupPCListVariable.Clear();
}



/*************************************************************************************
CLexParseWordScratch::Init - Call this to initializes the temporary word information.

inputs
   PWSTR          pszWord - Word string
   DWORD          dwPOSFlags - part-of-speech flags that are acceptable as a bit-field. If this is 0
                           then figure out the POS flags
   DWORD          dwPOSPreferred - Preferred part of speech, from the probability. 0..16, or -1 if dont have one
   PCLexParse     pLexParse - Parse lexicon
   PCMLexicon     pLex - Lexicon for word cases
returns
   BOOL - TRUE if success
*/
#define NOTPREFERREDSCORE              -0.2
BOOL CLexParseWordScratch::Init (PWSTR pszWord, DWORD dwPOSFlags, DWORD dwPOSPreferred,
                                 PCLexParse pLexParse, PCMLexicon pLex)
{
   if (m_pszWord)
      return FALSE;

   m_pszWord = pszWord;
   m_pLexParse = pLexParse;
   m_pLex = pLex;
   m_dwPOSFlags = dwPOSFlags;
   m_dwPOSPreferred = dwPOSPreferred;

   // figure out parts of speech is not set
   DWORD i, j;
   if (!m_dwPOSFlags) {
      CListVariable lForm, lDontRecurse;
      pLex->WordPronunciation (pszWord, &lForm, TRUE, pLex, &lDontRecurse);

      for (i = 0; i < lForm.Num(); i++) {
         BYTE bPOS = *((PBYTE)lForm.Get(i));
         m_dwPOSFlags |= 1 << POS_MAJOR_EXTRACT(bPOS);
      } // i
   }

   // small buffer for group info
   BYTE abTemp[sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT)];
   PLEXPARSEWORDSCRATCH plpws = (PLEXPARSEWORDSCRATCH)&abTemp[0];
   PLEXPARSEWORDSCRATCHRESULT plpwsr = (PLEXPARSEWORDSCRATCHRESULT)(plpws+1);
   memset (abTemp, 0, sizeof(abTemp));

   // fill in m_alvWord for all parts of speech
   for (i = 0; i < 16; i++) {
      if (!(m_dwPOSFlags & (1 << i)))
         continue;   // no POS

      plpws->lpwsrThis.wType = 0;
      plpws->lpwsrThis.wPOS = (WORD)i;
      plpws->dwWords = 1;
      plpws->fScore = (i == m_dwPOSPreferred) ? 0 : NOTPREFERREDSCORE;  // penalize if not preferred
      plpwsr->wPOS = (WORD)i;
      plpwsr->wType = 0;
      // NOTE: Intentionally only taking the first POS and ignoring the rest
      // since they'll have no bearing on later parts

      m_alvWord[i].Add (plpws, sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT));
   } // i

   // allocate memory for the group and rule bits
   DWORD dwGroup = (m_pLexParse->m_lPCLexParseGroup.Num() + 31) / 32;
   DWORD dwRule = (m_pLexParse->m_lPCLexParseRule.Num() + 31) / 32;
   if (!m_mem.Required ((dwGroup + dwRule) * sizeof(DWORD)))
      return FALSE;
   m_padwGroup = (DWORD*)m_mem.p;
   m_padwRule = m_padwGroup + dwGroup;
   memset (m_padwGroup, 0, (dwGroup + dwRule) * sizeof(DWORD));

   // loop through all the groups and see which ones this word is part of
   PCLexParseGroup *pplpg = (PCLexParseGroup*)m_pLexParse->m_lPCLexParseGroup.Get(0);
   for (i = 0; i < m_pLexParse->m_lPCLexParseGroup.Num(); i++) {
      DWORD dwRet = pplpg[i]->WordPartOfGroup (m_pszWord, m_dwPOSFlags);
      if (!dwRet)
         continue;

      plpws->lpwsrThis.wType = 1;
      plpws->lpwsrThis.wNumber = (WORD)i;
      plpws->dwWords = 1;
      plpwsr->wNumber = (WORD)i;
      plpwsr->wPOS = plpws->lpwsrThis.wPOS = 0;
      plpwsr->wType = 1;
      if ((m_dwPOSPreferred < 16) && (dwRet & (1 << m_dwPOSPreferred))) {
         // have a preferred match
         plpws->fScore = 0;
         plpwsr->wPOS = plpws->lpwsrThis.wPOS = (WORD)m_dwPOSPreferred;
      }
      else {
         // pick one
         plpws->fScore = NOTPREFERREDSCORE;
         for (j = 0; j < 16; j++)
            if (dwRet & (1 << j)) {
               plpwsr->wPOS = plpws->lpwsrThis.wPOS = (WORD)j;
               break;
            }
      }

      PCListVariable pl = new CListVariable;
      if (!pl)
         continue;   // shouldnt happen
      pl->Add (plpws, sizeof(LEXPARSEWORDSCRATCH) + sizeof(LEXPARSEWORDSCRATCHRESULT));

      m_hGroupPCListVariable.Add (i, &pl);
      m_padwGroup[i/32] |= (1 << (i%32));
   } // i

   return TRUE;
}


/*************************************************************************************
CLexParseWordScratch::RuleAlreadyParsed - Checks to see if a rule is already parsed.

inputs
   DWORD          dwRule - Rule number
   PCListVariable *ppList - If the rule is already parsed, this is either filled with
                  the results of the parse (see m_hRulePCListVariable), or NULL to indicate
                  that the rule doesn't fit
returns
   BOOL - TRUE if a parse has already been attempted. FALSE if none has yet
*/
BOOL CLexParseWordScratch::RuleAlreadyParsed (DWORD dwRule, PCListVariable *ppList)
{
   *ppList = NULL;   // to clear

   // see if flag set
   if (!(m_padwRule[dwRule/32] & (1 << (dwRule%32))))
      return FALSE;  // hasn't bee parsed yet

   // else, look in the hash
   PCListVariable *ppl = (PCListVariable*) m_hRulePCListVariable.Find (dwRule);
   if (ppl)
      *ppList = *ppl;   // found match
   return TRUE;
}




/*************************************************************************************
CLexParseWordScratch::RuleParsed - Tells the temporary record that a rule has been
parsed, and has it keep a record of the parse.

inputs
   DWORD             dwRule - Rule number
   PLEXPARSEWORDSCRATCH pInfo - Parse information. This can be NULL to indicate that the parse failed
   DWORD             dwNumResult - Number of result structures appended onto end of PLEXPARSEWORDSCRATCH
retuirns
   none
*/
void CLexParseWordScratch::RuleParsed (DWORD dwRule, PLEXPARSEWORDSCRATCH pInfo, DWORD dwNumResult)
{
   // set the flag
   m_padwRule[dwRule/32] |= (1 << (dwRule%32));

   // if there's no info then done
   if (!pInfo)
      return;

   // need to append the info
   PCListVariable *ppl = (PCListVariable*) m_hRulePCListVariable.Find (dwRule);
   if (ppl) {
      // append
      pInfo->lpwsrThis.wPOS = (WORD)ppl[0]->Num(); // to keep trackof the number
      ppl[0]->Add (pInfo, sizeof(*pInfo) + dwNumResult * sizeof(LEXPARSEWORDSCRATCHRESULT));
      return;
   }

   // else, create new list
   PCListVariable pl = new CListVariable;
   if (!pl)
      return;  // error. shouldnt happen
   pInfo->lpwsrThis.wPOS = (WORD)pl->Num(); // to keep trackof the number
   pl->Add (pInfo, sizeof(*pInfo) + dwNumResult * sizeof(LEXPARSEWORDSCRATCHRESULT));
   m_hRulePCListVariable.Add (dwRule, &pl);
}


