/*************************************************************************************
CTTSWork - Object used to create the tts voice. Not the tts voice itself.

begun 21/9/03 by Mike Rozak.
Copyright 2003 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <crtdbg.h>
#include <float.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "m3dwave.h"


// #define NOMODS       // turn off modifications to the voice
#ifdef NOMODS
#define NOMODS_CLEANSRFEATURE          // dont clean the SRFEATURES up
// #define NOMODS_ENERGYPERVOLUMETUNE  // fine-tune energy per volume to keep same energy afterwards
#endif

// BUGFIX - Halve this from 1.0 to 0.5 to see if sounds better
#define MISLABELSCALE            0.5   // how much to scale phonemes that are determined to be mislabeled
//#define MISLABELSCALE            1.0   // how much to scale phonemes that are determined to be mislabeled

// BUGFIX - Don't disable bottom scores since ASR accuracy and algorithms should be good enough to handle
// NOTE: When remove this and do BDL voice, "algorithms" sounds like "owe gee rhythms".
//#define NOMODS_DISABLEBOTTOMSCORES     // disable units from the bottom 25% of the triphones

// BUGFIX - Put this in because taking it out made things worse
// BUGFIX - Disable this since less of a hack
// NOTE: Not sure if this is the right thing to do. Right on the line since using units from
// all function words causes problems too.
// NOTE: When remove this and do BDL voice, "algorithms" starts to sound funny
#define NOMODS_UNDERWEIGHTFUNCTIONWORDS   // if defined, then underweight function words

// BUGFIX - Disable this since less of a hack
// #define NOMODS_MISCSCOREHACKS          // if defined, then miscellaneous score hacks are enabled

// BUGFIX - Disable this since theorically right
// #define NOMODS_TRIMLEFTRIGHT       // if defined, then enable trimming left/right bits off phonemes

// BUGFIX - Put this in so that slightly better training for end-of-word units
#define NOMODS_COMBINEWORDPOSUNTILTHEEND  // if defined, then word positions will be combined together until the specific triphones are created

// BUGFIX - Turn this off just in case signficantly affecting voices, since NOT minor
// effect. Blizzard voice came out lousy, but probably because of bug, but still putting off
// BUGFIX - Reenabling to test out
// #define NOMODS_ENERGYPERVOLUMETUNE  // fine-tune energy per volume to keep same energy afterwards


// #define RECALCSRFEATUREHACK

// BUGFIX - Put back in since have more memry
// #define DONTTRAINDURATION              // don't do SR model for different durations as an optimization, so faster building voice (less memory)

#ifdef DONTTRAINDURATION
#define DURATIONFIDELITYHACKSMALL      1
#else
#define DURATIONFIDELITYHACKSMALL      DURATIONFIDELITY
#endif

#define PITCHFIDELITYCENTER            1 // center of pitch fidelity
#define PITCHFIDELITY                  (PITCHFIDELITYCENTER*2+1)        // number of copies of phonemes based on pitch
#define PITCHFIDELITYPLUSONE           (PITCHFIDELITY+1)    // extra fidelity bin for "all"
#define OCTAVESPERPITCHFILDELITY       1.25        // Each pitch-fidelity point is centered around here, relative to central pitch
                                          // BUGFIX - Was 1.3. Lowered to 1.25 since gives a 1.25^3 = .95 octave range for speech

#define DURATIONFIDELITYCENTER         1  // center of duration fidelty
#define DURATIONFIDELITY               (DURATIONFIDELITYCENTER*2+1)     // number of copies of phoneme to store based on duration
#define DURATIONFIDELITYPLUSONE        (DURATIONFIDELITY+1)    // extra fidelity bin for "all"
#define SCALEPERDURATIONFIDELITY       1.5      // amount to scale for each duration length

#define ENERGYFIDELITYCENTER           1 // center of energy fidelity
#define ENERGYFIDELITY                 (ENERGYFIDELITYCENTER*2+1)     // number of copies of phoneme to store based on duration
#define ENERGYFIDELITYPLUSONE          (ENERGYFIDELITY+1)    // extra fidelity bin for "all"
#define SCALEPERENERGYFIDELITY         2.0      // amount to scale for each duration length

#define UNDEFINEDRANK                  1000000.0  // so know if not used
#define WEIGHTUNVOICEDFORPITCH         0.00001  // BUGFIX - Used to be 0.01

#define SRTRAINWEIGHTSCALE             2.0      // extra weight for training since will attentuate a lot in places

#define HYPVOICEDENERGYPENALTY         12        // penality applied for voiced/unvoiced mismatch, in dB
            // BUGFIX - Upped from 6 db to 12 db since some mismatches still
#define HYPPLOSIVENESSPENALTY          10       // 10 dB penalty for wrong sort of plosivness
#define HYPFUNCWORDPENALTY             10       // 10 dB penalty for phonemes from function words
#define HYPBRIGHTNESSPENALTY           6        // penalty applied to the "dullest" version.
                  // BUGFIX - Was 12, but upped to 24 to ensure bright formants
                  // BUGFIX - 24 seemed a bit high, so reduced down to 18
                  // BUGFIX - 18 too high. Reduced to 6, will have slight effect, but since it's a hack, not too much

// #define NOARTIFICALRANKS         // use "purer" rankings
   // NOTE: Tried the NOARTIFICALRANKS but still sounds better the old way, even with the female

#define EIGHTGIG                 8000000000        // 8 gigabytes of ram

#define MINNUMEXAMPLES           m_dwMinExamples           // minumum number of examples before use
#define MINIMUMSYLLABLES         MINNUMEXAMPLES           // minimum number of syllables before use
#define PARENTCATEGORYWEIGHT     10          // weight that parent categroy gets when determining energy, etc.
   // BUGFIX - Uppsed from 5 to 10 because forgot that training of SRModels is sparse, with
   // data spread out over 3x3x3 matrix
   // BUGFIX - Changed from 10 to 5
   // BUGFIX - Was 30, but I think this is too high, so changed to 15
   // BUGFIX - Changed from 15 to 10
   // BUGFIX - Changed to 50 (was MINIMUMEXAMPLES*2), so that have fairly consistent weights,
   // which should cause a more even timing, especially for voices without much data
   // BUGFIX - Was 50, but reduced to 10 since don't think would be enough data even for huge voice
   //    this way, if have 10 units, parent score counts half and new units count half

#define MAXTARGETCOSTVALUE       (SRCOMPAREWEIGHT*2.0)         // target costs can't be any more than this, just in case

#define AMUUNITSPERPHONEME       (SRSAMPLESPERSEC / 10)     // assume that average phoneme is 1/10th of a second

#define MISMATCHPARENTTHEORYWEIGHT        0.2      // weight parent score, but not as much as theory

#define ENERGYAVGWEIGHT_VOICED      1.0
#define ENERGYAVGWEIGHT_PLOSIVE     0.2
#define ENERGYAVGWEIGHT_UNVOICED    0.1

#define INCLUDEALLUNITS    1000000     // if m_dwTotalUnits >= INCLUDEALLUNITS then include all units

#define MAXPITCHRANGE      1000     // maximum range allowed for pitch variations
                                    // BUGFIX - Was 500 for chinese

#define MAXPHONETRIM       20       // maximum percent that can trim off a phoneme

#define PITCHMAXFEW        1.3      // add if > 30% pitch difference from other one
#define DURATIONMAXFEW     2.0      // add if > 100% duration difference from the other one

#define MINPAUSECOUNT(x)         ((x) * 3 / 4)

#define MNLPREDOVOWELSTXT           "c:\\temp\\MNLPRedoVowels.txt"      // filename

#define JOINPSOLAHARMONICS          256      // how many harmonics to calculate
#define HALFOCTAVEFORPCM            0.5      // how much to raise/lower and simualte PCM shift by

// MISMATCHSTRUCT - Used for sorting triphone mismatch scores
typedef struct {
   fp             fCompare;      // score. Lower is sorted earlier on list
   fp             afScore[TTSDEMIPHONES]; // scores
   fp             afScoreWeightWith[TTSDEMIPHONES];   // weight with tis
   DWORD          dwValue;       // value
   PVOID          pVoid;         // value
} MISMATCHSTRUCT, *PMISMATCHSTRUCT;

// UNITGROUPCOUNTSCORE - Infomration about a specific score in UNITGROUPCOUNT
typedef struct {
   float          fScore;        // score of the best one
   WORD           wWave;         // wave number (of the best)
   WORD           wPhoneIndex;   // phoneme index (of the best)
} UNITGROUPCOUNTSCORE, *PUNITGROUPCOUNTSCORE;

#define MAXUGCS            10     // keep the top 5 versions of a unit
   // BUGFIX - Changed from 5 to 10 to allow for better annealing
#define PUGTOTRY           3     // try (at most) the top third of the units to keep
#define UGCPASSES          10    // number of passes used to minimize the number of units
         // BUGFIX - Upped from 10 to 20 since seems to still be advancing fairly well at 10
         // BUGFIX - Restore to 10 since no real change between 10 and 20 on any voices

// UNITGROUPCOUNT - For counting the number of times a group
// of units occur
typedef struct {
   QWORD          qwPhonemes;    // array of 8 phonemes. +1 to the basic phoneme number, so 0 is end
   QWORD          qwWordPos;     // array of 8 word pos bit fields. 0x01 indicates at start of word, 0x02 at end of word
   DWORD          dwCount;       // number of times have seen this particular combination
   double         fScoreCount;   // score based on count and guestimated error incurred by not having unit
   BYTE           bNumPhones;    // number of phonemes to include, since not necessarily same as dwPhones
   BYTE           bPitchFidelity;   // pitch fidelity bin that this is associated with, used along with qwPhonemes for sort
   BYTE           bDurationFidelity;   // duration fidelty bind that this is associated with, used along with qwPhonemes for sort
   BYTE           bEnergyFidelity;  // energy fidelity bin that this is associated with, used along with qwPhonemes for sort

   DWORD          dwUGCSCount;      // number of entries in aUGCS
   DWORD          dwUGCSBest;       // which index into aUGCS is the one that's used
   UNITGROUPCOUNTSCORE aUGCS[MAXUGCS]; // top-N scores, sorted so lowest(best) scores are first
} UNITGROUPCOUNT, *PUNITGROUPCOUNT;

//#define UNITGROUPCOUNTSCORE_PLOSIVE    8     // plosives are important to have
//#define UNITGROUPCOUNTSCORE_VOICED     4     // voiced (non-plosive) are next most important
//#define UNITGROUPCOUNTSCORE_UNVOICED   2     // unvoiced (non-plosive) are least important since easy to mix and match
//#define UNITGROUPCOUNTSCORE_STANDALONE (UNITGROUPCOUNTSCORE_UNVOICED)     // stand-alone phones are important to have as backoff
            // BUGFIX - Used to be 8, but since stand-alone's are supposed
            // to be just as interchangable as unvoiced units, then
            // should be the same as unvoiced
            // BUGFIX - need to multiply by *2 since potentially multiplying by 2 for
            // other UNITGROUPInsert if fAtStartEnd

// TRIPHONETRAIN - Used to train up triphones
typedef struct {
   DWORD          dwCountScale;    // number of triphones used to calculate this. Note: This
                                    // is scaled by NUMFUNCWORDGROUP+1. So if 2 phonemes, then will
                                    // be 2*(NUMFUNCWORDGROUP+1).
   fp             afSRScoreMedian[TTSDEMIPHONES]; // median SR score (using original SR training)
   DWORD          dwDuration; // median duration
   DWORD          dwDurationSRFeat; // median duration in SR features
   fp             fEnergyMedian; // median average energy
   fp             fEnergyMedianHigh;   // energy that's higher than the median, to try to select brighter units
   fp             fEnergyRatio;  // median ratio of phoneme energy to word energy
   fp             fPitch;     // median pitch, in hz
   int            iPitch;     // median pitch value
   int            iPitchDelta;   // median pitch delta
   int            iPitchBulge;   // amount center bulges up/down
   PCPhoneme      apPhoneme[PITCHFIDELITY][DURATIONFIDELITY][ENERGYFIDELITY][TTSDEMIPHONES];   // phoneme that trained with, for different pitch points,
                                 // [0] = center, [1]= mid, [2] = high
} TRIPHONETRAIN, *PTRIPHONETRAIN;



// WEMPH - word emphasis information
#define MAXSYLLABLES    16    // maximum number of syllables that can be stored in a word

// WORDAN - Word analysis saved in CWaveAn
typedef struct {
   DWORD             dwIndexInWave; // word index as is appear in the wave
   DWORD             dwSubSentenceNum; // sentence number within the wave
   DWORD             dwSentenceType;   // from TYPICALSYLINFO_XXX

   DWORD             dwTimeStart; // start time in SRFETURE times
   DWORD             dwTimeEnd;  // end time in SRFEATURE times

   DWORD             dwPhoneStart;  // start phoneme index #
   DWORD             dwPhoneEnd; // ending phoneme index #
   DWORD             dwSylStart;    // starting syllable index #
   DWORD             dwSylEnd;      // ending syllable index #

   PWSTR             pszWord;    // word string, from the wave file's info
   fp                fFuncWordWeight;  // weighting if function word
   DWORD             dwFuncWordGroup;  // group the function word belongs to, 0..NUMFUNCWORDGROUP (inclusive)
   DWORD             dwWordIndex;   // index into lexicon
   DWORD             dwForm;     // form used from the lexicon

   fp                fPitch;     // calculated pitch
   fp                fEnergyAvg; // average energy (per SRFeature)

   CPoint            pWrdEmph;   // filled in by WordEmphTrainSyl()
} WORDAN, *PWORDAN;

typedef struct {
   PWORDAN        pwa;        // WORDAN where this came from
   PWSTR          pszWord;    // word string... note that this points to a string controled by pWave,
                              // so if pWave is destroyed this will be invalid
   CPoint         pWrdEmph;      // amount of emphasis, .p[0] = pitch scale, .p[1] = volume scale, .p[2] = duration scale
   WCHAR          wPunctLeft; // nearest punctuation to the left
   WCHAR          wPunctRight; // nearest punctuation to the right
   DWORD          dwPunctLeftDist;  // number of words away for left punct. 0 => immediately left
   DWORD          dwPunctRightDist; // number of words away for right punct. 0 => immediately right
   DWORD          dwPhoneStart;  // phoneme start (index into wave)
   DWORD          dwPhoneEnd;    // phoneme end (index into wave)
   DWORD          dwWordIndex;   // index into the wave

   DWORD          dwNumSyl;      // number of syllables i aSYLEMPH
   SYLEMPH        aSYLEMPH[MAXSYLLABLES]; // syllables

   PARSERULEDEPTH ParseRuleDepth;   // detailed rule depth
   PARSERULEDEPTH ParseRuleDepthNext;  // so can handle punctiation. This is the rule depth of the rule immediately followning
   BYTE           bPOS;          // part of speech in low nibble
   BYTE           bRuleDepthLowDetail;    // rule depth information
   BYTE           bPauseLeft;    // set to TRUE if there's a pause between this word and the one to the left
   BYTE           bFill;  // just fill
} WEMPH, *PWEMPH;


// PHONEBLACK - Infomration about phoneme stored on blacklist
typedef struct {
   WORD           wWaveIndex;       // wave recording
   WORD           wPhoneIndex;         // unit that's blacklisted. Phoneme index into dwWave
                                // if hibit==1 then it's a phoneme not necessarily in a word
                                // if hibit==0 then it's a phoneme marking the start of a word
} PHONEBLACK, *PPHONEBLACK;

// TPHONECOUNT - Used to keep track of number of instances of a triphone
typedef struct {
   BYTE           bPhoneLeft;       // phoneme to the left
   BYTE           bPhoneRight;      // phoneme to the right
   WORD           wTriPhone;        // combination of left and right
   DWORD          dwCount;          // count
} TPHONECOUNT, *PTPHONECOUNT;

// TTSMP - For storing information passed to dialog
typedef struct {
   PCTTSWork            pTTS;       // TTS working on
   PWSTR                pszRecOnly; // if record only one phrase then this points to it
   PCMem                memRecOnly; // memory where to store reconly

   // for the ttsmainaddmany page
   PCListVariable       plToAdd;
   PCListVariable       plAlreadyExistInToDo;
   PCListVariable       plAlreadyExistInWave;
   PCListVariable       plHaveMisspelled;
   PCListVariable       plHaveUnknown;
   PCMLexicon           pLexUnknown;
} TTSMP, *PTTSMP;

// CSORT - Used for sorting words by most frequent
typedef struct {
   PWSTR          pszWord;    // word string
   DWORD          dwCount;    // word count
} CSORT, *PCSORT;


// SYLAN - Syllable analysis for CWaveAn
typedef struct {
   DWORD             dwIndexInWave; // syllable index as is appear in the wave
   DWORD             dwSubSentenceNum; // sentence number within the wave
   DWORD             dwSentenceType;   // from TYPICALSYLINFO_XXX
   DWORD             dwIndexIntoSubSentence; // syllable index into the subsentence
   DWORD             dwSyllablesInSubSentence;  // number of syllables in the subsentence
   fp                fDurationScale;   // how much the syllable duration is to be scaled in
                                       // order to "normalize" the sub-sentence length.
                                       // this is initially filled with the duration and changed to scale later

   DWORD             dwTimeStart; // start time in SRFETURE times
   DWORD             dwTimeEnd;  // end time in SRFEATURE times

   DWORD             dwPhoneStart;  // start phoneme index #
   DWORD             dwPhoneEnd; // ending phoneme index #
   DWORD             dwWordIndex;   // word index that this is part of
   DWORD             dwVoiceStart; // phone where the voice section start
   DWORD             dwVoiceEnd;    // phone where the voiced section ends

   fp                fPitch;     // calculated pitch
   fp                fPitchDelta;   // change in pitch over syllable, pitch right / pitch left
   fp                fPitchBulge;   // amount center bulges up/down
   fp                fEnergyAvg; // average energy (per SRFeature)

   BYTE              bMultiStress;    // stress amount, 0 for none, 1 for primary, 2 for secondary, 3+ for chinese
   BYTE              bSylNum;    // syllable number... index into word

   TYPICALSYLINFO    TSI;        // syllable information that's calculated and filled in
} SYLAN, *PSYLAN;



// PHONEAN - Phoneme analysis saved in CWaveAn
typedef struct {
   //DWORD           dwIndexInWave; // NOTE: Phoneme index in m_lPHONEAN is same as appears in wave
   PWORDAN           pWord;         // word that is from, or NULL if from a silence one

   DWORD             dwTimeStart; // start time in SRFETURE times
   DWORD             dwTimeEnd;  // end time in SRFEATURE times
   DWORD             dwTrimLeft; // number of SRFEATURES that can trim from left to maximimize score
   DWORD             dwTrimRight; // number of SRFEATURES that can trim from right to maximimize score

   fp                fPitch;     // calculated pitch
   fp                fEnergyAvg; // average energy (per SRFeature)
   fp                fEnergyRatio;// fEnergyAvg / word's fEnergyAvg

   WORD              awTriPhone[NUMTRIPHONEGROUP];  // triphone id given type of grouping
   BYTE              bPhone;     // phoneme
   BYTE              bPhoneLeft;    // phoneme to left
   BYTE              bPhoneRight;   // phoneme to right
   BYTE              bWordPos;   // location within word, 0..3 for flags that indicate if
                                 // at start or end of word, etc.
   short             iPitch;     // how much pitch is higher/lower than surrounding word.
                              // 0 = no change, 1000 = 1 octave higher, -1000 = 1 octave lower
   short             iPitchDelta;   // change in pitch over wave, same units as iPitch
   short             iPitchBulge;   // amount pitch bulges up/down
   DWORD             dwWord;     // word number, index into CTTSWork::m_pLexWords, or -1 if not in one of those
   fp                fPitchDelta;  // ratio of right pitch over left pitch
   fp                fPitchBulge;   // amount pitch bulges up/down
   fp                fPitchStrength;   // how strong the pitch detect was, identify poor pitch detect
   fp                fVoicedEnergy; // amount of energy to voiced sound
   fp                fPlosiveness;  // how much corresponds to plosiveness looking for, lower numbers better
   fp                fBrightness;   // brightness, to encourage better defined formants
   BOOL              fIsPlosive; // set to TRUE if the sound is from a plosive, false if from non-plosive
   BOOL              fIsVoiced;     // set to TRUE if the phoneme is voiced
   DWORD             dwDuration; // duration (in samples)

   fp                fSRScoreGeneral;  // speech recognition score, but not trained to specific triphone
   fp                afSRScorePhone[TTSDEMIPHONES]; // score from specific phoneme
   fp                afSRScoreMegaPhone[TTSDEMIPHONES];   // SR score from comparison of megaphone
   fp                afSRScoreMegaPhoneUnique[TTSDEMIPHONES];   // this is LOWER if the unit sounds less like any other phonemes (ie: less mistakable)
   fp                afSRScoreGroupTriPhone[TTSDEMIPHONES];   // score specific to the tri-phone group that trained on
   fp                afSRScoreSpecificTriPhone[TTSDEMIPHONES];   // score specific to the tri-phone group that trained on
   fp                afSRScoreWeighted[TTSDEMIPHONES]; // weighted. If lots of this triphone then use fSRScoreTriPhone, else fSRScoreMegaPhone
   MISMATCHSTRUCT    aMMSMegaGroup[TRIPHONEMEGAGROUPMISMATCH]; // mismatch scores for the magagroup
   MISMATCHSTRUCT    aMMSGroup[TRIPHONEGROUPMISMATCH];    // mismatch scores for the triphone
   MISMATCHSTRUCT    aMMSSpecific[TRIPHONESPECIFICMISMATCH];    // mismatch scores for the triphone
   DWORD             dwSpecificMismatchAccuracy;  // if 0 then triphone of group, 1 then unstressed, 2 then full

   fp                afRankCompare[PITCHFIDELITY][DURATIONFIDELITY][ENERGYFIDELITY][TTSDEMIPHONES];  // rank used for internal comparisons, in dB.
                                    // different values based on the pitch

   fp                afRankAdd[TTSDEMIPHONES];      // rank used for adding, in dB
   BYTE              abRankAdd[TTSDEMIPHONES];      // rank used for adding, in RANKDBSCALE dB
   MISMATCHINFO      aMMIAdd[TRIPHONESPECIFICMISMATCH];    // mismatch information to add

   DWORD             dwWantInFinal;  // count of number of candidates that want in the final
   DWORD             dwWantInFinalAudio;  // include surrounding audio
   BOOL              fBad;          // set to TRUE if unit is bad and shouldnt be used
   DWORD             dwLexWord;     // lexicon word to use, or -1 if not associated with lexicon

   DWORD             dwTRIPHONETRAINIndex;   // index into pAnal->plTRIPHONETRAIN,
                                    // or -1 if not defined
} PHONEAN, *PPHONEAN;



// TPHONEINST - Instance information of triphone for use when analyzing
typedef struct {
   // NOTE: Central phone number implied
   PPHONEAN       pPHONEAN;        // phone analysis information

   BYTE           bPhoneLeft;      // phoneme to left
   BYTE           bPhoneRight;     // phoneme to right
   WORD           wTriPhone;       // triphone id
   //short          iPitch;     // how much pitch is higher/lower than surrounding word.
                              // 0 = no change, 1000 = 1 octave higher, -1000 = 1 octave lower
   //short          iPitchDelta;   // change in pitch over wave, same units as iPitch
   //DWORD          dwWord;     // word number, index into CTTSWork::m_pLexWords, or -1 if not in one of those

   DWORD          dwWave;     // wave number, index into CTTSWork::m_pWaveDir
   DWORD          dwPhoneIndex;  // phone index into PCM3DWave::m_lWVPHONEME from dwWave
} TPHONEINST, *PTPHONEINST;


// WORDINST - Instance information of a word for use when analyzing
typedef struct {
   // NOTE: Central word number implied
   DWORD          dwWave;     // wave number, index into CTTSWord::m_pWaveDir
   PWORDAN        pWORDAN;    // word analysis information
   // DWORD          dwWordIndex;// index into PCM3DWave::m_lWVWORD from dwWave
   // DWORD          dwPhoneStart;  // start phone number in CM3DWave::m_lWVPHONEME
   // DWORD          dwPhoneEnd; // end phone number (exclusive) in CM3DWave::m_lWVPHONEME
   // DWORD          dwForm;     // form number, from CMLexicon::WordPronunciation - since might have "the" = "thee" or "thuh"
   BOOL           fUsedAsBest;   // set to TRUE if this one used as best
} WORDINST, *PWORDINST;

// WAVEANINFO - Wave analysis info
//typedef struct {
//   PCM3DWave      pWave;      // wave
//   fp             fMaxEnergy; // maximum energy in the total wave
//} WAVEANINFO, *PWAVEANINFO;



// TTSREV - TTS review UI
typedef struct {
   PTTSANAL       pAnal;      // analysis information
   PCMTTS         pTTS;       // TTS to use
   PCTTSWork      pWork;      // work in progress
   PCM3DWave      pWaveOrig;  // original wave, no resyntheiss at all
   PCM3DWave      pWaveVocal; // original wave with vocal tract resynhthesized
   PCM3DWave      pWaveTrans; // original wave with transplanted prosody
   WCHAR          szSelected[512];  // currently selected wave
   WCHAR          szTTSSpeak[512];  // what's in the tts speak string
   int            iScroll;    // scrolling pos
} TTSREV, *PTTSREV;


// UNITRANK - For sorting triphones/words by rank
typedef struct {
   PWORDINST         pWordInst;  // instance - used by words only
   PTPHONEINST       pTPInst;    // instance - used by triphones only

   // NOTE: Genereally derived from TPIinst's PPHONEAN
   DWORD             dwTimeStart; // start time in SRFETURE times
   DWORD             dwTimeEnd;  // end time in SRFEATURE times
   fp                fEnergy;    // calculated energy
   fp                fPitch;     // calculated pitch
   fp                fPitchVar;  // amount pitch changes over the phoneme
   fp                fVoicedEnergy; // amount of energy to voiced sound
   fp                fPlosiveness;  // how much corresponds to plosiveness looking for, lower numbers better
   int               iPitchDelta;   // amount that pitch changed over time. 1000 = 1 octave
   int               iPitchBulge;   // amount pitch bulges up/down
   fp                fBrightness;   // brightness, to encourage better defined formants
   BOOL              fIsPlosive; // set to TRUE if the sound is from a plosive, false if from non-plosive
   BOOL              fIsVoiced;  // set to TRUE if the sound is from a voiced unit, false if non-voiced
   DWORD             dwDuration; // duration (in samples)

   fp                fSRScoreGeneral;  // score from initial SR score
   fp                afSRScorePhone[TTSDEMIPHONES]; // score from phone-specific SR score
   fp                afSRScoreMegaPhone[TTSDEMIPHONES];   // SR score from comparison of megaphone
   fp                afSRScoreGroupTriPhone[TTSDEMIPHONES];   // score specific to the tri-phone group that trained on
   fp                afSRScoreSpecificTriPhone[TTSDEMIPHONES];   // score specific to the tri-phone group that trained on
   fp                afSRScoreWeighted[TTSDEMIPHONES]; // weighted. If lots of this triphone then use fSRScoreTriPhone, else fSRScoreMegaPhone

   // scratch
   fp                fCompare;   // feature that comparing at moment
   fp                afRankCompare[PITCHFIDELITY][DURATIONFIDELITY][ENERGYFIDELITY][TTSDEMIPHONES];  // for comparison
   fp                afRankAdd[TTSDEMIPHONES];      // for adding phoneme
} UNITRANK, *PUNITRANK;


// JCPHONEMEINFO - Information needed for phonemes
typedef struct {
   PCWSTR            pszName;       // name of the phoneme
   char              szaName[16];   // name is ascii
   DWORD             dwNoStress;    // point to unstressed version index, or -1 if none
   DWORD             dwWithStress;  // number of stressed versions of this phoneme
   DWORD             adwWithStress[4]; // stressed versions, filled with dwWithStress
   BOOL              fVoiced;       // set to TRUE if voiced
   BOOL              fPlosive;      // set to TRUE if plosive
   DWORD             dwGroup;       // phoneme group, from 0 .. PIS_PHONEGROUPNUM-1
   DWORD             dwMegaGroup;   // megagroup
} JCPHONEMEINFO, *PJCPHONEMEINFO;

// CWaveAn - Wave analysis info
class CWaveAn : public CEscObject {
public:
   CWaveAn (void);
   ~CWaveAn (void);

   BOOL AnalyzeWave (PWSTR pszFile, /*PCProgressSocket pProgress,*/ DWORD dwWaveNum,
                            PCListFixed *paplTriPhone /*[][PHONEGROUPSQUARE]*/, PCListFixed paplWord[],
                            PCTTSWork pTTS, PCVoiceFile pVF, double *pafEnergyPerPitch, double *pafEnergyPerVolume,
                            LPCRITICAL_SECTION lpcs,
                            double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount);

   CListFixed        m_lWORDAN;     // word analysis for each word in the wave
   CListFixed        m_lPHONEAN;    // phoneme analysis for each phoneme in the wave
   CListFixed        m_lSYLAN;      // syllable analysis
   PCM3DWave         m_pWave;       // original wave information, freed with CWaveAn freed
   fp                m_fMaxEnergy;  // maximum energy
   fp                m_fAvgEneryPerPhone; // average energy per phoneme
   fp                m_fAvgEnergyForVoiced;  // average energy for voiced phonemes, based on SRFEATURE's
   fp                m_fAvgPitch;   // average pitch
   DWORD             m_dwAvgPitchCount;   // count used to generate average pitch
   fp                m_fAvgSyllableDur;   // average duration for a syllable, in seconds
   DWORD             m_dwSyllableCount;   // number of syllables
   BOOL              m_fExcludeFromProsodyModel;   // if you set this, the wave will be excluded from the prosody model
   WCHAR             m_szFile[256];    // filename

private:
   BOOL AnalyzeWaveInt (DWORD dwWaveNum,
                            PCListFixed *paplTriPhone/*[][PHONEGROUPSQUARE]*/, PCListFixed paplWord[],
                            PCTTSWork pTTS, PCVoiceFile pVF, double *pafEnergyPerPitch, double *pafEnergyPerVolume,
                            LPCRITICAL_SECTION lpcs,
                            double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount);
   void ENERGYPERPITCHIncorporate (PSRFEATURE psrf, fp fPitch, double *pafEnergyPerPitch);
   void ENERGYPERVOLUMEIncorporate (PSRFEATURE psrf, fp fEnergyRatio, double *pafEnergyPerVolume);

};
typedef CWaveAn *PCWaveAn;


// EMTCANALYZEWAVE - For multhreaded
typedef struct {
   // on all EMTCxxx
   DWORD          dwStart;       // start count
   DWORD          dwEnd;         // end count
   DWORD          dwType;        // type

   // specific
   PCListFixed    plWave;        // list of waves that scan through
   PCListFixed    *paplTriPhone;   // parameter
   PCListFixed    *paplWord;       // parameter
   PCTTSWork      pTTS;          // parameter
   PCVoiceFile    pVF;           // parameter
   double         *pafEnergyPerPitch;  // parameter
   double         *pafEnergyPerVolume; // parameter
   LPCRITICAL_SECTION lpcs;      // critical section
   double         *pafPhonemePitchSum; // As per TTSANAL
   double         *pafPhonemeDurationSum; // As per TTSANAL
   double         *pafPhonemeEnergySum; // As per TTSANAL
   DWORD          *padwPhonemeCount; // As per TTSANAL
} EMTCANALYZEWAVE, *PEMTCANALYZEWAVE;

// EMTCANALYSISPHONETRAIN - For multhreaded
typedef struct {
   // on all EMTCxxx
   DWORD          dwStart;       // start count
   DWORD          dwEnd;         // end count
   DWORD          dwType;        // type

   // specific
   PTTSANAL       pAnal;         // analysis info
   PTTSJOINCOSTS  pJC;           // join costs
   PCMTTS         pTTS;          // TTS
   DWORD          dwPass;        // pass to pass into AnalysisJoinCostsSub()
   DWORD          dwStartPhone;  // start phoneme, for triphone train
   DWORD          dwEndPhone;    // end phoneme, for triphone train
} EMTCANALYSISPHONETRAIN, *PEMTCANALYSISPHONETRAIN;



static int _cdecl UNITRANKSort (const void *elem1, const void *elem2);
PSRFEATURE CacheSRFeatures (PCM3DWave pWave, DWORD dwTimeStart, DWORD dwTimeEnd);
static void FakeSRFEATURE (PSRFEATURE psrf, DWORD dwNum, DWORD dwHash);
DWORD PWVPHONEMEToNum (PWVPHONEME pwp, PCMLexicon pLex);
void PWVPHONEMEToString (PWVPHONEME pwp, PCMLexicon pLex, PWSTR psz);

// globals
static char gszKeyLexFile[] = "LexFile";
static __int64 giTotalRAM = 0;


static int _cdecl MISMATCHSTRUCTSort (const void *elem1, const void *elem2)
{
   MISMATCHSTRUCT *pdw1, *pdw2;
   pdw1 = (MISMATCHSTRUCT*) elem1;
   pdw2 = (MISMATCHSTRUCT*) elem2;

   if (pdw1->fCompare < pdw2->fCompare)
      return -1;
   else if (pdw1->fCompare > pdw2->fCompare)
      return 1;
   else
      return 0;
}


/*************************************************************************************
TrainWeight - Affects the training weight based on the distances in pitch, duration,
and eneryg fidelity

inputs
   DWORD       dwPF1 - Closest match
   DWORD       dwPF2 - Training for
   DWORD       dwDF1 - Closest match
   DWORD       dwDF2 - Training for
   DWORD       dwEF1 - Closest match
   DWORD       dwEF2 - Training for
returns
   fp - Scale.
*/
__inline fp TrainWeight (DWORD dwPF1, DWORD dwPF2, DWORD dwDF1, DWORD dwDF2, DWORD dwEF1, DWORD dwEF2)
{
   int iPF = abs((int)dwPF1 - (int)dwPF2);
   int iDF = abs((int)dwDF1 - (int)dwDF2);
   int iEF = abs((int)dwEF1 - (int)dwEF2);

   fp fDist = iPF * iPF + iDF * iDF + iEF * iEF;

   return pow ((fp)0.5, sqrt(fDist));
}

/*************************************************************************************
PitchToFidelity - Given a pitch, converts to a fidelity number

inputs
   fp          fPitch - Pitch, in Hz
   fp          fAvgPitch - Voice's average pitch
returns
   DWORD - Pitch fidelity number, from 0..PITCHFIDELITY-1
*/
static __inline DWORD PitchToFidelity (fp fPitch, fp fAvgPitch)
{
   fp f = log (max(fPitch, CLOSE) / max(fAvgPitch, CLOSE)) / log(OCTAVESPERPITCHFILDELITY);
   int iVal = (int)floor(f + 0.5 + (fp)PITCHFIDELITYCENTER);
   iVal = max(iVal, 0);
   iVal = min(iVal, PITCHFIDELITY-1);
   return (DWORD)iVal;
}



/*************************************************************************************
DurationToFidelity - Given a duration, converts to a fidelity number

inputs
   fp          fDuration - Pitch, in samples
   fp          fAvgDuration - Voice's average duration for the phoneme
returns
   DWORD - Duration fidelity number, from 0..DURATIONFIDELITY-1
*/
static __inline DWORD DurationToFidelity (fp fDuration, fp fAvgDuration)
{
   fp f = log (fDuration / fAvgDuration) / log(SCALEPERDURATIONFIDELITY);
   int iVal = (int)floor(f + 0.5 + (fp)DURATIONFIDELITYCENTER);
   iVal = max(iVal, 0);
   iVal = min(iVal, DURATIONFIDELITY-1);
   return (DWORD)iVal;
}



/*************************************************************************************
EnergyToFidelity - Given a Energy, converts to a fidelity number

inputs
   fp          fEnergy - Pitch, in samples
   fp          fAvgEnergy - Voice's average Energy for the phoneme
returns
   DWORD - Energy fidelity number, from 0..ENERGYFIDELITY-1
*/
static __inline DWORD EnergyToFidelity (fp fEnergy, fp fAvgEnergy)
{
   fp f = log (max(fEnergy,CLOSE) / max(fAvgEnergy,CLOSE)) / log(SCALEPERENERGYFIDELITY);
   int iVal = (int)floor(f + 0.5 + (fp)ENERGYFIDELITYCENTER);
   iVal = max(iVal, 0);
   iVal = min(iVal, ENERGYFIDELITY-1);
   return (DWORD)iVal;
}



/*************************************************************************************
Plosiveness - Returns the delta of the energy over time, producing higher numbers
for more plosive sounds.

inputs
   PCM3DWave         pWave - Wave to use
   DWORD             dwStart - Start SRFEATURE
   DWORD             dwEnd - End SRFEATURE (exclusive)
returns
   fp - Plosiveness (higher numbers are brighter). This is sum of all values
*/
static fp Plosiveness (PCM3DWave pWave, DWORD dwStart, DWORD dwEnd)
{
   if (dwEnd <= dwStart)
      return 0;   // not enough points

   // cache the features
   DWORD dwCacheStart = dwStart ? (dwStart - 1) : 0;
   DWORD dwCacheEnd = min(dwEnd + 1, pWave->m_dwSRSamples);
   PSRFEATURE psrCache = CacheSRFeatures (pWave, dwCacheStart, dwCacheEnd);   // note: not worth counteracting pitch brightness for this
   if (!psrCache)
      return 0;

   double fSum = 0;
   double fTemp;
   DWORD i, j;
   for (j = 0; j < SRDATAPOINTS; j++) {
      for (i = dwStart; i < dwEnd; i++) {
         if (!i)
            continue;   // if at very start of wave cant tell how plosive going into

         PSRFEATURE psr = psrCache + (i - dwCacheStart); // pWave->m_paSRFeature + i;

         fTemp = DbToAmplitude(psr[0].acNoiseEnergy[j]) - DbToAmplitude(psr[-1].acNoiseEnergy[j]);
         fSum += fTemp * fTemp;

         fTemp = DbToAmplitude(psr[0].acVoiceEnergy[j]) - DbToAmplitude(psr[-1].acVoiceEnergy[j]);
         fSum += fTemp * fTemp;
      } // i
   } // j

   return (fp)sqrt(fSum);
}



#if 0 // go back to modified version of old formant brightness
/*************************************************************************************
FormantBrightness - This code detects how bright the formants are by
taking the delta of the energy as freuqnecy varies. I put this code
in so that it would counteract the tendency of speech recognition to
choose the more average sound.


inputs
   PCM3DWave         pWave - Wave to use
   DWORD             dwStart - Start SRFEATURE
   DWORD             dwEnd - End SRFEATURE (exclusive)
   BOOL              fPlosive - Set to TRUE if it's a plosive, FALSE if drawn out
returns
   fp - Brightness (higher numbers are brighter). This is sum of all values
*/
static fp FormantBrightness (PCM3DWave pWave, DWORD dwStart, DWORD dwEnd, BOOL fPlosive)
{
   if (dwEnd <= dwStart)
      return 0;

   DWORD dwNum = dwEnd - dwStart;

   // cache the features
   PSRFEATURE psr = CacheSRFeatures (pWave, dwStart, dwEnd);   // note: not worth counteracting pitch brightness for this
   if (!psr)
      return 0;

   // fill in the original
   //PSRFEATURE psr = pWave->m_paSRFeature + dwStart;
   DWORD dwTime, dwFreq;
   double fRet = 0, fCount = 0;
   for (dwTime = 0; dwTime < dwNum; dwTime++) {
      double fBrightSum = 0, fBrightSumSquare = 0;

      for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++) {
         double fSum = DbToAmplitude(psr[dwTime].acNoiseEnergy[dwFreq]) + DbToAmplitude(psr[dwTime].acVoiceEnergy[dwFreq]);
         fBrightSum += fSum;
         fBrightSumSquare += fSum * fSum;
      } // dwFreq

      if (fBrightSum > CLOSE)
         fBrightSumSquare /= (fBrightSum * fBrightSum);  // make sure same units
      else
         fBrightSumSquare = 0;

      // weighted average
      fRet += fBrightSumSquare * fBrightSum;
      fCount += fBrightSum;
   } // dwTime

   if (fCount > CLOSE)
      return fRet / fCount;
   else
      return 0;
}
#endif // 0

/*************************************************************************************
FormantBrightness - This code detects how bright the formants are by
taking the delta of the energy as freuqnecy varies. I put this code
in so that it would counteract the tendency of speech recognition to
choose the more average sound.


inputs
   PCM3DWave         pWave - Wave to use
   DWORD             dwStart - Start SRFEATURE
   DWORD             dwEnd - End SRFEATURE (exclusive)
   BOOL              fPlosive - Set to TRUE if it's a plosive, FALSE if drawn out
returns
   fp - Brightness (higher numbers are brighter). This is sum of all values
*/
static fp FormantBrightness (PCM3DWave pWave, DWORD dwStart, DWORD dwEnd, BOOL fPlosive)
{
   if (dwEnd <= dwStart)
      return 0;

   fp afEnergy[SRDATAPOINTS];
   fp afFilter[2][SRDATAPOINTS];

   // cache the features
   PSRFEATURE psr = CacheSRFeatures (pWave, dwStart, dwEnd);   // note: not worth counteracting pitch brightness for this
   if (!psr)
      return 0;
   DWORD dwNum = dwEnd - dwStart;

   // loop through all the times
   double fEnergySum = 0, fFilterDiffSum = 0;
   WORD dwTime, dwFreq;
   for (dwTime = 0; dwTime < dwNum; dwTime++, psr++) {
      // convert to energy
      for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++)
         afEnergy[dwFreq] = (fp)DbToAmplitude(psr->acNoiseEnergy[dwFreq]) + (fp)DbToAmplitude(psr->acVoiceEnergy[dwFreq]);

      // filter
      DWORD dwFilter;
      int iFreqWindow;
      for (dwFilter = 0; dwFilter < 2; dwFilter++) {
         int iWindowSize = dwFilter ? (int)SRPOINTSPEROCTAVE/2 : (int)SRPOINTSPEROCTAVE/12;
         for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++) {
            DWORD dwCount = 0;
            double fSum = 0;
            for (iFreqWindow = -iWindowSize; iFreqWindow <= iWindowSize; iFreqWindow++) {
               int iFreq = iFreqWindow + (int)dwFreq;
               DWORD dwWeight = (DWORD)(iWindowSize + 1 - abs(iFreqWindow));
               if ((iFreq < 0) || (iFreq >= SRDATAPOINTS))
                  continue;

               fSum += afEnergy[iFreq] * (double)dwWeight;
               dwCount += dwWeight;
            } // iFreqWindow

            if (dwCount)
               fSum /= (double)dwCount;
            afFilter[dwFilter][dwFreq] = fSum;
         } // dwFreq
      } // dwFilter

      // find the energy sum as well as the difference
      for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++) {
         // simulate weighting of ear
         double fWeight = (double)dwFreq / (double)SRDATAPOINTS;
         fWeight = fWeight * fWeight;  // so peak frequency is .72 of the way up the 7 octave scale
         fWeight = sin(fWeight * PI);
         fWeight = fWeight /* * fWeight */ * 0.9 + 0.1;   // some weight, even at lower frequencies

         fEnergySum += fWeight * afEnergy[dwFreq];
         fFilterDiffSum += fWeight * fabs(afFilter[0][dwFreq] - afFilter[1][dwFreq]);
      } // dwFreq
   } // dwTime

   if (fEnergySum > CLOSE)
      fFilterDiffSum /= fEnergySum;
   return fFilterDiffSum;

#if 0 // old code
   // fill in the original
   //PSRFEATURE psr = pWave->m_paSRFeature + dwStart;
   DWORD dwTime, dwFreq;
   for (dwTime = 0; dwTime < dwNum; dwTime++) for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++)
      pafOrig[dwTime*SRDATAPOINTS+dwFreq] = (fp)DbToAmplitude(psr[dwTime].acNoiseEnergy[dwFreq]) +
               (fp)DbToAmplitude(psr[dwTime].acVoiceEnergy[dwFreq]);

   // loop through wave and fill in a blurred image of it
   int iTimeWindow, iFreqWindow;
   int iTWSize = (fPlosive ? (pWave->m_dwSRSAMPLESPERSEC/50) : 0);
      // BUGFIX - Change timewindow from +/- 2 to +/- 0 so encourage vertical brightness
   for (dwTime = 0; dwTime < dwNum; dwTime++) for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++) {
      // loop over the window and sum
      DWORD dwCount = 0;
      fp fSum = 0;
      for (iTimeWindow = -iTWSize; iTimeWindow <= iTWSize; iTimeWindow++) {
         int iTime = iTimeWindow + (int)dwTime;
         if ((iTime < 0) || (iTime >= (int)dwNum))
            continue;
         for (iFreqWindow = -SRPOINTSPEROCTAVE/2; iFreqWindow <= SRPOINTSPEROCTAVE/2; iFreqWindow++) {
            int iFreq = iFreqWindow + (int)dwFreq;
            if ((iFreq < 0) || (iFreq >= SRDATAPOINTS))
               continue;

            fSum += pafOrig[iTime * SRDATAPOINTS + iFreq];
            dwCount++;
         } // iFreqWindow
      } // iTimeWindow

      if (dwCount)
         fSum /= (fp)dwCount;
      paf[dwTime * SRDATAPOINTS + dwFreq] = fSum;
   } // dwTime, dwFreq

   // calculate the difference between the actual signal and the blurred image
   double fDelta = 0, fTotal = 0;
   for (dwTime = 0; dwTime < dwNum; dwTime++) for (dwFreq = 0; dwFreq < SRDATAPOINTS; dwFreq++) {
      fp fVal = pafOrig[dwTime * SRDATAPOINTS + dwFreq];
      fp fBlur = paf[dwTime * SRDATAPOINTS + dwFreq];

      fDelta += fabs(fVal - fBlur);
      fTotal += fBlur;
   }
   if (fTotal)
      fDelta /= fTotal;

   return (fp)fDelta;
#endif // 0
}

/*************************************************************************************
CTTSWork::Constructor and destructor
*/
CTTSWork::CTTSWork (void)
{
   m_szFile[0] = m_szSRFile[0] = m_szLexicon[0] = 0;
   DWORD i;
   for (i = 0; i < NUMPROSODYTTS; i++)
      m_aszProsodyTTS[i][0] = 0;
   m_dwMinInstance = 1;
   m_dwWordCache = 1000;
   m_dwMinExamples = 5;
   m_dwTriPhoneGroup = 0;
   // m_dwTriPhonePitch = 0;
   m_fKeepLog = FALSE;
   m_fPauseLessOften = FALSE;
   m_fFullPCM = TRUE;   // BUGFIX - Forcing to always be true
   m_dwPCMCompress = 1;
   m_dwFreqCompress = 2;
   m_dwTimeCompress = 1;
   m_fWordStartEndCombine = FALSE;

   m_pWaveDir = new CWaveDirInfo;
   m_pWaveDirEx = new CWaveDirInfo;
   m_pWaveToDo = new CWaveToDo;
   m_pLexWords = new CMLexicon;
   m_pLexMisspelled = new CMLexicon;

#if 0 // old prosody
   m_pLexFuncWords = new CMLexicon;
   memset (m_apLexWordEmph, 0, sizeof(m_apLexWordEmph));
#endif // 0

   memset (m_apLexFuncWord, 0, sizeof(m_apLexFuncWord));

   m_dwTotalUnits = 3000;
   m_fWordEnergyAvg = 0;
   m_dwWordCount = 0;
   //m_dwMultiUnit = 0;
   // m_dwMultiSyllableUnit = 0;
   //m_dwConnectUnits = 0;
   m_dwUnitsAdded = 0;

   m_lPHONEBLACK.Init (sizeof(PHONEBLACK));

   m_pLex = NULL;

   // remember how much memory is in the computer
   MEMORYSTATUSEX ms;
   memset (&ms, 0, sizeof(ms));
   ms.dwLength = sizeof(ms);
   GlobalMemoryStatusEx (&ms);
   giTotalRAM = ms.ullTotalPhys;
}

CTTSWork::~CTTSWork (void)
{
   if (m_pWaveDir)
      delete m_pWaveDir;
   m_pWaveDir = NULL;

   if (m_pWaveDirEx)
      delete m_pWaveDirEx;
   m_pWaveDirEx = NULL;

   if (m_pWaveToDo)
      delete m_pWaveToDo;
   m_pWaveToDo = NULL;

   if (m_pLexWords)
      delete m_pLexWords;
   m_pLexWords = NULL;

   if (m_pLexMisspelled)
      delete m_pLexMisspelled;
   m_pLexMisspelled = NULL;

#if 0 // old prosody
   DWORD i;
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      if (m_apLexWordEmph[i])
         delete m_apLexWordEmph[i];
      m_apLexWordEmph[i] = NULL;
   }

   if (m_pLexFuncWords)
      delete m_pLexFuncWords;
   m_pLexFuncWords = NULL;
#endif // 0
   DWORD i;
   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      if (m_apLexFuncWord[i])
         delete m_apLexFuncWord[i];
      m_apLexFuncWord[i] = NULL;
   }

   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   m_pLex = NULL;
}


/*************************************************************************************
CTTSWork::LexiconRequired - Loads the lexicon if it isn't already loaded.
Returns TRUE if success, FALSE if error
*/
BOOL CTTSWork::LexiconRequired (void)
{
   if (m_pLex)
      return TRUE;

   m_pLex = MLexiconCacheOpen(m_szLexicon, FALSE);
   return (m_pLex ? TRUE : FALSE);
}

/*************************************************************************************
CTTSWork::LexiconSet - Sets a new lexicon to use

inputs
   PWSTR          pszLexicon - Lexicon
returns
   BOOL - TRUE if was able to open, FALSE if not
*/
BOOL CTTSWork::LexiconSet (PWSTR pszLexicon)
{
   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   wcscpy (m_szLexicon, pszLexicon);

   BOOL fRet = LexiconRequired ();


   return fRet;
}

/*************************************************************************************
CTTSWork::LexiconGet - Returns a pointer to the lexicon string. DO NOT modify it.
*/
PWSTR CTTSWork::LexiconGet (void)
{
   return m_szLexicon;
}

/*************************************************************************************
CTTSWork::Lexicon - Returns a pointer to the lexicon to use for SR. NULL if cant open
*/
PCMLexicon CTTSWork::Lexicon (void)
{
   LexiconRequired ();
   return m_pLex;
}


static PWSTR gpszTTSWork = L"TTSWork";
static PWSTR gpszSRFile = L"SRFile";
static PWSTR gpszLexicon = L"Lexicon";
static PWSTR gpszMinInstance = L"MinInstance";
static PWSTR gpszWordCache = L"WordCache";
static PWSTR gpszTriPhoneGroup = L"TriPhoneGroup";
static PWSTR gpszTriPhonePitch = L"TriPhonePitch";
static PWSTR gpszWaveDir = L"WaveDir";
static PWSTR gpszWaveDirEx = L"WaveDirEx";
static PWSTR gpszWaveToDo = L"WaveToDo";
static PWSTR gpszLexWords = L"LexWords";
static PWSTR gpszLexMisspelled = L"LexMisspelled";
static PWSTR gpszPhoneBlack = L"PhoneBlack";
static PWSTR gpszLexFuncWords = L"LexFuncWords";
static PWSTR gpszKeepLog = L"KeepLog";
static PWSTR gpszPauseLessOften = L"PauseLessOften";
static PWSTR gpszTimeCompress = L"TimeCompress";
static PWSTR gpszWordStartEndCombine = L"WordStartEndCombine";
static PWSTR gpszFreqCompress = L"FreqCompress";
static PWSTR gpszMultiUnit = L"MultiUnit";
static PWSTR gpszConnectUnits = L"ConnectUnits";
static PWSTR gpszMultiSyllableUnit = L"MultiSyllableUnit";
// static PWSTR gpszProsodyTTS = L"ProsodyTTS";
static PWSTR gpszMinExamples = L"MinExamples";
static PWSTR gpszTotalUnits = L"TotalUnits";
//static PWSTR gpszFullPCM = L"FullPCM";
static PWSTR gpszPCMCompress = L"PCMCompress";
static PWSTR gpszTTSTARGETCOSTS = L"TTSTARGETCOSTS";

/*************************************************************************************
CTTSWork::MMLTo - Standard call
*/
PCMMLNode2 CTTSWork::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszTTSWork);

   if (m_szSRFile[0])
      MMLValueSet (pNode, gpszSRFile, m_szSRFile);
   DWORD i;
   WCHAR szTemp[64];
   for (i = 0; i < NUMPROSODYTTS; i++) {
      if (!m_aszProsodyTTS[i][0])
         continue;
      swprintf (szTemp, L"ProsodyTTS%d", (int)i);
      MMLValueSet (pNode, szTemp, m_aszProsodyTTS[i]);
   } // i
   if (m_szLexicon[0])
      MMLValueSet (pNode, gpszLexicon, m_szLexicon);
   MMLValueSet (pNode, gpszMinInstance, (int)m_dwMinInstance);
   // MMLValueSet (pNode, gpszWordCache, (int)m_dwWordCache);
   MMLValueSet (pNode, gpszMinExamples, (int)m_dwMinExamples);
   MMLValueSet (pNode, gpszTriPhoneGroup, (int)m_dwTriPhoneGroup);
   // MMLValueSet (pNode, gpszTriPhonePitch, (int)m_dwTriPhonePitch);
   MMLValueSet (pNode, gpszKeepLog, (int)m_fKeepLog);
   MMLValueSet (pNode, gpszPauseLessOften, (int)m_fPauseLessOften);
   // MMLValueSet (pNode, gpszFullPCM, (int)m_fFullPCM);
   MMLValueSet (pNode, gpszTimeCompress, (int)m_dwTimeCompress);
   MMLValueSet (pNode, gpszWordStartEndCombine, (int)m_fWordStartEndCombine);
   MMLValueSet (pNode, gpszFreqCompress, (int)m_dwFreqCompress);
   MMLValueSet (pNode, gpszPCMCompress, (int)m_dwPCMCompress);
   //MMLValueSet (pNode, gpszMultiUnit, (int)m_dwMultiUnit);
   MMLValueSet (pNode, gpszTotalUnits, (int)m_dwTotalUnits);
   // MMLValueSet (pNode, gpszMultiSyllableUnit, (int)m_dwMultiSyllableUnit);
   //MMLValueSet (pNode, gpszConnectUnits, (int)m_dwConnectUnits);

   if (m_lPHONEBLACK.Num())
      MMLValueSet (pNode, gpszPhoneBlack, (PBYTE)m_lPHONEBLACK.Get(0), m_lPHONEBLACK.Num()*sizeof(PHONEBLACK));

   PCMMLNode2 pSub;
   pSub = m_pWaveDir->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszWaveDir);
      pNode->ContentAdd (pSub);
   }
   pSub = m_pWaveDirEx->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszWaveDirEx);
      pNode->ContentAdd (pSub);
   }
   pSub = m_pWaveToDo->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszWaveToDo);
      pNode->ContentAdd (pSub);
   }
   pSub = m_pLexWords->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLexWords);
      pNode->ContentAdd (pSub);
   }
   pSub = m_pLexMisspelled->MMLTo();
   if (pSub) {
      pSub->NameSet (gpszLexMisspelled);
      pNode->ContentAdd (pSub);
   }

#if 0 // old prosody
   DWORD i;
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      if (!m_apLexWordEmph[i])
         continue;

      WCHAR szTemp[64];
      swprintf (szTemp, L"LexWordEmph%d", (int)i);
      pSub = m_apLexWordEmph[i]->MMLTo();
      if (pSub) {
         pSub->NameSet (szTemp);
         pNode->ContentAdd (pSub);
      }
   }

   if (m_pLexFuncWords) {
      pSub = m_pLexFuncWords->MMLTo();
      if (pSub) {
         pSub->NameSet (gpszLexFuncWords);
         pNode->ContentAdd (pSub);
      }
   }
#endif // 0

   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      if (!m_apLexFuncWord[i])
         continue;

      WCHAR szTemp[64];
      swprintf (szTemp, L"FuncWordGroup%d", (int)i);
      pSub = m_apLexFuncWord[i]->MMLTo();
      if (pSub) {
         pSub->NameSet (szTemp);
         pNode->ContentAdd (pSub);
      }
   }

   if (m_memTTSTARGETCOSTS.m_dwCurPosn == sizeof(TTSTARGETCOSTS))
      MMLValueSet (pNode, gpszTTSTARGETCOSTS, (PBYTE)m_memTTSTARGETCOSTS.p, m_memTTSTARGETCOSTS.m_dwCurPosn);

   return pNode;
}

/*************************************************************************************
CTTSWork::MMLFrom - Standard call

inputs
   PCMMLNode2         pNode - Node to read from
   PWSTR             pszSrcFile - If the main lexicon doesnt exist, then the root
                           directory is taken from this and used
*/
BOOL CTTSWork::MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile)
{
   // wipe out
   if (m_pWaveDir)
      delete m_pWaveDir;
   m_pWaveDir = new CWaveDirInfo;

   if (m_pWaveDirEx)
      delete m_pWaveDirEx;
   m_pWaveDirEx = new CWaveDirInfo;

   m_lPHONEBLACK.Clear();

   if (m_pWaveToDo)
      delete m_pWaveToDo;
   m_pWaveToDo = new CWaveToDo;

   if (m_pLexWords)
      delete m_pLexWords;
   m_pLexWords = new CMLexicon;

   if (m_pLexMisspelled)
      delete m_pLexMisspelled;
   m_pLexMisspelled = new CMLexicon;

   DWORD i;
#if 0 // old prosody
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      if (m_apLexWordEmph[i])
         delete m_apLexWordEmph[i];
      m_apLexWordEmph[i] = NULL;
   }

   if (m_pLexFuncWords)
      delete m_pLexFuncWords;
   m_pLexFuncWords = NULL;
#endif // 0

   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      if (m_apLexFuncWord[i])
         delete m_apLexFuncWord[i];
      m_apLexFuncWord[i] = NULL;
   }

   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   m_pLex = NULL;

   // read in
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszSRFile);
   if (psz)
      wcscpy (m_szSRFile, psz);
   else
      m_szSRFile[0] = 0;
   WCHAR szTemp[64];
   for (i = 0; i < NUMPROSODYTTS; i++) {
      swprintf (szTemp, L"ProsodyTTS%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (psz)
         wcscpy (m_aszProsodyTTS[i], psz);
      else
         m_aszProsodyTTS[i][0] = 0;
   } // i
   psz = MMLValueGet (pNode, gpszLexicon);
   if (psz)
      wcscpy (m_szLexicon, psz);
   else
      m_szLexicon[0] = 0;

   m_dwMinInstance = (DWORD) MMLValueGetInt (pNode, gpszMinInstance, (int)1);
   // m_dwWordCache = (DWORD) MMLValueGetInt (pNode, gpszWordCache, (int)1000);
   m_dwMinExamples = (DWORD) MMLValueGetInt (pNode, gpszMinExamples, (int)5);
   m_dwMinExamples = max(m_dwMinExamples, 1);
   m_dwTriPhoneGroup = (DWORD) MMLValueGetInt (pNode, gpszTriPhoneGroup, (int)0);
   // m_dwTriPhonePitch = (DWORD) MMLValueGetInt (pNode, gpszTriPhonePitch, (int)0);
   m_fKeepLog = (BOOL) MMLValueGetInt (pNode, gpszKeepLog, (int)FALSE);
   m_fPauseLessOften = (BOOL) MMLValueGetInt (pNode, gpszPauseLessOften, (int)FALSE);
   // m_fFullPCM = (BOOL) MMLValueGetInt (pNode, gpszFullPCM, (int)FALSE);
   m_dwTimeCompress = (DWORD) MMLValueGetInt (pNode, gpszTimeCompress, 1);
   m_dwFreqCompress = (BOOL) MMLValueGetInt (pNode, gpszFreqCompress, (int)2);
   m_dwPCMCompress = (BOOL) MMLValueGetInt (pNode, gpszPCMCompress, (int)1);
   m_fWordStartEndCombine = (BOOL) MMLValueGetInt (pNode, gpszWordStartEndCombine, (int)FALSE);
   //m_dwMultiUnit = (int) MMLValueGetInt (pNode, gpszMultiUnit, 0);
   // m_dwMultiSyllableUnit = (int) MMLValueGetInt (pNode, gpszMultiSyllableUnit, 0);
   //m_dwConnectUnits = (int) MMLValueGetInt (pNode, gpszConnectUnits, 0);
   m_dwTotalUnits = (int) MMLValueGetInt (pNode, gpszTotalUnits, 3000);

   // target costs
   m_memTTSTARGETCOSTS.m_dwCurPosn = 0;
   MMLValueGetBinary (pNode, gpszTTSTARGETCOSTS, &m_memTTSTARGETCOSTS);

   // BUGFIX - Use new binary MML
   CMem mem;
   MMLValueGetBinary (pNode, gpszPhoneBlack, &mem);
   //psz = MMLValueGet (pNode, gpszPhoneBlack);
   if (mem.m_dwCurPosn /*psz*/) {
      //CMem mem;
      //if (!mem.Required(wcslen(psz)+1))
      //   return FALSE;
      //DWORD dwSize = MMLBinaryFromString (psz, (PBYTE)mem.p, mem.m_dwAllocated);
      size_t dwSize = mem.m_dwCurPosn;

      // fill list
      m_lPHONEBLACK.Init (sizeof(PHONEBLACK), mem.p, (DWORD)dwSize/sizeof(PHONEBLACK));
   }

   PCMMLNode2 pSub;
#if 0 // old prosody
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"LexWordEmph%d", (int)i);
      pSub = NULL;
      pNode->ContentEnum(pNode->ContentFind (szTemp), &psz, &pSub);
      if (!pSub)
         continue;

      m_apLexWordEmph[i] = new CMLexicon;
      if (!m_apLexWordEmph[i])
         continue;
      m_apLexWordEmph[i]->MMLFrom (pSub, NULL);
   }
#endif  // 0

   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"FuncWordGroup%d", (int)i);
      pSub = NULL;
      pNode->ContentEnum(pNode->ContentFind (szTemp), &psz, &pSub);
      if (!pSub)
         continue;

      m_apLexFuncWord[i] = new CMLexicon;
      if (!m_apLexFuncWord[i])
         continue;
      m_apLexFuncWord[i]->MMLFrom (pSub, NULL, FALSE);
   }

   // subelements
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWaveDir)) {
         m_pWaveDir->MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszWaveDirEx)) {
         m_pWaveDirEx->MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszWaveToDo)) {
         m_pWaveToDo->MMLFrom (pSub);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLexWords)) {
         m_pLexWords->MMLFrom (pSub, NULL, FALSE);
         continue;
      }
      else if (!_wcsicmp(psz, gpszLexMisspelled)) {
         m_pLexMisspelled->MMLFrom (pSub, NULL, FALSE);
         continue;
      }
#if 0 // old prosody
      else if (!_wcsicmp(psz, gpszLexFuncWords)) {
         if (!m_pLexFuncWords)
            m_pLexFuncWords = new CMLexicon;
         if (m_pLexFuncWords)
            m_pLexFuncWords->MMLFrom (pSub, NULL);
         continue;
      }
#endif // 0
   } // i

   if (!LexiconExists (m_szLexicon, pszSrcFile))
      return FALSE;

   return TRUE;
}


/*************************************************************************************
CTTSWork::Clone - Standard call
*/
CTTSWork *CTTSWork::Clone (void)
{
   PCTTSWork pNew = new CTTSWork;
   if (!pNew)
      return NULL;

   wcscpy (pNew->m_szFile, m_szFile);
   wcscpy (pNew->m_szSRFile, m_szSRFile);
   DWORD i;
   for (i = 0; i < NUMPROSODYTTS; i++)
      wcscpy (pNew->m_aszProsodyTTS[i], m_aszProsodyTTS[i]);
   pNew->m_dwMinInstance = m_dwMinInstance;
   pNew->m_dwWordCache = m_dwWordCache;
   pNew->m_dwMinExamples = m_dwMinExamples;
   pNew->m_dwTriPhoneGroup = m_dwTriPhoneGroup;
   // pNew->m_dwTriPhonePitch = m_dwTriPhonePitch;
   pNew->m_fKeepLog = m_fKeepLog;
   pNew->m_fPauseLessOften = m_fPauseLessOften;
   pNew->m_fFullPCM = m_fFullPCM;
   pNew->m_dwTimeCompress = m_dwTimeCompress;
   pNew->m_dwFreqCompress = m_dwFreqCompress;
   pNew->m_dwPCMCompress = m_dwPCMCompress;
   pNew->m_fWordStartEndCombine = m_fWordStartEndCombine;
   //pNew->m_dwMultiUnit = m_dwMultiUnit;
   //pNew->m_dwMultiSyllableUnit = m_dwMultiSyllableUnit;
   //pNew->m_dwConnectUnits = m_dwConnectUnits;
   pNew->m_dwUnitsAdded = m_dwUnitsAdded;
   pNew->m_dwTotalUnits = m_dwTotalUnits;
   wcscpy (pNew->m_szLexicon, m_szLexicon);
   pNew->m_lPHONEBLACK.Init (sizeof(PHONEBLACK), m_lPHONEBLACK.Get(0), m_lPHONEBLACK.Num());

   if (pNew->m_pWaveDir)
      delete pNew->m_pWaveDir;
   pNew->m_pWaveDir = m_pWaveDir->Clone();
   if (pNew->m_pWaveDirEx)
      delete pNew->m_pWaveDirEx;
   pNew->m_pWaveDirEx = m_pWaveDirEx->Clone();
   if (pNew->m_pWaveToDo)
      delete pNew->m_pWaveToDo;
   pNew->m_pWaveToDo = m_pWaveToDo->Clone();
   if (pNew->m_pLexWords)
      delete pNew->m_pLexWords;
   pNew->m_pLexWords = m_pLexWords->Clone();
   if (pNew->m_pLexMisspelled)
      delete pNew->m_pLexMisspelled;
   pNew->m_pLexMisspelled = m_pLexMisspelled->Clone();

   pNew->m_memTTSTARGETCOSTS.m_dwCurPosn = m_memTTSTARGETCOSTS.m_dwCurPosn;
   if (pNew->m_memTTSTARGETCOSTS.Required(m_memTTSTARGETCOSTS.m_dwCurPosn))
      memcpy (pNew->m_memTTSTARGETCOSTS.p, m_memTTSTARGETCOSTS.p, m_memTTSTARGETCOSTS.m_dwCurPosn);
   else
      pNew->m_memTTSTARGETCOSTS.p = NULL;

#if 0 // old prosody
   DWORD i;
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      if (pNew->m_apLexWordEmph[i])
         delete pNew->m_apLexWordEmph[i];
      pNew->m_apLexWordEmph[i] = NULL;

      if (!m_apLexWordEmph[i])
         continue;
      pNew->m_apLexWordEmph[i] = m_apLexWordEmph[i]->Clone();
   }

   if (pNew->m_pLexFuncWords)
      delete pNew->m_pLexFuncWords;
   pNew->m_pLexFuncWords = m_pLexFuncWords ? m_pLexFuncWords->Clone() : NULL;
#endif // 0, old prosody

   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      if (pNew->m_apLexFuncWord[i])
         delete pNew->m_apLexFuncWord[i];
      pNew->m_apLexFuncWord[i] = NULL;

      if (!m_apLexFuncWord[i])
         continue;
      pNew->m_apLexFuncWord[i] = m_apLexFuncWord[i]->Clone();
   }

   // note: pNew->m_pLex will be NULL since starts out that way with new object

   return pNew;
}

/*************************************************************************************
CTTSWork::Save - Saves the file

inputs
   PWSTR          szFile - Fle to save as, or use NULL to rely upon m_szFile
*/
BOOL CTTSWork::Save (WCHAR *szFile)
{
   if (!szFile)
      szFile = m_szFile;

   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;
   BOOL fRet;
   fRet = MMLFileSave (szFile, &GUID_TTSWork, pNode);
   delete pNode;

   if (fRet && (szFile != m_szFile))
      wcscpy (m_szFile, szFile);

   return fRet;
}


/*************************************************************************************
CTTSWork::Open - Opens the file

inputs
   WCHAR              *szFile - FIle. This file name is rememberd in to m_szFile
   PWSTR             pszSrcFile - If the main lexicon doesnt exist, then the root
                           directory is taken from this and used
*/
BOOL CTTSWork::Open (WCHAR *szFile)
{
   PCMMLNode2 pNode = MMLFileOpen (szFile, &GUID_TTSWork);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode, szFile)) {
      delete pNode;
      return FALSE;
   }

   // rembmeber the file
   wcscpy (m_szFile, szFile);

   delete pNode;
   return TRUE;
}


// RESEGWAVETHREAD - Structure for the resegmentation thread
typedef struct {
   HANDLE                  hThread;    // thread handle
   DWORD                   dwThreadID; // thread ID
   BOOL                    fAlsoClear; // set to TRUE if should clear all info
   BOOL                    fKeepExisting;  // if TRUE then keep the info, and only recalc if dirty
   BOOL                    fJustFeatures; // a special mode that only keeps the SR features, and doesn't reacalc SR
   BOOL                    fFullPCM;   // set if want to produce full PCM
   HWND                    hWnd;       // to display errors on

   // used for finetune
   DWORD                   dwMode;     // 0 for rescan SR, 1 for fine-tune phonemes
   PCMLexicon              pLex;       // lexicon



   CRITICAL_SECTION        critSec;    // critical section to access this data
   WCHAR                   szFile[256];  // file name. "" if nothing to process, "\a" if should shutdown
   PCVoiceFile             pVF;        // speech recognition training
} RESEWAVETHREAD, *PRESEWAVETHREAD;


/*************************************************************************************
ResegmentWavesThread - Thread used for each wave segment.
*/
static DWORD WINAPI ResegmentWavesThread(LPVOID lpParameter)
{
   PRESEWAVETHREAD pInfo = (PRESEWAVETHREAD) lpParameter;

   // repeat
   while (TRUE) {
      Sleep (10); // cheezy way to do this, but no need to do it right

      EnterCriticalSection (&pInfo->critSec);
      if (!pInfo->szFile[0]) {
         // nothing
         LeaveCriticalSection (&pInfo->critSec);
         continue;
      }
      if (pInfo->szFile[0] == L'\a') {
         // want to quit
         LeaveCriticalSection (&pInfo->critSec);
         return 0;
      }

      // else file
      LeaveCriticalSection (&pInfo->critSec);

      // convert to ansi
      char szTemp[256];
      WideCharToMultiByte (CP_ACP, 0, pInfo->szFile, -1, szTemp, sizeof(szTemp),0,0);
      
      // open the wave
      PCM3DWave pWave;
      pWave = new CM3DWave;
      if (!pWave)
         continue;
      if (!pWave->Open (NULL, szTemp)) {
         delete pWave;
         goto done;
      }

      if (pInfo->dwMode == 0) {
         // Disable this since no longer using #define REBUILDHIGHFREQ

#ifdef REBUILDHIGHFREQ
         pWave->FXSRFEATUREExtend ();
#else
         // clear all
         if (!pInfo->fKeepExisting && pInfo->fAlsoClear) {
            memset (pWave->m_adwPitchSamples, 0, sizeof(pWave->m_adwPitchSamples));
            pWave->m_dwSRSamples = 0;
         }

         // if keep info, may just exit quickly here
         if (pInfo->fKeepExisting) {
            BOOL fGood = TRUE;
            DWORD dwPitchSub;
            for (dwPitchSub = 0; dwPitchSub < PITCH_NUM; dwPitchSub++)
               if (!pWave->m_adwPitchSamples[dwPitchSub])
                  fGood = FALSE;
            if (!pWave->m_dwSRSamples)
               fGood = FALSE;
            if (!pWave->m_lWVPHONEME.Num())
               fGood = FALSE;
            if (!pWave->m_lWVWORD.Num())
               fGood = FALSE;

            if (fGood) {
               delete pWave;
               goto done;
            }
         }

         // calculate pitch
         if (!pWave->m_adwPitchSamples[PITCH_F0])
            pWave->CalcPitch (pInfo->fFullPCM ? WAVECALC_TTS_FULLPCM : WAVECALC_TTS_PARTIALPCM, NULL);

         // calculate SR features
         pWave->CalcSRFeaturesIfNeeded (pInfo->fFullPCM ? WAVECALC_TTS_FULLPCM : WAVECALC_TTS_PARTIALPCM, pInfo->hWnd, NULL);

   #ifndef RECALCSRFEATUREHACK   // For the hack, DONT do recognition
         if (!pInfo->fJustFeatures) // BUGFIX - Just features mode
            pInfo->pVF->Recognize ((PWSTR)pWave->m_memSpoken.p, pWave, FALSE, NULL);
   #endif

   #endif // !REBUILDHIGHFREQ

         // may need to calc PITCH_SUB
         if (!pWave->m_adwPitchSamples[PITCH_SUB])
            pWave->CalcPitchIfNeeded (WAVECALC_TTS_FULLPCM, NULL);

         // save changes
         pWave->Save (TRUE, NULL);
      } // dwMode == 0
      else if (pInfo->dwMode == 1) { // fine-tune
         if (!pWave->m_dwSRSamples || !pWave->m_lWVPHONEME.Num())
            goto doneanddelete;   // shouldnt happen

         CSRAnal SRAnal;
         PSRANALBLOCK psab;
         PCVoiceFile pVF = pInfo->pVF;
         PCMLexicon pLex = pInfo->pLex;
         DWORD dwSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
         PLEXPHONE plp;
         PLEXENGLISHPHONE pe;
         WCHAR szThis[16], szLeft[16], szRight[16];
         fp fAcceptableDelta = 0.5;


         char szHuge[10000];
         szHuge[0] = 0;

         // fill in the analysis
         fp fMaxEnergy;
         psab = SRAnal.Init (pWave->m_paSRFeature, pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
         if (!psab)
            goto doneanddelete;   // shouldnt happen

         // note this in the log
         sprintf (szHuge + strlen(szHuge), "\n%s: ", szTemp);

         // loop through all the phonemes
         DWORD wNum = pWave->m_lWVPHONEME.Num();
         PWVPHONEME pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
         BOOL fChanged = FALSE;
         DWORD dwPhone;
         DWORD j, k;
         for (j = 0; j+1 < wNum; j++) {   // dont go all the way to the last
            // get the phoneme number
            dwPhone = PWVPHONEMEToNum (pwp+j, pLex);
            if (dwPhone == dwSilence)
               continue;   // not of interest

            // make sure it's a vowel
            plp = pLex->PhonemeGetUnsort (dwPhone);
            if (!plp)
               continue;
            pe = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
            if (!pe)
               continue;
            if ((pe->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
               continue;   // not a vowel
            BYTE bMultiStress = plp->bStress;

            // get the phoneme strings
            PWVPHONEMEToString (pwp+j, pLex, szThis);
            PWVPHONEMEToString (j ? (pwp+(j-1)) : NULL, pLex, szLeft);
            PWVPHONEMEToString ((j+1 < wNum) ? (pwp+(j+1)) : NULL, pLex, szRight);

            // loop over all the vowels of the same stress and see if any match
            DWORD dwBest = (DWORD)-1;
            DWORD dwThisIndex = (DWORD)-1;
            fp fScoreSum = 0;
            fp fScoreThis = -1;
            fp fScoreBest;
            DWORD dwCountSum = 0;
            DWORD dwVowel;
            for (dwVowel = 0; dwVowel < pLex->PhonemeNum(); dwVowel++) {
               plp = pLex->PhonemeGetUnsort (dwVowel);
               if (!plp)
                  continue;
               if (plp->bStress != bMultiStress)
                  continue;   // exact match required
               pe = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
               if (!pe)
                  continue;
               if ((pe->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
                  continue;   // not a vowel

               // else, have a vowel, so see what this score is
               PCPhoneme pVRPhone = pVF->PhonemeGet (plp->szPhoneLong);
               if (!pVRPhone)
                 continue; // shouldnt happen

               // determine the start/stop of this phoneme
               DWORD dwStart = pwp[j].dwSample / pWave->m_dwSRSkip;
               DWORD dwEnd = pwp[j+1].dwSample / pWave->m_dwSRSkip;
               if (dwStart >= dwEnd)
                  continue; // shouldnt happen

               fp fSRScore = pVRPhone->Compare (psab + dwStart, dwEnd - dwStart, fMaxEnergy,
                  szLeft, szRight, pLex, FALSE /* so wide comparison */, TRUE /* feature distortion*/, TRUE /* force CI */,
                  FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

               // remember this
               fScoreSum += fSRScore;
               dwCountSum++;
               if ((dwBest == (DWORD)-1) || (fSRScore < fScoreBest)) {
                  // found a best match
                  dwBest = dwVowel;
                  fScoreBest = fSRScore;
               }
               if (dwVowel == dwPhone)
                  fScoreThis = fSRScore;   // found score for this
            } // dwVowel

            // if nothing found, or the current phoneme not found, then skip
            if (!dwCountSum || (fScoreThis == -1))
               continue;   // shouldnt happen

            // if the best match is this, then keep
            if (dwBest == dwPhone)
               continue;

            // figure out acceptable distance 
            fScoreSum /= (fp)dwCountSum;
            fp fOKDist = (fScoreSum - (0.0 + fScoreBest)/2.0) * fAcceptableDelta;

            if (fScoreBest > fScoreThis - fOKDist)
               continue;   // the best isnt so much better than the current score that willing to throw out

            // else, change
            fChanged = TRUE;
            plp = pLex->PhonemeGetUnsort (dwBest);

            // note this
            WCHAR swTemp[256];
            swprintf (swTemp, L"\n\t%s -> %s at %.2f sec",
               szThis, plp->szPhoneLong, (double)pwp[j].dwSample / (double)pWave->m_dwSamplesPerSec);
            WideCharToMultiByte (CP_ACP, 0, swTemp, -1, szTemp, sizeof(szTemp), 0, 0);
            strcat (szHuge, szTemp);

            // change it
            DWORD dwLen = (DWORD)wcslen(plp->szPhoneLong);
            for (k = 0; k < sizeof(pwp[j].awcNameLong) / sizeof(WCHAR); k++)
               pwp[j].awcNameLong[k] = (k >= dwLen) ? 0 : plp->szPhoneLong[k];
         } // j, over all phonemes in the wave

         // may need to calc PITCH_SUB
         if (!pWave->m_adwPitchSamples[PITCH_SUB]) {
            fChanged = TRUE;
            pWave->CalcPitchIfNeeded (WAVECALC_TTS_FULLPCM, NULL);
         }

         // save
         if (fChanged) //  && (dwPasses <= 1))
            pWave->Save (TRUE, NULL);

         if (!fChanged) // && pFile)
            strcat (szHuge, "Nothing changed");

         // BUGFIX - flush so can see results
         FILE *pFile = NULL;
         for (j = 0; !pFile && (j < 100); j++) {
            pFile = fopen (MNLPREDOVOWELSTXT, "a+t");
            if (pFile)
               break;
            Sleep (1);  // wait a short while and try again
         } // j
         if (pFile) {
            fputs (szHuge, pFile);
            fclose (pFile);
         }
      } // dwMode = 1

doneanddelete:
      delete pWave;

done:
      // clear the file
      EnterCriticalSection (&pInfo->critSec);
      pInfo->szFile[0] = 0;
      LeaveCriticalSection (&pInfo->critSec);
   }

   // shouldnt get hit
   return 0;
}


/*************************************************************************************
CTTSWork::ResegmentWaves - This is called from TTSMainPage when the user
choses the resegment option. Thus will cause SR to be re-run on all the waves.

inputs
   PCEscPage      pPage - Page
   DWORD          dwMode -
                     0 = resegment waves, using fKeepExisting and fJustFeatures
                     1 = fine-tune phonemes
   BOOL           fKeepExisting - If TRUE, then keep recordings that are already
                  OK.
   BOOL           fJustFeatures - A special mode that only recalculates features
                  and DOESN'T redo speech recognition
   PCVoiceFile    *pVF - Used if dwMode==1
returns
   BOOL - TRUE if succes, FALSE if failure.
*/
BOOL CTTSWork::ResegmentWaves (PCEscPage pPage, DWORD dwMode, BOOL fKeepExisting, BOOL fJustFeatures,
                               PCVoiceFile pVF)
{
   BOOL fAlsoClear = FALSE;
   if ((dwMode == 0) && !fKeepExisting) {
      if (IDYES != pPage->MBYesNo (
         L"Are you sure you wish to re-detect the phonemes?",
         L"Any phoneme/word timing changes you made will be discarded. "
         L"This will take a long time."))
         return TRUE;

      if (IDYES == pPage->MBYesNo (
         L"Do you want to recalculate speech recognition features?",
         L"You usually don't need to do this, and doing so will slow down "
         L"the processing."))
         fAlsoClear = TRUE;
   }

   PCMLexicon pLex = Lexicon();
   if (dwMode == 1) {
      // create a stub
      FILE *pFile = fopen (MNLPREDOVOWELSTXT, "wt");
      if (pFile) {
         fputs ("\n", pFile);
         fclose (pFile);
      }

      if (!pLex)
         return FALSE;  // shouldnt happen
   }


#ifdef RECALCSRFEATUREHACK
   pPage->MBInformation (L"Recalc SRFEATUREHACK is on", L"Dont ship with this!");
#endif

   PCEscControl pControl = pPage->ControlFind (L"rec");
   if (!pControl)
      return TRUE;

   ESCMLISTBOXGETCOUNT gc;
   memset (&gc, 0, sizeof(gc));
   pControl->Message (ESCM_LISTBOXGETCOUNT, &gc);
   DWORD dwNum = gc.dwNum;
   DWORD i;

   // one structure per possible thread
   DWORD dwThreads = HowManyProcessors ();
   RESEWAVETHREAD aInfo[MAXRAYTHREAD];
   CVoiceFile aVF[MAXRAYTHREAD];
   memset (&aInfo, 0, sizeof(aInfo));

   // load voice file
   for (i = 0; i < dwThreads; i++) {
      aInfo[i].fKeepExisting = fKeepExisting;
      aInfo[i].fJustFeatures = fJustFeatures;
      if (!aVF[i].Open (m_szSRFile)) {
         pPage->MBWarning (L"The speech recognition training file couldn't be opened.",
            L"You must have a training file to use this feature.");
         return TRUE;
      }
   }

   // set up the threads
   for (i = 0; i < dwThreads; i++) {
      InitializeCriticalSection (&aInfo[i].critSec);
      aInfo[i].fAlsoClear = fAlsoClear;
      aInfo[i].hWnd = pPage->m_pWindow->m_hWnd;
      aInfo[i].pVF = (dwMode == 1) ? pVF : (PCVoiceFile) &aVF[i];
      aInfo[i].szFile[0] = 0;
      aInfo[i].dwMode = dwMode;
      aInfo[i].pLex = pLex;
      aInfo[i].fFullPCM = m_fFullPCM;

      aInfo[i].hThread = CreateThread (NULL, 0, ResegmentWavesThread, &aInfo[i], 0, &aInfo[i].dwThreadID);
         // NOTE: not using ESCTHREADCOMMITSIZE because memory not much of an issue
      SetThreadPriority (aInfo[i].hThread, VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));  // so doesnt suck up all CPU
   }

   // get the percentages
   fp fStart = DoubleFromControl (pPage, L"resegstart") / 100.0 * (fp)dwNum;
   fp fEnd = DoubleFromControl (pPage, L"resegend") / 100.0 * (fp)dwNum;
   fStart = max(fStart, 0);
   fEnd = max(fEnd, 0);
   fStart = min(fStart, (fp)dwNum);
   fEnd = min(fEnd, (fp)dwNum);
   DWORD dwStart = (DWORD)fStart;
   dwNum = (DWORD)fEnd;

   CProgress Progress;
   Progress.Start (pPage->m_pWindow->m_hWnd, "Analyzing...", TRUE);

   // BUGFIX - Set thread priority
   SetThreadPriority (GetCurrentThread(),VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));

   DWORD j;
   for (i = dwStart; i < dwNum; i++) {
      // get the file
      ESCMLISTBOXGETITEM gi;
      memset (&gi, 0, sizeof(gi));
      gi.dwIndex = i;
      pControl->Message (ESCM_LISTBOXGETITEM, &gi);
      if (!gi.pszName || (gi.pszName[0] != L'f') || (gi.pszName[1] != L':'))
         continue;


      // finda  thread
      while (TRUE) {
         Sleep (10);
         for (j = 0; j < dwThreads; j++) {
            EnterCriticalSection (&aInfo[j].critSec);
            if (aInfo[j].szFile[0]) {
               // already occupied
               LeaveCriticalSection (&aInfo[j].critSec);
               continue;
            }
            break;   // found one
         } // j

         // if got to end and found nothing then wait some more
         if (j >= dwThreads)
            continue;
         break;   // else, found one
      } // while TRUE

      // else, have it, and in critical section
      wcscpy (aInfo[j].szFile, gi.pszName+2);
      LeaveCriticalSection (&aInfo[j].critSec);


      Progress.Update ((fp)(i-dwStart) / (fp)(dwNum-dwStart));
   } // i

   // BUGFIX - Set thread priority
   SetThreadPriority (GetCurrentThread(),VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));

   // release the threads
   for (i = 0; i < dwThreads; i++) {
      while (TRUE) {
         Sleep (10);
         EnterCriticalSection (&aInfo[i].critSec);
         if (aInfo[i].szFile[0]) {
            // still has a file
            LeaveCriticalSection (&aInfo[i].critSec);
            continue;
         }

         // set marker that should shut down
         aInfo[i].szFile[0] = '\a';
         LeaveCriticalSection (&aInfo[i].critSec);
         break;
      }

      // wait for shutdown
      WaitForSingleObject (aInfo[i].hThread, INFINITE);
      CloseHandle (aInfo[i].hThread);
      DeleteCriticalSection (&aInfo[i].critSec);
   } // i


   return TRUE;
}


/*************************************************************************************
PWVPHONEMEToString - Converts a PWVPHONEME to a string for the phoneme.

inputs
   PWVPHONEME        pwp - Phoneme. This can be NULL, in which case the string will be <sil>
   PCMLexicon        pLex - Lexicon to use
   PWSTR             psz - Filled with the string. Must be at least 9 characters
*/
void PWVPHONEMEToString (PWVPHONEME pwp, PCMLexicon pLex, PWSTR psz)
{
   if (!pwp) {
      wcscpy (psz, pLex->PhonemeSilence ());
      return;
   }

   // else
   memcpy (psz, pwp->awcNameLong, sizeof(pwp->awcNameLong));
   psz[sizeof(pwp->awcNameLong)/sizeof(WCHAR)] = 0;
}


/*************************************************************************************
PWVPHONEMEToNum - Converts a PWVPHONEME to an unsorted phoneme number

inputs
   PWVPHONEME        pwp - Phoneme. This can be NULL, in which case the string will be <sil>
   PCMLexicon        pLex - Lexicon to use
returns
   DWORD - Unsorted phoneme number
*/
DWORD PWVPHONEMEToNum (PWVPHONEME pwp, PCMLexicon pLex)
{
   WCHAR szPhone[16];
   PWVPHONEMEToString (pwp, pLex, szPhone);

   DWORD dwRet = pLex->PhonemeFindUnsort (szPhone);
   if (dwRet == (DWORD)-1)
      dwRet= pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   return dwRet;
}


static int _cdecl PFPSort (const void *elem1, const void *elem2)
{
   fp *pdw1, *pdw2;
   pdw1 = *((fp**) elem1);
   pdw2 = *((fp**) elem2);

   if (*pdw1 > *pdw2)
      return -1;
   else if (*pdw1 < *pdw2)
      return 1;
   else
      return 0;
}



/*************************************************************************************
CTTSWork::RedoVowels - Re-recognizes the vowels in all the waves, and saves
out any changes.

inputs
   PCEscPage      pPage - Page

returns
   BOOL - TRUE if succes, FALSE if failure.
*/
BOOL CTTSWork::RedoVowels (PCEscPage pPage)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex) {
      pPage->MBWarning (L"You must supply a lexicon.");
      return FALSE;
   }
   DWORD dwSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // fill in a list with all the file names
   CListVariable lFiles;
   DWORD i;
   PCWaveFileInfo *ppwfi = (PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(0);
   lFiles.Required (m_pWaveDir->m_lPCWaveFileInfo.Num());
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++, ppwfi++) {
      PCWaveFileInfo pwfi = ppwfi[0];
      lFiles.Add (pwfi->m_pszFile, (wcslen(pwfi->m_pszFile)+1)*sizeof(WCHAR));
   } // i

   CVoiceFile VF;
   VF.LexiconSet (pLex->m_szFile);
   VF.m_fCDPhone = FALSE;  // dont use context dependent
   {
      CProgress Progress;
      Progress.Start (pPage->m_pWindow->m_hWnd, "Analyzing, part 1...", TRUE);

      // step one, train a recognizer
      // Progress.Push (0, 0.5);
      CM3DWave Wave;
      char szTemp[256];
      PWSTR psz;
      BOOL fRet;
      CHashString hWordWeight;
      hWordWeight.Init (sizeof(fp));
      
      // two passes
      DWORD dwPass;
      for (dwPass = 0; dwPass < 2; dwPass++) {
         Progress.Push ((fp)dwPass / 2.0, (fp)(dwPass+1) / 2.0);

         for (i = 0; i < lFiles.Num(); i++) {
            // load it in
            psz = (PWSTR)lFiles.Get(i);
            WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
            if (!Wave.Open (NULL, szTemp)) {
               WCHAR szw[512];
               swprintf (szw, L"The recording, %s, couldn't be opened.", psz);
               EscMessageBox (pPage->m_pWindow->m_hWnd, ASPString(),
                  szw,
                  L"Analysis will stop.",
                  MB_ICONEXCLAMATION | MB_OK);
               return FALSE;
            }

            // remember if have phoneme info
            BOOL fSRInfo = (Wave.m_dwSRSamples ? TRUE : FALSE);

            // progress
            Progress.Push ((fp)i / (fp)lFiles.Num(), (fp)(i+1) / (fp)lFiles.Num());
            Progress.Update (0);
            fRet = VF.TrainSingleWave (&Wave, pPage->m_pWindow->m_hWnd, &Progress, TRUE,
               &hWordWeight, !dwPass);
            Progress.Pop ();

            // exit?
            if (!fRet)
               return FALSE;

            // save..
            if (!fSRInfo)
               Wave.Save (TRUE, NULL);
         } // i

         Progress.Pop();

         // if finshed pass 1 then need to calc the new word weights
         if (!dwPass) {
            CListFixed lpfp;
            fp *pfWeight;
            lpfp.Init (sizeof(fp*));

            lpfp.Required (hWordWeight.Num());
            for (i = 0; i < hWordWeight.Num(); i++) {
               pfWeight = (fp*)hWordWeight.Get(i);
               lpfp.Add (&pfWeight);
            }

            // sort
            fp **papfp = (fp**)lpfp.Get(0);
            qsort (papfp, lpfp.Num(), sizeof(fp*), PFPSort);

            // change the values
            fp f;
            for (i = 0; i < lpfp.Num(); i++) {
               f = (fp)i / 100;
               f = f * f;  //so very common words have very low score
               f = f * 0.9 + 0.1;  // so even most common words have some weight
               *(papfp[i]) = min(1.0, f);
            } // i
         } // if !dwPass
      } // dwPass
      //Progress.Pop();
   }

   return ResegmentWaves (pPage, 1, FALSE, FALSE, &VF);
   // pass this in
#if 0 // old code
   // change phonemes
   Progress.Push (.5, 1);
// #define MULTISRPASS        // indicates that should try this several times at different values, to test
   DWORD dwPasses = 1;
#ifdef MULTISRPASS
   dwPasses = 2;
#endif
   DWORD j, k;
   fp fAcceptableDelta;
   WCHAR szThis[16], szLeft[16], szRight[16];
   PLEXPHONE plp;
   PLEXENGLISHPHONE pe;
   CMem memSRANALBLOCK;
   CSRAnal SRAnal;
   PSRANALBLOCK psab;
   for (dwPass = 0; dwPass < dwPasses; dwPass++) {
      if (dwPasses > 1) {
         sprintf (szTemp, "c:\\temp\\MNLPRedoVowels%d.txt", (int)dwPass);
         fAcceptableDelta = 0.5 - (fp)dwPass * 0.1;
      }
      else {
         strcpy (szTemp, MNLPREDOVOWELSTXT);
         fAcceptableDelta = 0.5;
      }
      FILE *pFile = fopen (szTemp, "wt");

      // over all the waves
      for (i = 0; i < lFiles.Num(); i++) {
         Progress.Update ((fp)i / (fp)lFiles.Num());

         // load it in
         psz = (PWSTR)lFiles.Get(i);
         WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
         if (!Wave.Open (NULL, szTemp))
            continue;
         if (!Wave.m_dwSRSamples || !Wave.m_lWVPHONEME.Num())
            continue;   // shouldnt happen

         // fill in the analysis
         fp fMaxEnergy;
         psab = SRAnal.Init (Wave.m_paSRFeature, Wave.m_dwSRSamples, &fMaxEnergy);
         if (!psab)
            continue;   // shouldnt happen

         // note this in the log
         if (pFile) {
            if (!(i%16))
               fflush (pFile);
            fprintf (pFile, "\n%s: ", szTemp);
         }

         // loop through all the phonemes
         DWORD wNum = Wave.m_lWVPHONEME.Num();
         PWVPHONEME pwp = (PWVPHONEME) Wave.m_lWVPHONEME.Get(0);
         BOOL fChanged = FALSE;
         DWORD dwPhone;
         for (j = 0; j+1 < wNum; j++) {   // dont go all the way to the last
            // get the phoneme number
            dwPhone = PWVPHONEMEToNum (pwp+j, pLex);
            if (dwPhone == dwSilence)
               continue;   // not of interest

            // make sure it's a vowel
            plp = pLex->PhonemeGetUnsort (dwPhone);
            if (!plp)
               continue;
            pe = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
            if (!pe)
               continue;
            if ((pe->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
               continue;   // not a vowel
            BYTE bIsStressed = plp->bStress;

            // get the phoneme strings
            PWVPHONEMEToString (pwp+j, pLex, szThis);
            PWVPHONEMEToString (j ? (pwp+(j-1)) : NULL, pLex, szLeft);
            PWVPHONEMEToString ((j+1 < wNum) ? (pwp+(j+1)) : NULL, pLex, szRight);

            // loop over all the vowels of the same stress and see if any match
            DWORD dwBest = (DWORD)-1;
            DWORD dwThisIndex = (DWORD)-1;
            fp fScoreSum = 0;
            fp fScoreThis = -1;
            fp fScoreBest;
            DWORD dwCountSum = 0;
            DWORD dwVowel;
            for (dwVowel = 0; dwVowel < pLex->PhonemeNum(); dwVowel++) {
               plp = pLex->PhonemeGetUnsort (dwVowel);
               if (!plp)
                  continue;
               if (plp->bStress != bIsStressed)
                  continue;   // exact match required
               pe = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
               if (!pe)
                  continue;
               if ((pe->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
                  continue;   // not a vowel

               // else, have a vowel, so see what this score is
               PCPhoneme pVRPhone = VF.PhonemeGet (plp->szPhone);
               if (!pVRPhone)
                 continue; // shouldnt happen

               // determine the start/stop of this phoneme
               DWORD dwStart = pwp[j].dwSample / Wave.m_dwSRSkip;
               DWORD dwEnd = pwp[j+1].dwSample / Wave.m_dwSRSkip;
               if (dwStart >= dwEnd)
                  continue; // shouldnt happen

               fp fSRScore = pVRPhone->Compare (psab + dwStart, dwEnd - dwStart, fMaxEnergy,
                  szLeft, szRight, pLex, FALSE /* so wide comparison */, TRUE /* feature distortion*/, TRUE /* force CI */,
                  FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

               // remember this
               fScoreSum += fSRScore;
               dwCountSum++;
               if ((dwBest == (DWORD)-1) || (fSRScore < fScoreBest)) {
                  // found a best match
                  dwBest = dwVowel;
                  fScoreBest = fSRScore;
               }
               if (dwVowel == dwPhone)
                  fScoreThis = fSRScore;   // found score for this
            } // dwVowel

            // if nothing found, or the current phoneme not found, then skip
            if (!dwCountSum || (fScoreThis == -1))
               continue;   // shouldnt happen

            // if the best match is this, then keep
            if (dwBest == dwPhone)
               continue;

            // figure out acceptable distance 
            fScoreSum /= (fp)dwCountSum;
            fp fOKDist = (fScoreSum - (0.0 + fScoreBest)/2.0) * fAcceptableDelta;

            if (fScoreBest > fScoreThis - fOKDist)
               continue;   // the best isnt so much better than the current score that willing to throw out

            // else, change
            fChanged = TRUE;
            plp = pLex->PhonemeGetUnsort (dwBest);

            // note this
            WCHAR swTemp[256];
            swprintf (swTemp, L"\n\t%s -> %s at %.2f sec",
               szThis, plp->szPhone, (double)pwp[j].dwSample / (double)Wave.m_dwSamplesPerSec);
            WideCharToMultiByte (CP_ACP, 0, swTemp, -1, szTemp, sizeof(szTemp), 0, 0);
            if (pFile)
               fprintf (pFile, szTemp);

            // change it
            DWORD dwLen = (DWORD)wcslen(plp->szPhone);
            for (k = 0; k < sizeof(pwp[j].awcName) / sizeof(WCHAR); k++)
               pwp[j].awcName[k] = (k >= dwLen) ? 0 : plp->szPhone[k];
         } // j, over all phonemes in the wave

         // save
         if (fChanged && (dwPasses <= 1))
            Wave.Save (TRUE, NULL);

         if (!fChanged && pFile)
            fprintf (pFile, "Nothing changed");

         // BUGFIX - flush so can see results
         if (pFile)
            fflush (pFile);
      } // i

      if (pFile)
         fclose (pFile);
   } // dwPass
   Progress.Pop();
#endif // 0

   // finally, alert user that saved
   pPage->MBInformation (L"Transcript of changes saved",
      L"A list of all the changed phonemes has been saved under c:\\temp\\MNLPRedoVowels.txt. "
      L"You might want to look at the list to see what vowels were changed.");





   return TRUE;
}


/****************************************************************************
TTSMainAddManyPage
*/
static BOOL TTSMainAddManyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTTSMP pTTSMP = (PTTSMP) pPage->m_pUserData;
   PCTTSWork pVF = pTTSMP->pTTS;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      pPage->Message (ESCM_USER+94);
      break;

   case ESCM_USER+94:   // set the text of all the words
      {
         MemZero (&gMemTemp);
         DWORD dwNum = pTTSMP->pLexUnknown->WordNum();
         DWORD i;
         WCHAR szWord[256];
         for (i = 0; i < dwNum; i++) {
            // get the text
            if (!pTTSMP->pLexUnknown->WordGet(i, szWord, sizeof(szWord), NULL))
               continue;

            // see if it's on the misspelled list
            if (pVF->m_pLexMisspelled->WordExists(szWord))
               continue;   // dont add

            // else, show
            MemCat (&gMemTemp, szWord);
            MemCat (&gMemTemp, L"\r\n");
         } // i

         PCEscControl pControl = pPage->ControlFind (L"addtolex");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR)gMemTemp.p);
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszWrd = L"wrd:";
         DWORD dwWrdLen = (DWORD)wcslen(pszWrd);
         if (!wcsncmp (psz, pszWrd, dwWrdLen)) {
            DWORD dwNum = _wtoi(psz + dwWrdLen);
            WCHAR szWord[256];
            if (!pTTSMP->pLexUnknown->WordGet(dwNum, szWord, sizeof(szWord), NULL))
               return TRUE;

            // add/remove
            if (pVF->m_pLexMisspelled->WordExists(szWord))
               pVF->m_pLexMisspelled->WordRemove (pVF->m_pLexMisspelled->WordFind(szWord));
            else {
               // add 
               CListVariable lPron;
               pVF->m_pLexMisspelled->WordSet (szWord, &lPron);
            }

            pPage->Message (ESCM_USER+94);

            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Add many sentences page";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"UNKWORDS")) {
            DWORD dwNum = pTTSMP->pLexUnknown->WordNum();
            DWORD dwColumns = 4;
            DWORD dwRows = (dwNum + dwColumns - 1) / dwColumns;
            DWORD i, j;

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<tr>");

            DWORD dwIndex = 0;
            WCHAR szWord[256];
            // CListVariable lPron;
            if (dwNum) for (i = 0; i < dwColumns; i++) {
               MemCat (&gMemTemp, L"<td><bold>");
               for (j = 0; (j < dwRows) && (dwIndex < dwNum); j++, dwIndex++) {
                  MemCat (&gMemTemp, L"<button style=x checkbox=true name=wrd:");
                  MemCat (&gMemTemp, (int)dwIndex);

                  if (!pTTSMP->pLexUnknown->WordGet(dwIndex, szWord, sizeof(szWord), NULL))
                     szWord[0] = 0;

                  // see if it's misspelled and check it
                  if (pVF->m_pLexMisspelled->WordExists (szWord))
                     MemCat (&gMemTemp, L" checked=true");

                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, szWord);
                  MemCat (&gMemTemp, L"</button><br/>");
               } // j
               MemCat (&gMemTemp, L"</bold></td>");
            } // i

            if (!dwNum)
               MemCat (&gMemTemp, L"<td>No unknown words</td>");

            MemCat (&gMemTemp, L"</tr>");
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SENTUNKNOWN") || !_wcsicmp(p->pszSubName, L"SENTMISSPELLED") ||
            !_wcsicmp(p->pszSubName, L"SENTONLIST") || !_wcsicmp(p->pszSubName, L"SENTADDED")) {

            MemZero (&gMemTemp);
            DWORD i;
            PCListVariable pl = NULL, pl2 = NULL;
            if (!_wcsicmp(p->pszSubName, L"SENTUNKNOWN"))
               pl = pTTSMP->plHaveUnknown;
            else if (!_wcsicmp(p->pszSubName, L"SENTMISSPELLED"))
               pl = pTTSMP->plHaveMisspelled;
            else if (!_wcsicmp(p->pszSubName, L"SENTONLIST")) {
               pl = pTTSMP->plAlreadyExistInToDo;
               pl2 = pTTSMP->plAlreadyExistInWave;
            }
            else // sentadded
               pl = pTTSMP->plToAdd;

            DWORD dwNum = pl->Num() + (pl2 ? pl2->Num() : 0);
            for (i = 0; i < dwNum; i++) {
               MemCat (&gMemTemp, L"<li>");
               MemCatSanitize (&gMemTemp, (PWSTR)(i >= pl->Num() ? pl2->Get(i-pl->Num()) : pl->Get(i)));
               MemCat (&gMemTemp, L"</li>");
            } // i

            if (!i)
               MemCat (&gMemTemp, L"<li>None</li>");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;

   } // switch

   return DefPage (pPage, dwMessage, pParam);
}

/****************************************************************************
TTSMainPage
*/
static BOOL TTSMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTTSMP pTTSMP = (PTTSMP) pPage->m_pUserData;
   PCTTSWork pVF = pTTSMP->pTTS;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         ComboBoxSet (pPage, L"triphonegroup", pVF->m_dwTriPhoneGroup);
         // ComboBoxSet (pPage, L"triphonepitch", pVF->m_dwTriPhonePitch);
         ComboBoxSet (pPage, L"timecompress", pVF->m_dwTimeCompress);
         ComboBoxSet (pPage, L"freqcompress", pVF->m_dwFreqCompress);
         ComboBoxSet (pPage, L"pcmcompress", pVF->m_dwPCMCompress);

         PCEscControl pControl;

         pControl = pPage->ControlFind (L"keeplog");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pVF->m_fKeepLog);
         //pControl = pPage->ControlFind (L"WordStartEndCombine");
         //if (pControl)
         //   pControl->AttribSetBOOL (Checked(), pVF->m_fWordStartEndCombine);

         pControl = pPage->ControlFind (L"pauselessoften");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pVF->m_fPauseLessOften);

         //pControl = pPage->ControlFind (L"fullpcm");
         //if (pControl)
         //   pControl->AttribSetBOOL (Checked(), pVF->m_fFullPCM);

         WCHAR szTemp[64];
         DWORD i;
         for (i = 0; i < NUMPROSODYTTS; i++) {
            swprintf (szTemp, L"prosodytts%d", (int) i);
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), pVF->m_aszProsodyTTS[i]);
         }

         if (pVF->m_memTTSTARGETCOSTS.m_dwCurPosn != sizeof(TTSTARGETCOSTS))
            if (pControl = pPage->ControlFind (L"deljoincosts"))
               pControl->Enable (FALSE);

         DoubleToControl (pPage, L"mininstance", (double)pVF->m_dwMinInstance);
         // DoubleToControl (pPage, L"wordcache", (double)pVF->m_dwWordCache);
         DoubleToControl (pPage, L"minexamples", (double)pVF->m_dwMinExamples);
         //DoubleToControl (pPage, L"multiunit", (double)pVF->m_dwMultiUnit);
         //DoubleToControl (pPage, L"multisyllableunit", (double)pVF->m_dwMultiSyllableUnit);
         //DoubleToControl (pPage, L"connectunits", (double)pVF->m_dwConnectUnits);
         DoubleToControl (pPage, L"totalunits", (double)pVF->m_dwTotalUnits);

         // update the file list
         pPage->Message (ESCM_USER+82);
         pPage->Message (ESCM_USER+84);
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"triphonegroup")) {
            if (dwVal == pVF->m_dwTriPhoneGroup)
               return TRUE;

            pVF->m_dwTriPhoneGroup = dwVal;
            return TRUE;
         }
         //else if (!_wcsicmp(psz, L"triphonepitch")) {
         //   if (dwVal == pVF->m_dwTriPhonePitch)
         //      return TRUE;

         //   pVF->m_dwTriPhonePitch = dwVal;
         //   return TRUE;
         //}
         else if (!_wcsicmp(psz, L"timecompress")) {
            if (dwVal == pVF->m_dwTimeCompress)
               return TRUE;

            pVF->m_dwTimeCompress = dwVal;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"freqcompress")) {
            if (dwVal == pVF->m_dwFreqCompress)
               return TRUE;

            pVF->m_dwFreqCompress = dwVal;
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"pcmcompress")) {
            if (dwVal == pVF->m_dwPCMCompress)
               return TRUE;

            pVF->m_dwPCMCompress = dwVal;
            return TRUE;
         }
      }
      break;

   case ESCM_USER+82:   // update the file list, doing a scan to sync
      {
         // scan
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Updating list...");

            Progress.Push (0, 0.75);
            pVF->m_pWaveDir->SyncFiles (&Progress, NULL);
            Progress.Pop ();

            Progress.Push (0.75, 1.0);
            pVF->m_pWaveDirEx->SyncFiles (&Progress, NULL);
            Progress.Pop ();
         }

         // update list
         pPage->Message (ESCM_USER+83);
      }
      return TRUE;

   case ESCM_USER+83:   // udate the file list, NO SCAN
      {
         WCHAR szTextFilter[256], szSpeakerFilter[256];
         szTextFilter[0] = szSpeakerFilter[0] = 0;
         PCEscControl pControl;
         DWORD dwNeed;
         pControl = pPage->ControlFind (L"textfilter");
         if (pControl)
            pControl->AttribGet (Text(), szTextFilter, sizeof(szTextFilter), &dwNeed);
         pControl = pPage->ControlFind (L"speakerfilter");
         if (pControl)
            pControl->AttribGet (Text(), szSpeakerFilter, sizeof(szSpeakerFilter), &dwNeed);

         pVF->m_pWaveDir->FillListBox (pPage, L"rec", szTextFilter, szSpeakerFilter, NULL);

         pVF->m_pWaveDirEx->FillListBox (pPage, L"recex", NULL, NULL, NULL);

         // find the stats
         pControl = pPage->ControlFind (L"recordcount");
         if (pControl) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"%d entries", (int) pVF->m_pWaveDir->m_lPCWaveFileInfo.Num());
            ESCMSTATUSTEXT st;
            memset (&st, 0, sizeof(st));
            st.pszText = szTemp;
            pControl->Message (ESCM_STATUSTEXT, &st);
         }
         pControl = pPage->ControlFind (L"recordexcount");
         if (pControl) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"%d entries", (int) pVF->m_pWaveDirEx->m_lPCWaveFileInfo.Num());
            ESCMSTATUSTEXT st;
            memset (&st, 0, sizeof(st));
            st.pszText = szTemp;
            pControl->Message (ESCM_STATUSTEXT, &st);
         }
      }
      return TRUE;


   case ESCM_USER+84:   // udate the to-do- list
      {
         WCHAR szTextFilter[256];
         szTextFilter[0] = 0;
         PCEscControl pControl;
         DWORD dwNeed;
         pControl = pPage->ControlFind (L"todotextfilter");
         if (pControl)
            pControl->AttribGet (Text(), szTextFilter, sizeof(szTextFilter), &dwNeed);

         pVF->m_pWaveToDo->FillListBox (pPage, L"todo", szTextFilter);

         // find the stats
         pControl = pPage->ControlFind (L"todocount");
         if (pControl) {
            WCHAR szTemp[64];
            swprintf (szTemp, L"%d entries", (int) pVF->m_pWaveToDo->m_lToDo.Num());
            ESCMSTATUSTEXT st;
            memset (&st, 0, sizeof(st));
            st.pszText = szTemp;
            pControl->Message (ESCM_STATUSTEXT, &st);
         }
      }
      return TRUE;


   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz || !p->pszName)
            break;

         // list
         if (!_wcsicmp(psz, L"rec")) {
            // if click on file the play that file
            if ((p->pszName[0] == L'f') && (p->pszName[1] == L':')) {
               pVF->m_pWaveDir->PlayFile (NULL, p->pszName+2);
               return TRUE;
            }
            return TRUE;
         } // recording
         if (!_wcsicmp(psz, L"recex")) {
            // if click on file the play that file
            if ((p->pszName[0] == L'f') && (p->pszName[1] == L':')) {
               pVF->m_pWaveDirEx->PlayFile (NULL, p->pszName+2);
               return TRUE;
            }
            return TRUE;
         } // recording
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"mininstance") || !_wcsicmp(psz, L"wordcache") || !_wcsicmp(psz, L"multiunit") ||
               !_wcsicmp(psz, L"multisyllableunit") || !_wcsicmp(psz, L"connectunits") || !_wcsicmp(psz, L"minexamples") ||
               !_wcsicmp(psz, L"totalunits") ) {
            pVF->m_dwMinInstance = (DWORD) DoubleFromControl (pPage, L"mininstance");
            pVF->m_dwMinInstance = max(pVF->m_dwMinInstance, 1);

            // pVF->m_dwMultiUnit = (DWORD) DoubleFromControl (pPage, L"multiunit");
            //pVF->m_dwMultiSyllableUnit = (DWORD) DoubleFromControl (pPage, L"multisyllableunit");
            //pVF->m_dwConnectUnits = (DWORD) DoubleFromControl (pPage, L"connectunits");

            pVF->m_dwTotalUnits = (DWORD) DoubleFromControl (pPage, L"totalunits");
            pVF->m_dwTotalUnits = max(pVF->m_dwTotalUnits, 1);

            // pVF->m_dwWordCache = (DWORD) DoubleFromControl (pPage, L"wordcache");
            pVF->m_dwMinExamples = (DWORD) DoubleFromControl (pPage, L"minexamples");
            pVF->m_dwMinExamples = max(pVF->m_dwMinExamples, 1);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"textfilter") || !_wcsicmp(psz, L"speakerfilter")) {
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todotextfilter")) {
            pPage->Message (ESCM_USER+84);
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

         // figure out the record list current selection
         PWSTR pszSelRec = NULL;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"rec");
         if (pControl) {
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD)pControl->AttribGetInt(CurSel());
            if (pControl->Message (ESCM_LISTBOXGETITEM, &gi))
               pszSelRec = gi.pszName;

            if (pszSelRec && (pszSelRec[0] == L'f') && (pszSelRec[1] == L':'))
               pszSelRec += 2;
            else
               pszSelRec = NULL;
         }

         // figure out the record list current selection
         PWSTR pszSelRecEx = NULL;
         pControl = pPage->ControlFind (L"recex");
         if (pControl) {
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD)pControl->AttribGetInt(CurSel());
            if (pControl->Message (ESCM_LISTBOXGETITEM, &gi))
               pszSelRecEx = gi.pszName;

            if (pszSelRecEx && (pszSelRecEx[0] == L'f') && (pszSelRecEx[1] == L':'))
               pszSelRecEx += 2;
            else
               pszSelRecEx = NULL;
         }

         // figure out the to-do list current selection
         PWSTR pszSelToDo = NULL;
         pControl = pPage->ControlFind (L"todo");
         if (pControl) {
            ESCMLISTBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = (DWORD)pControl->AttribGetInt(CurSel());
            if (pControl->Message (ESCM_LISTBOXGETITEM, &gi))
               pszSelToDo = gi.pszName;

            if (pszSelToDo && (pszSelToDo[0] == L'f') && (pszSelToDo[1] == L':'))
               pszSelToDo += 2;
            else
               pszSelToDo = NULL;
         }

         PWSTR pszProsodyTTSOpen = L"prosodyttsopen";
         DWORD dwProsodyTTSOpenLen = (DWORD)wcslen(pszProsodyTTSOpen);
         if (!_wcsicmp(psz, L"keeplog")) {
            pVF->m_fKeepLog = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"pauselessoften")) {
            pVF->m_fPauseLessOften = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         //else if (!_wcsicmp(psz, L"fullpcm")) {
         //   pVF->m_fFullPCM = p->pControl->AttribGetBOOL(Checked());
         //   return TRUE;
         //}
         else if (!_wcsicmp(psz, L"deljoincosts")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you want to delete the target costs?"))
               return TRUE;

            pVF->m_memTTSTARGETCOSTS.m_dwCurPosn = 0;

            if (pControl = pPage->ControlFind (L"deljoincosts"))
               pControl->Enable (FALSE);

            return TRUE;
         }
         else if (!_wcsnicmp(psz, pszProsodyTTSOpen, dwProsodyTTSOpenLen)) {
            DWORD dwNum = _wtoi(psz + dwProsodyTTSOpenLen);
            WCHAR szTemp[256];
            wcscpy (szTemp, pVF->m_aszProsodyTTS[dwNum]);

            if (TTSProsodyFileOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE, TRUE))
               wcscpy (pVF->m_aszProsodyTTS[dwNum], szTemp);
            else
               pVF->m_aszProsodyTTS[dwNum][0] = 0;

            swprintf (szTemp, L"prosodytts%d", (int)dwNum);
            PCEscControl pControl;
            if (pControl = pPage->ControlFind (szTemp))
               pControl->AttribSet (Text(), pVF->m_aszProsodyTTS[dwNum]);
            return TRUE;
         }
         //else if (!_wcsicmp(psz, L"WordStartEndCombine")) {
         //   pVF->m_fWordStartEndCombine = p->pControl->AttribGetBOOL(Checked());
         //   return TRUE;
         //}
         else if (!_wcsicmp(psz, L"comwords")) {
            pVF->ScanCommonWords (pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todoadd")) {
            WCHAR szAdd[512];
            DWORD dwNeed;
            szAdd[0] = 0;
            PCEscControl pControl = pPage->ControlFind (L"todoaddedit");
            if (pControl)
               pControl->AttribGet (Text(), szAdd, sizeof(szAdd), &dwNeed);
            if (!szAdd[0]) {
               pPage->MBWarning (L"You must type in a sentence to add.");
               return TRUE;
            }

            pVF->m_pWaveToDo->AddToDo (szAdd);
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todoremove")) {
            if (!pszSelToDo) {
               pPage->MBWarning (L"You must select a sentence before you can remove it.");
               return TRUE;
            }

            pVF->m_pWaveToDo->RemoveToDo (pszSelToDo);
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todoaddmany")) {
            pTTSMP->plToAdd->Clear();
            pTTSMP->plAlreadyExistInToDo->Clear();
            pTTSMP->plAlreadyExistInWave->Clear();
            pTTSMP->plHaveMisspelled->Clear();
            pTTSMP->plHaveUnknown->Clear();
            pTTSMP->pLexUnknown->Clear();

            if (!pVF->ToDoReadIn (NULL, pPage->m_pWindow->m_hWnd, pTTSMP->plToAdd, pTTSMP->plAlreadyExistInToDo,
               pTTSMP->plAlreadyExistInWave, pTTSMP->plHaveMisspelled, pTTSMP->plHaveUnknown, pTTSMP->pLexUnknown)) {
               pPage->MBWarning (L"The text file wasn't read in.");
               return TRUE;
            }

            // add the successful ones to the list
            DWORD i;
            for (i = 0; i < pTTSMP->plToAdd->Num(); i++)
               pVF->m_pWaveToDo->AddToDo ((PWSTR) pTTSMP->plToAdd->Get(i));

            pPage->Exit (L"TTSMainAddMany");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todorecommend")) {
            DWORD dwNum = pVF->m_pWaveToDo->m_lToDo.Num();

            DWORD dwRet = pVF->Recommend(pPage->m_pWindow->m_hWnd);
            if (!dwRet) {
               pPage->MBWarning (L"Recommended failed.", L"You may not have selected a lexicon to use.");
               return TRUE;
            }


            // update list
            pPage->Message (ESCM_USER+84);

            // depending upon add
            switch (dwRet) {
            case 1:  // need more
               pPage->MBInformation (L"New recommendations were add but you WILL NEED more.",
                  L"Not enough words, or not enough of a variety of words, were found to fill in "
                  L"all the phoneme information. You should use \"Recommend\" again with a different "
                  L"text file.");
               break;
            case 2:  // could be better
               pPage->MBInformation (L"New recommendations were add but you SHOULD get more.",
                  L"Not enough words, or not enough of a variety of words, were found to fill in "
                  L"all the phoneme information. You should use \"Recommend\" again with a different "
                  L"text file.");
               break;
            default:
            case 3:  // good
               pPage->MBInformation (
                  (dwNum == pVF->m_pWaveToDo->m_lToDo.Num()) ?
                  L"Nothing was added because you already have enough information recorded." :
                  L"New sentences were added."
                  );
                  break;
            }

            // tell user about deleting unsure
            if (dwNum != pVF->m_pWaveToDo->m_lToDo.Num())
               pPage->MBInformation (L"If you are unsure how to pronounce any of the words "
                     L"in the new sentence then delete the culprit sentences and press \"Recommend\" again.");

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"todoclearall")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear out the to-do list?"))
               return TRUE;

            pVF->m_pWaveToDo->ClearToDo ();
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearmisspelled")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear out the misspelled words list?"))
               return TRUE;

            pVF->m_pLexMisspelled->Clear();
            
            pPage->MBInformation (L"The words have been cleared.");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"recordsel") || !_wcsicmp(psz, L"recordall")) {
            BOOL fSel = !_wcsicmp(psz, L"recordsel");
            if (fSel && !pszSelToDo) {
               pPage->MBWarning (L"You must select a sentence before you can record it.");
               return TRUE;
            }
            pTTSMP->pszRecOnly = NULL;
            if (fSel && pszSelToDo) {
               MemZero (pTTSMP->memRecOnly);
               MemCat (pTTSMP->memRecOnly, pszSelToDo);
               pTTSMP->pszRecOnly = (PWSTR)pTTSMP->memRecOnly->p;
            }
            pPage->Exit (L"record");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"edit")) {
            if (!pszSelRec) {
               pPage->MBWarning (L"You must select a file before you can edit it.");
               return TRUE;
            }

            pVF->m_pWaveDir->EditFile (NULL, pszSelRec, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"rerecord")) {
#if 0 // Hack for blizzard 2007
            pVF->BlizzardSmallVoice (pPage->m_pWindow->m_hWnd);

            // update the list
            pPage->Message (ESCM_USER+82);

            return TRUE;
#endif

            if (!pszSelRec) {
               pPage->MBWarning (L"You must select a file before you can re-record it.");
               return TRUE;
            }

            CM3DWave Wave;
            char szTemp[512];
            WideCharToMultiByte (CP_ACP, 0, pszSelRec, -1, szTemp, sizeof(szTemp), 0, 0);
            if (!Wave.Open (NULL, szTemp)) {
               pPage->MBWarning (L"The wave file couldn't be opened.");
               return TRUE;
            }

            PCM3DWave pNew = Wave.Record (pPage->m_pWindow->m_hWnd, NULL);
            if (!pNew)
               return TRUE;   // user cancelled

            // save the new one
            strcpy (pNew->m_szFile, szTemp);
            MemZero (&pNew->m_memSpoken);
            MemCat (&pNew->m_memSpoken, (PWSTR)Wave.m_memSpoken.p);
            MemZero (&pNew->m_memSpeaker);
            MemCat (&pNew->m_memSpeaker, (PWSTR)Wave.m_memSpeaker.p);

            // save it
            pNew->Save (FALSE, NULL);
            delete pNew;

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"add")) {
            if (pVF->m_pWaveDir->AddFilesUI (pPage->m_pWindow->m_hWnd, TRUE)) {
               pPage->Message (ESCM_USER+82);
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addex")) {
            if (pVF->m_pWaveDirEx->AddFilesUI (pPage->m_pWindow->m_hWnd, TRUE)) {
               pPage->Message (ESCM_USER+82);
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"remove")) {
            if (!pszSelRec) {
               pPage->MBWarning (L"You must select a file before you can remove it.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the selected file from the training list?"))
               return TRUE;

            pVF->m_pWaveDir->RemoveFile (pszSelRec);
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"removeex")) {
            if (!pszSelRecEx) {
               pPage->MBWarning (L"You must select a file before you can remove it.");
               return TRUE;
            }

            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the selected file from the training list?"))
               return TRUE;

            pVF->m_pWaveDirEx->RemoveFile (pszSelRecEx);
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearall")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear out the training list?"))
               return TRUE;

            pVF->m_pWaveDir->ClearFiles ();
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearallex")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear out the training list?"))
               return TRUE;

            pVF->m_pWaveDirEx->ClearFiles ();
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"segmentempty"))
            return pVF->ResegmentWaves (pPage, 0, TRUE, FALSE, NULL);
         else if (!_wcsicmp(psz, L"resegment")) {
            BOOL fJustFeatures = FALSE;
            if (GetKeyState (VK_CONTROL) < 0) {
               fJustFeatures = TRUE;
               if (IDYES != pPage->MBYesNo (L"Do you only want to recalculate only the features?",
                     L"This option won't recalculate the phonemes if it doesn't have to, but all features will be recalculated."))
                  fJustFeatures = FALSE;
            }

            return pVF->ResegmentWaves (pPage, 0, FALSE, fJustFeatures, NULL);
         }
         else if (!_wcsicmp(psz, L"redovowels"))
            return pVF->RedoVowels (pPage);
         else if (!_wcsicmp(psz, L"import2008")) {
            pVF->Import2008 (pPage);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"import2009mandarin")) {
            pVF->Import2009Mandarin (pPage);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"SaveRecTranscript")) {
            if (pVF->SaveRecTranscript (pPage))
               pPage->MBInformation (L"The file has been saved.");
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;
         if (!_wcsicmp(psz, L"newmaster")) {
            WCHAR szTemp[256];
            wcscpy (szTemp, pVF->LexiconGet());

            if (!MLexiconOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // set it
            pVF->LexiconSet (szTemp);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newtrain")) {
            WCHAR szTemp[256];
            wcscpy (szTemp, pVF->m_szSRFile);

            if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, szTemp, sizeof(szTemp)/sizeof(WCHAR), FALSE))
               return TRUE;

            // try loading it and verifying the same lexicon
            CVoiceFile Voice;
            if (!Voice.Open (szTemp)) {
               pPage->MBWarning (L"The speech recognition training file couldn't be opened.",
                  L"It might not be a valid speech recognition training file.");
               return TRUE;
            }

            // get the lexicon and make sure it's the same name...
            PWSTR pszLex = Voice.LexiconGet ();
            if (_wcsicmp(pszLex, pVF->LexiconGet())) {
               if (IDYES != pPage->MBYesNo (
                  L"The default training file uses a different lexicon than yours. Do you still wish to use the default training?",
                  L"If the lexicons are different then the guessed phonemes will be based upon the wrong language."))
                  return TRUE;
            }

            // set it
            wcscpy (pVF->m_szSRFile, szTemp);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Master TTS Voice main page";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"SRFILE")) {
            p->pszSubString = pVF->m_szFile;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MASTERLEX")) {
            PWSTR psz = pVF->LexiconGet();
            p->pszSubString = (psz && pVF->Lexicon()) ? psz : L"NO LEXICON (you MUST choose one)";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"DEFTRAIN")) {
            p->pszSubString = pVF->m_szSRFile[0] ? pVF->m_szSRFile : L"NO TRAINING (you MUST choose one)";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"MISSPELLED")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int) pVF->m_pLexMisspelled->WordNum());
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CTTSWork::DialogMain - Brings up the main voice file editing dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CTTSWork::DialogMain (PCEscWindow pWindow)
{
   PWSTR pszRet;

   TTSMP tm;
   CMem memRecOnly;
   CListVariable lToAdd, lAlreadyExistInToDo, lAlreadyExistInWave, lHaveMisspelled, lHaveUnknown;
   CMLexicon LexUnknown;
   memset (&tm, 0, sizeof(tm));
   tm.pTTS = this;
   tm.memRecOnly = &memRecOnly;
   tm.plToAdd = &lToAdd;
   tm.plAlreadyExistInToDo = &lAlreadyExistInToDo;
   tm.plAlreadyExistInWave = &lAlreadyExistInWave;
   tm.plHaveMisspelled = &lHaveMisspelled;
   tm.plHaveUnknown = &lHaveUnknown;
   tm.pLexUnknown = &LexUnknown;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTTSMAIN, TTSMainPage, &tm);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, L"train")) {
      // BUGFIX - make sure function words
      DWORD i;
      for (i = 0; i < NUMFUNCWORDGROUP; i++)
         if (!m_apLexFuncWord[i] || !m_apLexFuncWord[i]->WordNum()) {
            EscMessageBox (pWindow->m_hWnd, ASPString(),
               L"Not enough common words.",
               L"You must scan a large text file using the \"Identify common words\" button above.",
               MB_ICONINFORMATION | MB_OK);
            goto redo;
         }

      if (Analyze (pWindow->m_hWnd, pWindow))
         goto redo;
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"joincosts")) {
      if (JoinCosts (pWindow->m_hWnd, pWindow)) {
         EscMessageBox (pWindow->m_hWnd, L"Target costs", L"The target costs for the voice have been calculated and saved into the voice.", NULL, MB_OK);

         goto redo;
      }
      else
         return FALSE;
   }
   else if (!_wcsicmp(pszRet, L"TTSMainAddMany")) {
      pszRet = pWindow->PageDialog (ghInstance, IDR_MMLTTSMAINADDMANY, TTSMainAddManyPage, &tm);
      if (!pszRet)
         return FALSE;
      goto redo;
   }
   else if (!_wcsicmp(pszRet, L"record")) {
      BOOL fSel = (tm.pszRecOnly ? TRUE : FALSE);
      DWORD dwNum;
      CWaveToDo wtd;
      if (fSel)
         wtd.AddToDo (tm.pszRecOnly);
      dwNum = RecBatch (m_fFullPCM ? WAVECALC_TTS_FULLPCM : WAVECALC_TTS_PARTIALPCM,
         pWindow, m_szSRFile, TRUE, TRUE,
         fSel ? &wtd : m_pWaveToDo, m_pWaveDir, m_szFile, L"TTSRec");

      if (fSel && dwNum)
         m_pWaveToDo->RemoveToDo (tm.pszRecOnly);

      // go back
      goto redo;
   }
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return FALSE;
}


/*************************************************************************************
TTSWorkFileOpenDialog - Dialog box for opening a CTTSWork

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL TTSWorkFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
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
   ofn.lpstrFilter = "TTS Voice Project (*.mtv)\0*.mtv\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the TTS Voice Project" :
      "Open TTS Voice Project";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mtv";
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
CTTSWork::ScanCommonWords - Reads in a text file and scans it for the most common
words. These are then copied into the m_pLexWords list. Dialog boxes are brought
up as necessary. It also fills in m_apLexWordEmph[] and m_apLexFuncWord[].

inputs
   HWND           hWnd - To bring messages up to
returns
   BOOL - TRUE if success
*/
static int _cdecl CSORTSort (const void *elem1, const void *elem2)
{
   CSORT *pdw1, *pdw2;
   pdw1 = (CSORT*) elem1;
   pdw2 = (CSORT*) elem2;

   return (int)pdw2->dwCount - (int)pdw1->dwCount;
}

BOOL CTTSWork::ScanCommonWords (HWND hWnd)
{
   m_pLexWords->Clear();
#if 0 // old prosody
   if (!m_pLexFuncWords)
      m_pLexFuncWords = new CMLexicon;
   if (!m_pLexFuncWords)
      return FALSE;
   m_pLexFuncWords->Clear();
#endif // 0

   // allocate for prosofy
   DWORD i;
#if 0 // old prosody
   for (i = 0; i < NUMLEXWORDEMPH; i++) {
      if (!m_apLexWordEmph[i])
         m_apLexWordEmph[i] = new CMLexicon;
      if (!m_apLexWordEmph[i])
         return FALSE;
      m_apLexWordEmph[i]->Clear();
   } // i
#endif // 0

   for (i = 0; i < NUMFUNCWORDGROUP; i++) {
      if (!m_apLexFuncWord[i])
         m_apLexFuncWord[i] = new CMLexicon;
      if (!m_apLexFuncWord[i])
         return FALSE;
      m_apLexFuncWord[i]->Clear();
   } // i

   if (!m_dwWordCache) {
      EscMessageBox (hWnd, ASPString(),
         L"You have set the \"Recordings of entire words\" to 0, but a text file still needs to be scanned.",
         L"The text file will be scanned to learn how the most common words affect prosody.",
         MB_ICONINFORMATION | MB_OK);
   }

   // if no lexion erro
   PCMLexicon pLex;
   pLex = Lexicon();
   if (!pLex) {
      EscMessageBox (hWnd, ASPString(),
         L"The lexicon couldn't be loaded.",
         L"You must have specify a lexicon for the TTS voice.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   // get the file
   CBTree TreeCount;
   PCMMLNode2 pNode = pLex->TextScan (NULL, hWnd, &TreeCount);
   if (pNode)
      delete pNode;
   if (!pNode)
      return FALSE;

   // make a new list so can sort
   CSORT cs;
   CListFixed lCount;
   lCount.Init (sizeof(CSORT));
   lCount.Required (TreeCount.Num());
   for (i = 0; i < TreeCount.Num(); i++) {
      cs.pszWord = TreeCount.Enum(i);
      if (!cs.pszWord)
         continue;

      cs.dwCount = *((DWORD*)TreeCount.Find(cs.pszWord));
      lCount.Add (&cs);
   } // i
   qsort (lCount.Get(0), lCount.Num(), sizeof(CSORT), CSORTSort);

   // keep a list of words without pronunciations
   CListVariable lPron, lForm;

   // loop through all the words
   PCSORT pcs = (PCSORT)lCount.Get(0);
   for (i = 0; i < lCount.Num(); i++, pcs++) {
      // figure out if word goes into word cache
      if (m_pLexWords->WordNum() < m_dwWordCache) {
         // does it have pronunciation
         if (!pLex->WordExists(pcs->pszWord))
            lPron.Add (pcs->pszWord, (wcslen(pcs->pszWord)+1)*2);
         else
            m_pLexWords->WordSet (pcs->pszWord, &lForm);
      }

      // add this to the function words list
      DWORD dwVal = i >> 4;   // so 16 in base function group
      DWORD dwBin = 0;
      for (; dwVal; dwVal /= 2, dwBin++);
      if (dwBin < NUMFUNCWORDGROUP)
         m_apLexFuncWord[dwBin]->WordSet (pcs->pszWord, &lForm);

#if 0 // old prosody
      // keep track of top function words
      if (i < NUMFUNCWORDS)
         m_pLexFuncWords->WordSet (pcs->pszWord, &lForm);

      // keep track of word for most-common words list...
      DWORD dwGroup, dw;
      for (dw = i >> 8, dwGroup = 0; dw; dw >>= 1, dwGroup++);
      if (dwGroup < NUMLEXWORDEMPH)
         m_apLexWordEmph[dwGroup]->WordSet (pcs->pszWord, &lForm);
#endif // 0
   } // i

   // if OOV words exist then warn user
   WCHAR szTemp[512];

   if (lPron.Num()) {
      swprintf (szTemp, L"%d of the top %d words do not have a pronunciation in the lexicon "
         L"so they weren't added to the list. Do you want to save this list to a text file?", (int) lPron.Num(),
         (int)m_dwWordCache);
      if (IDYES != EscMessageBox (hWnd, ASPString(),
         szTemp,
         L"You can that edit the lexicon using the mXac NLP Editor and read the text file "
         L"in using \"Recommend words that need pronunciations\".",
         MB_ICONQUESTION | MB_YESNO))
         goto skippron;

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
      ofn.hwndOwner = hWnd;
      ofn.hInstance = ghInstance;
      ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0\0\0";
      ofn.lpstrFile = szTemp;
      ofn.nMaxFile = sizeof(szTemp);
      ofn.lpstrTitle = "Save text file of words that need pronunciations";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = "txt";
      // nFileExtension 

      if (!GetSaveFileName(&ofn))
         goto skippron;

      // BUGFIX - Save diretory
      strcpy (szInitial, ofn.lpstrFile);
      szInitial[ofn.nFileOffset] = 0;
      SetLastDirectory(szInitial);


      // save the file
      char szaTemp[512];
      FILE *f;
      OUTPUTDEBUGFILE(ofn.lpstrFile);
      f = fopen(ofn.lpstrFile, "wt");
      if (!f) {
         EscMessageBox (hWnd, ASPString(),
            L"The file couldn't be saved.",
            L"You may be saving to a write-protected file.",
            MB_ICONEXCLAMATION | MB_OK);
         goto skippron;
      }

      for (i = 0; i < lPron.Num(); i++) {
         PWSTR psz = (PWSTR) lPron.Get(i);
         if (!psz)
            continue;
         WideCharToMultiByte (CP_ACP, 0, psz, -1, szaTemp, sizeof(szTemp), 0,0);
         fprintf (f, "%s\n", szaTemp);
      } // i

      fclose (f);
   } // if oov
skippron:

   // say that file scanned in
   // if there aren't enough words then warn user
   if (m_pLexWords->WordNum() < m_dwWordCache) {
      swprintf (szTemp, L"Only %d of %d words were filled.", (int) m_pLexWords->WordNum(),
         (int)m_dwWordCache);
      EscMessageBox (hWnd, ASPString(),
         szTemp,
         L"You may need to scan with a much larger text file to fill your word list.",
         MB_ICONINFORMATION | MB_OK);
   }
   else {
      swprintf (szTemp, L"All %d words were filled, from a total of %d unique words.",
         (int)m_dwWordCache, (int)TreeCount.Num());
      EscMessageBox (hWnd, ASPString(),
         szTemp,
         NULL,
         MB_ICONINFORMATION | MB_OK);
   }

   return TRUE;
}



/*************************************************************************************
PhoneToTriPhoneNumber - Converts two phonemes (unsorted) into a triphone number.

inputs
   BYTE        bLeft - Left phone
   BYTE        bRight - Right phone
   PCMLexicon  pLex - Lexicon
   DWORD       dwTriPhoneGroup - if 0 group LR phones into PIS_PHONEGROUPNUM groups, 1 use phoneme
                  numbers but ignore stress, 2 use full thing
                  Only NUMTRIPHONEGROUP possible values
returns
   WORD - Triphone number
*/
WORD PhoneToTriPhoneNumber (BYTE bLeft, BYTE bRight, PCMLexicon pLex, DWORD dwTriPhoneGroup)
{
   PLEXPHONE plp;
   if (dwTriPhoneGroup == 0) {
      PLEXENGLISHPHONE pe;

      // left
      plp = pLex->PhonemeGetUnsort(bLeft);
      pe = NULL;
      if (plp)
         pe = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (pe)
         bLeft = (BYTE)PIS_FROMPHONEGROUP(pe->dwShape);
      else
         bLeft = 255;   // unknown

      // right
      plp = pLex->PhonemeGetUnsort(bRight);
      pe = NULL;
      if (plp)
         pe = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (pe)
         bRight = (BYTE)PIS_FROMPHONEGROUP(pe->dwShape);
      else
         bRight = 255;   // unknown
   }
   else if (dwTriPhoneGroup == 1) {
      // at least ignore l/r stress indicators
      WORD wNum = (WORD)pLex->PhonemeNum();

      // left
      plp = pLex->PhonemeGetUnsort(bLeft);
      if (!plp)
         bLeft = 255;
      else if ((plp->bStress) && (plp->wPhoneOtherStress < wNum))
         bLeft = (BYTE)plp->wPhoneOtherStress;

      // right
      plp = pLex->PhonemeGetUnsort(bRight);
      if (!plp)
         bRight = 255;
      else if ((plp->bStress) && (plp->wPhoneOtherStress < wNum))
         bRight = (BYTE)plp->wPhoneOtherStress;
   }

   return (WORD)bLeft + ((WORD)bRight << 8);
}

/*************************************************************************************
CTTSWork::FindNewTriPhoneOrWord - This takes a list of phonemes and words, and
figures out if adding them would fill in a vacant tri-phone or word slot.
If so, it return TRUE, otherwise FALSE

inputs
   DWORD           *padwPhone - Unsorted phoneme numbers
   DWORD          dwNum - Number of phonemes
   PCMMLNode2      pWords - MMLNode that contains Words in it.
   PCListFixed    palTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] pointers to CListFixed for
                  the triphones. The triphones are stored as TPHONECOUNT structures.
                  CListFixed may be allocated. Elements within the list may be incremented
                  or added.
   DWORD          *padwWordCount - Pointer to an array of m_pLexWords->WordNum() elements
                  that are incremented if a word is hit
returns
   BOOL - TRUE if incrementing the value when the count is below the number of copies
      that want, FALSE if not filling in any empty requirements
*/
BOOL CTTSWork::FindNewTriPhoneOrWord (DWORD *padwPhone, DWORD dwNum, PCMMLNode2 pWords,
                                      PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount)
{
   BOOL fRet = FALSE;
   CTextParse TextParse;

   // get silence phoneme
   PCMLexicon pLex = Lexicon();
   BYTE bSilence = (BYTE) pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

   // loop through all the phonemes
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      // dont care if this is silence
      // BUGFIX - Use loword padwPhone[i] since hiword contains start/end of word info
      if (LOWORD(padwPhone[i]) == bSilence)
         continue;
      BYTE bCenter = (BYTE)LOWORD(padwPhone[i]);
      BYTE bWordPos = m_fWordStartEndCombine ? 0 : (BYTE)(padwPhone[i] >> 24);

      // left and right phonemes...
      BYTE bLeft = (i ? (BYTE)LOWORD(padwPhone[i-1]) : bSilence);
      BYTE bRight = (((i+1) < dwNum) ? (BYTE)LOWORD(padwPhone[i+1]) : bSilence);

      // convert the left and right phonemes to triphone number
      WORD wTriPhone = PhoneToTriPhoneNumber (bLeft, bRight, pLex, m_dwTriPhoneGroup);

      // if there isn't a  list then add
      if (!palTriPhone[bWordPos][bCenter]) {
         palTriPhone[bWordPos][bCenter] = new CListFixed;
         palTriPhone[bWordPos][bCenter]->Init (sizeof(TPHONECOUNT));
      }

      // see if can find one
      PTPHONECOUNT ptp = (PTPHONECOUNT) palTriPhone[bWordPos][bCenter]->Get(0);
      DWORD dwNumTPhone = palTriPhone[bWordPos][bCenter]->Num();
      for (j = 0; j < dwNumTPhone; j++) {
         if (ptp[j].wTriPhone == wTriPhone)
            break;
      } // j
      if (j < dwNumTPhone) {
         if (ptp[j].dwCount < m_dwMinInstance)
            fRet = TRUE;
         ptp[j].dwCount++;
      }
      else {
         // add
         TPHONECOUNT tc;
         memset (&tc, 0, sizeof(tc));
         tc.dwCount = 1;
         tc.wTriPhone = wTriPhone;
         tc.bPhoneLeft = bLeft;
         tc.bPhoneRight = bRight;
         palTriPhone[bWordPos][bCenter]->Add (&tc);
         fRet = TRUE;
      }
   } // i, over triphones


   // look for words
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pWords->ContentNum(); i++) {
      pSub = NULL;
      pWords->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (_wcsicmp(psz, TextParse.Word()))
         continue;   // only care about words

      // get the text
      psz = pSub->AttribGetString(TextParse.Text());
      if (!psz)
         continue;

      // can find
      DWORD dwIndex = m_pLexWords->WordFind (psz);
      if (dwIndex == -1)
         continue;   // dont care
      if (padwWordCount[dwIndex] < m_dwMinInstance)
         fRet = TRUE;
      padwWordCount[dwIndex]++;
   } // i - over words

   return fRet;
}

/*************************************************************************************
CTTSWork::RecommendFromText - Recommends a sentence from text.

inputs
   PCMMLNode2         pSent - Sentence node to use, from CTextParse::ParseText()
   PCTextParse       pTextParse - Text parser to use
   PCMem             pMemString - If function returns TRUE then this is filled with the
                     string for the sentence
   PCListFixed    palTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] pointers to CListFixed for
                  the triphones. The triphones are stored as TPHONECOUNT structures.
                  CListFixed may be allocated. Elements within the list may be incremented
                  or added.
   DWORD          *padwWordCount - Pointer to an array of m_pLexWords->WordNum() elements
                  that are incremented if a word is hit
returns
   BOOL - TRUE if should keep, FALSE if doesn't make any difference
*/
BOOL CTTSWork::RecommendFromText (PCMMLNode2 pSent, PCTextParse pTextParse, PCMem pMemString,
                                  PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount)
{
   // get the phonemes
   CListFixed lPhone;
   lPhone.Init (sizeof(DWORD));
   PCMLexicon pLex = Lexicon();
   pTextParse->ParseAddPronunciation (pSent, FALSE);
   if (!SentenceToPhonemeString (pSent, pLex, pTextParse, /*FALSE,*/ &lPhone))
      return FALSE;

   // if any of the words in the text are not in the lexicon then ignore...
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pSent->ContentNum(); i++) {
      pSub = NULL;
      pSent->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, pTextParse->Word()))
         continue;
      psz = pSub->AttribGetString(pTextParse->Text());
      if (!psz)
         continue;

      if (!pLex->WordExists(psz))
         return FALSE;
   } // i

   // see if want
   BOOL fWant = FindNewTriPhoneOrWord ((DWORD*)lPhone.Get(0), lPhone.Num(),
      pSent, palTriPhone, padwWordCount);
   if (!fWant)
      return FALSE;

   // if want it then make sentece
   if (!pTextParse->ParseToText (pSent, pMemString))
      return FALSE;

   // else ok
   return TRUE;
}



/*************************************************************************************
CTTSWork::RecommendFromLex - Given a list of words, this loops through them sequentially,
   creating 5-word phrases to speak. Only those words that create new pronunciations
   are used.

inputs
   DWORD             *pdwCurWord - Current word index in papszWord
   PWSTR             *papszWord - Pointer to an array of PWSTR for list of words
   DWORD             dwNum - Number of words in papszWord
   PCTextParse       pTextParse - Text parser to use
   PCMem             pMemString - If function returns TRUE then this is filled with the
                     string for the sentence
   PCListFixed    palTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] pointers to CListFixed for
                  the triphones. The triphones are stored as TPHONECOUNT structures.
                  CListFixed may be allocated. Elements within the list may be incremented
                  or added.
   DWORD          *padwWordCount - Pointer to an array of m_pLexWords->WordNum() elements
                  that are incremented if a word is hit
returns
   BOOL - TRUE if manage to create a sentence, FALSE if no sentence and no more words left
*/
BOOL CTTSWork::RecommendFromLex (DWORD *pdwCurWord, PWSTR *papszWord, DWORD dwNum,
                                 PCTextParse pTextParse, PCMem pMemString,
                                 PCListFixed palTriPhone[][PHONEGROUPSQUARE], DWORD *padwWordCount)
{
   // zero out
   MemZero (pMemString);
   DWORD dwWords = 0;

   // lex
   PCMLexicon pLex = Lexicon();
   CListVariable lForm, lDontRecurse;
   CListFixed lPhone;
   DWORD i;
   PBYTE pab;

   for (;dwWords < 5; *pdwCurWord = *pdwCurWord + 1) {
      // stop if cant go futher
      if (*pdwCurWord >= dwNum)
         break;

      // see if this word is in the lexicon
      lForm.Clear();
      lDontRecurse.Clear();
      if (!pLex->WordPronunciation (papszWord[*pdwCurWord], &lForm, FALSE, NULL, &lDontRecurse))
         continue;
      if (!lForm.Num())
         continue;

      // subtract 1 from the phoneme numbers to get the right value
      lPhone.Init (sizeof(DWORD));
      DWORD dw;
      pab = (PBYTE)lForm.Get(0);
      if (!pab)
         continue;
      pab++;   // so skip the POS
      DWORD dwLen = (DWORD)strlen((char*)pab);
      lPhone.Required (dwLen);
      for (i = 0; i < dwLen; i++) {
         dw = (DWORD)pab[i] - 1;
         
         // BUGFIX - mark start/end of word
         if (i == 0)
            dw |= (1 << 24);  // so know start of word
         if (i+1 == dwLen)
            dw |= (2 << 24);  // so know at end of word

         lPhone.Add (&dw);
      }

      // create sentence
      PCMMLNode2 pSent = new CMMLNode2;
      if (!pSent)
         continue;
      PCMMLNode2 pSub = pSent->ContentAddNewNode ();
      if (!pSub) {
         delete pSent;
         continue;
      }
      pSub->NameSet (pTextParse->Word());
      pSub->AttribSetString (pTextParse->Text(), papszWord[*pdwCurWord]);

      // see if want
      BOOL fWant = FindNewTriPhoneOrWord ((DWORD*)lPhone.Get(0), lPhone.Num(), pSent, palTriPhone, padwWordCount);
      delete pSent;
      if (!fWant)
         continue;

      // if want add word
      if (dwWords)
         MemCat (pMemString, L", ");
      MemCat (pMemString, papszWord[*pdwCurWord]);
      dwWords++;
   } // over all words

   return (dwWords ? TRUE : FALSE);
}



/*************************************************************************************
CTTSWork::Recommend - This recommends a set of words to speak based upon the
lexicon. It already takes into account the words in the dictionary and in the
recommended sentences list. It then adds the remaining to the recommended
sentences list.

inputs
   HWND        hWnd - To pull up text file open
returns
   DWORD - 3 if success and dont need to record any more words,
      2 if success but should still record more,
      1 if success byt need to record more,
      0 if error because of lex, etc.
*/
DWORD CTTSWork::Recommend (HWND hWnd)
{
   CMem memString;
   CListFixed lPhone;
   lPhone.Init (sizeof(DWORD));
   DWORD dwRet = 0;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return 0;  // no lexicon
   CTextParse TextParse;
   TextParse.Init (pLex->LangIDGet(), pLex);

   // open file
   CListFixed lWords;
   CListFixed lSentences;
   CBTree TreeWords;
   PWSTR *ppszWords;
   DWORD i, j;
   lWords.Init (sizeof(PWSTR));
   lSentences.Init (sizeof(PCMMLNode2));
   PCMMLNode2 pNode = pLex->TextScan (NULL, hWnd, &TreeWords);
   if (!pNode)
      return 0;   // since pressed cancel

   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      lSentences.Add (&pSub);
   }

   lWords.Required (TreeWords.Num());
   for (i = 0; i < TreeWords.Num(); i++) {
      psz = TreeWords.Enum(i);
      if (!psz)
         continue;
      lWords.Add (&psz);
   }

   // randomsize
   srand(GetTickCount());
   ppszWords = (PWSTR*)lWords.Get(0);
   DWORD dwNumWords = lWords.Num();
   for (i = 0; i < dwNumWords; i++) {
      j = rand() % dwNumWords;
      PWSTR pszTemp = ppszWords[i];
      ppszWords[i] = ppszWords[j];
      ppszWords[j] = pszTemp;
   } // i
   PCMMLNode2 *ppSent = (PCMMLNode2*) lSentences.Get(0);
   DWORD dwNumSent = lSentences.Num();
   for (i = 0; i < dwNumSent; i++) {
      j = rand() % dwNumSent;
      PCMMLNode2 pTemp = ppSent[i];
      ppSent[i] = ppSent[j];
      ppSent[j] = pTemp;
   }

   // create a counter for triphones that have
   PCListFixed aplTriPhone[4][PHONEGROUPSQUARE];    // [y][x] wehre [x] is the unsortd central phoneme number,
                                       // [y] = 0 for in word, 1 start of word, 2 end of word
   memset (aplTriPhone, 0, sizeof(aplTriPhone));

   // counters for the words...
   CMem memWordCount;
   if (!memWordCount.Required (m_pLexWords->WordNum() * sizeof(DWORD)))
      goto done;
   DWORD *padwWordCount = (DWORD*) memWordCount.p;
   memset (padwWordCount, 0, m_pLexWords->WordNum() * sizeof(DWORD));

   {
      CProgress Progress;
      Progress.Start (hWnd, "Analyzing...", TRUE);

      Progress.Push(0,.3);
      // look through wave files and use them to fill in phone count
      PCMMLNode2 pNodeSent;
      for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
         if ((i%10) == 0)
            Progress.Update ((fp)i / (fp)m_pWaveDir->m_lPCWaveFileInfo.Num());

         PCWaveFileInfo pfi = *((PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(i));
         if (!WaveToPhonemeString (pfi->m_pszFile, pLex, /*TRUE,*/ &lPhone, &pNodeSent))
            continue;

         // add recommendations
         FindNewTriPhoneOrWord ((DWORD*)lPhone.Get(0), lPhone.Num(), pNodeSent,
                              aplTriPhone, padwWordCount);
         delete pNodeSent;
      } // i

      Progress.Pop();
      Progress.Push(.3, .5);
      // look through existing recommended sentences and use them to fill in phone count
      for (i = 0; i < m_pWaveToDo->m_lToDo.Num(); i++) {
         if ((i%100) == 0)
            Progress.Update ((fp)i / (fp)m_pWaveToDo->m_lToDo.Num());

         PWSTR psz = (PWSTR)m_pWaveToDo->m_lToDo.Get(i);
         if (!psz)
            continue;

         if (!SentenceToPhonemeString(psz, pLex, &TextParse, /*FALSE,*/ &lPhone, &pNodeSent))
            continue;

         // add recommendations
         FindNewTriPhoneOrWord ((DWORD*)lPhone.Get(0), lPhone.Num(), pNodeSent,
                              aplTriPhone, padwWordCount);
         delete pNodeSent;
      } // i


      Progress.Pop();
      Progress.Push(.5, 1);
      // loop over all sentences and all words and see what come up with
      DWORD dwCurSent = 0;
      DWORD dwCurWord = 0;
      DWORD dwSentUsed = 0, dwWordUsed = 0;
      while ((dwCurSent < dwNumSent) || (dwCurWord < dwNumWords)) {  // BUGFIX - was &&
         BOOL fRet;

         if ((m_pWaveToDo->m_lToDo.Num()%10) == 0)
            Progress.Update ((fp)(dwCurSent+dwCurWord) / (fp)(dwNumSent + dwNumWords));

         // sentence first.. repeat until get at least once
         // BUGFIX - Two sentences between words
         // BUGFIX - Make sentence more common since necessary for prosody info
         for (i = 0; i < 4; i++) {
            fRet = FALSE;
            while (!fRet && (dwCurSent < dwNumSent)) {
               fRet = RecommendFromText (ppSent[dwCurSent], &TextParse, &memString,aplTriPhone, padwWordCount);
               if (fRet) {
                  m_pWaveToDo->m_lToDo.Add ((PWSTR)memString.p, (wcslen((PWSTR)memString.p)+1)*sizeof(WCHAR));
                  dwSentUsed++;
               }

               dwCurSent++;
            }
         }

         // set of words
         if (dwCurWord < dwNumWords) {
            fRet = RecommendFromLex (&dwCurWord, ppszWords, dwNumWords,
               &TextParse, &memString, aplTriPhone, padwWordCount);
            if (fRet) {
               m_pWaveToDo->m_lToDo.Add ((PWSTR)memString.p, (wcslen((PWSTR)memString.p)+1)*sizeof(WCHAR));
               dwWordUsed++;
            }
         }
      } // while have original text or words to go through

      Progress.Pop();
   }

   // see if all phonemes covered and all triphones
   DWORD dwMissedPhone = 0;
   DWORD dwLowTriPhone = 0;
   MemZero (&memString);
   for (i = 0; i < pLex->PhonemeNum(); i++) {
      PLEXPHONE plp = pLex->PhonemeGetUnsort (i);
      if (!plp)
         continue;

      if (!aplTriPhone[0][i] && !aplTriPhone[1][i] && !aplTriPhone[2][i] && !aplTriPhone[3][i]) {
         dwMissedPhone++;

         // add to string
         MemCat (&memString, plp->szPhoneLong);
         MemCat (&memString, L" - ");
         MemCat (&memString, plp->szSampleWord);
         MemCat (&memString, L"\r\n");
         continue;
      }

      // loop through all elements
      DWORD k;
      for (k = 0; k < 4; k++) {
         if (!aplTriPhone[k][i])
            continue;

         PTPHONECOUNT ptc = (PTPHONECOUNT) aplTriPhone[k][i]->Get(0);
         DWORD dwNumTri = aplTriPhone[k][i]->Num();
         for (j = 0; j < dwNumTri; j++, ptc++)
            if (ptc->dwCount < m_dwMinInstance)
               dwLowTriPhone++;
      }
   } // i

   // if missed some phones alert
   if (dwMissedPhone) {
      EscMessageBox (hWnd, ASPString(),
         L"Some phonemes were not even included in the sample text.",
         (PWSTR)memString.p,
         MB_ICONEXCLAMATION | MB_OK);
   }

   // see if all words hit
   DWORD dwMissedWord = 0;
   DWORD dwLowWord = 0;
   MemZero (&memString);
   for (i = 0; i < m_pLexWords->WordNum(); i++) {
      if (!padwWordCount[i]) {
         WCHAR szWord[256];
         szWord[0] = 0;
         m_pLexWords->WordGet(i, szWord, sizeof(szWord), NULL);
         dwMissedWord++;
         MemCat (&memString, szWord);
         MemCat (&memString, L"\r\n");
         continue;
      }

      if (padwWordCount[i] < m_dwMinInstance)
         dwLowWord++;
   } // i
   
   // if missed some phones alert
   if (dwMissedWord) {
      EscMessageBox (hWnd, ASPString(),
         L"Some words that should be pre-recorded were not even included in the sample text.",
         (PWSTR)memString.p,
         MB_ICONEXCLAMATION | MB_OK);
   }

   if (dwNumSent < 5000)
      dwRet = 1;  // if small file then always recommend more
   else if (dwMissedWord || dwMissedPhone)
      dwRet = 1;
   else if (dwLowWord || dwLowTriPhone)
      dwRet = 2;
   else
      dwRet = 3;
done:
   // finally
   if (pNode)
      delete pNode;
   DWORD k;
   for (k = 0; k < 4; k++) for (i = 0; i < PHONEGROUPSQUARE; i++)
      if (aplTriPhone[k][i])
         delete aplTriPhone[k][i];
   return dwRet;
}



/*************************************************************************************
CTTSWork::ToDoReadIn - This reads in a text file with one sentence per line.
It then figures out what sentencs are OK to be added, which ones already exist,
which ones have misspellings, which ones have unknown words.

inputs
   char              *pszText - Text File. If this is NULL a common file dilaog
                           will pop up asking for the file.
   HWND              hWnd - To bring up dialog box for file name
   PCListVariable    plToAdd - This is filled in with a list of strings to add to the to-do list.
   PCListVariable    plAlreadyExistInToDo - Filled with strings that already exist in to-do list.
   PCListVariable    plAlreadyExistInWave - Filled in with strings that already exist in the wave files.
   PCListVariable    plHaveMisspelled - Filled in with sentences containing misspelled words
   PCListVariable    plHaveUnknown - Filled in with sentences containing unknown words
   PCMLexicon        plLexUnknown - Unknown words are adde here
returns
   BOOL - TRUE if managed to open the file, FALSE if cant
*/
BOOL CTTSWork::ToDoReadIn (char *pszText, HWND hWnd, PCListVariable plToAdd,
                           PCListVariable plAlreadyExistInToDo, PCListVariable plAlreadyExistInWave,
                           PCListVariable plHaveMisspelled, PCListVariable plHaveUnknown,
                           PCMLexicon plLexUnknown)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;

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
      ofn.lpstrTitle = "Load text file of phrases";
      ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
      ofn.lpstrDefExt = "txt";
      // nFileExtension 

      if (!GetOpenFileName(&ofn))
         return FALSE;
      pszText = szFile;
   } // ask file name

   // read in the binary
   CMem memFile, memTemp;
   FILE *f;
   OUTPUTDEBUGFILE (pszText);
   f = fopen (pszText, "rb");
   if (!f)
      return FALSE;

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

   PWSTR pszUnicode;
   CMem memUnicode;
   if (!fUnicode) {
      if (!memUnicode.Required ((DWORD)(iLen+4) * sizeof(WCHAR)))
         return FALSE;
      MultiByteToWideChar (CP_ACP, 0, (char*)memFile.p, -1, (PWSTR)memUnicode.p, (DWORD)memUnicode.m_dwAllocated/sizeof(WCHAR));
      pszUnicode = (PWSTR)memUnicode.p;
   }
   else
      pszUnicode = (PWSTR)memFile.p;

   CListVariable lForm; // so have blank one around
#if 0 // def _DEBUG  // make a misspelled word
   m_pLexMisspelled->WordSet (L"mikkk", &lForm);
#endif

   // text parse
   CTextParse TP;
   TP.Init (pLex->LangIDGet(), pLex);

   // repeat, looking for cr/lf
   WCHAR *pwCur = pszUnicode;
   DWORD i;
   while (TRUE) {
      PCMMLNode2 pNode;
      WCHAR *pwNext;

      if (!pwCur[0])
         break;
      while ((pwCur[0] == L'\r') || (pwCur[0] == L'\n'))
         pwCur++;
      for (pwNext = pwCur+1; pwNext[0] && !((pwNext[0] == L'\r') || (pwNext[0] == L'\n')); pwNext++);

      // convert to NULL
      if (pwNext[0]) {
         pwNext[0] = 0;
         pwNext++;
      }
      if (!pwCur[0]) {  // empty
         pwCur = pwNext;
         continue;
      }

      // make sure this isn't already on the review list
      for (i = 0; i < plToAdd->Num(); i++)
         if (!_wcsicmp (pwCur, (PWSTR)plToAdd->Get(i))) {
            plAlreadyExistInToDo->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
            pwCur = pwNext;
            break;
         }
      if (i < plToAdd->Num())
         continue;

      for (i = 0; i < m_pWaveToDo->m_lToDo.Num(); i++)
         if (!_wcsicmp (pwCur, (PWSTR)m_pWaveToDo->m_lToDo.Get(i))) {
            plAlreadyExistInToDo->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
            pwCur = pwNext;
            break;
         }
      if (i < m_pWaveToDo->m_lToDo.Num())
         continue;

      // make sure not on existing wave files
      for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
         PCWaveFileInfo pwi = *((PCWaveFileInfo *)m_pWaveDir->m_lPCWaveFileInfo.Get(i));
         if (pwi->m_pszText && !_wcsicmp(pwCur, pwi->m_pszText)) {
            plAlreadyExistInWave->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
            pwCur = pwNext;
            break;
         }
      } //i
      if (i < m_pWaveDir->m_lPCWaveFileInfo.Num())
         continue;

      // parse
      pNode = TP.ParseFromText (pwCur, FALSE, FALSE);
#ifdef _DEBUG
      if (MyStrIStr(pwCur, L"vcr"))
         i  = 1;
#endif

      // if nothing there then continue
      if (!pNode)
         continue;

      BOOL fHaveMisspelled = FALSE, fHaveUnknown = FALSE;
      for (i = 0; i < pNode->ContentNum(); i++) {
         PWSTR psz;
         PCMMLNode2 pElem;
         pElem = NULL;
         if (!pNode->ContentEnum (i, &psz, &pElem))
            break;
         if (!pElem)
            continue;

         // only care about words
         psz = pElem->NameGet();
         if (!psz || _wcsicmp(psz, TP.Word()))
            continue;   // not important

         psz = pElem->AttribGetString(TP.Text());
         if (!psz)
            continue;

         // see if it's misspelled
         if (m_pLexMisspelled->WordExists (psz))
            fHaveMisspelled = TRUE;

         // see if unknown
         if (!pLex->WordExists(psz)) {
            fHaveUnknown = TRUE;

            plLexUnknown->WordSet (psz, &lForm);
         }
      } // while pNode->ContentNum()
      delete pNode;

      // if misspelled then add this
      if (fHaveMisspelled) {
         plHaveMisspelled->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
         pwCur = pwNext;
         continue;
      }

      // if unknown then add this
      if (fHaveUnknown) {
         plHaveUnknown->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
         pwCur = pwNext;
         continue;
      }
      
      // else add for to-do
      plToAdd->Add (pwCur, (wcslen(pwCur)+1)*sizeof(WCHAR));
      pwCur = pwNext;
   } // while read in


   // done
   return TRUE;
}

/*************************************************************************************
CTTSWork::AnalyzeAllWaves - This analyzes all waves in the wave list (m_pWaveDir).
The analysis pulls out indecies to all the triphonesn and words (that care about).
It then fills in the appropriate structures to keep track of the triphones/words.

inputs
   PCProgressSocket  pProgress - Progress to use
   PCListFixed       paplTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] entries for storing
                        the tri-phone info extracted info. New lists will be created as necessary
                        to store TPHONELIST structures.
   PCListFixed       paplWord[] - Pointer to an array of m_pLexWords.WordNum() entries which contain
                        pointers to lists of word instances. New lists will be created as necessary
                        to store WORDINST structures
   fp                *pfAveragePitch - Fills this in with the average pitch
   fp                *pfAvgSyllableDur - Filled with average syllable duration
   double            *pafEnergyPerPitch - From TTSANAL. Filled in
   char              *pacEnergyPerPitch - From TTSANAL. Filled in
   double            *pafEnergyPerVolume - From TTSANAL. Filled in
   char              *pacEnergyPerVolume - From TTSANAL. Filled in
   double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount - As per TTSANAL
returns
   PCListFixed - Returns a pointer to a list that contains PCWaveAn for each of the
               wave files in m_pWaveDir. All of the waves must be freed, and then
               the list must be freed. Returns NULL if error.
*/
PCListFixed CTTSWork::AnalyzeAllWaves (PCProgressSocket pProgress,
                            PCListFixed paplTriPhone[][PHONEGROUPSQUARE], PCListFixed paplWord[],
                            fp *pfAveragePitch, fp *pfAvgSyllableDur, PCVoiceFile pVF,
                            double *pafEnergyPerPitch, char *pacEnergyPerPitch,
                            double *pafEnergyPerVolume, char *pacEnergyPerVolume,
                            double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount)
{
   *pfAveragePitch = 0;
   *pfAvgSyllableDur = 0;
   double fSumPitch = 0;   // BUGFIX - make a double since may not be accurate enough as fp
   DWORD dwPitchCount = 0;
   double fSumSyllableDur = 0;
   DWORD dwSyllableCount = 0;
   BOOL fError = FALSE;
   PCListFixed plWave = new CListFixed;
   if (!plWave)
      return NULL;
   plWave->Init (sizeof(PCWaveAn));

   // load voice file
   //CVoiceFile VF;
   PCMLexicon pLex = Lexicon();
   //if (!VF.Open (m_szSRFile)) {
   if (!pVF) {
      delete plWave;
      return NULL;
   }

   // zero out energy calc
   m_fWordEnergyAvg = 0;
   m_dwWordCount = 0;

   DWORD i, j, k;
   DWORD dwNum = m_pWaveDir->m_lPCWaveFileInfo.Num();
   double afDurationGivenLength[SENTENCELENGTHTOBINNUM]; // to calc average duration of syllable given length of sentence
   DWORD adwDurationGivenLength[SENTENCELENGTHTOBINNUM];
   memset (afDurationGivenLength, 0, sizeof(afDurationGivenLength));
   memset (adwDurationGivenLength, 0, sizeof(adwDurationGivenLength));

   // first loop and create empty wave objects
   for (i = 0; i < dwNum; i++) {
      PCWaveAn pwa = new CWaveAn;
      if (!pwa) {
         fError = TRUE;
         goto done;
      }


      // try to load it in
      PCWaveFileInfo pfi = *((PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(i));
      if (!pfi) {
         delete pwa;
         pwa = NULL;
         fError = TRUE;
         goto done;
      }

      // temporarily store the filename
      wcscpy (pwa->m_szFile, pfi->m_pszFile);

      // while at it, look for a match in the exclude list
      // NOTE: Slow search, but don't really care
      PCWaveFileInfo *ppfi = (PCWaveFileInfo*)m_pWaveDirEx->m_lPCWaveFileInfo.Get(0);
      for (j = 0; j < m_pWaveDirEx->m_lPCWaveFileInfo.Num(); j++) {
         if (!ppfi[j]->m_pszFile)
            continue;   // don't expect, but just in case

         if (!_wcsicmp(pfi->m_pszFile, ppfi[j]->m_pszFile)) {
            // found match
            pwa->m_fExcludeFromProsodyModel = TRUE;
            break;
         }
      } // k

done:
      if (pwa)
         plWave->Add (&pwa);
      if (fError)
         break;
   } // i

   // scan all in multihreads
   CRITICAL_SECTION cs;
   InitializeCriticalSection (&cs);
   EMTCANALYZEWAVE eaw;
   memset (&eaw, 0, sizeof(eaw));
   eaw.dwType = 10;
   eaw.lpcs = &cs;
   eaw.pafEnergyPerPitch = pafEnergyPerPitch;
   eaw.pafEnergyPerVolume = pafEnergyPerVolume;
   eaw.paplTriPhone = &paplTriPhone[0][0];
   eaw.paplWord = paplWord;
   eaw.plWave = plWave;
   eaw.pTTS = this;
   eaw.pVF = pVF;
   eaw.pafPhonemePitchSum = pafPhonemePitchSum;
   eaw.pafPhonemeDurationSum = pafPhonemeDurationSum;
   eaw.pafPhonemeEnergySum = pafPhonemeEnergySum;
   eaw.padwPhonemeCount = padwPhonemeCount;

   ThreadLoop (0, plWave->Num(), 16, &eaw, sizeof(eaw), pProgress);
   DeleteCriticalSection (&cs);
   //   if (!pwa->AnalyzeWave (pfi->m_pszFile, pProgress, i, paplTriPhone, paplWord, this, pVF,
   //      pafEnergyPerPitch)) {
   //      delete pwa;
   //      pwa = NULL;
   //      fError = TRUE;
   //      goto done;
   //   }


   // loop through waves
   if (!fError) for (i = 0; i < plWave->Num(); i++) {
      PCWaveAn pwa = *((PCWaveAn*)plWave->Get(i));
      if (!pwa) {
         fError = TRUE;
         break;
      }


      // figure out average pitch
      fSumPitch += log(max(1.0, pwa->m_fAvgPitch) / SRBASEPITCH) * (double)pwa->m_dwAvgPitchCount;
      dwPitchCount += pwa->m_dwAvgPitchCount;

      fSumSyllableDur += pwa->m_fAvgSyllableDur * (double)pwa->m_dwSyllableCount;
      dwSyllableCount += pwa->m_dwSyllableCount;

#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nSent %d pitch = %g", (int)i, pwa->m_fAvgPitch);
      EscOutputDebugString (szTemp);
#endif

      // loop over all the syllables in the sentence and calcualte the average duration
      // of a syllable given the length of the sentence
      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      for (j = 0; j < pwa->m_lSYLAN.Num(); j++, psa++) {
         DWORD dwBin = SentenceLengthToBin (psa->dwSyllablesInSubSentence);
         afDurationGivenLength[dwBin] += psa->fDurationScale;
         adwDurationGivenLength[dwBin]++;
      } // j
   } // i

   // determine the average durations given subsentence length
   for (i = 0; i < SENTENCELENGTHTOBINNUM; i++) {
      if (adwDurationGivenLength[i])
         afDurationGivenLength[i] /= (double)adwDurationGivenLength[i];

#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nBin %d duration  = %g", (int)i, afDurationGivenLength[i]);
      EscOutputDebugString (szTemp);
#endif
   }

   // go back and "normalize" the length of the syllables
   for (i = 0; i < plWave->Num(); i++) {
      PCWaveAn pwa = *((PCWaveAn*)plWave->Get(i));
      //if (!pwa)    - let it crash so find out
      //   continue;

      // BUGFIX - Since crashing here
      if (!pwa)
         continue;

      // find the number of subsentences
      DWORD dwSentMax = 0;
      PSYLAN psa = pwa ? (PSYLAN) pwa->m_lSYLAN.Get(0) : NULL; // BUGFIX - Was crashing here

      for (j = 0; j < (pwa ? pwa->m_lSYLAN.Num() : 0); j++)
         dwSentMax = max(dwSentMax, psa[j].dwSubSentenceNum);

      // loop over all the subsentences
      for (j = 0; j <= dwSentMax; j++) {
         // sum together the durations
         fp fSum = 0;
         DWORD dwCount = 0;
         for (k = 0; k < pwa->m_lSYLAN.Num(); k++)
            if (psa[k].dwSubSentenceNum == j) {
               fSum += psa[k].fDurationScale;
               dwCount++;
            }
         if (!dwCount || !fSum)
            continue;

         // scale so average
         fSum /= (fp)dwCount;
         DWORD dwBin = SentenceLengthToBin (dwCount);
         if (afDurationGivenLength[dwBin])
            fSum = afDurationGivenLength[dwBin] / fSum;
         else
            fSum = 1;   // no change. shouldnt happen

#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nSent %d duration scale = %g", (int)i, (double) fSum);
      EscOutputDebugString (szTemp);
#endif

         // write in new length changes
         for (k = 0; k < pwa->m_lSYLAN.Num(); k++)
            if (psa[k].dwSubSentenceNum == j)
               psa[k].fDurationScale = fSum;
      } // j
   } // i

   // energy
   m_fWordEnergyAvg /= (double)m_dwWordCount;
   m_fWordEnergyAvg = max(m_fWordEnergyAvg, 1);

   if (fError) {
      PCWaveAn *ppwa = (PCWaveAn*)plWave->Get(0);
      for (i = 0; i < plWave->Num(); i++)
         delete ppwa[i];
      delete plWave;
      return NULL;
   }

   // fill in pitch
   if (dwPitchCount)
      *pfAveragePitch = exp(fSumPitch / (fp)dwPitchCount) * SRBASEPITCH;
   if (dwSyllableCount)
      *pfAvgSyllableDur = (fp)(fSumSyllableDur / (double)dwSyllableCount);


   // loop through energies and determine weighting

   // first, make blur a bit so everything has some data
   double afBlur[max(ENERGYPERVOLUMENUM,ENERGYPERPITCHNUM)];
   double afBackup[max(max(ENERGYPERVOLUMENUM,ENERGYPERPITCHNUM),SRDATAPOINTS)];
   double f;
   for (i = 0; i < ENERGYPERPITCHNUM; i++)
      afBlur[i] = pow ((fp)0.001, (fp)i / (fp)ENERGYPERPITCHPOINTSPEROCTAVE);      // 1/10000th strength per octave
         // BUGFIX - Make much less of a drop-off. Was 0.00001, changed to 0.001 sp
         // that no problems with really high registers
   for (i = 0; i < SRDATAPOINTS; i++) {
      for (j = 0; j < ENERGYPERPITCHNUM; j++)
         afBackup[j] = pafEnergyPerPitch[j * (SRDATAPOINTS+1) + i];

      // include all other energies, but reduce the effect by the distance from the
      // one we're producing
      for (j = 0; j < ENERGYPERPITCHNUM; j++) {
         f = 0;

         for (k = 0; k < ENERGYPERPITCHNUM; k++)
            f += afBackup[k] * afBlur[abs((int)j - (int)k)];
   
         pafEnergyPerPitch[j * (SRDATAPOINTS+1) + i] = f;
      } // j
   } // i

   // need to blur along SRDatapoints
   int iOffset, iCur;
   DWORD dwWeight, dwCount;
   double fSum;
#define FILTERAMT       (SRPOINTSPEROCTAVE/2)
   for (j = 0; j <  ENERGYPERPITCHNUM; j++) {
      memcpy (afBackup, pafEnergyPerPitch + j * (SRDATAPOINTS+1), sizeof(double) * SRDATAPOINTS);

      fSum = 0;
      for (i = 0; i < SRDATAPOINTS; i++) {
         f = 0;
         dwCount = 0;
         for (iOffset = -FILTERAMT; iOffset <= FILTERAMT; iOffset++) {
            iCur = iOffset + (int) i;
            if ((iCur < 0) || (iCur >= SRDATAPOINTS))
               continue;
            dwWeight = FILTERAMT - (DWORD) abs(iOffset) + 1;
            dwCount += dwWeight;
            f += (double)dwWeight * afBackup[iCur];
         } // iOffset

         f /= (double)dwCount;

         pafEnergyPerPitch[j*(SRDATAPOINTS+1)+i] = f;
         fSum += f;
      } // i

      // normalize
      if (fSum)
         fSum = 1.0 / fSum;
      for (i = 0; i < SRDATAPOINTS; i++)
         pafEnergyPerPitch[j*(SRDATAPOINTS+1)+i] *= fSum;

   } // j

   // find the voice's average pitch in terms of an index
   f = log(*pfAveragePitch / (fp)ENERGYPERPITCHBASE) / log((fp)2) * ENERGYPERPITCHPOINTSPEROCTAVE + 0.5;
   f = max(f, 0);
   f = min(f, (double)ENERGYPERPITCHNUM-1);
   DWORD dwAvgPitchIndex = (DWORD)f;

#define SAVEPITCHFILE   // BUGBUG - turn on

#ifdef SAVEPITCHFILE
   FILE *file = fopen("c:\\temp\\pitch.txt", "wt");
#endif // 0

   // calculate Db
   for (i = 0; i < SRDATAPOINTS; i++) {
      // value of the average
      f = pafEnergyPerPitch[dwAvgPitchIndex*(SRDATAPOINTS+1)+i];
      f = max(f, EPSILON);   // just in case

#if 0 // def _DEBUG
      char szTemp[16];
      OutputDebugString ("\r\n");
#endif

#ifdef SAVEPITCHFILE
      if (file)
         fprintf (file, "\n");
#endif // 0

      // Db
      for (j = 0; j < ENERGYPERPITCHNUM; j++) {
         pacEnergyPerPitch[j*SRDATAPOINTS + i] = AmplitudeToDb (
            max(pafEnergyPerPitch[j*(SRDATAPOINTS+1)+i], EPSILON) / f * (fp)0x8000);

#if 0 // def _DEBUG
         if (j)
            OutputDebugString (", ");
         sprintf (szTemp, "%d", (int)pacEnergyPerPitch[j*SRDATAPOINTS + i]);
         OutputDebugString (szTemp);
#endif
#ifdef SAVEPITCHFILE
         if (file) {
            if (j)
               fprintf (file, ", ");
            fprintf (file, "%d", (int)pacEnergyPerPitch[j*SRDATAPOINTS + i]);
         }
#endif // 0
      }
   } // i

#if 0 // def _DEBUG
      OutputDebugString ("\r\n");
#endif
#ifdef SAVEPITCHFILE
   if (file)
      fclose (file);
#endif // 0





   // loop through energies and determine weighting for volume

   // first, make blur a bit so everything has some data
   for (i = 0; i < ENERGYPERVOLUMENUM; i++)
      afBlur[i] = pow ((fp)0.01, (fp)i / (fp)ENERGYPERVOLUMEPOINTSPEROCTAVE);      // 1/100th strength per octave of energy
   for (i = 0; i < SRDATAPOINTS; i++) {
      for (j = 0; j < ENERGYPERVOLUMENUM; j++)
         afBackup[j] = pafEnergyPerVolume[j * (SRDATAPOINTS+1) + i];

      // include all other energies, but reduce the effect by the distance from the
      // one we're producing
      for (j = 0; j < ENERGYPERVOLUMENUM; j++) {
         f = 0;

         for (k = 0; k < ENERGYPERVOLUMENUM; k++)
            f += afBackup[k] * afBlur[abs((int)j - (int)k)];
   
         pafEnergyPerVolume[j * (SRDATAPOINTS+1) + i] = f;
      } // j
   } // i

   // need to blur along SRDatapoints
   for (j = 0; j <  ENERGYPERVOLUMENUM; j++) {
      memcpy (afBackup, pafEnergyPerVolume + j * (SRDATAPOINTS+1), sizeof(double) * SRDATAPOINTS);

      fSum = 0;
      for (i = 0; i < SRDATAPOINTS; i++) {
         f = 0;
         dwCount = 0;
         for (iOffset = -FILTERAMT; iOffset <= FILTERAMT; iOffset++) {
            iCur = iOffset + (int) i;
            if ((iCur < 0) || (iCur >= SRDATAPOINTS))
               continue;
            dwWeight = FILTERAMT - (DWORD) abs(iOffset) + 1;
            dwCount += dwWeight;
            f += (double)dwWeight * afBackup[iCur];
         } // iOffset

         f /= (double)dwCount;

         pafEnergyPerVolume[j*(SRDATAPOINTS+1)+i] = f;
         fSum += f;
      } // i

      // normalize
      if (fSum)
         fSum = 1.0 / fSum;
      for (i = 0; i < SRDATAPOINTS; i++)
         pafEnergyPerVolume[j*(SRDATAPOINTS+1)+i] *= fSum;

   } // j

   // know the central volume index
   DWORD dwAvgVolumeIndex = ENERGYPERVOLUMECENTER;

#define SAVEVOLUMEFILE  // BUGBUG

#ifdef SAVEVOLUMEFILE
   //FILE *file;
   file = fopen("c:\\temp\\volume.txt", "wt");
#endif // 0

   // calculate Db
   for (i = 0; i < SRDATAPOINTS; i++) {
      // value of the average
      f = pafEnergyPerVolume[dwAvgVolumeIndex*(SRDATAPOINTS+1)+i];
      f = max(f, EPSILON);   // just in case

#if 0 // def _DEBUG
      char szTemp[16];
      OutputDebugString ("\r\n");
#endif

#ifdef SAVEVOLUMEFILE
      if (file)
         fprintf (file, "\n");
#endif // 0

      // Db
      for (j = 0; j < ENERGYPERVOLUMENUM; j++) {
         pacEnergyPerVolume[j*SRDATAPOINTS + i] = AmplitudeToDb (
            max(pafEnergyPerVolume[j*(SRDATAPOINTS+1)+i], EPSILON) / f * (fp)0x8000);

#if 0 // def _DEBUG
         if (j)
            OutputDebugString (", ");
         sprintf (szTemp, "%d", (int)pacEnergyPerVolume[j*SRDATAPOINTS + i]);
         OutputDebugString (szTemp);
#endif
#ifdef SAVEVOLUMEFILE
         if (file) {
            if (j)
               fprintf (file, ", ");
            fprintf (file, "%d", (int)pacEnergyPerVolume[j*SRDATAPOINTS + i]);
         }
#endif // 0
      }
   } // i

#if 0 // def _DEBUG
      OutputDebugString ("\r\n");
#endif
#ifdef SAVEVOLUMEFILE
   if (file)
      fclose (file);
#endif // 0


   return plWave;
}


/*************************************************************************************
CTTSWork::AnalysisInit - Initialize the analysis information.

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PCMem             pmemAnal - Memory to use for pAnal lists.
   PTTSANAL          pAnal - Fill this with analysis information
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisInit (PCProgressSocket pProgress, PCMem pmemAnal, PTTSANAL pAnal)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   // memset (pAnal, 0, sizeof(*pAnal)); BUGFIX - dont clear because already zeroed
   pAnal->dwPhonemes = pLex->PhonemeNum();

   // enough memory to store the words
   DWORD dwSize = m_pLexWords->WordNum() * sizeof(PCListFixed) +
      m_pWaveDir->m_lPCWaveFileInfo.Num() * sizeof(PCListFixed) +
      // micropauses no longer in pAnal->dwPhonemes * pLex->PhonemeNum() * 2 * sizeof(DWORD) +
      (DWORD)pow((fp)(POS_MAJOR_NUM+1), (fp)(TTSPROSNGRAM*2+1)) * (DWORD)pow((fp)3, (fp)(TTSPROSNGRAMBIT*2)) * sizeof(TTSWORKNGRAM) +
      4 * pAnal->dwPhonemes * PHONEGROUPSQUARE * sizeof(TRIPHONETRAIN) +
      4 * pAnal->dwPhonemes * (pAnal->dwPhonemes+1) * (pAnal->dwPhonemes+1) * sizeof(TRIPHONETRAIN) +
      /* BUGFIX - ignore start/end word for phonemes ...4 * */ pAnal->dwPhonemes * sizeof(TRIPHONETRAIN) +
      pAnal->dwPhonemes * PHONEMEGAGROUPSQUARE * sizeof(TRIPHONETRAIN) +
      (SRDATAPOINTS + 1) * sizeof(double) * ENERGYPERPITCHNUM +
      SRDATAPOINTS * sizeof(char) * ENERGYPERPITCHNUM +
      (SRDATAPOINTS + 1) * sizeof(double) * ENERGYPERVOLUMENUM +
      SRDATAPOINTS * sizeof(char) * ENERGYPERVOLUMENUM
      ;
   if (!pmemAnal->Required (dwSize))
      return FALSE;
   memset (pmemAnal->p, 0, dwSize);

   pAnal->paplWords = (PCListFixed*) pmemAnal->p;
   pAnal->paplWEMPH = pAnal->paplWords + m_pLexWords->WordNum();
   //pAnal->padwMicroPause = (DWORD*)(pAnal->paplWEMPH + m_pWaveDir->m_lPCWaveFileInfo.Num());
#if 0 // old prosody
   pAnal->paTTSNGram = (PTTSWORKNGRAM) (pAnal->padwMicroPause + pAnal->dwPhonemes * pAnal->dwPhonemes * 2);
#endif // 0
#if 0 // old prosody
   pAnal->paTRIPHONETRAIN = (PTRIPHONETRAIN) (pAnal->paTTSNGram +
      (DWORD)pow(POS_MAJOR_NUM+1, TTSPROSNGRAM*2+1) * (DWORD)pow(3, TTSPROSNGRAMBIT*2));
      // note: already initialized to 0
#else
   //pAnal->paTRIPHONETRAIN = (PTRIPHONETRAIN) (pAnal->padwMicroPause + pAnal->dwPhonemes * pAnal->dwPhonemes * 2);
   pAnal->paGroupTRIPHONETRAIN = (PTRIPHONETRAIN) (pAnal->paplWEMPH + m_pWaveDir->m_lPCWaveFileInfo.Num());
   pAnal->paSpecificTRIPHONETRAIN = (PTRIPHONETRAIN)pAnal->paGroupTRIPHONETRAIN + (4 * pAnal->dwPhonemes * PHONEGROUPSQUARE);
#endif
   pAnal->paPHONETRAIN = (PTRIPHONETRAIN)pAnal->paSpecificTRIPHONETRAIN + (4 * pAnal->dwPhonemes * (pAnal->dwPhonemes+1) * (pAnal->dwPhonemes+1));
      // note: already initialized to 0
   pAnal->paMegaPHONETRAIN = (PTRIPHONETRAIN)pAnal->paPHONETRAIN + pAnal->dwPhonemes;
   pAnal->pafEnergyPerPitch = (double*) ((PTRIPHONETRAIN)pAnal->paMegaPHONETRAIN + pAnal->dwPhonemes * PHONEMEGAGROUPSQUARE);
   pAnal->pacEnergyPerPitch = (char*) (pAnal->pafEnergyPerPitch + ((SRDATAPOINTS+1)*ENERGYPERPITCHNUM));
   pAnal->pafEnergyPerVolume = (double*) (pAnal->pacEnergyPerPitch + SRDATAPOINTS * ENERGYPERPITCHNUM);
   pAnal->pacEnergyPerVolume = (char*) (pAnal->pafEnergyPerVolume + ((SRDATAPOINTS+1)*ENERGYPERVOLUMENUM));


   // load in the sr voice
   pAnal->pOrigSR = new CVoiceFile;
   if (pAnal->pOrigSR) {
      if (!pAnal->pOrigSR->Open (m_szSRFile)) {
         delete pAnal->pOrigSR;
         pAnal->pOrigSR = NULL;
      }
   }
   pAnal->plPCWaveAn = AnalyzeAllWaves (pProgress, pAnal->paplTriPhone, pAnal->paplWords, &pAnal->fAvgPitch,
      &pAnal->fAvgSyllableDur, pAnal->pOrigSR, pAnal->pafEnergyPerPitch, pAnal->pacEnergyPerPitch,
      pAnal->pafEnergyPerVolume, pAnal->pacEnergyPerVolume,
      pAnal->afPhonemePitchSum, pAnal->afPhonemeDurationSum, pAnal->afPhonemeEnergySum, pAnal->adwPhonemeCount);
   if (!pAnal->plPCWaveAn)
      return FALSE;
   pAnal->plTRIPHONETRAIN = new CListFixed;
   pAnal->plTRIPHONETRAIN->Init (sizeof(TRIPHONETRAIN));

   return TRUE;

}




static int _cdecl TPHONEINSTSort (const void *elem1, const void *elem2)
{
   TPHONEINST *pdw1, *pdw2;
   pdw1 = (TPHONEINST*) elem1;
   pdw2 = (TPHONEINST*) elem2;

   PPHONEAN pa1 = pdw1->pPHONEAN, pa2 = pdw2->pPHONEAN;
   
   DWORD i;
   int iRet;
   for (i = 0; i < NUMTRIPHONEGROUP; i++) {
      iRet = (int) pa1->awTriPhone[i] - (int) pa2->awTriPhone[i];
      if (iRet)
         return iRet;
   } // i

   return 0;   // same
}

/*************************************************************************************
GetGroupTRIPHONETRAIN - Get the appropriate triphone train.

inputs
   PCTTSWork      pWork - Work tts
   PTTSANAL       pAnal - Misc info
   PPHONEAN       ppa - Phone analysis info
   PCMLexicon     pLex - Used for overriding phonemes
   DWORD          dwPhoneLeftOverride - If -1 then get from ppa. Otherwise, override to this phoneme
   DWORD          dwPhoneRightOverride - if -1 then get from ppa. Otherwise, override to this phoneme
returns
   PTRIPHONETRAIN - Location
*/
PTRIPHONETRAIN GetGroupTRIPHONETRAIN (PCTTSWork pWork, PTTSANAL pAnal, PPHONEAN ppa,
                                 PCMLexicon pLex, DWORD dwPhoneLeftOverride = (DWORD)-1, DWORD dwPhoneRightOverride = (DWORD)-1)
{
   DWORD dwTPhoneCompact = ppa->awTriPhone[0];
   // BUGFIX - Cant use shifts because no longer just 16
   DWORD dwPhoneGroupLeft = (dwTPhoneCompact & 0xff) % PIS_PHONEGROUPNUM;
   DWORD dwPhoneGroupRight = ((dwTPhoneCompact & 0xff00) >> 8) % PIS_PHONEGROUPNUM;

   // allow overrides
   if (dwPhoneLeftOverride != (DWORD)-1)
      dwPhoneGroupLeft = pLex->PhonemeToGroup (dwPhoneLeftOverride);
   if (dwPhoneRightOverride != (DWORD)-1)
      dwPhoneGroupRight = pLex->PhonemeToGroup (dwPhoneRightOverride);

   dwTPhoneCompact = dwPhoneGroupLeft + dwPhoneGroupRight * PIS_PHONEGROUPNUM;
   //dwTPhoneCompact = (dwTPhoneCompact & 0x0f) +
   //   ((dwTPhoneCompact & 0x0f00) >> 4);

   DWORD dwWordPos = ppa->bWordPos;
#ifdef NOMODS_COMBINEWORDPOSUNTILTHEEND
   dwWordPos = 0;
#endif
   return ((PTRIPHONETRAIN) pAnal->paGroupTRIPHONETRAIN) +
      (
         (DWORD)ppa->bPhone +
         dwTPhoneCompact * pAnal->dwPhonemes +
         (pWork->m_fWordStartEndCombine ? 0 : dwWordPos) * pAnal->dwPhonemes * PHONEGROUPSQUARE
      );
}


/*************************************************************************************
GetSpecifcTRIPHONETRAIN - Get the appropriate triphone train.

inputs
   PCTTSWork      pWork - Work tts
   PTTSANAL       pAnal - Misc info
   PPHONEAN       ppa - Phone analysis info
   DWORD          dwDetail - 1 for unstressed, 2 for exact phoneme
   PCMLexicon     pLex - Used for overriding phonemes
   DWORD          dwPhoneLeftOverride - If -1 then get from ppa. Otherwise, override to this phoneme
   DWORD          dwPhoneRightOverride - if -1 then get from ppa. Otherwise, override to this phoneme
returns
   PTRIPHONETRAIN - Location
*/
PTRIPHONETRAIN GetSpecificTRIPHONETRAIN (PCTTSWork pWork, PTTSANAL pAnal, PPHONEAN ppa, DWORD dwDetail,
                                 PCMLexicon pLex, DWORD dwPhoneLeftOverride = (DWORD)-1, DWORD dwPhoneRightOverride = (DWORD)-1)
{
   DWORD dwTPhoneCompact = ppa->awTriPhone[dwDetail]; // BUGFIX - Dont think can do max(dwDetail,1)] wihtout causing problems;
         // always at least detail of 1
   DWORD dwPhoneGroupLeft = (dwTPhoneCompact & 0xff);
   DWORD dwPhoneGroupRight = ((dwTPhoneCompact & 0xff00) >> 8);

   // allow overrides
   if (dwPhoneLeftOverride != (DWORD)-1)
      switch (dwDetail) {
         case 0:  // groups
            dwPhoneGroupLeft = pLex->PhonemeToGroup(dwPhoneLeftOverride);
            break;
         case 1:  // unstressed
            dwPhoneGroupLeft = pLex->PhonemeToUnstressed (dwPhoneLeftOverride);
            break;
         case 2:  // stressed:
         default:
            // do nothing
            dwPhoneGroupLeft = dwPhoneLeftOverride;
            break;
      }
   if (dwPhoneRightOverride != (DWORD)-1)
      switch (dwDetail) {
         case 0:  // groups
            dwPhoneGroupRight = pLex->PhonemeToGroup(dwPhoneRightOverride);
            break;
         case 1:  // unstressed
            dwPhoneGroupRight = pLex->PhonemeToUnstressed (dwPhoneRightOverride);
            break;
         case 2:  // stressed:
         default:
            // do nothing
            dwPhoneGroupRight = dwPhoneRightOverride;
            break;
      }

   DWORD dwNumPhone = pAnal->dwPhonemes;
   if (dwPhoneGroupLeft >= dwNumPhone)
      dwPhoneGroupLeft = dwNumPhone;
   if (dwPhoneGroupRight >= dwNumPhone)
      dwPhoneGroupRight = dwNumPhone;

   DWORD dwNumPhonePlusOne = dwNumPhone+1;
   dwTPhoneCompact = dwPhoneGroupLeft + dwPhoneGroupRight * dwNumPhonePlusOne;
   //dwTPhoneCompact = (dwTPhoneCompact & 0x0f) +
   //   ((dwTPhoneCompact & 0x0f00) >> 4);

   return ((PTRIPHONETRAIN) pAnal->paSpecificTRIPHONETRAIN) +
      (
         (DWORD)ppa->bPhone +
         dwTPhoneCompact * pAnal->dwPhonemes +
         (DWORD)(pWork->m_fWordStartEndCombine ? 0 : ppa->bWordPos) * pAnal->dwPhonemes * dwNumPhonePlusOne * dwNumPhonePlusOne
      );
}

/*************************************************************************************
GetPHONETRAIN - Get the appropriate triphone train.

inputs
   PCTTSWork      pWork - Work tts
   PTTSANAL       pAnal - Misc info
   PPHONEAN       ppa - Phone analysis info
returns
   PTRIPHONETRAIN - Location
*/
PTRIPHONETRAIN GetPHONETRAIN (PCTTSWork pWork, PTTSANAL pAnal, PPHONEAN ppa)
{
   return ((PTRIPHONETRAIN) pAnal->paPHONETRAIN) +
      (
         (DWORD)ppa->bPhone // +
         // BUGFIX - Ignore start/end word for just phones (DWORD)(pWork->m_fWordStartEndCombine ? 0 : ppa->bWordPos) * pAnal->dwPhonemes
      );
}

/*************************************************************************************
GetMegaPHONETRAIN - Get the appropriate mega-group triphone train.

inputs
   PCTTSWork      pWork - Work tts
   PTTSANAL       pAnal - Misc info
   PPHONEAN       ppa - Phone analysis info
   BYTE           bPhoneOverride - If this isn't 255 then the phoneme used is bPhoneOverride
   BYTE           bLeftOverride - If this is 255 then left megagroup from the triphone, else use this megagroup, 0..PIS_PHONEMEGAGROUPNUM-1
   BYTE           bRightOverride - If this is 255 then right megagroup from the triphone, else use this megagroup, 0..PIS_PHONEMEGAGROUPNUM-1
returns
   PTRIPHONETRAIN - Location
*/
PTRIPHONETRAIN GetMegaPHONETRAIN (PCTTSWork pWork, PTTSANAL pAnal, PPHONEAN ppa, BYTE bPhoneOverride = 255,
                                  BYTE bLeftOverride = 255, BYTE bRightOverride = 255)
{
   DWORD dwTPhoneCompact = ppa->awTriPhone[0];
   DWORD dwLeft = ((dwTPhoneCompact & 0xff) % PIS_PHONEGROUPNUM);
   DWORD dwRight = (((dwTPhoneCompact & 0xff00) >> 8) % PIS_PHONEGROUPNUM);
   dwLeft = LexPhoneGroupToMega (dwLeft);
   dwRight = LexPhoneGroupToMega (dwRight);
   if (bLeftOverride != 255)
      dwLeft = bLeftOverride;
   if (bRightOverride != 255)
      dwRight = bRightOverride;

   dwTPhoneCompact = dwLeft + dwRight * PIS_PHONEMEGAGROUPNUM;

   return ((PTRIPHONETRAIN) pAnal->paMegaPHONETRAIN) +
      (
         (DWORD)((bPhoneOverride == 255) ? ppa->bPhone : bPhoneOverride) +
         dwTPhoneCompact * pAnal->dwPhonemes
      );
}


/*************************************************************************************
CacheSRFeatures - Caches the SRFeatures directly from the wave.
NOT thread safe.

inputs
   PCM3DWave         pWave - Wave
   DWORD             dwTimeStart - Start feature
   DWORD             dwTimeEnd - End feature (exclusive)
returns
   PSRFEATURE - Required features, or NULL if error
*/
PSRFEATURE CacheSRFeatures (PCM3DWave pWave, DWORD dwTimeStart, DWORD dwTimeEnd)
{
   PSRFEATURE psrOrig = pWave->CacheSRFeatures (dwTimeStart, dwTimeEnd);
   if (!psrOrig)
      return NULL;

#if 0 // def _DEBUG
   // test to fill in waves with junk to see what's modified
   PWVPHONEME pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
   DWORD i;
   for (i = 0; i < pWave->m_lWVPHONEME.Num(); i++, pwp++) {
      DWORD dwStart = pwp->dwSample / pWave->m_dwSRSkip;
      DWORD dwEnd = ((i+1 < pWave->m_lWVPHONEME.Num()) ? pwp[1].dwSample : pWave->m_dwSamples) / pWave->m_dwSRSkip;
      dwStart = max(dwStart, dwTimeStart);
      dwEnd = min(dwEnd, dwTimeEnd);

      if (dwStart < dwEnd)
         FakeSRFEATURE (psrOrig + (dwStart - dwTimeStart), dwEnd - dwStart, pwp->awcName[0]);
   } // i
#endif // _DEBUG

   return psrOrig;
}


/*************************************************************************************
CTTSWork::CacheSRFeaturesWithAdjust - This caches SRFeatures by calling the wav'e CacheSRFeatures(),
but it also counterweights the voiced segments by the pitch.

inputs
   PCM3DWave         pWave - Wave
   DWORD             dwTimeStart - Start feature
   DWORD             dwTimeEnd - End feature (exclusive)
   BOOL              fIsVoiced - Set to TRUE if it's a voiced phoneme
   fp                fAvgEnergyForVoiced - From CWaveAn
   PCMTTS            pTTS - TTS vpoice
   DWORD             dwThread - 0 .. MAXRAYTHREAD-1
returns
   PSRFEATURE - Required features, or NULL if error
*/
PSRFEATURE CTTSWork::CacheSRFeaturesWithAdjust (PCM3DWave pWave, DWORD dwTimeStart, DWORD dwTimeEnd, BOOL fIsVoiced,
                                      fp fAvgEnergyForVoiced, PCMTTS pTTS,DWORD dwThread)
{
   PSRFEATURE psrOrig = CacheSRFeatures (pWave, dwTimeStart, dwTimeEnd);
   if (!psrOrig)
      return NULL;
   if (!fIsVoiced)
      return psrOrig;

   // else, need to convert
   DWORD dwNeed = (dwTimeEnd - dwTimeStart) * sizeof(SRFEATURE);
   if (!m_amemCacheConvert[dwThread].Required (dwNeed))
      return NULL;   // error. shouldnt appen
   memcpy (m_amemCacheConvert[dwThread].p, psrOrig, dwNeed);

   psrOrig = (PSRFEATURE)m_amemCacheConvert[dwThread].p;
   DWORD i, j;
   int iVal;
   for (i = 0; i < (dwTimeEnd - dwTimeStart); i++, psrOrig++) {
      // get the pitch
      fp fPitch = pWave->PitchAtSample (PITCH_F0, (i+dwTimeStart) * pWave->m_dwSRSkip, 0);
      char *pacConvert = pTTS->EnergyPerPitchGet (fPitch);

      // BUGBUG - may want sub-pitch too

      // get this volume
      fp fEnergyOrig = SRFEATUREEnergy (FALSE, psrOrig);
      fp fEnergyRatio = fEnergyOrig / max(fAvgEnergyForVoiced, CLOSE);;
      char *pacConvertVolume = pTTS->EnergyPerVolumeGet (fEnergyRatio);

      for (j = 0; j < SRDATAPOINTS; j++) {
         iVal = (int)psrOrig->acVoiceEnergy[j];
         if (iVal < SRABSOLUTESILENCE+10)
            continue;   // dont allow to change at all
         if (pacConvertVolume)
            iVal -= (int)pacConvertVolume[j];   // BUGFIX - Was pacConvert, NOT pacConvertVolume
         iVal = max(iVal, SRABSOLUTESILENCE);
         iVal = min(iVal, SRMAXLOUDNESS);
         psrOrig->acVoiceEnergy[j] = (char)iVal;
      } // i


      int iDelta = 0;
#ifndef NOMODS_ENERGYPERVOLUMETUNE
         // BUGFIX - if the energy has changed after doing the volume-based-energy
         // adjust then rescale
         fp fEnergyNew = SRFEATUREEnergy (FALSE, psrOrig);
         fEnergyNew = fEnergyOrig / max(fEnergyNew, CLOSE);
         iDelta = AmplitudeToDb (fEnergyNew * (fp)0x8000);
         iDelta = max(iDelta, -12); // dont make too much change
         iDelta = min(iDelta, 12);
#endif

      // loop and adjust by fine-tuning amount and pitch
      for (j = 0; j < SRDATAPOINTS; j++) {
         iVal = (int)psrOrig->acVoiceEnergy[j];
         if (iVal < SRABSOLUTESILENCE+10)
            continue;   // dont allow to change at all
         iVal += iDelta;
         if (pacConvert)
            iVal -= (int)pacConvert[j];
         iVal = max(iVal, SRABSOLUTESILENCE);
         iVal = min(iVal, SRMAXLOUDNESS);
         psrOrig->acVoiceEnergy[j] = (char)iVal;
      } // i
   } // i

   return (PSRFEATURE)m_amemCacheConvert[dwThread].p;
}


/*************************************************************************************
DetermineStartEnd - Give a phoneme to analyze, determines the start/end to train
and compare given the demiphone.

inputs
   PPHONEAN          ppa - Phoneme
   DWORD             dwDemi - Demiphone. (DWORD)-1 for entire thing
   DWORD             *pdwStart - Fill in with start
   DWORD             *pdwLength - Fill in with the length in units
returns
   none
*/
__inline void DetermineStartEnd (PPHONEAN ppa, DWORD dwDemi, DWORD *pdwStart, DWORD *pdwLength)
{
   DWORD dwEntireStart = ppa->dwTimeStart;
   DWORD dwEntireLength = ppa->dwTimeEnd - ppa->dwTimeStart;

#ifdef NOMODS_TRIMLEFTRIGHT
   // ignore the trimmed-off bits
   dwEntireStart += ppa->dwTrimLeft;
   dwEntireLength -= (ppa->dwTrimLeft + ppa->dwTrimRight);
#endif

   DWORD dwStart, dwEnd;
   if (dwDemi == (DWORD)-1) {
      dwStart = dwEntireStart;
      dwEnd = dwEntireStart + dwEntireLength;
   }
   else {
      dwStart = dwEntireStart + dwDemi * dwEntireLength / TTSDEMIPHONES;
      dwEnd = dwEntireStart + (dwDemi+1) * dwEntireLength / TTSDEMIPHONES;
   }

   DWORD dwLength = max(dwEnd - dwStart, 1); // so at least 1 in length
   dwStart = min(dwStart, dwEnd - dwLength); // so don't go ober

   *pdwStart = dwStart;
   *pdwLength = dwLength;
}

/*************************************************************************************
CTTSWork::AnalysisPHONETRAINSub - Sub-training of indiviual wave.

inputs
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS engine
   DWORD             dwWave - Wave index
   DWORD             dwThread - 0..MAXRAYTHREAD-1

*/
void CTTSWork::AnalysisPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread)
{
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   BYTE bSilence;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());


   PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(dwWave));

   // cache the entire wave since will be accessing it call
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);

#ifndef OLDSR
   //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
   psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
   psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);
   psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

   // in each wave loop through all the phonemes
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   DWORD j;
   DWORD dwDemi;
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      PTRIPHONETRAIN paPHONETRAIN = GetPHONETRAIN(this, pAnal, ppa);

      // get the pitch
      // BUGFIX - Even use pitch for unvoiced
      //DWORD dwPitchFidelity = ppa->fIsVoiced ?
      //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
      //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
      DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paPHONETRAIN->fPitch /* pAnal->fAvgPitch*/ );
#ifdef DONTTRAINDURATION
      DWORD dwDurationFidelity = 0;
#else
      DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paPHONETRAIN->dwDuration);
#endif
      DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paPHONETRAIN->fEnergyMedian);

      _ASSERTE (paPHONETRAIN->fPitch);
      _ASSERTE (paPHONETRAIN->dwDuration);
      _ASSERTE (paPHONETRAIN->fEnergyMedian);

      if (!paPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity])
         continue;   // shouldnt happen, but because no training

#ifdef OLDSR
      // cache the features
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
      if (!psrCache)
         continue;

      // calculate the energy for each...
      lEnergy.Clear();
      for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
         fp fEnergy = SRFEATUREEnergy (psrCache + 
            (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
         lEnergy.Add (&fEnergy);
      }

      // train on this
      ppa->fSRScorePhone = paPHONETRAIN->pPhoneme->Compare (
         psrCache + ppa->dwTrimLeft /*pWave->m_paSRFeature + dwPhoneStart*/,
         (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
         ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight), pwa->m_fMaxEnergy, TRUE, TRUE);
#else
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

         ppa->afSRScorePhone[dwDemi] = paPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
      }
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
#endif // OLDSR
      // doesn
      // if (!ppa->fSRScorePhone)
      //   continue;
   } // j

#if 0 // BUGFIX - Using delta in SR, so take this hack out
   // BUGFIX - Since the SR score for plosives is encouraging blurred plossives,
   // underweight the plosive score
   fp fScoreLast = 0;
   ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // only if plosive
      if (!ppa->fIsPlosive) {
         // store this away for new last
         fScoreLast = ppa->fSRScorePhone;
         continue;
      }

      // left
      BOOL fHaveLeft = j && (ppa[-1].bPhone != bSilence) && (ppa[-1].dwTimeEnd > ppa[-1].dwTimeStart);
      BOOL fHaveRight = (j+1 < pwa->m_lPHONEAN.Num()) && (ppa[1].bPhone != bSilence) && (ppa[1].dwTimeEnd > ppa[1].dwTimeStart);
      DWORD dwWeight = 0;
      fp fAdjusted = 0;
      if (fHaveLeft) {
         dwWeight++;
         fAdjusted += fScoreLast;
      }
      if (fHaveRight) {
         dwWeight++;
         fAdjusted += ppa[1].fSRScorePhone;
      }
      // store this away for new last
      fScoreLast = ppa->fSRScorePhone;
      if (!dwWeight)
         continue;   // no point since nothing around it

      // else, adjust score, weighting phonemes on left/right with a 2:1 ratio
      ppa->fSRScorePhone = (ppa->fSRScorePhone + fAdjusted / (fp)dwWeight * 2.0) / 3.0;
   } // j
#endif // 0

   // release the SR features so don't use too much memory
   pwa->m_pWave->ReleaseSRFeatures();
}

/*************************************************************************************
WeightWithParent - Weights a value with the parent

inputs
   PTRIPHONETRAIN       pThis - This group, where can get weight from
   fp                   fValueParent - Paren't value
   fp                   fValue - This value
returns
   fp - New value, weighted
*/
__inline fp WeightParent (PTRIPHONETRAIN pThis, fp fValueParent, fp fValue)
{
   _ASSERTE (pThis);

   fp fCount = max(pThis->dwCountScale, 1);
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
   fCount /= (fp)(NUMFUNCWORDGROUP+1); // to counteract count scale
#endif
   fp fWeightThis = fCount / (fCount + (fp)PARENTCATEGORYWEIGHT);

   return fWeightThis * fValue + (1.0 - fWeightThis) * fValueParent;
}

__inline int WeightParentInt (PTRIPHONETRAIN pThis, fp fValueParent, fp fValue)
{
   return (int) floor(WeightParent(pThis, fValueParent, fValue) + 0.5);
}

/*************************************************************************************
CTTSWork::AnalysisPHONETRAIN - Fills in the triphonetrain structures for individual phonemes

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS engine
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k;
   DWORD dwNum;
   CListFixed lUNITRANK;
   lUNITRANK.Init (sizeof(UNITRANK));
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif

   UNITRANK ur;
   DWORD dwDemi;
   memset (&ur, 0, sizeof(ur));
   for (i = 0; i < dwNumPhone; i++) {
      lUNITRANK.Clear();

      // BUGFIX - Since combining word start/end, loop through all the word start/end
      PTPHONEINST ptiStart = NULL;
      for (j = 0; j < 4; j++) {
         PCListFixed plLook = pAnal->paplTriPhone[j][i];
         _ASSERTE (!m_fWordStartEndCombine || !j || !plLook || !plLook->Num());
         if (!plLook)
            continue;

         // get list
         PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
         dwNum = plLook->Num();

         // NOTE: Just add all triphones in
         if (dwNum)
            ptiStart = pti;   // so remember at least one
         DWORD dwInGroup = dwNum;
         for (; dwNum; dwNum--, pti++) {
            ur.pTPInst = pti;

            // add multiple copies so can weight depending upon function words or not
            DWORD dwAdd;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
            dwAdd = NUMFUNCWORDGROUP+1;
            if (pti && pti->pPHONEAN && pti->pPHONEAN->pWord)
               dwAdd = pti->pPHONEAN->pWord->dwFuncWordGroup+1;
#else
            dwAdd = 1;
#endif

            for (; dwAdd; dwAdd--)
               lUNITRANK.Add (&ur);
         } // dwNum
      } // j

      if (!ptiStart)
         continue;   // didn't find

      PTRIPHONETRAIN paPHONETRAIN = GetPHONETRAIN (this, pAnal, ptiStart->pPHONEAN);

      // know what's in the group, try and find some medians
      DWORD dwAttrib;
      PUNITRANK pur = (PUNITRANK)lUNITRANK.Get(0);
      for (dwAttrib = 0; dwAttrib < 8; dwAttrib++) {
         for (k = 0; k < lUNITRANK.Num(); k++) {

            // BUGFIX - Removed because don't think it's supposed to be there,
            // and because it's definitely a bug if it's there
            // skip general SR score calculation
            //if (k == 0)
            //   continue;

            switch (dwAttrib) {
            case 0: // SR score
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fSRScoreGeneral;
               break;

            case 1: // duration
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->dwDuration;
               break;

            case 2: // energy
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyAvg;
               break;

            case 3: // ipitch
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitch;
               break;

            case 4: // ipitch delta
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchDelta;
               break;

            case 5: // energy ratio
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyRatio;
               break;

            case 6:  // fPitch
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fPitch;
               break;

            case 7: // ipitch bulge
               pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchBulge;
               break;
            } // switch
         } // k

         // sort
         qsort (pur, lUNITRANK.Num(), sizeof(UNITRANK), UNITRANKSort);

         // find the median value
         DWORD dwMid = lUNITRANK.Num()/2;
         fp fMid = pur[dwMid].fCompare;

         // write this out
         paPHONETRAIN->dwCountScale = lUNITRANK.Num();
         switch (dwAttrib) {
         case 0: // SR score
            // BUGFIX - allow 3/4 of the phones to pass through, not just 1/2
            // Do this so (hopefully) don't eliminate any bright phonemes at the start
            // paPHONETRAIN->fSRScoreMedian = fMid;
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) 
               paPHONETRAIN->afSRScoreMedian[dwDemi] = pur[lUNITRANK.Num()*3/4].fCompare;
            break;

         case 1: // duration
            paPHONETRAIN->dwDuration = (DWORD)fMid;
            paPHONETRAIN->dwDurationSRFeat = pur[dwMid].pTPInst->pPHONEAN->dwTimeEnd - pur[dwMid].pTPInst->pPHONEAN->dwTimeStart;
            break;

         case 2: // energy
            paPHONETRAIN->fEnergyMedian = fMid;

            // BUGFIX - Try to remember slightly higher than normal energy so favor those
            paPHONETRAIN->fEnergyMedianHigh = pur[lUNITRANK.Num()*3/4].fCompare;

            break;

         case 3: // ipitch
            paPHONETRAIN->iPitch = (int)fMid;
            break;

         case 4: // ipitch delta
            paPHONETRAIN->iPitchDelta = (int)fMid;
            break;

         case 5: // energy ratio
            paPHONETRAIN->fEnergyRatio = fMid;
            break;

         case 6:  // pitch
            paPHONETRAIN->fPitch = fMid;
            break;

         case 7: // ipitch bulge
            paPHONETRAIN->iPitchBulge = (int)fMid;
            break;
         } // switch
      } // dwAttrab
   } // i

   // loop through all phonemes and make a model from the phonemes of the same
   // type so get a better "average" phoneme
   BYTE bSilence;
   // DWORD dwDemi;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
   pProgress->Push (0, 0.5);
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);

#ifndef OLDSR
      psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
      psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);
      psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // get the memory where the phone goes
         PTRIPHONETRAIN paPHONETRAIN = GetPHONETRAIN(this, pAnal, ppa);

         // default weighting
         fp fWeight = 1.0;

         // if this SR error > the median error then don't bother including it
         // in the training database
#ifdef NOMODS_MISCSCOREHACKS
         fp fScoreSum = 0, fMegaScoreSum = 0;
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            fScoreSum += ppa->fSRScoreGeneral;
            fMegaScoreSum += paPHONETRAIN->afSRScoreMedian[dwDemi];
         }
         if (fScoreSum > fMegaScoreSum)
            fWeight = 0.25; // BUGFIX - Was a straght continue, but instead, underweight
            //continue;
#endif

         // train the phase model
         DWORD dwStart, dwLength;
         PCPhaseModel pPM = pTTS->PhaseModelGet (ppa->bPhone, TRUE);
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);
         DWORD dwFrame, dwPitchBin;
         fp fPitchAtFrame;
         if (pPM) for (dwFrame = dwStart; dwFrame < dwStart + dwLength; dwFrame++) {
            fPitchAtFrame = pwa->m_pWave->PitchAtSample (PITCH_F0, (fp)dwFrame * (fp)pwa->m_pWave->m_dwSRSkip, 0);
            // BUGBUG - may want sub-pitch too

            dwPitchBin = pPM->Pitch (fPitchAtFrame, pAnal->fAvgPitch);
            pPM->Train (dwPitchBin,
               &((ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwFrame)->sr,
               psrCache + dwFrame);
         } // dwFrame


         // BUGFIX - weight louder units more than quieter ones since
         // they're more likely to have bright formants
         // BUGFIX - Don't do this anymore since choosing too loud, and now have better approach with multiple train bins
         // fWeight *= ppa->fEnergyAvg / pwa->m_fAvgEneryPerPhone;

#ifdef NOMODS_MISCSCOREHACKS
         // BUGFIX - If voiced, then include the strength of the pitch detect
         if (ppa->fIsVoiced)
            fWeight *= ppa->fPitchStrength;
#endif

         // if there isn't any phoneme training here then create
         DWORD dwPF, dwDF, dwEF;
         for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            if (!paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
               paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = new CPhoneme;
               if (!paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
                  break;   // error
               paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->HalfSize (TRUE);
            }

            // train more distant pitches with this too, but with lower weights
            // BUGFIX - Even use pitch for unvoiced
            //DWORD dwPitchFidelity = ppa->fIsVoiced ?
            //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
            //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
            DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch,
               pAnal->afPhonemePitchSum[ppa->bPhone] / (double)pAnal->adwPhonemeCount[ppa->bPhone] /*pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
            DWORD dwDurationFidelity = 0;
#else
            DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration,
               pAnal->afPhonemeDurationSum[ppa->bPhone] / (double)pAnal->adwPhonemeCount[ppa->bPhone] );
#endif
            DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, 
               pAnal->afPhonemeEnergySum[ppa->bPhone] / (double)pAnal->adwPhonemeCount[ppa->bPhone] );

            fp fWeightPitchFidelity = SRTRAINWEIGHTSCALE *
               TrainWeight (dwPitchFidelity, dwPF, dwDurationFidelity, dwDF, dwEnergyFidelity, dwEF);
#ifdef OLDSR
            // cache the features
            PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
            if (!psrCache)
               continue;

            // calculate the energy for each...
            lEnergy.Clear();
            for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
               fp fEnergy = SRFEATUREEnergy (psrCache + 
                  (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
               lEnergy.Add (&fEnergy);
            }

            // train on this
            if (!paPHONETRAIN->pPhoneme->Train (
               psrCache + ppa->dwTrimLeft/*pat[i].pWave->m_paSRFeature+pat[i].dwStart*/,
               (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
               ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight),
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex))
               continue;
#else
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            // train on this
            if (!paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               (ppa->pWord ? ppa->pWord->fFuncWordWeight : 1) * fWeight * fWeightPitchFidelity))
               break;   // error
#endif // OLDSR
         } // dwPF
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i
   pProgress->Pop ();

   // loop through all the phase models and fill out
   for (i = 0; i < pAnal->dwPhonemes; i++) {
      PCPhaseModel pPM = pTTS->PhaseModelGet (i, TRUE);
      if (pPM)
         pPM->FillOut();
   }

   // loop through all the phonemes and make sure safe for multithreded
   PTRIPHONETRAIN paPHONETRAIN = (PTRIPHONETRAIN) pAnal->paPHONETRAIN;
   DWORD dwPF, dwDF, dwEF;
   for (i = 0; i < pAnal->dwPhonemes; i++, paPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->PrepForMultiThreaded();

   // go through all the phonemes and see what kind of accuracy they have
   pProgress->Push (0.5, 1);
   EMTCANALYSISPHONETRAIN emapt;
   memset (&emapt, 0, sizeof(emapt));
   emapt.dwType = 20;
   emapt.pAnal = pAnal;
   emapt.pTTS = pTTS;
   ThreadLoop (0, pAnal->plPCWaveAn->Num(), 16, &emapt, sizeof(emapt), pProgress);
   pProgress->Pop();

   // now that done with training, free all the phoneme training information since wont
   // need it anymore
   paPHONETRAIN = (PTRIPHONETRAIN) pAnal->paPHONETRAIN;
   //DWORD dwPF;
   for (i = 0; i < /* BUGFIX - ignore star/end word 4 * */ pAnal->dwPhonemes ; i++, paPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
            delete paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
            paPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = NULL;
         }


   return TRUE;

}




/*************************************************************************************
CTTSWork::AnalysisMegaPHONETRAINSub - Sub-training of indiviual wave.

inputs
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS engine
   DWORD             dwWave - Wave index
   DWORD             dwThread - 0..MAXRAYTHREAD-1

*/
void CTTSWork::AnalysisMegaPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread)
{
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   DWORD j;
   BYTE bSilence;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());


   PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(dwWave));

   // cache the entire wave since will be accessing it call
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);

#ifndef OLDSR
   //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
   psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
   psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);
   psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

   DWORD dwDemi;
   DWORD i;

   // in each wave loop through all the phonemes
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   CListFixed lMISMATCHSTRUCTTheory;   // list of theoretical mismatches
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      PTRIPHONETRAIN paMegaPHONETRAIN = GetMegaPHONETRAIN(this, pAnal, ppa);

      // get the pitch
      // BUGFIX - Even use pitch for unvoiced
      // DWORD dwPitchFidelity = ppa->fIsVoiced ?
      //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
      //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
      DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paMegaPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
      DWORD dwDurationFidelity = 0;
#else
      DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paMegaPHONETRAIN->dwDuration);
#endif
      DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paMegaPHONETRAIN->fEnergyMedian);

      _ASSERTE (paMegaPHONETRAIN->fPitch);
      _ASSERTE (paMegaPHONETRAIN->dwDuration);
      _ASSERTE (paMegaPHONETRAIN->fEnergyMedian);

      // get the memory where the phone goes
      if (!paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity])
         continue;   // shouldnt happen, but because no training

#ifdef OLDSR
      // cache the features
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust(pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
      if (!psrCache)
         continue;

      // calculate the energy for each...
      lEnergy.Clear();
      for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
         fp fEnergy = SRFEATUREEnergy (psrCache + 
            (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
         lEnergy.Add (&fEnergy);
      }

      // train on this
      ppa->fSRScoreMegaPhone = paMegaPHONETRAIN->pPhoneme->Compare (
         psrCache + ppa->dwTrimLeft /*pWave->m_paSRFeature + dwPhoneStart*/,
         (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
         ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight), pwa->m_fMaxEnergy, TRUE, TRUE);
#else
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

         ppa->afSRScoreMegaPhone[dwDemi] = paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                           // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      } // dwDemi
#endif // OLDSR
      // BUGIFX - disable this
      // if (!ppa->fSRScoreMegaPhone)
      //   continue;

      // BUGFIX - loop through all the phonemes with the same stress, and same
      // megaphone group, seeing if any other phonemes are similar.
      // Try to choose most distinct phonemes
      PLEXPHONE plpThis = pLex->PhonemeGetUnsort (ppa->bPhone);
      PLEXENGLISHPHONE pleThis = plpThis ? MLexiconEnglishPhoneGet(plpThis->bEnglishPhone) : NULL;
      PLEXPHONE plpTry;
      PLEXENGLISHPHONE pleTry;
      DWORD dwNumPhone = pLex->PhonemeNum();
      DWORD k;
      fp afBestScore[TTSDEMIPHONES];
      DWORD adwFoundBest[TTSDEMIPHONES];
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         afBestScore[dwDemi] = 0;
         adwFoundBest[dwDemi] = (DWORD)-1;
      }
      if (plpThis && pleThis) for (k = 0; k < dwNumPhone; k++) {
         // dont bother with self
         if (k == ppa->bPhone)
            continue;

         plpTry = pLex->PhonemeGetUnsort (k);
         if (!plpTry || (plpTry->bStress != plpThis->bStress))
            continue;   // only if same stress level

         // check the phone group
         pleTry = plpTry ? MLexiconEnglishPhoneGet(plpTry->bEnglishPhone) : NULL;
         if (!pleTry || (PIS_FROMPHONEGROUP(pleTry->dwShape) != PIS_FROMPHONEGROUP(pleThis->dwShape)) )
            continue;

         paMegaPHONETRAIN = GetMegaPHONETRAIN(this, pAnal, ppa, k);

         // if no training whatsoever then abort
         BOOL fFound = FALSE;
         DWORD dwPF, dwDF, dwEF;
         for (dwPF = 0; (dwPF < PITCHFIDELITY) && !fFound; dwPF++)  for (dwDF = 0; (dwDF < DURATIONFIDELITYHACKSMALL) && !fFound; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            if (paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
               fFound = TRUE;
               break;
            }
         if (!fFound)
            continue;   // no training at all


         // get the pitch
         // BUGFIX - Even use pitch for unvoiced
         // DWORD dwPitchFidelity = ppa->fIsVoiced ?
         //    PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
         //    PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
         DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paMegaPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
         DWORD dwDurationFidelity = 0;
#else
         DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paMegaPHONETRAIN->dwDuration);
#endif
         DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paMegaPHONETRAIN->fEnergyMedian);

         _ASSERTE (paMegaPHONETRAIN->fPitch);
         _ASSERTE (paMegaPHONETRAIN->dwDuration);
         _ASSERTE (paMegaPHONETRAIN->fEnergyMedian);

         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            // found, so find training
            if (!paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
               continue;   // might not be any training

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            // get the sr score
            fp fCompare = paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
               FALSE /* slow */, FALSE /* all examplars */);
            
            // keep this as best
            if ((adwFoundBest[dwDemi] == (DWORD)-1) || (fCompare < afBestScore[dwDemi])) {
               afBestScore[dwDemi] = fCompare;
               adwFoundBest[dwDemi] = k;
            }
         } // dwDemi
      } // k

      // if found a phoneme, remember to adjust this score.
      // fSRScoreMegaPhoneUnique will be higher if this unit sound more like another phoneme.
      // or negative is this is fairly unique sounding
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         if (adwFoundBest[dwDemi] != (DWORD)-1)
            ppa->afSRScoreMegaPhoneUnique[dwDemi] = ppa->afSRScoreMegaPhone[dwDemi] - afBestScore[dwDemi];
         ppa->afSRScoreMegaPhoneUnique[dwDemi] += SRCOMPAREWEIGHT / 8.0;
            // BUGFIX - Adding 10.0 since typically get -10.0 for this, and want net of nothing added
         ppa->afSRScoreMegaPhoneUnique[dwDemi] *= MISLABELSCALE;
            // scale this to control how weighty mislabeled scores are

         ppa->afSRScoreMegaPhone[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this
      } // dwDemi


      // look through all possible megaphone combinations and see which ones might produce the
      // best SR results
      lMISMATCHSTRUCTTheory.Init (sizeof(MISMATCHSTRUCT));
      MISMATCHSTRUCT mms;
      DWORD dwLeft, dwRight;
      DWORD dwTPhoneCompact = ppa->awTriPhone[0];
      DWORD dwLeftThis = ((dwTPhoneCompact & 0xff) % PIS_PHONEGROUPNUM);
      DWORD dwRightThis = (((dwTPhoneCompact & 0xff00) >> 8) % PIS_PHONEGROUPNUM);
      dwLeftThis = LexPhoneGroupToMega (dwLeftThis);
      dwRightThis = LexPhoneGroupToMega (dwRightThis);
      for (dwLeft = 0; dwLeft < PIS_PHONEMEGAGROUPNUM; dwLeft++)
         for (dwRight = 0; dwRight < PIS_PHONEMEGAGROUPNUM; dwRight++) {
            // if this is an exact match then don't skip because already calculated in afSRScoreMegaPhone
            if ((dwLeft == dwLeftThis) && (dwRight == dwRightThis))
               continue;

            // see if can get the info
            paMegaPHONETRAIN = GetMegaPHONETRAIN (this, pAnal, ppa, 255, (BYTE)dwLeft, (BYTE)dwRight);
            if (!paMegaPHONETRAIN)
               continue;   // not found

            DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paMegaPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
            DWORD dwDurationFidelity = 0;
#else
            DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paMegaPHONETRAIN->dwDuration);
#endif
            DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paMegaPHONETRAIN->fEnergyMedian);

            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               if (!paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
                  break;
            if (dwDemi < TTSDEMIPHONES)
               continue;   // no SR

            _ASSERTE (paMegaPHONETRAIN->fPitch);
            _ASSERTE (paMegaPHONETRAIN->dwDuration);
            _ASSERTE (paMegaPHONETRAIN->fEnergyMedian);

            // find a phoneme that matches the megagroup
            DWORD dwPhoneLeft, dwPhoneRight;
            for (dwPhoneLeft = 0; dwPhoneLeft < pLex->PhonemeNum(); dwPhoneLeft++)
               if (pLex->PhonemeToMegaGroup(dwPhoneLeft) == dwLeft)
                  break;
            if (dwPhoneLeft >= pLex->PhonemeNum())
               dwPhoneLeft = bSilence;
            for (dwPhoneRight = 0; dwPhoneRight < pLex->PhonemeNum(); dwPhoneRight++)
               if (pLex->PhonemeToMegaGroup(dwPhoneRight) == dwRight)
                  break;
            if (dwPhoneRight >= pLex->PhonemeNum())
               dwPhoneRight = bSilence;

            // if get this far then has all the necessary info to do SR, so guestimate a score
            mms.fCompare =
               ((dwLeft == dwLeftThis) ? 0 :
                  (UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwPhoneLeft, pLex, FALSE, 5) - UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwPhoneLeft, pLex, FALSE, 4))) +
               ((dwRight == dwRightThis) ? 0 :
                  (UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwPhoneRight, pLex, TRUE, 5) - UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwPhoneRight, pLex, TRUE, 4)));

            mms.dwValue = (dwRight << 8) | dwLeft;
            mms.pVoid = paMegaPHONETRAIN;
            lMISMATCHSTRUCTTheory.Add (&mms);
         } //dwLeft, dwRight

      // sort
      if (lMISMATCHSTRUCTTheory.Num() >= 2)
         qsort (lMISMATCHSTRUCTTheory.Get(0), lMISMATCHSTRUCTTheory.Num(), sizeof(MISMATCHSTRUCT), MISMATCHSTRUCTSort);

      // keep only the top 4 of these
      PMISMATCHSTRUCT pmms = (PMISMATCHSTRUCT)lMISMATCHSTRUCTTheory.Get(0);
      for (i = 0; i < min(lMISMATCHSTRUCTTheory.Num(), TRIPHONEMEGAGROUPMISMATCH); i++, pmms++) {
         paMegaPHONETRAIN = (PTRIPHONETRAIN) pmms->pVoid;

         DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paMegaPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
         DWORD dwDurationFidelity = 0;
#else
         DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paMegaPHONETRAIN->dwDuration);
#endif
         DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paMegaPHONETRAIN->fEnergyMedian);

         _ASSERTE (paMegaPHONETRAIN->fPitch);
         _ASSERTE (paMegaPHONETRAIN->dwDuration);
         _ASSERTE (paMegaPHONETRAIN->fEnergyMedian);

         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            pmms->afScore[dwDemi] = paMegaPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
               FALSE /* slow */, FALSE /* all examplars */);
                              // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
   
            pmms->afScore[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this
         } // dwDemi

         // write this
         ppa->aMMSMegaGroup[i] = *pmms;
      } // over best megagroup mismatches

   } // j, over all phonemes in the wave

   // BUGFIX - Since have delta in SR, remove plosive score hack
#if 0
   // BUGFIX - Since the SR score for plosives is encouraging blurred plossives,
   // underweight the plosive score
   fp fScoreLast = 0;
   ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // only if plosive
      if (!ppa->fIsPlosive) {
         // store this away for new last
         fScoreLast = ppa->fSRScoreMegaPhone;
         continue;
      }

      // left
      BOOL fHaveLeft = j && (ppa[-1].bPhone != bSilence) && (ppa[-1].dwTimeEnd > ppa[-1].dwTimeStart);
      BOOL fHaveRight = (j+1 < pwa->m_lPHONEAN.Num()) && (ppa[1].bPhone != bSilence) && (ppa[1].dwTimeEnd > ppa[1].dwTimeStart);
      DWORD dwWeight = 0;
      fp fAdjusted = 0;
      if (fHaveLeft) {
         dwWeight++;
         fAdjusted += fScoreLast;
      }
      if (fHaveRight) {
         dwWeight++;
         fAdjusted += ppa[1].fSRScoreMegaPhone;
      }
      // store this away for new last
      fScoreLast = ppa->fSRScoreMegaPhone;
      if (!dwWeight)
         continue;   // no point since nothing around it

      // else, adjust score, weighting phonemes on left/right with a 2:1 ratio
      ppa->fSRScoreMegaPhone = (ppa->fSRScoreMegaPhone + fAdjusted / (fp)dwWeight * 2.0) / 3.0;
   } // j
#endif // 0

   // release the SR features so don't use too much memory
   pwa->m_pWave->ReleaseSRFeatures();

}


/*************************************************************************************
CTTSWork::AnalysisMegaPHONETRAIN - Fills in the triphonetrain structures for phonemes
in the mega-groups, defined by LexPhoneGroupToMega()

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS to use
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisMegaPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k;
   DWORD dwNum;
   CListFixed lUNITRANK;
   lUNITRANK.Init (sizeof(UNITRANK));
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif

   DWORD dwDemi;
   UNITRANK ur;
   memset (&ur, 0, sizeof(ur));
   for (i = 0; i < dwNumPhone; i++) {
      // loop through all the mega-groups
      DWORD dwMegaGroup;
      for (dwMegaGroup = 0; dwMegaGroup < PHONEMEGAGROUPSQUARE; dwMegaGroup++) {
         DWORD dwMegaLeft = dwMegaGroup % PIS_PHONEMEGAGROUPNUM;
         DWORD dwMegaRight = dwMegaGroup / PIS_PHONEMEGAGROUPNUM;

         lUNITRANK.Clear();

         // BUGFIX - Since combining word start/end, loop through all the word start/end
         PTPHONEINST ptiStart = NULL;
         for (j = 0; j < 4; j++) {
            PCListFixed plLook = pAnal->paplTriPhone[j][i];
            _ASSERTE (!m_fWordStartEndCombine || !j || !plLook || !plLook->Num());
            if (!plLook)
               continue;

            // get list
            PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
            dwNum = plLook->Num();

            // NOTE: Just add all triphones in
            DWORD dwInGroup = dwNum;
            for (; dwNum; dwNum--, pti++) {
               // make sure the phoneme mega-group units match
               DWORD dwLeft = (pti->pPHONEAN->awTriPhone[0] & 0xff) % PIS_PHONEGROUPNUM;
               DWORD dwRight = ((pti->pPHONEAN->awTriPhone[0] & 0xff00) >> 8) % PIS_PHONEGROUPNUM;
               dwLeft = LexPhoneGroupToMega(dwLeft);
               dwRight = LexPhoneGroupToMega(dwRight);
               if ((dwLeft != dwMegaLeft) || (dwRight != dwMegaRight))
                  continue;   // dont count this one


               ur.pTPInst = pti;

               // rmember start
               if (!ptiStart)
                  ptiStart = pti;

               // add multiple copies so can weight depending upon function words or not
               DWORD dwAdd;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
               dwAdd = NUMFUNCWORDGROUP+1;
               if (pti && pti->pPHONEAN && pti->pPHONEAN->pWord)
                  dwAdd = pti->pPHONEAN->pWord->dwFuncWordGroup+1;
#else
               dwAdd = 1;
#endif

               for (; dwAdd; dwAdd--)
                  lUNITRANK.Add (&ur);
            } // dwNum
         } // j

         if (!ptiStart)
            continue;   // didn't find

         PTRIPHONETRAIN paMegaPHONETRAIN = GetMegaPHONETRAIN (this, pAnal, ptiStart->pPHONEAN);

         // know what's in the group, try and find some medians
         DWORD dwAttrib;
         PUNITRANK pur = (PUNITRANK)lUNITRANK.Get(0);
         for (dwAttrib = 0; dwAttrib < 8; dwAttrib++) {
            for (k = 0; k < lUNITRANK.Num(); k++) {

               // BUGFIX - Removed because don't think it's supposed to be there,
               // and because it's definitely a bug if it's there
               // skip general SR score calculation
               //if (k == 0)
               //   continue;

               switch (dwAttrib) {
               case 0: // SR score
                  pur[k].fCompare = 0;
                  for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                     pur[k].fCompare += pur[k].pTPInst->pPHONEAN->afSRScorePhone[dwDemi] / (fp)TTSDEMIPHONES;
                  break;

               case 1: // duration
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->dwDuration;
                  break;

               case 2: // energy
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyAvg;
                  break;

               case 3: // ipitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitch;
                  break;

               case 4: // ipitch delta
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchDelta;
                  break;

               case 5: // energy ratio
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyRatio;
                  break;

               case 6: // pitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fPitch;
                  break;

               case 7: // ipitch bulge
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchBulge;
                  break;
               } // switch
            } // k

            // sort
            qsort (pur, lUNITRANK.Num(), sizeof(UNITRANK), UNITRANKSort);

            // find the median value
            DWORD dwMid = lUNITRANK.Num()/2;
            fp fMid = pur[dwMid].fCompare;

            // write this out
            paMegaPHONETRAIN->dwCountScale = lUNITRANK.Num();
            switch (dwAttrib) {
            case 0: // SR score
               // BUGFIX - allow 3/4 of the phones to pass through, not just 1/2
               // Do this so (hopefully) don't eliminate any bright phonemes at the start
               // paMegaPHONETRAIN->fSRScoreMedian = fMid;
               for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                  paMegaPHONETRAIN->afSRScoreMedian[dwDemi] = pur[lUNITRANK.Num()*3/4].fCompare;
               break;

            case 1: // duration
               paMegaPHONETRAIN->dwDuration = (DWORD)fMid;
               paMegaPHONETRAIN->dwDurationSRFeat = pur[dwMid].pTPInst->pPHONEAN->dwTimeEnd - pur[dwMid].pTPInst->pPHONEAN->dwTimeStart;
               break;

            case 2: // energy
               paMegaPHONETRAIN->fEnergyMedian = fMid;

               // BUGFIX - Try to remember slightly higher than normal energy so favor those
               paMegaPHONETRAIN->fEnergyMedianHigh = pur[lUNITRANK.Num()*3/4].fCompare;

               break;

            case 3: // ipitch
               paMegaPHONETRAIN->iPitch = (int)fMid;
               break;

            case 4: // ipitch delta
               paMegaPHONETRAIN->iPitchDelta = (int)fMid;
               break;

            case 5: // energy ratio
               paMegaPHONETRAIN->fEnergyRatio = fMid;
               break;

            case 6: // fPitch
               paMegaPHONETRAIN->fPitch = fMid;
               break;

            case 7: // ipitch bulge
               paMegaPHONETRAIN->iPitchBulge = (int)fMid;
               break;
            } // switch
         } // dwAttrab

         // adjust to weight by the parent category
         PTRIPHONETRAIN pPHONETRAIN = GetPHONETRAIN (this, pAnal, ptiStart->pPHONEAN);

         paMegaPHONETRAIN->dwDuration = (DWORD) WeightParentInt (paMegaPHONETRAIN, pPHONETRAIN->dwDuration, paMegaPHONETRAIN->dwDuration);
         paMegaPHONETRAIN->dwDurationSRFeat = (DWORD) WeightParentInt (paMegaPHONETRAIN, pPHONETRAIN->dwDurationSRFeat, paMegaPHONETRAIN->dwDurationSRFeat);
         paMegaPHONETRAIN->iPitch = WeightParentInt (paMegaPHONETRAIN, pPHONETRAIN->iPitch, paMegaPHONETRAIN->iPitch);
         paMegaPHONETRAIN->iPitchDelta = WeightParentInt (paMegaPHONETRAIN, pPHONETRAIN->iPitchDelta, paMegaPHONETRAIN->iPitchDelta);
         paMegaPHONETRAIN->iPitchBulge = WeightParentInt (paMegaPHONETRAIN, pPHONETRAIN->iPitchBulge, paMegaPHONETRAIN->iPitchBulge);
         paMegaPHONETRAIN->fEnergyMedian = WeightParent (paMegaPHONETRAIN, pPHONETRAIN->fEnergyMedian, paMegaPHONETRAIN->fEnergyMedian);
         paMegaPHONETRAIN->fEnergyMedianHigh = WeightParent (paMegaPHONETRAIN, pPHONETRAIN->fEnergyMedianHigh, paMegaPHONETRAIN->fEnergyMedianHigh);
         paMegaPHONETRAIN->fEnergyRatio = WeightParent (paMegaPHONETRAIN, pPHONETRAIN->fEnergyRatio, paMegaPHONETRAIN->fEnergyRatio);
         paMegaPHONETRAIN->fPitch = WeightParent (paMegaPHONETRAIN, pPHONETRAIN->fPitch, paMegaPHONETRAIN->fPitch);
#if 0 // replaced with cleaner code
         DWORD dwCount = paMegaPHONETRAIN->dwCountScale / (NUMFUNCWORDGROUP+1); // counteract weight
         dwCount = max(dwCount, 1);
         paMegaPHONETRAIN->dwDuration = (paMegaPHONETRAIN->dwDuration * dwCount +
            pPHONETRAIN->dwDuration * PARENTCATEGORYWEIGHT + (dwCount+PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paMegaPHONETRAIN->dwDurationSRFeat = (paMegaPHONETRAIN->dwDurationSRFeat * dwCount +
            pPHONETRAIN->dwDurationSRFeat * PARENTCATEGORYWEIGHT + (dwCount + PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paMegaPHONETRAIN->fEnergyMedian = (paMegaPHONETRAIN->fEnergyMedian * (fp) dwCount +
            pPHONETRAIN->fEnergyMedian * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->fEnergyMedianHigh = (paMegaPHONETRAIN->fEnergyMedianHigh * (fp) dwCount +
            pPHONETRAIN->fEnergyMedianHigh * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->iPitch = (paMegaPHONETRAIN->iPitch * (int) dwCount +
            pPHONETRAIN->iPitch * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->iPitchDelta = (paMegaPHONETRAIN->iPitchDelta * (int) dwCount +
            pPHONETRAIN->iPitchDelta * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->iPitchBulge = (paMegaPHONETRAIN->iPitchBulge * (int) dwCount +
            pPHONETRAIN->iPitchBulge * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->fEnergyRatio = (paMegaPHONETRAIN->fEnergyRatio * (fp) dwCount +
            pPHONETRAIN->fEnergyRatio * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paMegaPHONETRAIN->fPitch = (paMegaPHONETRAIN->fPitch * (fp) dwCount +
            pPHONETRAIN->fPitch * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
#endif
      } // dwMEgaGroup
   } // i

   // loop through all phonemes and make a model from the phonemes of the same
   // type so get a better "average" phoneme
   BYTE bSilence;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
   pProgress->Push (0, 0.5);
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);

#ifndef OLDSR
      //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
      psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
      psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);
      psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // get the memory where the phone goes
         PTRIPHONETRAIN paMegaPHONETRAIN = GetMegaPHONETRAIN(this, pAnal, ppa);

         // default weighting
         fp fWeight = 1.0;

#ifdef NOMODS_MISCSCOREHACKS
         // if this SR error > the median error then don't bother including it
         // in the training database
         fp fScoreSum = 0, fMegaScoreSum = 0;
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            fScoreSum += ppa->afSRScorePhone[dwDemi];
            fMegaScoreSum += paMegaPHONETRAIN->afSRScoreMedian[dwDemi];
         }
         if (fScoreSum > fMegaScoreSum)
            fWeight = 0.25; // BUGFIX - Was a straght continue, but instead, underweight
            //continue;
#endif

         // BUGFIX - weight louder units more than quieter ones since
         // they're more likely to have bright formants
         // BUGFIX - Don't do this anymore since choosing too loud, and now have better approach with multiple train bins
         // fWeight *= ppa->fEnergyAvg / pwa->m_fAvgEneryPerPhone;

#ifdef NOMODS_MISCSCOREHACKS
         // BUGFIX - If voiced, then include the strength of the pitch detect
         if (ppa->fIsVoiced)
            fWeight *= ppa->fPitchStrength;
#endif

         DWORD dwPF, dwDF, dwEF;
         for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++)  for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            // if there isn't any phoneme training here then create
            if (!paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
               paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = new CPhoneme;
               if (!paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
                  continue;   // error
               paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->HalfSize(TRUE);
            }

            // train more distant pitches with this too, but with lower weights
            // BUGFIX - Even use pitch for unvoiced
            // DWORD dwPitchFidelity = ppa->fIsVoiced ?
            //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
            //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
            DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paMegaPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
            DWORD dwDurationFidelity = 0;
#else
            DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paMegaPHONETRAIN->dwDuration);
#endif
            DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paMegaPHONETRAIN->fEnergyMedian);

            _ASSERTE (paMegaPHONETRAIN->fPitch);
            _ASSERTE (paMegaPHONETRAIN->dwDuration);
            _ASSERTE (paMegaPHONETRAIN->fEnergyMedian);

            fp fWeightPitchFidelity = SRTRAINWEIGHTSCALE *
               TrainWeight (dwPitchFidelity, dwPF, dwDurationFidelity, dwDF, dwEnergyFidelity, dwEF);
#ifdef OLDSR
            // cache the features
            PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
            if (!psrCache)
               continue;

            // calculate the energy for each...
            lEnergy.Clear();
            for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
               fp fEnergy = SRFEATUREEnergy (psrCache + 
                  (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
               lEnergy.Add (&fEnergy);
            }

            // train on this
            if (!paMegaPHONETRAIN->pPhoneme->Train (
               psrCache + ppa->dwTrimLeft/*pat[i].pWave->m_paSRFeature+pat[i].dwStart*/,
               (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
               ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight),
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex))
               continue;
#else
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            // train on this
            if (!paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               (ppa->pWord ? ppa->pWord->fFuncWordWeight : 1) * fWeight * fWeightPitchFidelity))
               break;
#endif // OLDSR
         } // dwPF
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i
   pProgress->Pop ();


   // loop through all the phonemes and make sure safe for multithreded
   PTRIPHONETRAIN paMegaPHONETRAIN = (PTRIPHONETRAIN) pAnal->paMegaPHONETRAIN;
   DWORD dwPF, dwDF, dwEF;
   for (i = 0; i < pAnal->dwPhonemes * PHONEMEGAGROUPSQUARE; i++, paMegaPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->PrepForMultiThreaded();

   // go through all the phonemes and see what kind of accuracy they have
   pProgress->Push (0.5, 1);
   EMTCANALYSISPHONETRAIN emapt;
   memset (&emapt, 0, sizeof(emapt));
   emapt.dwType = 30;
   emapt.pAnal = pAnal;
   emapt.pTTS = pTTS;
   ThreadLoop (0, pAnal->plPCWaveAn->Num(), 16, &emapt, sizeof(emapt), pProgress);
   pProgress->Pop();

   // now that done with training, free all the phoneme training information since wont
   // need it anymore
   paMegaPHONETRAIN = (PTRIPHONETRAIN) pAnal->paMegaPHONETRAIN;
   for (i = 0; i < pAnal->dwPhonemes * PHONEMEGAGROUPSQUARE; i++, paMegaPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
            delete paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
            paMegaPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = NULL;
         }


   return TRUE;

}

/*************************************************************************************
CTTSWork::AnalysisGroupTRIPHONETRAINSub - Sub-training of indiviual wave.

inputs
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS engine
   DWORD             dwWave - Wave index
   DWORD             dwThread - 0..MAXRAYTHREAD-1
   DWORD             dwStartPhone - Starting phoneme that's acceptable. Used so can multipass and reduce memory requirements
   DWORD             dwEndPhone - Ending phoneme that's start of unacceptable. Used so can multipass and reduce memory requirements
*/
void CTTSWork::AnalysisGroupTRIPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread,
                                         DWORD dwStartPhone, DWORD dwEndPhone)
{
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   DWORD j;
   BYTE bSilence;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());


   DWORD dwDemi;
   PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(dwWave));

   // cache the entire wave since will be accessing it call
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);

#ifndef OLDSR
   //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
   psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
   psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);
   psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

   // in each wave loop through all the phonemes
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   CListFixed lMISMATCHSTRUCTTheory;
   CListFixed lPhoneLeft, lPhoneRight;
   CHashPVOID hAlreadyAdded;
   DWORD i;
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // BUGFIX - make sure in phoneme range
      if ( ((DWORD)ppa->bPhone < dwStartPhone) || ((DWORD)ppa->bPhone >= dwEndPhone))
         continue;   // not part of this pass

      PTRIPHONETRAIN paTRIPHONETRAIN = GetGroupTRIPHONETRAIN(this, pAnal, ppa, pLex);

      // get the pitch
      // BUGFIX - Even use pitch for unvoiced
      //DWORD dwPitchFidelity = ppa->fIsVoiced ?
      //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
      //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
      DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paTRIPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
      DWORD dwDurationFidelity = 0;
#else
      DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paTRIPHONETRAIN->dwDuration);
#endif
      DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paTRIPHONETRAIN->fEnergyMedian);

      _ASSERTE (paTRIPHONETRAIN->fPitch);
      _ASSERTE (paTRIPHONETRAIN->dwDuration);
      _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         // get the memory where the phone goes
         if (!paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
            continue;   // shouldnt happen, but because no training


         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

         ppa->afSRScoreGroupTriPhone[dwDemi] = paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                           // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

         // BUGFIX - Weight tre triphone my the megaphoneunique value
         // so encourage unique-sounding phonemes, not ones that sound like other
         // phonemes in the group
         ppa->afSRScoreGroupTriPhone[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this

         // BUGFIX - Weight this in with parent
         ppa->afSRScoreGroupTriPhone[dwDemi] = WeightParent (paTRIPHONETRAIN, ppa->afSRScoreMegaPhone[dwDemi], ppa->afSRScoreGroupTriPhone[dwDemi]);
      } // dwDemi

      // look through all possible phone combinations and see which ones might produce the
      // best SR results
      lMISMATCHSTRUCTTheory.Init (sizeof(MISMATCHSTRUCT));
      MISMATCHSTRUCT mms, mmsThis;
      PMISMATCHSTRUCT pmms;
      DWORD dwLeft, dwRight;
      DWORD k;
      hAlreadyAdded.Init (sizeof(DWORD)); // hash to make sure only deal with one triphone model
      hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so wont match against self
      PTRIPHONETRAIN paTRIPHONETRAINThis = paTRIPHONETRAIN;
      for (i = 0; i <= TRIPHONEMEGAGROUPMISMATCH; i++) { // intenionally <=
         if (i >= TRIPHONEMEGAGROUPMISMATCH) {
            // use this megagroup as the basis
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mmsThis.afScore[dwDemi] = ppa->afSRScoreMegaPhone[dwDemi];

            DWORD dwTPhoneCompact = ppa->awTriPhone[0];
            DWORD dwLeftThis = ((dwTPhoneCompact & 0xff) % PIS_PHONEGROUPNUM);
            DWORD dwRightThis = (((dwTPhoneCompact & 0xff00) >> 8) % PIS_PHONEGROUPNUM);
            dwLeftThis = LexPhoneGroupToMega (dwLeftThis);
            dwRightThis = LexPhoneGroupToMega (dwRightThis);
            mmsThis.dwValue = (dwRightThis << 8) | dwLeftThis;

            pmms = &mmsThis;
         }
         else {
            pmms = &ppa->aMMSMegaGroup[i];
            if (!pmms->pVoid)
               continue;   // empty
         }

         // figure out possible phonemes on left and right
         lPhoneLeft.Init (sizeof(DWORD));
         lPhoneRight.Init (sizeof(DWORD));
         for (dwRight = 0; dwRight < 2; dwRight++) {
            DWORD dwMegaGroup = dwRight ? (pmms->dwValue >> 8) : (pmms->dwValue & 0xff);
            PCListFixed plTemp = dwRight ? &lPhoneRight : &lPhoneLeft;

            if (pLex->PhonemeToMegaGroup(bSilence) == dwMegaGroup) {
               k = bSilence;
               plTemp->Add (&k);
               continue;
            }

            for (k = 0; k < pLex->PhonemeNum(); k++)
               if (pLex->PhonemeToMegaGroup(k) == dwMegaGroup)
                  plTemp->Add (&k);
         }

         // if either list empty then fail
         if (!lPhoneLeft.Num() || !lPhoneRight.Num())
            continue;

         // loop through all possible phoneme combinations and guestimate a value
         DWORD *padwLeft = (DWORD*)lPhoneLeft.Get(0);
         DWORD *padwRight = (DWORD*)lPhoneRight.Get(0);
         for (dwLeft = 0; dwLeft < lPhoneLeft.Num(); dwLeft++) for (dwRight = 0; dwRight < lPhoneRight.Num(); dwRight++) {
            DWORD dwLeftPhone = padwLeft[dwLeft];
            DWORD dwRightPhone = padwRight[dwRight];

            paTRIPHONETRAIN = GetGroupTRIPHONETRAIN (this, pAnal, ppa, pLex, dwLeftPhone, dwRightPhone);
            if (!paTRIPHONETRAIN)
               continue;

            // make sure not already on the list
            if (hAlreadyAdded.FindIndex (paTRIPHONETRAIN) != (DWORD)-1)
               continue;

            // make sure has training
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               if (!paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
                  break;
            if (dwDemi < TTSDEMIPHONES) {
               hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so save time later
               continue;   // no training
            }
            
            _ASSERTE (paTRIPHONETRAIN->fPitch);
            _ASSERTE (paTRIPHONETRAIN->dwDuration);
            _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

            // store this away
            mms.fCompare = 0;
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mms.fCompare += pmms->afScore[dwDemi] / (fp)TTSDEMIPHONES * MISMATCHPARENTTHEORYWEIGHT;

            // NOTE: not sure if I should add the previously calculated scores to this
            if (pLex->PhonemeToMegaGroup(ppa->bPhoneLeft) != pLex->PhonemeToMegaGroup (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 5);
            else if (pLex->PhonemeToGroup(ppa->bPhoneLeft) != pLex->PhonemeToGroup (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 4);
            else if (pLex->PhonemeToUnstressed(ppa->bPhoneLeft) != pLex->PhonemeToUnstressed (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 3);
            else if ((DWORD)ppa->bPhoneLeft != dwLeftPhone)
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 2);

            if (pLex->PhonemeToMegaGroup(ppa->bPhoneRight) != pLex->PhonemeToMegaGroup (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 5);
            else if (pLex->PhonemeToGroup(ppa->bPhoneRight) != pLex->PhonemeToGroup (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 4);
            else if (pLex->PhonemeToUnstressed(ppa->bPhoneRight) != pLex->PhonemeToUnstressed (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 3);
            else if ((DWORD)ppa->bPhoneRight != dwRightPhone)
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 2);

            // Because doing phoneme group, set dwRightPhone and dwLeftPhone that way
            dwRightPhone = pLex->PhonemeToGroup(dwRightPhone);
            dwLeftPhone = pLex->PhonemeToGroup(dwLeftPhone);
            mms.dwValue = (dwRightPhone << 8) | dwLeftPhone;
            mms.pVoid = paTRIPHONETRAIN;

            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mms.afScoreWeightWith[dwDemi] = pmms->afScore[dwDemi];

            lMISMATCHSTRUCTTheory.Add (&mms);

            // note that have added this
            hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so save time later
         } // dwLeft and dwRight

      } // over all megagroup triphones



      // sort
      if (lMISMATCHSTRUCTTheory.Num() >= 2)
         qsort (lMISMATCHSTRUCTTheory.Get(0), lMISMATCHSTRUCTTheory.Num(), sizeof(MISMATCHSTRUCT), MISMATCHSTRUCTSort);

      // keep only the top 8 of these
      pmms = (PMISMATCHSTRUCT)lMISMATCHSTRUCTTheory.Get(0);
      for (i = 0; i < min(lMISMATCHSTRUCTTheory.Num(), TRIPHONEGROUPMISMATCH); i++, pmms++) {
         paTRIPHONETRAIN = (PTRIPHONETRAIN) pmms->pVoid;

         _ASSERTE (!i || (pmms->pVoid != pmms[-1].pVoid));
         _ASSERTE (paTRIPHONETRAIN->fPitch);
         _ASSERTE (paTRIPHONETRAIN->dwDuration);
         _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            pmms->afScore[dwDemi] = paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
               FALSE /* slow */, FALSE /* all examplars */);
                              // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
   
            pmms->afScore[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this

            pmms->afScore[dwDemi] = WeightParent (paTRIPHONETRAIN, pmms->afScoreWeightWith[dwDemi], pmms->afScore[dwDemi]);
               // BUGFIX - Was paTRIPHONETRAINThis, when should be paTRIPHONETRAIN
         } // dwDemi

         // write this
         ppa->aMMSGroup[i] = *pmms;
      } // over best megagroup mismatches

      // BUGFIX - disable
      //if (!ppa->fSRScoreTriPhone)
      //   continue;
   } // j, voer all phonemes

   // BUGFIX - Since have delta in SR, remove plosive hack
#if 0
   // BUGFIX - Since the SR score for plosives is encouraging blurred plossives,
   // underweight the plosive score
   fp fScoreLast = 0;
   ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // only if plosive
      if (!ppa->fIsPlosive) {
         // store this away for new last
         fScoreLast = ppa->fSRScoreTriPhone;
         continue;
      }

      // left
      BOOL fHaveLeft = j && (ppa[-1].bPhone != bSilence) && (ppa[-1].dwTimeEnd > ppa[-1].dwTimeStart);
      BOOL fHaveRight = (j+1 < pwa->m_lPHONEAN.Num()) && (ppa[1].bPhone != bSilence) && (ppa[1].dwTimeEnd > ppa[1].dwTimeStart);
      DWORD dwWeight = 0;
      fp fAdjusted = 0;
      if (fHaveLeft) {
         dwWeight++;
         fAdjusted += fScoreLast;
      }
      if (fHaveRight) {
         dwWeight++;
         fAdjusted += ppa[1].fSRScoreTriPhone;
      }
      // store this away for new last
      fScoreLast = ppa->fSRScoreTriPhone;
      if (!dwWeight)
         continue;   // no point since nothing around it


      // else, adjust score, weighting phonemes on left/right with a 2:1 ratio
      ppa->fSRScoreTriPhone = (ppa->fSRScoreTriPhone + fAdjusted / (fp)dwWeight * 2.0) / 3.0;
   } // j
#endif // 0

   // release the SR features so don't use too much memory
   pwa->m_pWave->ReleaseSRFeatures();

}



/*************************************************************************************
CTTSWork::AnalysisSpecificTRIPHONETRAINSub - Sub-training of indiviual wave.

inputs
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS engine
   DWORD             dwWave - Wave index
   DWORD             dwThread - 0..MAXRAYTHREAD-1
   DWORD             dwStartPhone - Starting phoneme that's acceptable. Used so can multipass and reduce memory requirements
   DWORD             dwEndPhone - Ending phoneme that's start of unacceptable. Used so can multipass and reduce memory requirements
*/
void CTTSWork::AnalysisSpecificTRIPHONETRAINSub (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWave, DWORD dwThread,
                                         DWORD dwStartPhone, DWORD dwEndPhone)
{
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   DWORD j;
   BYTE bSilence;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());


   DWORD dwDemi;
   PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(dwWave));

   // cache the entire wave since will be accessing it call
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);

#ifndef OLDSR
   //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
   psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
   psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);
   psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

   // in each wave loop through all the phonemes
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   CListFixed lMISMATCHSTRUCTTheory;
   CListFixed lPhoneLeft, lPhoneRight;
   CHashPVOID hAlreadyAdded;
   DWORD i;
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // BUGFIX - make sure in phoneme range
      if ( ((DWORD)ppa->bPhone < dwStartPhone) || ((DWORD)ppa->bPhone >= dwEndPhone))
         continue;   // not part of this pass

      PTRIPHONETRAIN paTRIPHONETRAIN = GetSpecificTRIPHONETRAIN(this, pAnal, ppa, m_dwTriPhoneGroup, pLex);

      // get the pitch
      // BUGFIX - Even use pitch for unvoiced
      //DWORD dwPitchFidelity = ppa->fIsVoiced ?
      //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
      //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
      DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paTRIPHONETRAIN->fPitch /* pAnal->fAvgPitch*/);
#ifdef DONTTRAINDURATION
      DWORD dwDurationFidelity = 0;
#else
      DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paTRIPHONETRAIN->dwDuration);
#endif
      DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paTRIPHONETRAIN->fEnergyMedian);

      _ASSERTE (paTRIPHONETRAIN->fPitch);
      _ASSERTE (paTRIPHONETRAIN->dwDuration);
      _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         // get the memory where the phone goes
         if (!paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
            continue;   // shouldnt happen, but because no training

   #ifdef OLDSR
         // cache the features
         PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
         if (!psrCache)
            continue;

         // calculate the energy for each...
         lEnergy.Clear();
         for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
            fp fEnergy = SRFEATUREEnergy (psrCache + 
               (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
            lEnergy.Add (&fEnergy);
         }

         // train on this
         ppa->fSRScoreTriPhone = paTRIPHONETRAIN->pPhoneme->Compare (
            psrCache + ppa->dwTrimLeft /*pWave->m_paSRFeature + dwPhoneStart*/,
            (fp*)lEnergy.Get(0) + ppa->dwTrimLeft, 
            ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight),
            pwa->m_fMaxEnergy, TRUE, TRUE);
   #else
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);
         ppa->afSRScoreSpecificTriPhone[dwDemi] = paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                           // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
   #endif

         // BUGFIX - Weight tre triphone my the megaphoneunique value
         // so encourage unique-sounding phonemes, not ones that sound like other
         // phonemes in the group
         ppa->afSRScoreSpecificTriPhone[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this

         // BUGFIX - Weight this in with parent
         ppa->afSRScoreSpecificTriPhone[dwDemi] = WeightParent (paTRIPHONETRAIN, ppa->afSRScoreGroupTriPhone[dwDemi], ppa->afSRScoreSpecificTriPhone[dwDemi]);
      } // dwDemi

      // look through all possible phone combinations and see which ones might produce the
      // best SR results
      lMISMATCHSTRUCTTheory.Init (sizeof(MISMATCHSTRUCT));
      MISMATCHSTRUCT mms, mmsThis;
      PMISMATCHSTRUCT pmms;
      DWORD dwLeft, dwRight;
      DWORD k;
      hAlreadyAdded.Init (sizeof(DWORD)); // hash to make sure only deal with one triphone model
      hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so wont match against self
      PTRIPHONETRAIN paTRIPHONETRAINThis = paTRIPHONETRAIN;
      ppa->dwSpecificMismatchAccuracy = m_dwTriPhoneGroup; // BUGFIX - Don't think can do max(1,x) without causing problems: max(m_dwTriPhoneGroup, 1);   // always at least 1
      for (i = 0; i <= TRIPHONEGROUPMISMATCH; i++) { // intenionally <=
         if (i >= TRIPHONEGROUPMISMATCH) {
            // use this megagroup as the basis
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mmsThis.afScore[dwDemi] = ppa->afSRScoreGroupTriPhone[dwDemi];

            DWORD dwTPhoneCompact = ppa->awTriPhone[0];
            DWORD dwLeftThis = (dwTPhoneCompact & 0xff);
            DWORD dwRightThis = ((dwTPhoneCompact & 0xff00) >> 8);
            mmsThis.dwValue = (dwRightThis << 8) | dwLeftThis;

            pmms = &mmsThis;
         }
         else {
            pmms = &ppa->aMMSGroup[i];
            if (!pmms->pVoid)
               continue;   // empty
         }

         // figure out possible phonemes on left and right
         lPhoneLeft.Init (sizeof(DWORD));
         lPhoneRight.Init (sizeof(DWORD));
         for (dwRight = 0; dwRight < 2; dwRight++) {
            DWORD dwGroup = dwRight ? (pmms->dwValue >> 8) : (pmms->dwValue & 0xff);
            PCListFixed plTemp = dwRight ? &lPhoneRight : &lPhoneLeft;

            if (pLex->PhonemeToGroup(bSilence) == dwGroup) {
               k = bSilence;
               plTemp->Add (&k);
               continue;
            }

            for (k = 0; k < pLex->PhonemeNum(); k++)
               if (pLex->PhonemeToGroup(k) == dwGroup)
                  plTemp->Add (&k);
         }

         // if either list empty then fail
         if (!lPhoneLeft.Num() || !lPhoneRight.Num())
            continue;

         // loop through all possible phoneme combinations and guestimate a value
         DWORD *padwLeft = (DWORD*)lPhoneLeft.Get(0);
         DWORD *padwRight = (DWORD*)lPhoneRight.Get(0);
         for (dwLeft = 0; dwLeft < lPhoneLeft.Num(); dwLeft++) for (dwRight = 0; dwRight < lPhoneRight.Num(); dwRight++) {
            DWORD dwLeftPhone = padwLeft[dwLeft];
            DWORD dwRightPhone = padwRight[dwRight];

            paTRIPHONETRAIN = GetSpecificTRIPHONETRAIN (this, pAnal, ppa, m_dwTriPhoneGroup, pLex, dwLeftPhone, dwRightPhone);
            if (!paTRIPHONETRAIN)
               continue;

            // make sure not already on the list
            if (hAlreadyAdded.FindIndex (paTRIPHONETRAIN) != (DWORD)-1)
               continue;

            // make sure has training
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               if (!paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi])
                  break;
            if (dwDemi < TTSDEMIPHONES) {
               hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so save time later
               continue;   // no training
            }
            
            _ASSERTE (paTRIPHONETRAIN->fPitch);
            _ASSERTE (paTRIPHONETRAIN->dwDuration);
            _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

            // store this away
            mms.fCompare = 0;
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mms.fCompare += pmms->afScore[dwDemi] / (fp)TTSDEMIPHONES * MISMATCHPARENTTHEORYWEIGHT;


            // NOTE: not sure if I should add the previously calculated scores to this
            if (pLex->PhonemeToMegaGroup(ppa->bPhoneLeft) != pLex->PhonemeToMegaGroup (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 5);
            else if (pLex->PhonemeToGroup(ppa->bPhoneLeft) != pLex->PhonemeToGroup (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 4);
            else if (pLex->PhonemeToUnstressed(ppa->bPhoneLeft) != pLex->PhonemeToUnstressed (dwLeftPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 3);
            else if ((DWORD)ppa->bPhoneLeft != dwLeftPhone)
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwLeftPhone, pLex, FALSE, 2);

            if (pLex->PhonemeToMegaGroup(ppa->bPhoneRight) != pLex->PhonemeToMegaGroup (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 5);
            else if (pLex->PhonemeToGroup(ppa->bPhoneRight) != pLex->PhonemeToGroup (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 4);
            else if (pLex->PhonemeToUnstressed(ppa->bPhoneRight) != pLex->PhonemeToUnstressed (dwRightPhone))
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 3);
            else if ((DWORD)ppa->bPhoneRight != dwRightPhone)
               mms.fCompare += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, dwRightPhone, pLex, TRUE, 2);

            switch (ppa->dwSpecificMismatchAccuracy) {
               case 0:  // phone groups
                  dwRightPhone = pLex->PhonemeToGroup(dwRightPhone);
                  dwLeftPhone = pLex->PhonemeToGroup(dwLeftPhone);
                  break;
               case 1:  // stressed/unstressed
                  dwRightPhone = pLex->PhonemeToUnstressed(dwRightPhone);
                  dwLeftPhone = pLex->PhonemeToUnstressed(dwLeftPhone);
                  break;
               default:
                  // do nothing already right
                  break;
            } // accurac
            mms.dwValue = (dwRightPhone << 8) | dwLeftPhone;
            mms.pVoid = paTRIPHONETRAIN;

            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               mms.afScoreWeightWith[dwDemi] = pmms->afScore[dwDemi];

            lMISMATCHSTRUCTTheory.Add (&mms);

            // note that have added this
            hAlreadyAdded.Add (paTRIPHONETRAIN, &k);  // so save time later
         } // dwLeft and dwRight

      } // over all megagroup triphones



      // sort
      if (lMISMATCHSTRUCTTheory.Num() >= 2)
         qsort (lMISMATCHSTRUCTTheory.Get(0), lMISMATCHSTRUCTTheory.Num(), sizeof(MISMATCHSTRUCT), MISMATCHSTRUCTSort);

      // keep only the top 8 of these
      pmms = (PMISMATCHSTRUCT)lMISMATCHSTRUCTTheory.Get(0);
      for (i = 0; i < min(lMISMATCHSTRUCTTheory.Num(), TRIPHONESPECIFICMISMATCH); i++, pmms++) {
         paTRIPHONETRAIN = (PTRIPHONETRAIN) pmms->pVoid;

         _ASSERTE (!i || (pmms->pVoid != pmms[-1].pVoid));
         _ASSERTE (paTRIPHONETRAIN->fPitch);
         _ASSERTE (paTRIPHONETRAIN->dwDuration);
         _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            pmms->afScore[dwDemi] = paTRIPHONETRAIN->apPhoneme[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity][dwDemi]->Compare (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE, TRUE /* feature distortion */, TRUE,
               FALSE /* slow */, FALSE /* all examplars */);
                              // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
   
            pmms->afScore[dwDemi] += ppa->afSRScoreMegaPhoneUnique[dwDemi];  // penalize/help by this

            pmms->afScore[dwDemi] = WeightParent (paTRIPHONETRAIN, pmms->afScoreWeightWith[dwDemi], pmms->afScore[dwDemi]);
               // BUGFIX - Was paTRIPHONETRAINThis, when should be paTRIPHONETRAIN
         } // dwDemi

         // BUGFIX - subtract this score since only store delta in final
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            pmms->afScore[dwDemi] -= ppa->afSRScoreSpecificTriPhone[dwDemi];

         // write this
         ppa->aMMSSpecific[i] = *pmms;
      } // over best megagroup mismatches

      // BUGFIX - disable
      //if (!ppa->fSRScoreTriPhone)
      //   continue;
   } // j, voer all phonemes

   // BUGFIX - Since have delta in SR, remove plosive hack
#if 0
   // BUGFIX - Since the SR score for plosives is encouraging blurred plossives,
   // underweight the plosive score
   fp fScoreLast = 0;
   ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      // only if plosive
      if (!ppa->fIsPlosive) {
         // store this away for new last
         fScoreLast = ppa->fSRScoreTriPhone;
         continue;
      }

      // left
      BOOL fHaveLeft = j && (ppa[-1].bPhone != bSilence) && (ppa[-1].dwTimeEnd > ppa[-1].dwTimeStart);
      BOOL fHaveRight = (j+1 < pwa->m_lPHONEAN.Num()) && (ppa[1].bPhone != bSilence) && (ppa[1].dwTimeEnd > ppa[1].dwTimeStart);
      DWORD dwWeight = 0;
      fp fAdjusted = 0;
      if (fHaveLeft) {
         dwWeight++;
         fAdjusted += fScoreLast;
      }
      if (fHaveRight) {
         dwWeight++;
         fAdjusted += ppa[1].fSRScoreTriPhone;
      }
      // store this away for new last
      fScoreLast = ppa->fSRScoreTriPhone;
      if (!dwWeight)
         continue;   // no point since nothing around it


      // else, adjust score, weighting phonemes on left/right with a 2:1 ratio
      ppa->fSRScoreTriPhone = (ppa->fSRScoreTriPhone + fAdjusted / (fp)dwWeight * 2.0) / 3.0;
   } // j
#endif // 0

   // release the SR features so don't use too much memory
   pwa->m_pWave->ReleaseSRFeatures();

}


/*************************************************************************************
CTTSWork::AnalysisGroupTRIPHONETRAINMultiPass - Calls AnalysisTRIPHONETRAIN
in multiple passes to reduce memory usage

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS voice
returns
   BOOL - TRUE if success
*/
#define TPTGROUPMULTIPASSNUM       12           // do two passes to minimize memory
   // BUGFIX - 6 to 12 because not enough memory for large voice
   // BUGFIX - Uppsed from 4 to 6 to try and minimize memory usage
   // BUGFIX - Upped to 4 since added duration in
   // BUGFIX - Upped from 2 to 4 to further minimize memory
   // BUGFIX - Lowered to 3 because with 4 not using as much CPU - tradeoff between memory/CPU
BOOL CTTSWork::AnalysisGroupTRIPHONETRAINMultiPass (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD dwPasses = TPTGROUPMULTIPASSNUM;

   if (pAnal->plPCWaveAn->Num() < 5000)
      dwPasses = max(dwPasses * 2 / 3, 1);

   if (giTotalRAM >= EIGHTGIG)
      dwPasses = max(dwPasses / 2, 1);

   DWORD dwPass;
   BOOL fRet;
   for (dwPass = 0; dwPass < dwPasses; dwPass++) {
      if (pProgress)
         pProgress->Push ((fp)dwPass / (fp)dwPasses, (fp)(dwPass+1) / (fp)dwPasses);
      
      fRet = AnalysisGroupTRIPHONETRAIN (pProgress, pAnal, pTTS,
         dwPass * dwNumPhone / dwPasses,
         (dwPass+1) * dwNumPhone / dwPasses);

      if (pProgress)
         pProgress->Pop();

      if (!fRet)
         return FALSE;
   } // dwPass

   return TRUE;
}

/*************************************************************************************
CTTSWork::AnalysisSpecificTRIPHONETRAINMultiPass - Calls AnalysisTRIPHONETRAIN
in multiple passes to reduce memory usage

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS voice
returns
   BOOL - TRUE if success
*/
#define TPTSPECIFICMULTIPASSNUM       24           // do two passes to minimize memory
   // BUGFIX - Lowered back to 24 and do check for large voices
   // BUGFIX - Upped to 32 again because problem with large voices when 24
   // BUGFIX - Lowered from 32 to 24 because my voice only went above 6 meg once or twice with 32
   // BUGFIX - Upped from 24 to 32 because having some difficulty with too much memory for some triphones
   // BUGFIX - 12 to 24 because not enough memory for large voice
   // BUGFIX - Upped from 8 to 12 to try to minimize memory usage
   // BUGFIX - Upped to 4 since added duration in
   // BUGFIX - Upped from 2 to 4 to further minimize memory
   // BUGFIX - Lowered to 3 because with 4 not using as much CPU - tradeoff between memory/CPU
BOOL CTTSWork::AnalysisSpecificTRIPHONETRAINMultiPass (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   // if this is a large voice with detailed grouping then one pass per phoneme
   DWORD dwPasses = TPTSPECIFICMULTIPASSNUM;
   BOOL fLotsOfData = FALSE;
   if ((pAnal->plPCWaveAn->Num() >= 5000) && (m_dwTriPhoneGroup >= 2)) {
      dwPasses = min(dwPasses*2, (dwNumPhone+1) / 2); // BUGFIX - Doing every phone takes way too long
      fLotsOfData = TRUE;
   }

   if (giTotalRAM >= EIGHTGIG) {
      if (fLotsOfData)
         dwPasses = max(dwPasses * 2 / 3, 1);
      else
         dwPasses = max(dwPasses / 2, 1);
   }

   DWORD dwPass;
   BOOL fRet;
   for (dwPass = 0; dwPass < dwPasses; dwPass++) {
      if (pProgress)
         pProgress->Push ((fp)dwPass / (fp)dwPasses, (fp)(dwPass+1) / (fp)dwPasses);
      
      fRet = AnalysisSpecificTRIPHONETRAIN (pProgress, pAnal, pTTS,
         dwPass * dwNumPhone / dwPasses,
         (dwPass+1) * dwNumPhone / dwPasses);

      if (pProgress)
         pProgress->Pop();

      if (!fRet)
         return FALSE;
   } // dwPass

   return TRUE;
}

/*************************************************************************************
CTTSWork::AnalysisGroupTRIPHONETRAIN - Fills in the triphonetrain structures

NOTE: This uses the triphone groups of PIS_PHONEGROUPNUM x #phones x PIS_PHONEGROUPNUM, to ensure that
there are enough copies of every triphone when calculating the typical model,
along with typical duration, etc.

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS voice
   DWORD             dwStartPhone - Start phoneme number, so can-mutiplass training to reduce memory footprint
   DWORD             dwEndPhone - End phoneme number.
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisGroupTRIPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS,
                                      DWORD dwStartPhone, DWORD dwEndPhone)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k;
   DWORD dwNum;
   DWORD dwDemi;
   CListFixed lUNITRANK;
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   lUNITRANK.Init (sizeof(UNITRANK));
   UNITRANK ur;
   memset (&ur, 0, sizeof(ur));
   DWORD dwGroupLeft, dwGroupRight;
#ifndef NOMODS_COMBINEWORDPOSUNTILTHEEND
   DWORD dwThisWordPos;
   for (dwThisWordPos = 0; dwThisWordPos < 4; dwThisWordPos++)
#endif
   for (i = dwStartPhone; i < dwEndPhone; i++)
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++)
         for (dwGroupRight = 0; dwGroupRight < PIS_PHONEGROUPNUM; dwGroupRight++) {
         lUNITRANK.Clear();

         WORD wTriPhone = (dwGroupRight << 8) | dwGroupLeft;
         PTPHONEINST ptiStart = NULL;

         for (j = 0; j < 4; j++) {
#ifndef NOMODS_COMBINEWORDPOSUNTILTHEEND
            // must match word position
            if (j != dwThisWordPos)
               continue;
#endif
            PCListFixed plLook = pAnal->paplTriPhone[j][i];
            _ASSERTE (!m_fWordStartEndCombine || !j || !plLook || !plLook->Num());
            if (!plLook)
               continue;

            // sort the list by the triphone number(s) to make sure in a nice order
            PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
            dwNum = plLook->Num();
            // qsort (pti, dwNum, sizeof(TPHONEINST), TPHONEINSTSort);

            // now can loop through and group by triphone
            // DWORD dwInGroup = 0;
            // lUNITRANK.Clear();
            for (; dwNum; dwNum--, pti++) {
               if (pti->pPHONEAN->awTriPhone[0] != wTriPhone)
                  continue;
               
               ur.pTPInst = pti;

               // remermber this
               if (!ptiStart)
                  ptiStart = pti;

               // add multiple copies so that overweight non-function words
               DWORD dwAdd;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
               dwAdd = NUMFUNCWORDGROUP+1;
               if (pti && pti->pPHONEAN && pti->pPHONEAN->pWord)
                  dwAdd = pti->pPHONEAN->pWord->dwFuncWordGroup+1;
#else
               dwAdd = 1;
#endif
               for (; dwAdd; dwAdd--)
                  lUNITRANK.Add (&ur);

               // dwInGroup++;
               //dwNum--;
               //pti++;
            } // over dwNum
         } // j
         if (!ptiStart)
            continue;   // not found

         PTRIPHONETRAIN paTRIPHONETRAIN = GetGroupTRIPHONETRAIN (this, pAnal, ptiStart->pPHONEAN, pLex);

         // get the parent category
         PTRIPHONETRAIN pMegaPHONETRAIN = GetMegaPHONETRAIN (this, pAnal, ptiStart->pPHONEAN);

         // know what's in the group, try and find some medians
         DWORD dwAttrib;
         PUNITRANK pur = (PUNITRANK)lUNITRANK.Get(0);
         for (dwAttrib = 0; dwAttrib < 8; dwAttrib++) {
            for (k = 0; k < lUNITRANK.Num(); k++) {

               switch (dwAttrib) {
               case 0: // SR score
                  pur[k].fCompare = 0;
                  for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                     pur[k].fCompare += pur[k].pTPInst->pPHONEAN->afSRScoreMegaPhone[dwDemi] / (fp)TTSDEMIPHONES;
                  // pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fSRScoreMegaPhone;
                  break;

               case 1: // duration
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->dwDuration;
                  break;

               case 2: // energy
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyAvg;
                  break;

               case 3: // ipitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitch;
                  break;

               case 4: // ipitch delta
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchDelta;
                  break;

               case 5: // energy ratio
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyRatio;
                  break;

               case 6: // fPitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fPitch;
                  break;

               case 7: // ipitch bulge
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchBulge;
                  break;
               } // switch
            } // k

            // sort
            qsort (pur, lUNITRANK.Num(), sizeof(UNITRANK), UNITRANKSort);

            // find the median value
            DWORD dwMid = lUNITRANK.Num()/2;
            fp fMid = pur[dwMid].fCompare;

            // write this out
            paTRIPHONETRAIN->dwCountScale = lUNITRANK.Num();
            switch (dwAttrib) {
            case 0: // SR score
               // BUGFIX - allow 3/4 of the phones to pass through, not just 1/2
               // Do this so (hopefully) don't eliminate any bright phonemes at the start
               // paTRIPHONETRAIN->fSRScoreMedian = fMid;
               for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                  paTRIPHONETRAIN->afSRScoreMedian[dwDemi] = pur[lUNITRANK.Num()*3/4].fCompare;
               break;

            case 1: // duration
               paTRIPHONETRAIN->dwDuration = (DWORD)fMid;
               paTRIPHONETRAIN->dwDurationSRFeat = pur[dwMid].pTPInst->pPHONEAN->dwTimeEnd - pur[dwMid].pTPInst->pPHONEAN->dwTimeStart;
               break;

            case 2: // energy
               paTRIPHONETRAIN->fEnergyMedian = fMid;

               // BUGFIX - Try to remember slightly higher than normal energy so favor those
               paTRIPHONETRAIN->fEnergyMedianHigh = pur[lUNITRANK.Num()*3/4].fCompare;

               break;

            case 3: // ipitch
               paTRIPHONETRAIN->iPitch = (int)fMid;
               break;

            case 4: // ipitch delta
               paTRIPHONETRAIN->iPitchDelta = (int)fMid;
               break;

            case 5: // energy ratio
               paTRIPHONETRAIN->fEnergyRatio = fMid;
               break;

            case 6: // pitch
               paTRIPHONETRAIN->fPitch = fMid;
               break;

            case 7: // ipitch bulge
               paTRIPHONETRAIN->iPitchBulge = (int)fMid;
               break;
            } // switch
         } // dwAttrab

         // adjust to weight by the parent category
         paTRIPHONETRAIN->dwDuration = (DWORD) WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->dwDuration, paTRIPHONETRAIN->dwDuration);
         paTRIPHONETRAIN->dwDurationSRFeat = (DWORD) WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->dwDurationSRFeat, paTRIPHONETRAIN->dwDurationSRFeat);
         paTRIPHONETRAIN->iPitch = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitch, paTRIPHONETRAIN->iPitch);
         paTRIPHONETRAIN->iPitchDelta = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitchDelta, paTRIPHONETRAIN->iPitchDelta);
         paTRIPHONETRAIN->iPitchBulge = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitchBulge, paTRIPHONETRAIN->iPitchBulge);
         paTRIPHONETRAIN->fEnergyMedian = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyMedian, paTRIPHONETRAIN->fEnergyMedian);
         paTRIPHONETRAIN->fEnergyMedianHigh = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyMedianHigh, paTRIPHONETRAIN->fEnergyMedianHigh);
         paTRIPHONETRAIN->fEnergyRatio = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyRatio, paTRIPHONETRAIN->fEnergyRatio);
         paTRIPHONETRAIN->fPitch = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fPitch, paTRIPHONETRAIN->fPitch);

#if 0 // replaced by cleaner code
         DWORD dwCount = paTRIPHONETRAIN->dwCountScale / (NUMFUNCWORDGROUP+1); // counteract weight
         dwCount = max(dwCount, 1);
         paTRIPHONETRAIN->dwDuration = (paTRIPHONETRAIN->dwDuration * dwCount +
            pMegaPHONETRAIN->dwDuration * PARENTCATEGORYWEIGHT + (dwCount+PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paTRIPHONETRAIN->dwDurationSRFeat = (paTRIPHONETRAIN->dwDurationSRFeat * dwCount +
            pMegaPHONETRAIN->dwDurationSRFeat * PARENTCATEGORYWEIGHT + (dwCount + PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paTRIPHONETRAIN->fEnergyMedian = (paTRIPHONETRAIN->fEnergyMedian * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyMedian * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fEnergyMedianHigh = (paTRIPHONETRAIN->fEnergyMedianHigh * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyMedianHigh * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitch = (paTRIPHONETRAIN->iPitch * (int) dwCount +
            pMegaPHONETRAIN->iPitch * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitchDelta = (paTRIPHONETRAIN->iPitchDelta * (int) dwCount +
            pMegaPHONETRAIN->iPitchDelta * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitchBulge = (paTRIPHONETRAIN->iPitchBulge * (int) dwCount +
            pMegaPHONETRAIN->iPitchBulge * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fEnergyRatio = (paTRIPHONETRAIN->fEnergyRatio * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyRatio * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fPitch = (paTRIPHONETRAIN->fPitch * (fp) dwCount +
            pMegaPHONETRAIN->fPitch * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
#endif  // 0
   } // i, dwGroupLeft,dwGroupRight

   // loop through all phonemes and make a model from the phonemes of the same
   // type so get a better "average" phoneme
   BYTE bSilence;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
   pProgress->Push (0, 0.5);
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);

#ifndef OLDSR
      //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
      psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
      psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);
      psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // make sure OK for the pass
         if ( ((DWORD)ppa->bPhone < dwStartPhone) || ((DWORD)ppa->bPhone >= dwEndPhone))
            continue;

         // get the memory where the phone goes
         PTRIPHONETRAIN paTRIPHONETRAIN = GetGroupTRIPHONETRAIN(this, pAnal, ppa, pLex);

         // if this SR error > the median error then underweight
         fp fWeight = 1.0;

#ifdef NOMODS_MISCSCOREHACKS
         fp fScoreSum = 0, fMegaScoreSum = 0;
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            fScoreSum += ppa->afSRScoreMegaPhone[dwDemi];
            fMegaScoreSum += paTRIPHONETRAIN->afSRScoreMedian[dwDemi];
         }
         if (fScoreSum > fMegaScoreSum)
            fWeight = 0.25; // BUGFIX - Was a straght continue, but instead, underweight
#endif

         // BUGFIX - weight louder units more than quieter ones since
         // they're more likely to have bright formants
         // BUGFIX - Don't do this anymore since choosing too loud, and now have better approach with multiple train bins
         // fWeight *= ppa->fEnergyAvg / pwa->m_fAvgEneryPerPhone;

#ifdef NOMODS_MISCSCOREHACKS
         // BUGFIX - If voiced, then include the strength of the pitch detect
         if (ppa->fIsVoiced)
            fWeight *= ppa->fPitchStrength;
#endif

         // over all pitch
         DWORD dwPF, dwDF, dwEF;
         for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++)  for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            // if there isn't any phoneme training here then create
            if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
               paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = new CPhoneme;
               if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
                  continue;   // error
               paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->HalfSize (TRUE);
            }

            // train more distant pitches with this too, but with lower weights
            // BUGFIX - Even use pitch for unvoiced
            //DWORD dwPitchFidelity = ppa->fIsVoiced ?
            //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
            //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
            DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paTRIPHONETRAIN->fPitch /* pAnal->fAvgPitch */);
#ifdef DONTTRAINDURATION
            DWORD dwDurationFidelity = 0;
#else
            DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paTRIPHONETRAIN->dwDuration);
#endif
            DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paTRIPHONETRAIN->fEnergyMedian);

            _ASSERTE (paTRIPHONETRAIN->fPitch);
            _ASSERTE (paTRIPHONETRAIN->dwDuration);
            _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

            fp fWeightPitchFidelity = SRTRAINWEIGHTSCALE *
               TrainWeight (dwPitchFidelity, dwPF, dwDurationFidelity, dwDF, dwEnergyFidelity, dwEF);

#ifdef OLDSR
            // cache the features
            PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
            if (!psrCache)
               continue;

            // calculate the energy for each...
            lEnergy.Clear();
            for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
               fp fEnergy = SRFEATUREEnergy (psrCache + 
                  (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
               lEnergy.Add (&fEnergy);
            }

            // train on this
            if (!paTRIPHONETRAIN->pPhoneme->Train (
               psrCache + ppa->dwTrimLeft/*pat[i].pWave->m_paSRFeature+pat[i].dwStart*/,
               (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
               ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight),
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex))
               continue;
#else
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               (ppa->pWord ? ppa->pWord->fFuncWordWeight : 1.0) * fWeight * fWeightPitchFidelity ))
               continue;
#endif
         } // dwPF
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i
   pProgress->Pop ();



   // loop through all the phonemes and make sure safe for multithreded
   PTRIPHONETRAIN paTRIPHONETRAIN = (PTRIPHONETRAIN) pAnal->paGroupTRIPHONETRAIN;
   DWORD dwPF, dwDF, dwEF;
   for (i = 0; i < 4 * pAnal->dwPhonemes * PHONEGROUPSQUARE; i++, paTRIPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->PrepForMultiThreaded();

   // go through all the phonemes and see what kind of accuracy they have
   pProgress->Push (0.5, 1);
   EMTCANALYSISPHONETRAIN emapt;
   memset (&emapt, 0, sizeof(emapt));
   emapt.dwType = 40;
   emapt.pAnal = pAnal;
   emapt.pTTS = pTTS;
   emapt.dwStartPhone = dwStartPhone;
   emapt.dwEndPhone = dwEndPhone;
   ThreadLoop (0, pAnal->plPCWaveAn->Num(), 16, &emapt, sizeof(emapt), pProgress);
   pProgress->Pop();

   // now that done with training, free all the phoneme training information since wont
   // need it anymore
   paTRIPHONETRAIN = (PTRIPHONETRAIN) pAnal->paGroupTRIPHONETRAIN;
   for (i = 0; i < 4 * pAnal->dwPhonemes * PHONEGROUPSQUARE; i++, paTRIPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
            delete paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
            paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = NULL;
         }

   return TRUE;

}



/*************************************************************************************
CTTSWork::AnalysisSpecificTRIPHONETRAIN - Fills in the triphonetrain structures

NOTE: This uses the triphone groups of PIS_PHONEGROUPNUM x #phones x PIS_PHONEGROUPNUM, to ensure that
there are enough copies of every triphone when calculating the typical model,
along with typical duration, etc.

inputs
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PCMTTS            pTTS - TTS voice
   DWORD             dwStartPhone - Start phoneme number, so can-mutiplass training to reduce memory footprint
   DWORD             dwEndPhone - End phoneme number.
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisSpecificTRIPHONETRAIN (PCProgressSocket pProgress, PTTSANAL pAnal, PCMTTS pTTS,
                                      DWORD dwStartPhone, DWORD dwEndPhone)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k;
   DWORD dwNum;
   DWORD dwDemi;
   CListFixed lUNITRANK;
#ifdef OLDSR
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));
#else
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
#endif
   lUNITRANK.Init (sizeof(UNITRANK));
   UNITRANK ur;
   memset (&ur, 0, sizeof(ur));
   DWORD dwTriPhoneGroup = m_dwTriPhoneGroup; // BUGFIX - Don't think can do max(x, 1) safely; // at least 1
   for (i = dwStartPhone; i < dwEndPhone; i++) for (j = 0; j < 4; j++) {   // specific triphones always care about word position
      PCListFixed plLook = pAnal->paplTriPhone[j][i];
      _ASSERTE (!m_fWordStartEndCombine || !j || !plLook || !plLook->Num());
      if (!plLook)
         continue;

      // sort the list by the triphone number(s) to make sure in a nice order
      PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
      dwNum = plLook->Num();
      qsort (pti, dwNum, sizeof(TPHONEINST), TPHONEINSTSort);

      // now can loop through and group by triphone
      while (dwNum) {
         PTPHONEINST ptiStart = pti;
         DWORD dwInGroup = 0;
         lUNITRANK.Clear();
         while (dwNum && (pti->pPHONEAN->awTriPhone[dwTriPhoneGroup] == ptiStart->pPHONEAN->awTriPhone[dwTriPhoneGroup])) {
            ur.pTPInst = pti;

            // add multiple copies so that overweight non-function words
            DWORD dwAdd;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
            dwAdd = NUMFUNCWORDGROUP+1;
            if (pti && pti->pPHONEAN && pti->pPHONEAN->pWord)
               dwAdd = pti->pPHONEAN->pWord->dwFuncWordGroup+1;
#else
            dwAdd = 1;
#endif
            for (; dwAdd; dwAdd--)
               lUNITRANK.Add (&ur);

            dwInGroup++;
            dwNum--;
            pti++;
         }

         PTRIPHONETRAIN paTRIPHONETRAIN = GetSpecificTRIPHONETRAIN (this, pAnal, ptiStart->pPHONEAN, m_dwTriPhoneGroup, pLex);

         // get the parent category
         PTRIPHONETRAIN pMegaPHONETRAIN = GetGroupTRIPHONETRAIN (this, pAnal, ptiStart->pPHONEAN, pLex);

         // know what's in the group, try and find some medians
         DWORD dwAttrib;
         PUNITRANK pur = (PUNITRANK)lUNITRANK.Get(0);
         for (dwAttrib = 0; dwAttrib < 8; dwAttrib++) {
            for (k = 0; k < lUNITRANK.Num(); k++) {

               switch (dwAttrib) {
               case 0: // SR score
                  pur[k].fCompare = 0;
                  for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                     pur[k].fCompare += pur[k].pTPInst->pPHONEAN->afSRScoreGroupTriPhone[dwDemi] / (fp)TTSDEMIPHONES;
                  // pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fSRScoreMegaPhone;
                  break;

               case 1: // duration
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->dwDuration;
                  break;

               case 2: // energy
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyAvg;
                  break;

               case 3: // ipitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitch;
                  break;

               case 4: // ipitch delta
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchDelta;
                  break;

               case 5: // energy ratio
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fEnergyRatio;
                  break;

               case 6: // fPitch
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->fPitch;
                  break;

               case 7: // ipitch bulge
                  pur[k].fCompare = pur[k].pTPInst->pPHONEAN->iPitchBulge;
                  break;
               } // switch
            } // k

            // sort
            qsort (pur, lUNITRANK.Num(), sizeof(UNITRANK), UNITRANKSort);

            // find the median value
            DWORD dwMid = lUNITRANK.Num()/2;
            fp fMid = pur[dwMid].fCompare;

            // write this out
            paTRIPHONETRAIN->dwCountScale = lUNITRANK.Num();
            switch (dwAttrib) {
            case 0: // SR score
               // BUGFIX - allow 3/4 of the phones to pass through, not just 1/2
               // Do this so (hopefully) don't eliminate any bright phonemes at the start
               // paTRIPHONETRAIN->fSRScoreMedian = fMid;
               for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                  paTRIPHONETRAIN->afSRScoreMedian[dwDemi] = pur[lUNITRANK.Num()*3/4].fCompare;
               break;

            case 1: // duration
               paTRIPHONETRAIN->dwDuration = (DWORD)fMid;
               paTRIPHONETRAIN->dwDurationSRFeat = pur[dwMid].pTPInst->pPHONEAN->dwTimeEnd - pur[dwMid].pTPInst->pPHONEAN->dwTimeStart;
               break;

            case 2: // energy
               paTRIPHONETRAIN->fEnergyMedian = fMid;

               // BUGFIX - Try to remember slightly higher than normal energy so favor those
               paTRIPHONETRAIN->fEnergyMedianHigh = pur[lUNITRANK.Num()*3/4].fCompare;

               break;

            case 3: // ipitch
               paTRIPHONETRAIN->iPitch = (int)fMid;
               break;

            case 4: // ipitch delta
               paTRIPHONETRAIN->iPitchDelta = (int)fMid;
               break;

            case 5: // energy ratio
               paTRIPHONETRAIN->fEnergyRatio = fMid;
               break;

            case 6: // pitch
               paTRIPHONETRAIN->fPitch = fMid;
               break;

            case 7: // ipitch bulge
               paTRIPHONETRAIN->iPitchBulge = (int)fMid;
               break;
            } // switch
         } // dwAttrab

         // adjust to weight by the parent category
         paTRIPHONETRAIN->dwDuration = (DWORD) WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->dwDuration, paTRIPHONETRAIN->dwDuration);
         paTRIPHONETRAIN->dwDurationSRFeat = (DWORD) WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->dwDurationSRFeat, paTRIPHONETRAIN->dwDurationSRFeat);
         paTRIPHONETRAIN->iPitch = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitch, paTRIPHONETRAIN->iPitch);
         paTRIPHONETRAIN->iPitchDelta = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitchDelta, paTRIPHONETRAIN->iPitchDelta);
         paTRIPHONETRAIN->iPitchBulge = WeightParentInt (paTRIPHONETRAIN, pMegaPHONETRAIN->iPitchBulge, paTRIPHONETRAIN->iPitchBulge);
         paTRIPHONETRAIN->fEnergyMedian = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyMedian, paTRIPHONETRAIN->fEnergyMedian);
         paTRIPHONETRAIN->fEnergyMedianHigh = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyMedianHigh, paTRIPHONETRAIN->fEnergyMedianHigh);
         paTRIPHONETRAIN->fEnergyRatio = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fEnergyRatio, paTRIPHONETRAIN->fEnergyRatio);
         paTRIPHONETRAIN->fPitch = WeightParent (paTRIPHONETRAIN, pMegaPHONETRAIN->fPitch, paTRIPHONETRAIN->fPitch);

#if 0 // replaced by cleaner code
         DWORD dwCount = paTRIPHONETRAIN->dwCountScale / (NUMFUNCWORDGROUP+1); // counteract weight
         dwCount = max(dwCount, 1);
         paTRIPHONETRAIN->dwDuration = (paTRIPHONETRAIN->dwDuration * dwCount +
            pMegaPHONETRAIN->dwDuration * PARENTCATEGORYWEIGHT + (dwCount+PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paTRIPHONETRAIN->dwDurationSRFeat = (paTRIPHONETRAIN->dwDurationSRFeat * dwCount +
            pMegaPHONETRAIN->dwDurationSRFeat * PARENTCATEGORYWEIGHT + (dwCount + PARENTCATEGORYWEIGHT)/2) /
            (dwCount + PARENTCATEGORYWEIGHT);
               // BUGFIX - Make sure to round
         paTRIPHONETRAIN->fEnergyMedian = (paTRIPHONETRAIN->fEnergyMedian * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyMedian * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fEnergyMedianHigh = (paTRIPHONETRAIN->fEnergyMedianHigh * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyMedianHigh * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitch = (paTRIPHONETRAIN->iPitch * (int) dwCount +
            pMegaPHONETRAIN->iPitch * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitchDelta = (paTRIPHONETRAIN->iPitchDelta * (int) dwCount +
            pMegaPHONETRAIN->iPitchDelta * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->iPitchBulge = (paTRIPHONETRAIN->iPitchBulge * (int) dwCount +
            pMegaPHONETRAIN->iPitchBulge * (int)PARENTCATEGORYWEIGHT) /
            ((int)dwCount + (int)PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fEnergyRatio = (paTRIPHONETRAIN->fEnergyRatio * (fp) dwCount +
            pMegaPHONETRAIN->fEnergyRatio * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
         paTRIPHONETRAIN->fPitch = (paTRIPHONETRAIN->fPitch * (fp) dwCount +
            pMegaPHONETRAIN->fPitch * (fp)PARENTCATEGORYWEIGHT) /
            (fp)(dwCount + PARENTCATEGORYWEIGHT);
#endif // 0
      } // while TRUE, going through groups
   } // i, j

   // loop through all phonemes and make a model from the phonemes of the same
   // type so get a better "average" phoneme
   BYTE bSilence;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
   pProgress->Push (0, 0.5);
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);

#ifndef OLDSR
      //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
      psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
      psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);
      psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // make sure OK for the pass
         if ( ((DWORD)ppa->bPhone < dwStartPhone) || ((DWORD)ppa->bPhone >= dwEndPhone))
            continue;

         // get the memory where the phone goes
         PTRIPHONETRAIN paTRIPHONETRAIN = GetSpecificTRIPHONETRAIN(this, pAnal, ppa, m_dwTriPhoneGroup, pLex);

         // if this SR error > the median error then underweight
         fp fWeight = 1.0;

#ifdef NOMODS_MISCSCOREHACKS
         fp fScoreSum = 0, fMegaScoreSum = 0;
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            fScoreSum += ppa->afSRScoreGroupTriPhone[dwDemi];
            fMegaScoreSum += paTRIPHONETRAIN->afSRScoreMedian[dwDemi];
         }
         if (fScoreSum > fMegaScoreSum)
            fWeight = 0.25; // BUGFIX - Was a straght continue, but instead, underweight
#endif

         // BUGFIX - weight louder units more than quieter ones since
         // they're more likely to have bright formants
         // BUGFIX - Don't do this anymore since choosing too loud, and now have better approach with multiple train bins
         // fWeight *= ppa->fEnergyAvg / pwa->m_fAvgEneryPerPhone;

#ifdef NOMODS_MISCSCOREHACKS
         // BUGFIX - If voiced, then include the strength of the pitch detect
         if (ppa->fIsVoiced)
            fWeight *= ppa->fPitchStrength;
#endif

         // over all pitch
         DWORD dwPF, dwDF, dwEF;
         for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++)  for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            // if there isn't any phoneme training here then create
            if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
               paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = new CPhoneme;
               if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
                  continue;   // error
               paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->HalfSize (TRUE);
            }

            // train more distant pitches with this too, but with lower weights
            // BUGFIX - Even use pitch for unvoiced
            //DWORD dwPitchFidelity = ppa->fIsVoiced ?
            //   PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch) :
            //   PITCHFIDELITYCENTER; // unvoiced are in the cetner      // get the memory where the phone goes
            DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, paTRIPHONETRAIN->fPitch /* pAnal->fAvgPitch */);
#ifdef DONTTRAINDURATION
            DWORD dwDurationFidelity = 0;
#else
            DWORD dwDurationFidelity = DurationToFidelity (ppa->dwDuration, paTRIPHONETRAIN->dwDuration);
#endif
            DWORD dwEnergyFidelity = EnergyToFidelity (ppa->fEnergyAvg, paTRIPHONETRAIN->fEnergyMedian);

            _ASSERTE (paTRIPHONETRAIN->fPitch);
            _ASSERTE (paTRIPHONETRAIN->dwDuration);
            _ASSERTE (paTRIPHONETRAIN->fEnergyMedian);

            fp fWeightPitchFidelity = SRTRAINWEIGHTSCALE *
               TrainWeight (dwPitchFidelity, dwPF, dwDurationFidelity, dwDF, dwEnergyFidelity, dwEF);

#ifdef OLDSR
            // cache the features
            PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsVoiced, pTTS);
            if (!psrCache)
               continue;

            // calculate the energy for each...
            lEnergy.Clear();
            for (k = ppa->dwTimeStart; k < ppa->dwTimeEnd; k++) {
               fp fEnergy = SRFEATUREEnergy (psrCache + 
                  (k - ppa->dwTimeStart)/*pat[i].pWave->m_paSRFeature + k*/);
               lEnergy.Add (&fEnergy);
            }

            // train on this
            if (!paTRIPHONETRAIN->pPhoneme->Train (
               psrCache + ppa->dwTrimLeft/*pat[i].pWave->m_paSRFeature+pat[i].dwStart*/,
               (fp*)lEnergy.Get(0) + ppa->dwTrimLeft,
               ppa->dwTimeEnd - ppa->dwTimeStart - (ppa->dwTrimLeft + ppa->dwTrimRight),
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex))
               continue;
#else
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, dwDemi, &dwStart, &dwLength);

            if (!paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               (ppa->pWord ? ppa->pWord->fFuncWordWeight : 1.0) * fWeight * fWeightPitchFidelity ))
               continue;
#endif
         } // dwPF
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i
   pProgress->Pop ();



   // loop through all the phonemes and make sure safe for multithreded
   PTRIPHONETRAIN paTRIPHONETRAIN = (PTRIPHONETRAIN) pAnal->paSpecificTRIPHONETRAIN;
   DWORD dwPF, dwDF, dwEF;
   for (i = 0; i < 4 * pAnal->dwPhonemes * (pAnal->dwPhonemes+1) * (pAnal->dwPhonemes+1); i++, paTRIPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]->PrepForMultiThreaded();

   // go through all the phonemes and see what kind of accuracy they have
   pProgress->Push (0.5, 1);
   EMTCANALYSISPHONETRAIN emapt;
   memset (&emapt, 0, sizeof(emapt));
   emapt.dwType = 45;
   emapt.pAnal = pAnal;
   emapt.pTTS = pTTS;
   emapt.dwStartPhone = dwStartPhone;
   emapt.dwEndPhone = dwEndPhone;
   ThreadLoop (0, pAnal->plPCWaveAn->Num(), 16, &emapt, sizeof(emapt), pProgress);
   pProgress->Pop();

   // now that done with training, free all the phoneme training information since wont
   // need it anymore
   paTRIPHONETRAIN = (PTRIPHONETRAIN) pAnal->paSpecificTRIPHONETRAIN;
   for (i = 0; i < 4 * pAnal->dwPhonemes * (pAnal->dwPhonemes+1) * (pAnal->dwPhonemes+1); i++, paTRIPHONETRAIN++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi]) {
            delete paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
            paTRIPHONETRAIN->apPhoneme[dwPF][dwDF][dwEF][dwDemi] = NULL;
         }

   return TRUE;

}


/*************************************************************************************
CTTSWork::AnalysisFree - Frees all the memory allocated into a PTTSANAL structure

inputs
   PTTSANAL          pAnal - Fill this with analysis information
*/
void CTTSWork::AnalysisFree (PTTSANAL pAnal)
{
   // free up all triphones
   DWORD i,j;
   for (i = 0; i < 4; i++) for (j = 0; j < PHONEGROUPSQUARE; j++) {
      _ASSERTE (!m_fWordStartEndCombine || !i || !pAnal->paplTriPhone[i][j] || !pAnal->paplTriPhone[i][j]->Num());
      if (pAnal->paplTriPhone[i][j])
         delete pAnal->paplTriPhone[i][j];
   }

   // free up all the words
   if (pAnal->paplWords) {
      for (i = 0; i < m_pLexWords->WordNum(); i++)
         if (pAnal->paplWords[i])
            delete pAnal->paplWords[i];
   }

   // free up all the emphasis lists
   if (pAnal->paplWEMPH) {
      for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++)
         if (pAnal->paplWEMPH[i])
            delete pAnal->paplWEMPH[i];
   }

   // free up all the waves
   if (pAnal->plPCWaveAn) {
      PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(0);
      for (i = 0; i < pAnal->plPCWaveAn->Num(); i++)
         delete ppwa[i];
      delete pAnal->plPCWaveAn;
   }

   if (pAnal->plTRIPHONETRAIN)
      delete pAnal->plTRIPHONETRAIN;

   if (pAnal->pOrigSR)
      delete pAnal->pOrigSR;

   // free up all the triphone information
   DWORD dwPF, dwDF, dwEF;
   DWORD dwDemi;
   PTRIPHONETRAIN ptpt = (PTRIPHONETRAIN) pAnal->paGroupTRIPHONETRAIN;
   for (i = 0; i < 4 * pAnal->dwPhonemes * PHONEGROUPSQUARE; i++, ptpt++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            delete ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
   ptpt = (PTRIPHONETRAIN) pAnal->paSpecificTRIPHONETRAIN;
   for (i = 0; i < 4 * pAnal->dwPhonemes * (pAnal->dwPhonemes+1) * (pAnal->dwPhonemes+1); i++, ptpt++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            delete ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
   ptpt = (PTRIPHONETRAIN) pAnal->paPHONETRAIN;
   for (i = 0; i < /* BUGFIX - ignore start/end word 4 * */ pAnal->dwPhonemes; i++, ptpt++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            delete ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi];
   ptpt = (PTRIPHONETRAIN) pAnal->paMegaPHONETRAIN;
   for (i = 0; i < pAnal->dwPhonemes * PHONEMEGAGROUPSQUARE; i++, ptpt++)
      for (dwPF = 0; dwPF < PITCHFIDELITY; dwPF++) for (dwDF = 0; dwDF < DURATIONFIDELITYHACKSMALL; dwDF++) for (dwEF = 0; dwEF < ENERGYFIDELITY; dwEF++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         if (ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi])
            delete ptpt->apPhoneme[dwPF][dwDF][dwEF][dwDemi];

   memset (pAnal, 0, sizeof(*pAnal));
}


#if 0 // dead code - no longer used
/*************************************************************************************
CTTSWork::AnalysisTrainPhone - This looks in the analysis information and trains
a phone instance using the sum of all phonemes that match the requirements.

inputs
   PTTANAL        pAnal - Analysis information
   DWORD          dwPhone - Phoneme to train
   WORD           wWordPos - Word position, 0 for center, 1 for start of word, 2 for end of word, 3 for start&end
   WORD           wTriPhone - Tri-phone ID
   DWORD          dwWord - The word index (m_pLexWords->WordNum()) to limit training to,
                  or use -1 for all words
returns
   PCPhoneme - Phoneme instance information
*/
// APHONET - Information about phone to train
typedef struct {
   PCM3DWave      pWave;         // wave it's in
   fp             fMaxEnergy;    // max energy in the wave it's in
   DWORD          dwStart;       // start time (in SRFEATURE units)
   DWORD          dwEnd;         // end time (in SRFEATURE units)
   fp             fSRScore;      // SR score using main model...
} APHONET, *PAPHONET;

static int _cdecl APHONETSort (const void *elem1, const void *elem2)
{
   APHONET *pdw1, *pdw2;
   pdw1 = (APHONET*) elem1;
   pdw2 = (APHONET*) elem2;

   if (pdw1->fSRScore < pdw2->fSRScore)
      return -1;
   else if (pdw1->fSRScore > pdw2->fSRScore)
      return 1;
   else
      return 0;
}


PCPhoneme CTTSWork::AnalysisTrainPhone (PTTSANAL pAnal, DWORD dwPhone, WORD wWordPos,
                                        WORD wTriPhone, DWORD dwWord)
{
   if (m_fWordStartEndCombine)
      wWordPos = 0;

   PCListFixed plLook = pAnal->paplTriPhone[m_fWordStartEndCombine ? 0 : wWordPos][dwPhone];
   if (!plLook)
      return NULL;   // definite no words

   PCMLexicon pLex = Lexicon();

   PCPhoneme pPhone = new CPhoneme;
   if (!pPhone)
      return NULL;

   // train
   DWORD i, j;
   PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
   CListFixed lEnergy, lPhone;
   lEnergy.Init (sizeof(fp));
   lPhone.Init (sizeof(APHONET));
   for (i = 0; i < plLook->Num(); i++, pti++) {
      // skip
      if (pti->wTriPhone != wTriPhone)
         continue;
      if ((dwWord != -1) && (pti->pPHONEAN->dwWord != dwWord))
         continue;

      // wave
      PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(pti->dwWave);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      if (!pwa)
         continue;
      PCM3DWave pWave = pwa->m_pWave;
      if (!pWave)
         continue;

      // get the start and end time
      PWVPHONEME pwp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(pti->dwPhoneIndex);
      DWORD dwStart = pwp->dwSample;
      DWORD dwEnd = (pti->dwPhoneIndex+1 < pWave->m_lWVPHONEME.Num()) ? pwp[1].dwSample : pWave->m_dwSamples;
      if (!pWave->m_dwSRSkip)
         pWave->m_dwSRSkip = pWave->m_dwSamplesPerSec / pWave->m_dwSRSAMPLESPERSEC;
      dwStart = (dwStart + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
      dwEnd = (dwEnd + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
      dwStart = min(dwStart, pWave->m_dwSRSamples);
      dwEnd = min(dwEnd, pWave->m_dwSRSamples);
      if (dwEnd <= dwStart)
         continue;   // 0-length

      // cache the features
      PSRFEATURE psrCache = CacheSRFeatures (pWave, dwStart, dwEnd); not used, but would have to counteract pitch
      if (!psrCache)
         continue;

      // calculate the energy for each...
      lEnergy.Clear();
      for (j = dwStart; j < dwEnd; j++) {
         fp fEnergy = SRFEATUREEnergy (psrCache + (j-dwStart)/*pWave->m_paSRFeature + j*/);
         lEnergy.Add (&fEnergy);
      }

      // left and right and center phone unstressed
      PWVPHONEME apwp[3];
      DWORD adwPhone[3];
      apwp[0] = pti->dwPhoneIndex ? &pwp[-1] : NULL;
      apwp[1] = pwp;
      apwp[2] = (pti->dwPhoneIndex+1 < pWave->m_lWVPHONEME.Num()) ? &pwp[1] : NULL;
      DWORD dwSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
      for (j = 0; j < 3; j++) {
         WCHAR szTemp[16];
         memset (szTemp, 0, sizeof(szTemp));
         if (apwp[j])
            memcpy (szTemp, apwp[j]->awcName, sizeof(apwp[j]->awcName));
         adwPhone[j] = pLex->PhonemeFindUnsort(szTemp);
         if (adwPhone[j] == -1)
            adwPhone[j] = dwSilence;
         PLEXPHONE plp = pLex->PhonemeGetUnsort(adwPhone[j]);
         if (!plp)
            adwPhone[j] = dwSilence;
      }

      PCPhoneme pPhoneOrig = NULL;
      if (pAnal->pOrigSR)
         pPhoneOrig = pAnal->pOrigSR->PhonemeGet ((pLex->PhonemeGetUnsort(adwPhone[1]))->szPhone);


      // keep
      APHONET at;
      memset (&at, 0, sizeof(at));
      at.dwEnd = dwEnd;
      at.dwStart = dwStart;
      at.fMaxEnergy = pwa->m_fMaxEnergy;
      at.pWave = pWave;
      at.fSRScore = 0;
      if (pPhoneOrig) {
         // cache the features
         PSRFEATURE psrCache = CacheSRFeatures (at.pWave, at.dwStart, at.dwEnd); not used but would have to counteract pitch
         if (psrCache)
            at.fSRScore = pPhoneOrig->Compare (psrCache bad code /*at.pWave->m_paSRFeature + at.dwStart*/,
               (fp*)lEnergy.Get(0), at.dwEnd - at.dwStart, at.fMaxEnergy,
               adwPhone[0] == dwSilence, adwPhone[2] == dwSilence);
      }
      lPhone.Add (&at);
   } // i

   // get phones
   PAPHONET pat = (PAPHONET) lPhone.Get(0);
   DWORD dwCount = lPhone.Num();
   if (!dwCount)
      return FALSE;
   qsort (pat, dwCount, sizeof(APHONET), APHONETSort);

   // BUGFIX -train on only the best 50% so that avoid the more wishy-washy phones
   if ((dwCount >= 2) && pAnal->pOrigSR)
      dwCount = (dwCount+1)/2;
   for (i = 0; i < dwCount; i++) {
      // cache the features
      PSRFEATURE psrCache = CacheSRFeatures (pat[i].pWave, pat[i].dwStart, pat[i].dwEnd); not used but would have to counteract pitch
      if (!psrCache)
         continue;

      // calculate the energy for each...
      lEnergy.Clear();
      for (j = pat[i].dwStart; j < pat[i].dwEnd; j++) {
         fp fEnergy = SRFEATUREEnergy (psrCache + 
            (j - pat[i].dwStart)/*pat[i].pWave->m_paSRFeature + j*/);
         lEnergy.Add (&fEnergy);
      }

      // train on this
      if (!pPhone->Train (psrCache bad code/*pat[i].pWave->m_paSRFeature+pat[i].dwStart*/, (fp*)lEnergy.Get(0),
         pat[i].dwEnd - pat[i].dwStart, pat[i].fMaxEnergy,
         (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex))
         continue;
   } // i

   return pPhone;

}
#endif // 0



/****************************************************************************
TTSReviewPageGetSel - Get the currently selected file.

inputs
   PCEscPage      pPage - Page
returns
   PWSTR - string, or NULL if cant find
*/
static PWSTR TTSReviewPageGetSel (PCEscPage pPage)
{
   PWSTR pszSelRec = NULL;
   PCEscControl pControl;
   pControl = pPage->ControlFind (L"rec");
   if (pControl) {
      ESCMLISTBOXGETITEM gi;
      memset (&gi, 0, sizeof(gi));
      gi.dwIndex = (DWORD)pControl->AttribGetInt(CurSel());
      if (pControl->Message (ESCM_LISTBOXGETITEM, &gi))
         pszSelRec = gi.pszName;

      if (pszSelRec && (pszSelRec[0] == L'f') && (pszSelRec[1] == L':'))
         pszSelRec += 2;
      else
         pszSelRec = NULL;
   }
   
   return pszSelRec;
}


/****************************************************************************
TTSReviewPage
*/
static BOOL TTSReviewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PTTSREV prp = (PTTSREV) pPage->m_pUserData;
   PCTTSWork pVF = prp->pWork;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // update the file list
         pPage->Message (ESCM_USER+82);

         // set the tts speak
         PCEscControl pControl = pPage->ControlFind (L"synthtext");
         if (pControl)
            pControl->AttribSet (Text(), prp->szTTSSpeak);

         // set the selection
         WCHAR szTemp[600];
         wcscpy (szTemp, L"f:");
         wcscat (szTemp, prp->szSelected);
         ESCMLISTBOXSELECTSTRING ss;
         memset (&ss, 0, sizeof(ss));
         ss.iStart = -1;
         ss.psz = szTemp;
         ss.fExact = TRUE;
         pControl = pPage->ControlFind (L"rec");
         if (pControl)
            pControl->Message (ESCM_LISTBOXSELECTSTRING, &ss);

         // read in wave file
         if (prp->pWaveOrig && prp->pWaveTrans)
            pPage->Message (ESCM_USER+91);   // just resynthesize
         else
            pPage->Message (ESCM_USER+90);

         // scroll to the right location
         pPage->VScroll (prp->iScroll);
         pPage->Invalidate();
      }
      break;

   case ESCM_USER+82:   // update the file list, doing a scan to sync
      {
         // scan
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Updating list...");
            pVF->m_pWaveDir->SyncFiles (&Progress, NULL);
         }

         // update list
         pPage->Message (ESCM_USER+83);
      }
      return TRUE;

   case ESCM_USER+83:   // udate the file list, NO SCAN
      {
         pVF->m_pWaveDir->FillListBox (pPage, L"rec", L"", L"", NULL);
      }
      return TRUE;

   case ESCM_USER+90:   // realod in file
      {
         if (prp->pWaveOrig)
            delete prp->pWaveOrig;
         if (prp->pWaveTrans)
            delete prp->pWaveTrans;
         if (prp->pWaveVocal)
            delete prp->pWaveVocal;
         prp->pWaveOrig = prp->pWaveTrans = prp->pWaveVocal = NULL;

         // get the name of the file
         PWSTR pszFile = TTSReviewPageGetSel (pPage);
         if (!pszFile)
            return 0;   // cant load

         // try to load in
         char szFile[256];
         WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile, sizeof(szFile), 0,0);
         prp->pWaveOrig = new CM3DWave;
         if (!prp->pWaveOrig)
            return 0;
         if (!prp->pWaveOrig->Open (NULL, szFile))
            return 0;

         // generate original resynthesized
         prp->pWaveTrans = prp->pWaveOrig->Clone();
         if (!prp->pWaveTrans)
            return 0;
         CVoiceSynthesize vs;
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Loading...", TRUE);
            vs.SynthesizeFromSRFeature (4, prp->pWaveTrans, NULL, 0, 0.0, NULL, TRUE, &Progress);
         }

         // resynthesized
         pPage->Message (ESCM_USER+91);
      }
      return TRUE;

   case ESCM_USER+91:   // regenerate TTS voice
      {
         if (!prp->pWaveOrig)
            return 0;
         if (prp->pWaveVocal)
            delete prp->pWaveVocal;
         prp->pWaveVocal = new CM3DWave;
         if (!prp->pWaveVocal)
            return 0;
         prp->pWaveVocal->ConvertSamplesAndChannels (22050, 1, NULL);   // make sure 22 kHz
         prp->pWaveVocal->MakePCM();

         // get the prosody info..
         CListFixed lPhone, lWord, lDur, lVol;
         CListVariable lPitch;
         if (!TTSWaveToTransPros (prp->pWaveOrig, prp->pWork->Lexicon(), 3,
            &lPhone, &lWord, &lDur, &lPitch, &lVol, NULL, NULL))
            return 0;

         CProgress Progress;
         Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing...", TRUE);

         SYNGENFEATURESCANDIDATE Candidate;
         memset (&Candidate, 0, sizeof(Candidate));
         Candidate.dwNum = lPhone.Num();
         Candidate.fAbsPitch = TRUE;
         Candidate.fVolAbsolute = TRUE;
         Candidate.padwDur = (DWORD*)lDur.Get(0);
         Candidate.padwPhone = (DWORD*)lPhone.Get(0);
         // intentionally not setting Candidate.padwWord
         Candidate.pafVol = (fp*)lVol.Get(0);
         Candidate.papszWord = (PWSTR*)lWord.Get(0);
         Candidate.paTTSGS = NULL;
         Candidate.plPitch = &lPitch;

         // deal with NULL pProgressWave
         CProgressWaveTTSToWave ToWave;
         ToWave.m_pWave = prp->pWaveVocal;
         prp->pWaveVocal->BlankWaveToSize (0, TRUE);   // make sure empty

         if (!prp->pTTS->SynthGenWaveInt (1, &Candidate, prp->pWaveVocal->m_dwSamplesPerSec, 1 /*iTTSQuality*/, FALSE /* fDisablePCM */, &Progress, NULL, &ToWave))
            return 0;
#if 0 // newer version but cant use because of timing issues
         // get the prosody info..
         // BUGFIX - Use official TP code instead of other ttswavetotranspros
         CTTSTransPros TransPros;
         CMem memTP;
         TransPros.m_dwDurType = TransPros.m_dwPitchType = TransPros.m_dwVolType = 2;
         if (!TransPros.WaveToPerPhoneTP (L"Transplanted prosody", prp->pWaveOrig, 3,
            prp->pWork->Lexicon(), &memTP))
            return 0;

         CProgress Progress;
         Progress.Start (pPage->m_pWindow->m_hWnd, "Synthesizing...", TRUE);
         if (!prp->pTTS->SynthGenWave (prp->pWaveVocal, (PWSTR)memTP.p, TRUE, &Progress))
            return 0;
#endif // 0

         return 0;
      }

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK)pParam;
         if (!p->psz)
            break;
         PWSTR psz = p->psz;

         if ((psz[0] == L'e') && (psz[1] == L':')) {
            pVF->m_pWaveDir->EditFile (NULL, psz+2, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         else if ((psz[0] == L'a') && (psz[1] == L'w') && (psz[2] == L':')) {
            DWORD dwWave = _wtoi(psz+3);
            if (!(psz = wcschr (psz+3, L'-')))
               return TRUE;
            DWORD dwUnit = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            DWORD dwWord = _wtoi(psz+1);

            pVF->BlacklistAdd ((WORD)dwWave, FALSE, dwUnit);

            // need to rebuild the word table
            // BUGFIX - Since no longer do by word... pVF->AnalysisWord (prp->pAnal, prp->pTTS, dwWord, TRUE, TRUE);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'a') && (psz[1] == L't') && (psz[2] == L':')) {
            DWORD dwWave = _wtoi(psz+3);
            if (!(psz = wcschr (psz+3, L'-')))
               return TRUE;
            DWORD dwUnit = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            DWORD dwPhone = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            DWORD dwWordPos = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            BYTE bPhoneLeft = (BYTE) _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            BYTE bPhoneRight = (BYTE) _wtoi(psz+1);

            WORD wTriPhone = PhoneToTriPhoneNumber (bPhoneLeft, bPhoneRight,
               prp->pWork->Lexicon(), prp->pWork->m_dwTriPhoneGroup);


            pVF->BlacklistAdd ((WORD)dwWave, TRUE, dwUnit);

            // need to rebuild the triphone
            pVF->AnalysisTriPhone (prp->pAnal, prp->pTTS, dwPhone,
               pVF->m_fWordStartEndCombine ? 0 : (WORD)dwWordPos,
               wTriPhone, TRUE, TRUE);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'c') && (psz[1] == L'w') && (psz[2] == L':')) {
            DWORD dwWord = _wtoi(psz+3);

            pVF->BlacklistClearWord (prp->pAnal, dwWord);

            // need to rebuild the word table
            // BUGFIX - Since no longer do by word ... pVF->AnalysisWord (prp->pAnal, prp->pTTS, dwWord, TRUE, TRUE);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'c') && (psz[1] == L't') && (psz[2] == L':')) {
            DWORD dwWave = _wtoi(psz+3);
            if (!(psz = wcschr (psz+3, L'-')))
               return TRUE;
            DWORD dwPhoneIndex = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            DWORD dwPhone = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            DWORD dwWordPos = _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            BYTE bPhoneLeft = (BYTE) _wtoi(psz+1);
            if (!(psz = wcschr (psz+1, L'-')))
               return TRUE;
            BYTE bPhoneRight = (BYTE) _wtoi(psz+1);

            WORD wTriPhone = PhoneToTriPhoneNumber (bPhoneLeft, bPhoneRight,
               prp->pWork->Lexicon(), prp->pWork->m_dwTriPhoneGroup);

            pVF->BlacklistClearTriPhone (prp->pAnal, (WORD)dwPhone, (WORD)dwWordPos, wTriPhone);

            // need to rebuild the triphone
            pVF->AnalysisTriPhone (prp->pAnal, prp->pTTS, dwPhone,
               pVF->m_fWordStartEndCombine ? 0 : (WORD)dwWordPos,
               wTriPhone, TRUE, TRUE);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'p') && (psz[1] == L':')) {
            DWORD dwStart = _wtoi(psz+2);
            psz = wcschr (psz+2, L'-');
            DWORD dwEnd = psz ? _wtoi(psz+1) : (dwStart+1);

            if (prp->pWaveTrans)
               prp->pWaveTrans->QuickPlayStop();
            if (prp->pWaveVocal) {
               PWVPHONEME pwp1 = (PWVPHONEME) prp->pWaveVocal->m_lWVPHONEME.Get(dwStart);
               PWVPHONEME pwp2 = (PWVPHONEME) prp->pWaveVocal->m_lWVPHONEME.Get(dwEnd);

               prp->pWaveVocal->QuickPlayStop();
               prp->pWaveVocal->QuickPlay(pwp1 ? pwp1->dwSample : 0,
                  pwp2 ? pwp2->dwSample : prp->pWaveVocal->m_dwSamples);
            }

            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz || !p->pszName)
            break;

         // list
         if (!_wcsicmp(psz, L"rec")) {
            // if click on file the play that file
            if ((p->pszName[0] == L'f') && (p->pszName[1] == L':')) {
               if (_wcsicmp(prp->szSelected, p->pszName+2)) {
                  wcscpy (prp->szSelected, p->pszName+2);

                  // clear wave so realods
                  if (prp->pWaveOrig)
                     delete prp->pWaveOrig;
                  prp->pWaveOrig = NULL;
                  prp->szTTSSpeak[0] = 0; // just to blank out

                  pPage->Exit (RedoSamePage());
               }
               return TRUE;
            }
            return TRUE;
         } // recording
      }
      return TRUE;


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         // figure out the record list current selection
         PWSTR pszSelRec =TTSReviewPageGetSel (pPage);

         if (!_wcsicmp(psz, L"playorig")) {
            if (prp->pWaveVocal)
               prp->pWaveVocal->QuickPlayStop();
            if (prp->pWaveTrans) {
               prp->pWaveTrans->QuickPlayStop();
               prp->pWaveTrans->QuickPlay();
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"playnew")) {
            if (prp->pWaveTrans)
               prp->pWaveTrans->QuickPlayStop();
            if (prp->pWaveVocal) {
               prp->pWaveVocal->QuickPlayStop();
               prp->pWaveVocal->QuickPlay();
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"makevoice")) {
            WCHAR szFile[256];
            szFile[0] = 0;
            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE))
               return TRUE;

            if (!prp->pTTS->Save (szFile)) 
               pPage->MBWarning (L"The file couldn't be saved.",
                  L"You may have tried to save over a write protected file.");
            else
               pPage->MBInformation (L"File saved.");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"makeprosody")) {
            WCHAR szFile[256];
            szFile[0] = 0;
            if (!TTSProsodyFileOpenDialog (pPage->m_pWindow->m_hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), TRUE, FALSE))
               return TRUE;

            PCTTSProsody pPros = prp->pTTS->TTSProsodyGet();

            if (!pPros || !pPros->Save (szFile)) 
               pPage->MBWarning (L"The file couldn't be saved.",
                  L"You may have tried to save over a write protected file.");
            else
               pPage->MBInformation (L"File saved.");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"synthnow")) {
            prp->szTTSSpeak[0];
            PCEscControl pControl = pPage->ControlFind (L"synthtext");
            DWORD dwNeed;
            if (pControl)
               pControl->AttribGet (Text(), prp->szTTSSpeak, sizeof(prp->szTTSSpeak), &dwNeed);
            if (!prp->szTTSSpeak[0]) {
               pPage->MBWarning (L"You should type in some text to speak.");
               return TRUE;
            }

            // delte some waves
            if (prp->pWaveVocal)
               delete prp->pWaveVocal;
            prp->pWaveVocal = NULL;
            if (prp->pWaveTrans)
               delete prp->pWaveTrans;
            prp->pWaveTrans = NULL;
            if (prp->pWaveOrig)
               delete prp->pWaveOrig;
            prp->pWaveOrig = NULL;

            // create original
            prp->pWaveOrig = new CM3DWave;
            if (!prp->pWaveOrig)
               return TRUE;
            prp->pWaveOrig->ConvertSamplesAndChannels (22050, 1, NULL);

            // synthesize voice
            {
               CProgress Progress;
               Progress.Start (pPage->m_pWindow->m_hWnd, "Speaking...");
               prp->pTTS->SynthGenWave (prp->pWaveOrig, prp->pWaveOrig->m_dwSamplesPerSec, prp->szTTSSpeak, FALSE, 1 /*iTTSQuality*/, FALSE /* fDisablePCM */, &Progress);
            }

            // clone
            prp->pWaveTrans = prp->pWaveOrig->Clone();

            // redo
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"markreview") || !_wcsicmp(psz, L"markreviewsynth")) {
            BOOL fReviewSynth = !_wcsicmp(psz, L"markreviewsynth");

            // get the phonemes for this sentence
            // do transplanted prosody calc on this
            PCMLexicon pLex = prp->pWork->Lexicon();
            if (!pLex)
               return TRUE;
            CListFixed lPhone, lWord, lDur, lVol, lTriPhone;
            CListVariable lPitch;

            PCM3DWave pWave = prp->pWaveOrig;
            if (!pWave)
               return TRUE;

            if (!TTSWaveToTransPros (pWave, pLex, 1, &lPhone, &lWord, &lDur,
               &lPitch, &lVol, &lTriPhone, prp->pTTS))
               return TRUE;

            PCMTTSTriPhoneAudio *ppTriPhone = (PCMTTSTriPhoneAudio*) lTriPhone.Get(0);

            // loop through all triphones and mark
            DWORD i;
            for (i = 0; i < lTriPhone.Num(); i++)
               if (ppTriPhone[i])
                  ppTriPhone[i]->m_fReviewed = TRUE;


            if (fReviewSynth) {
               // loop through the lexicon checking out all words...
               PCMLexicon pLex = prp->pWork->Lexicon();
               if (!pLex)
                  return TRUE;

               DWORD dwWord;
               CListVariable lForm, lDontRecurse;
               CListFixed lPhone, lWord, lTriPhone;
               DWORD dwInSent = 0;
               prp->szTTSSpeak[0] = 0;
               for (dwWord = 0; (dwInSent < 5) && (dwWord < pLex->WordNum()); dwWord++) {
                  WCHAR szWord[256];
                  lForm.Clear();
                  lDontRecurse.Clear();
                  // BUGFIX - this will need different meaning of word get
                  if (!pLex->WordGet(dwWord, szWord, sizeof(szWord), NULL))
                     continue;
                  if (!pLex->WordPronunciation (szWord, &lForm, FALSE, NULL, &lDontRecurse))
                     continue;
                  if (lForm.Num() < 1)
                     continue;

                  // if doesnt start alphetically ignore
                  // do this so dont get any interference with text parsing
                  DWORD dwLen = (DWORD)wcslen(szWord);
                  for (i = 0 ; i < dwLen; i++)
                     if (!iswalpha (szWord[i]) && (szWord[i] != L'\''))
                        break;
                  if (i < dwLen)
                     continue;

                  // make up the phonemes
                  PBYTE pbForm = ((PBYTE)lForm.Get(0)) + 1;
                  lPhone.Init(sizeof(DWORD));
                  lWord.Init(sizeof(DWORD));
                  DWORD dwPhone, dwWord2;
                  dwWord2 = prp->pWork->m_pLexWords->WordFind(szWord);
                  dwLen = (DWORD)strlen((char*)pbForm);
                  lPhone.Required (dwLen);
                  lWord.Required (dwLen);
                  for (i = 0; i < dwLen; i++) {
                     dwPhone = (DWORD)pbForm[i]-1;
                     if (i == 0)
                        dwPhone |= (1 << 24);
                     if (i+1 == dwLen)
                        dwPhone |= (2 << 24);
                     lPhone.Add (&dwPhone);
                     lWord.Add (&dwWord2);
                  } // i

                  lTriPhone.Clear();
                  // NOTE: This is only looking for words in the lexicon, so
                  // doing an average pitch is probably OK to do
                  fp fBestScore;
                  prp->pTTS->SynthDetermineTriPhoneAudio (prp->pTTS->AvgPitchGet(), (DWORD*)lPhone.Get(0), (DWORD*)lWord.Get(0),
                     NULL, NULL, NULL, FALSE, lPhone.Num(), 1 /*iTTSQuality*/,
                     FALSE /*fDisablePCM*/, &lTriPhone, &fBestScore, NULL);
                  ppTriPhone = (PCMTTSTriPhoneAudio*) lTriPhone.Get(0);

                  // loop through all triphones and see if reviewed
                  for (i = 0; i < lTriPhone.Num(); i++)
                     if (ppTriPhone[i] && !ppTriPhone[i]->m_fReviewed)
                        break;
                  if (i >= lTriPhone.Num())
                     continue;   // nothing here to check

                  // else need to review
                  if (dwInSent)
                     wcscat (prp->szTTSSpeak, L", ");
                  dwInSent++;
                  wcscat (prp->szTTSSpeak, szWord);
               }

               if (!dwInSent) {
                  pPage->MBInformation (L"You're finished reviewing.",
                     L"All the remaining words have reviewed phonemes. You may wish to review sentences "
                     L"since phonemes connecting words may not have been reviewed.");
                  return TRUE;
               }

               // delte some waves
               if (prp->pWaveVocal)
                  delete prp->pWaveVocal;
               prp->pWaveVocal = NULL;
               if (prp->pWaveTrans)
                  delete prp->pWaveTrans;
               prp->pWaveTrans = NULL;
               if (prp->pWaveOrig)
                  delete prp->pWaveOrig;
               prp->pWaveOrig = NULL;

               // create original
               prp->pWaveOrig = new CM3DWave;
               if (!prp->pWaveOrig)
                  return TRUE;
               prp->pWaveOrig->ConvertSamplesAndChannels (22050, 1, NULL);

               // synthesize voice
               {
                  CProgress Progress;
                  Progress.Start (pPage->m_pWindow->m_hWnd, "Speaking...");
                  prp->pTTS->SynthGenWave (prp->pWaveOrig, prp->pWaveOrig->m_dwSamplesPerSec, prp->szTTSSpeak, FALSE, 1 /*iTTSQuality*/, FALSE /* fDisablePCM */, &Progress);
               }

               // clone
               prp->pWaveTrans = prp->pWaveOrig->Clone();

               // redo
               pPage->Exit (RedoSamePage());
               return TRUE;
            }
            else {
               // figure out current selection
               PCEscControl pControl;
               pControl = pPage->ControlFind (L"rec");
               if (!pControl)
                  return TRUE;

               // loop through all the items, seeing if they're the one
               DWORD dwCurSel;
               CM3DWave Wave;
               for (dwCurSel = pControl->AttribGetInt (CurSel())+1; ; dwCurSel++) {
                  ESCMLISTBOXGETITEM gi;
                  memset (&gi, 0, sizeof(gi));
                  gi.dwIndex = dwCurSel;
                  PWSTR pszSelRec = NULL;
                  if (pControl->Message (ESCM_LISTBOXGETITEM, &gi))
                     pszSelRec = gi.pszName;

                  if (pszSelRec && (pszSelRec[0] == L'f') && (pszSelRec[1] == L':'))
                     pszSelRec += 2;
                  else
                     pszSelRec = NULL;

                  if (!pszSelRec) {
                     pPage->MBInformation (L"You're finished reviewing.",
                        L"All the remaining sentences have reviewed phonemes.");
                     return TRUE;
                  }

                  // load this in
                  char szTemp[256];
                  WideCharToMultiByte (CP_ACP, 0, pszSelRec, -1, szTemp, sizeof(szTemp), 0, 0);
                  if (!Wave.Open (NULL, szTemp))
                     continue;

                  // get info...
                  if (!TTSWaveToTransPros (&Wave, pLex, 1, &lPhone, &lWord, &lDur,
                     &lPitch, &lVol, &lTriPhone, prp->pTTS))
                     continue;

                  ppTriPhone = (PCMTTSTriPhoneAudio*) lTriPhone.Get(0);

                  // loop through all triphones and see if reviewed
                  for (i = 0; i < lTriPhone.Num(); i++)
                     if (ppTriPhone[i] && !ppTriPhone[i]->m_fReviewed)
                        break;
                  if (i >= lTriPhone.Num())
                     continue;   // nothing here to check

                  // else need to review
                  wcscpy (prp->szSelected, pszSelRec);
                  if (prp->pWaveOrig)
                     delete prp->pWaveOrig;
                  prp->pWaveOrig = NULL;
                  prp->szTTSSpeak[0] = 0; // just to blank out
                  pPage->Exit (RedoSamePage());
                  return TRUE;
               } // dwCurSel
            } // !fReviewSynth
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"editnew")) {
            if (!prp->pWaveVocal) {
               pPage->MBWarning (L"There isn't any resynthesized voice to save.");
               return TRUE;
            }

            WCHAR szFile[256];
            szFile[0] = 0;
            if (!WaveFileOpen (pPage->m_pWindow->m_hWnd, TRUE, szFile, NULL))
               return TRUE;

            // save
            char szTemp[256];
            WideCharToMultiByte (CP_ACP, 0, szFile, -1, szTemp, sizeof(szTemp), 0, 0);
            strcpy (prp->pWaveVocal->m_szFile, szTemp);
            if (!prp->pWaveVocal->Save (TRUE, NULL, NULL)) {
               pPage->MBWarning (L"There file couldn't be saved.");
               return TRUE;
            }

            // editor
            prp->pWork->m_pWaveDir->EditFile (NULL, szFile, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Review TTS voice";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"NUMUNITS")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, (int)pVF->m_dwUnitsAdded);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"PHONEREC")) {
            MemZero (&gMemTemp);

            // load in the wave and get all the semgents out
            CM3DWave WaveTemp;
            PCM3DWave pWaveOrig;
            if (prp->pWaveOrig)
               pWaveOrig = prp->pWaveOrig;
            else {
               pWaveOrig = &WaveTemp;
               char szFile[256];
               WideCharToMultiByte (CP_ACP, 0, prp->szSelected, -1, szFile, sizeof(szFile), 0,0);
               if (!WaveTemp.Open (NULL, szFile))
                  return 0;
            }

            // do transplanted prosody calc on this
            PCMLexicon pLex = prp->pWork->Lexicon();
            if (!pLex)
               return 0;
            CListFixed lPhone, lWord, lDur, lVol, lTriPhone;
            CListVariable lPitch;
            if (!TTSWaveToTransPros (pWaveOrig, pLex, 1, &lPhone, &lWord,
               &lDur, &lPitch, &lVol, &lTriPhone, prp->pTTS))
               return 0;

            DWORD dwNum = lPhone.Num();
            DWORD *padwPhone = (DWORD*) lPhone.Get(0);
            PWSTR *ppsz = (PWSTR*)lWord.Get(0);

            // find the wave number
            PCWaveFileInfo *ppwfi = (PCWaveFileInfo*) pVF->m_pWaveDir->m_lPCWaveFileInfo.Get(0);
            DWORD dwNumWFI = pVF->m_pWaveDir->m_lPCWaveFileInfo.Num();
            DWORD i;
            for (i = 0; i < dwNumWFI; i++)
               if (!_wcsicmp(prp->szSelected, ppwfi[i]->m_pszFile))
                  break;
            DWORD dwCurWave = (i < dwNumWFI) ? i : 0;

            // loop through all the words
            DWORD dwWordStart, dwWordEnd;
            PCMTTSTriPhoneAudio *ppTriPhone = (PCMTTSTriPhoneAudio*) lTriPhone.Get(0);
            BYTE pSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
            for (dwWordStart = 0; dwWordStart < dwNum; dwWordStart = dwWordEnd) {
               for (dwWordEnd = dwWordStart+1; dwWordEnd < dwNum; dwWordEnd++)
                  if (ppsz[dwWordStart] != ppsz[dwWordEnd])
                     break;

               // if this is an empty word then ignore
               if (!ppsz[dwWordStart] || !(ppsz[dwWordStart])[0])
                  continue;
               
               // if only one phoneme and it's silence then ignore
               if ((dwWordEnd == dwWordStart+1) && ((BYTE)padwPhone[dwWordStart]==pSilence))
                  continue;

               // determine if word reviewed
               BOOL fWordReviewed = TRUE;
               for (i = dwWordStart; i < dwWordEnd; i++)
                  if (ppTriPhone[i*TTSDEMIPHONES] && !ppTriPhone[i*TTSDEMIPHONES]->m_fReviewed)
                     fWordReviewed = FALSE;

               // make a table entry and show
               MemCat (&gMemTemp, L"<tr><td width=25%%><a ");
               if (!fWordReviewed)
                  MemCat (&gMemTemp, L"color=#ff0000 ");
               MemCat (&gMemTemp, L"href=p:");
               MemCat (&gMemTemp, (int)dwWordStart);
               MemCat (&gMemTemp, L"-");
               MemCat (&gMemTemp, (int)dwWordEnd);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, ppsz[dwWordStart]);
               MemCat (&gMemTemp, L"</a>");
               if (!fWordReviewed)
                  MemCat (&gMemTemp, L" (Not reviewed)");

               // if the word is a whole word recording then indicate so
               DWORD dwWord = pVF->m_pLexWords->WordFind (ppsz[dwWordStart]);

               // BUGFIX - Only a common word if marked as existing
               if (dwWord != -1) {
                  PCListFixed plLook = prp->pAnal->paplWords[dwWord];
                  PWORDINST pwi = plLook ? (PWORDINST) plLook->Get(0) : NULL;
                  DWORD dwBest;
                  if (plLook) for (dwBest = 0; dwBest < plLook->Num(); dwBest++, pwi++)
                     if (pwi->fUsedAsBest)
                        break;
                  if (!plLook || (dwBest >= plLook->Num()))
                     dwWord = -1;
               }

               if (-1 != dwWord) {
                  MemCat (&gMemTemp, L"<br/>&tab;(Common word)");

                  // how many words
                  DWORD dwBlack, dwTotal;
                  dwBlack = pVF->BlacklistNumWord (prp->pAnal, dwWord, &dwTotal);

                  // display
                  MemCat (&gMemTemp, L"<br/>&tab;<bold>");
                  MemCat (&gMemTemp, (int)dwTotal);
                  MemCat (&gMemTemp, L"</bold> candidates");
                  
                  if (dwBlack) {
                     MemCat (&gMemTemp, L"<br/>&tab;<bold>");
                     MemCat (&gMemTemp, (int)dwBlack);
                     MemCat (&gMemTemp, L"</bold> marked as bad");
                     MemCat (&gMemTemp, L"<br/>&tab;<a href=cw:");
                     MemCat (&gMemTemp, (int)dwWord);
                     MemCat (&gMemTemp, L">(Clear list)</a>");
                  } // if dwblack

                  // find which word is best so can add to blacklist
                  DWORD dwBest = -1;
                  PCListFixed plLook = prp->pAnal->paplWords[dwWord];
                  PWORDINST pwi = plLook ? (PWORDINST) plLook->Get(0) : NULL;
                  if (plLook) for (dwBest = 0; dwBest < plLook->Num(); dwBest++, pwi++)
                     if (pwi->fUsedAsBest)
                        break;
                  if (plLook && (dwBest >= plLook->Num()))
                     pwi = NULL;

                  // allow to add to blacklist
                  MemCat (&gMemTemp, L"<br/>&tab;<a href=aw:");
                  MemCat (&gMemTemp, (int)(pwi ? pwi->dwWave : 0));
                  MemCat (&gMemTemp, L"-");
                  MemCat (&gMemTemp, (int)(pwi ? pwi->pWORDAN->dwPhoneStart : 0));
                  MemCat (&gMemTemp, L"-");
                  MemCat (&gMemTemp, (int)dwWord);
                  MemCat (&gMemTemp, L">(Mark as bad)</a>");
               } // if dwword

               MemCat (&gMemTemp, L"</td><td width=75%%>");

               // all phonemes
               DWORD j;
               for (j = dwWordStart; j < dwWordEnd; j++) {
                  PLEXPHONE plp = pLex->PhonemeGetUnsort ((WORD)padwPhone[j]);
                  if (!plp)
                     continue;

                  if (!ppTriPhone[j*TTSDEMIPHONES])
                     continue;
                  BOOL fPhoneReviewed = ppTriPhone[j*TTSDEMIPHONES]->m_fReviewed;

                  if (j != dwWordStart)
                     MemCat (&gMemTemp, L"<p/>");

                  MemCat (&gMemTemp, L"<bold><a ");
                  if (!fPhoneReviewed)
                     MemCat (&gMemTemp, L"color=#ff0000 ");
                  MemCat (&gMemTemp, L"href=p:");
                  MemCat (&gMemTemp, (int)j);
                  MemCat (&gMemTemp, L">");
                  MemCatSanitize (&gMemTemp, plp->szPhoneLong);
                  MemCat (&gMemTemp, L"</a>");
                  MemCat (&gMemTemp, L"</bold>");
                  if (!fPhoneReviewed)
                     MemCat (&gMemTemp, L" (Not reviewed)");


                  // figure out the number of units that match
                  DWORD dwTotal, dwBlack;
                  if ((dwWord == -1) || (ppTriPhone[j*TTSDEMIPHONES]->m_dwWord != dwWord))
                     dwBlack = pVF->BlacklistNumTriPhone (prp->pAnal, (WORD)padwPhone[j],
                        pVF->m_fWordStartEndCombine ? 0 : ppTriPhone[j*TTSDEMIPHONES]->m_wWordPos, ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneLeft, ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneRight, &dwTotal);
                  else
                     dwTotal = dwBlack = 0;  // so know that from common word

                  // get the wave...
                  PCWaveFileInfo pwi = *((PCWaveFileInfo*)prp->pWork->m_pWaveDir->m_lPCWaveFileInfo.Get(ppTriPhone[j*TTSDEMIPHONES]->m_wOrigWave));
                  PCM3DWave pWave = ((PCWaveAn*)prp->pAnal->plPCWaveAn->Get(ppTriPhone[j*TTSDEMIPHONES]->m_wOrigWave))[0]->m_pWave;
                  PWVPHONEME pPhone = (PWVPHONEME) pWave->m_lWVPHONEME.Get(ppTriPhone[j*TTSDEMIPHONES]->m_wOrigPhone);

                  MemCat (&gMemTemp, L"<br/>&tab;From <a href=\"e:");
                  MemCatSanitize (&gMemTemp, pwi->m_pszFile);
                  MemCat (&gMemTemp, L"\">");
                  MemCatSanitize (&gMemTemp, pwi->m_pszFile);
                  MemCat (&gMemTemp, L"</a>");
                  MemCat (&gMemTemp, L" at ");
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"%g sec", (double)pPhone->dwSample / (double)pWave->m_dwSamplesPerSec);
                  MemCat (&gMemTemp, szTemp);

                  if (dwTotal) {
                     // number of candidates
                     MemCat (&gMemTemp, L"<br/>&tab;<bold>");
                     MemCat (&gMemTemp, (int)dwTotal);
                     MemCat (&gMemTemp, L"</bold> canditates");

                     if (dwBlack) {
                        MemCat (&gMemTemp, L"<br/>&tab;<bold>");
                        MemCat (&gMemTemp, (int)dwBlack);
                        MemCat (&gMemTemp, L"</bold> marked as bad <a href=ct:");
                        MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wOrigWave);
                        MemCat (&gMemTemp, L"-");
                        MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wOrigPhone);
                        MemCat (&gMemTemp, L"-");
                        MemCat (&gMemTemp, (int)(WORD)padwPhone[j]);
                        MemCat (&gMemTemp, L"-");
                        MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wWordPos);
                        MemCat (&gMemTemp, L"-");
                        MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneLeft);
                        MemCat (&gMemTemp, L"-");
                        MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneRight);
                        MemCat (&gMemTemp, L">(Clear list)</a>");
                     } // if dwblack

                     // need to be able to add to blacklist
                     MemCat (&gMemTemp, L"<br/>&tab;<a href=at:");
                     MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wOrigWave);
                     MemCat (&gMemTemp, L"-");
                     MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wOrigPhone);
                     MemCat (&gMemTemp, L"-");
                     MemCat (&gMemTemp, (int)(WORD)padwPhone[j]);
                     MemCat (&gMemTemp, L"-");
                     MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_wWordPos);
                     MemCat (&gMemTemp, L"-");
                     MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneLeft);
                     MemCat (&gMemTemp, L"-");
                     MemCat (&gMemTemp, (int)ppTriPhone[j*TTSDEMIPHONES]->m_bPhoneRight);
                     MemCat (&gMemTemp, L">Mark as bad</a>");
                  }

               } // j

               MemCat (&gMemTemp, L"</td></tr>");
            } // dwWordStart

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}



/*************************************************************************************
CTTSWork::Analyze - Analyzes all the wave file and saves a TTS voice.

inputs
   HWND           hWnd - Window to bring up progress bar and UI for saving
   PCEscWindow    pWindow - Window where to bring up UI for modifying/testing
returns
   BOOL - TRUE if user pressed back, FALSE if cancels
*/
BOOL CTTSWork::Analyze (HWND hWnd, PCEscWindow pWindow)
{
   // BUGFIX - Always combine start/end
   // BGUFIX - Changed back to FALSE in order to improve prosody
   m_fWordStartEndCombine = FALSE;

   CMTTS tts;
   tts.LexiconSet (m_szLexicon);
   tts.TriPhoneGroupSet (m_dwTriPhoneGroup);
   tts.KeepLogSet (m_fKeepLog);
   BOOL fFullPCM = FALSE;
   if (m_dwFreqCompress == 0) // per frame PCM
      fFullPCM = m_fFullPCM;
   if (m_dwPCMCompress)
      fFullPCM = TRUE;
   tts.FullPCMSet (fFullPCM);
   if (m_memTTSTARGETCOSTS.m_dwCurPosn == sizeof(TTSTARGETCOSTS))
      tts.TTSTARGETCOSTSSet ((PTTSTARGETCOSTS) m_memTTSTARGETCOSTS.p);

      // BUGFIX - Only do fullPCM is not compressing frequency
   if (!tts.LexWordsSet (m_pLexWords))
      return TRUE;
   

   // set random so reproducable
   srand (1000);

#if 0 // old prosody
   // transfer over the most common words for scaling
   tts.LexWordEmphSet (m_apLexWordEmph);

   // transfer over function words
   tts.LexFuncWordsSet (m_pLexFuncWords);
#endif

#ifdef _DEBUG
         // BUGFIX - Turn off continual testing so that runs faster

         // Get current flag
         int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

         // Turn on leak-checking bit
         tmpFlag &= ~(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF);
         //tmpFlag |=  _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;

         tmpFlag = LOWORD(tmpFlag) | (_CRTDBG_CHECK_EVERY_1024_DF << 4); // BUGFIX - So dont check for memory overwrites that often, make things faster

         // Set flag to the new value
         _CrtSetDbgFlag( tmpFlag );
#endif // _DEBUG

   // analysis
   TTSANAL Anal;
   CMem memAnal;
   memset (&Anal, 0, sizeof(Anal));

   // see if there's a tts voice to get from. if not then done
   CMTTS TTSTemp[NUMPROSODYTTS];
   CTTSProsody TTSProsodyTemp[NUMPROSODYTTS];
   DWORD i;
   for (i = 0; i < NUMPROSODYTTS; i++) if (m_aszProsodyTTS[i][0]) {
      PCTTSProsody pTTSProsody;
      PCMTTS pTTSCur;
      BOOL fIsTTS = FALSE;
      DWORD dwLen = (DWORD)wcslen(m_aszProsodyTTS[i]);
      if ((dwLen >= 4) && !_wcsicmp(m_aszProsodyTTS[i] + (dwLen-4), L".tts"))
         fIsTTS = TRUE;

      pTTSProsody = NULL;
      pTTSCur = NULL;
      if (fIsTTS) {
         // get from tts voice
         if (TTSTemp[i].Open(m_aszProsodyTTS[i])) {
            pTTSProsody = TTSTemp[i].TTSProsodyGet();
            pTTSCur = &TTSTemp[i];
         }
      }
      else {
         // get from prosody model
         if (TTSProsodyTemp[i].Open (m_aszProsodyTTS[i]))
            pTTSProsody = &TTSProsodyTemp[i];

         // TTSProsodyTemp[i].LexiconSet (Lexicon());
      }

      if (!pTTSProsody) {
         EscMessageBox (hWnd, ASPString(),
            m_aszProsodyTTS[i],
            L"The TTS voice (or prosody model) used for additional prosody couldn't be opened.",
            MB_ICONEXCLAMATION | MB_OK);
      }
      else {
         Anal.apTTS[i] = pTTSCur;
         Anal.apTTSProsody[i] = pTTSProsody;
      }
   }

   {
      CProgress Progress;
      Progress.Start (hWnd, "Analyzing...", TRUE);
      Progress.Push (0, .1);
      if (!AnalysisInit (&Progress, &memAnal, &Anal)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisInit.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop ();


      // pull out pitch from analysys
      tts.AvgPitchSet(Anal.fAvgPitch);
      tts.AvgSyllableDurSet (Anal.fAvgSyllableDur);
      tts.EnergyPerPitchSet (Anal.pacEnergyPerPitch);
      tts.EnergyPerVolumeSet (Anal.pacEnergyPerVolume);
      for (i = 0; i < NUMFUNCWORDGROUP; i++)
         tts.LexFuncWordsSet (m_apLexFuncWord[i], i);

      Progress.Push (.1, .2);

      // NOTE: must call AnalysisPHONETRAIN no matter the #ifdef OLDUSEALLSRSCORE
      if (!AnalysisPHONETRAIN (&Progress, &Anal, &tts)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisPHONETRAIN.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop ();

      // analyze the mega-group phonemes
      Progress.Push (.2, .4);
      if (!AnalysisMegaPHONETRAIN (&Progress, &Anal, &tts)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisMegaPHONETRAIN.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop ();

      Progress.Push (.4, .6);
      if (!AnalysisGroupTRIPHONETRAINMultiPass (&Progress, &Anal, &tts)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisGroupTRIPHONETRAINMultiPass.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop ();

      Progress.Push (.6, .85);
      if (!AnalysisSpecificTRIPHONETRAINMultiPass (&Progress, &Anal, &tts)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisSpecificTRIPHONETRAINMultiPass.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop ();

      Progress.Push (.85, .9);
      if (!AnalysisAllTriPhones (&Anal, &tts, &Progress)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisAllTriPhones.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop();

      Progress.Push (.9, .93);
#if 0 // no longer used
      if (!AnalysisAllWords (&Anal, &tts, &Progress) ||
            !AnalysisWordSyllableUnit (&Anal, &tts) ||
            !AnalysisMultiUnit(&Anal, &tts) ||
            !AnalysisSyllableUnit(&Anal, &tts) ||
            !AnalysisDiphones (&Anal, &tts) ||
            !AnalysisTriPhoneGroup (&Anal, &tts) ||
            !AnalysisConnectUnits(&Anal, &tts)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
#endif
      CListFixed lUNITGROUPCOUNT;
      lUNITGROUPCOUNT.Init (sizeof(UNITGROUPCOUNT));
      if (!AnalysisSingleUnits (&Anal, &tts, &lUNITGROUPCOUNT)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisSingleUnits.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }

      // will need to do based on longer units
      DWORD dwPhone;
      PCMLexicon pLex = tts.Lexicon();
      DWORD dwNumPhone = pLex->PhonemeNum();
      for (dwPhone = 0; dwPhone <= dwNumPhone; dwPhone++) {
         Progress.Update ((fp)dwPhone / (fp)(dwNumPhone+1));
         BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

         AnalysisMultiUnits (&Anal, &tts, &lUNITGROUPCOUNT, (dwPhone < dwNumPhone) ? (BYTE)dwPhone : bSilence);
      } // dwPhone

      // take best ones and mark as wanted
      if (!AnalysisSelectFromUNITGROUPCOUNT(&Anal, &tts, &lUNITGROUPCOUNT)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisSelectFromUNITGROUPCOUNT.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      Progress.Pop();

      Progress.Push (.93, .95);
      if (!AnalysisWriteTriPhones (&Anal, &tts, &Progress)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in AnalysisWriteTriPhones.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         AnalysisFree (&Anal);
         return TRUE;
      }
      // store away the number of units
      tts.m_dwUnits = m_dwUnitsAdded;
      tts.m_fPauseLessOften = m_fPauseLessOften;
      Progress.Pop();


#ifdef _DEBUG
      WCHAR szTemp[64];
      swprintf (szTemp, L"\r\nAverage pitch = %g", Anal.fAvgPitch);
      EscOutputDebugString (szTemp);
#endif

      // next step is to analyze prosody
      Progress.Push (.95, .99);
      Progress.Update (0);
      AdjustSYLANPitch (&Anal, &tts);
      WordEmphExtractAllWaves (&Anal, &tts, &Progress);
      Progress.Pop();

      // make sure to update triphone numbers
      tts.FillInTriPhoneIndex ();

      Progress.Push (.99, 1);

      Progress.Update (.1);
      PhoneEmph (&Anal, &tts);

#if 0 // old prosody
      Progress.Update (.2);
      WordEmphFromWordLength (&Anal, &tts);

      Progress.Update (0.3);
      WordEmphFromCommon (&Anal);

      Progress.Update (.4);
      WordEmphFromPunct (&Anal, &tts);
#endif // 0, old prosody

      Progress.Update (.6);
      // NOTE - take out since really messes up n-gram: WordEmphFromFuncWord (&Anal, &tts);

      Progress.Update (.8);
#if 0 // old prosody
      WordEmphFromNGram (&Anal, &tts);
#endif // 0, old prosody
      WordEmphProduceSentSyl (&Anal, &tts);

      Progress.Pop ();

#if 0 // old prosody
      // copy over learned info
      tts.LexWordEmphScaleSet (Anal.apLexWordEmphScale);
#endif
   }

   // pack analysis info into a structure
   TTSREV tr;
   memset (&tr, 0, sizeof(tr));
   tr.pAnal = &Anal;
   tr.pTTS = &tts;
   tr.pWork = this;
   PCWaveFileInfo pwi = *((PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(0));
   wcscpy (tr.szSelected, pwi->m_pszFile);
   PWSTR psz;
redo:
   psz = pWindow->PageDialog (ghInstance, IDR_MMLTTSREVIEW, TTSReviewPage, &tr);
   tr.iScroll = pWindow->m_iExitVScroll;
   if (psz && !_wcsicmp(psz, RedoSamePage()))
      goto redo;

   // delete waves...
   if (tr.pWaveOrig)
      delete tr.pWaveOrig;
   if (tr.pWaveTrans)
      delete tr.pWaveTrans;
   if (tr.pWaveVocal)
      delete tr.pWaveVocal;
   tr.pWaveOrig = tr.pWaveTrans = tr.pWaveVocal = NULL;

   // done
   AnalysisFree (&Anal);

   return (psz && !_wcsicmp(psz,Back())) ? TRUE : FALSE;
}


/*************************************************************************************
RankToScore - Converts a ranking in a list to a score.

inputs
   DWORD          dwRank - Rank, from 0..dwOutOf-1
   DWORD          dwOutOf - Rank out of how many
   DWORD          dwIdeal - Ideal rank (highest score), everything to left and right is lower
returns
   fp - Score, 1 being best, going down to near 0.
*/
fp RankToScore (DWORD dwRank, DWORD dwOutOf, DWORD dwIdeal)
{
   // distance from the ideal, and how far can go
   fp fDist, fMax;
   if (dwRank == dwIdeal)
// #define FLIPONHEAD   // just for testing
#ifdef FLIPONHEAD
      return 0.0001;
#else
      return 1;
#endif
   else if (dwRank > dwIdeal) {
      fDist = dwRank - dwIdeal;
      fMax = dwOutOf - dwIdeal - 1;
   }
   else if (dwRank < dwIdeal) {
      fDist = dwIdeal - dwRank;
      fMax = dwIdeal;
   }
   
   // if within half of the max then use practically 1
   fMax /= 2;
   fDist -= fMax;
   if (fDist <= 0)
#ifdef FLIPONHEAD
      return 0.01;
#else
      return 0.99;
#endif

   // else, use linear falloff
   // fDist = 1.0 - fDist / fMax * 0.75; // but never completely to 0
   // BUGFIX - Use sin() function as abitrator
   // fDist = 0.5 + cos(fDist / fMax * PI) / 2.0;
   fDist = cos(fDist / fMax * PI/2);   // BUGFIX Different ramp
   fDist = max(fDist, 0.01); // but never completely to 0
#ifdef FLIPONHEAD
   fDist = 1 - fDist;
#endif
   return fDist;
}


/*************************************************************************************
CTTSWork::TTSWaveAddPhoneSingle - Adds a single phoneme to the TTS wave.

NOTE: You should probably call TTSWaveAddPhone(), since it also adds the surrounding
phonemes.

inputs
   PTTANAL     pAnal - Analysis information
   DWORD       dwWaveNum - Wave number (from the list of wave files loaded)
   DWORD       dwPhoneIndex - Phoneme number into the wave
   BOOL        fRightHalf - If TRUE then add the right half of the phoneme, FALSE the left half
                  NOTE: Decided to ignore this since not enough data surrounding phoneme
                  to make good blend
   PCMTTS      pTTS - TTS to add it to
   PCM3DWave   pWavePCM - Version of the wave loaded that has PCM in it. Only needed
               if (m_dwPCMCompress is not zero)
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::TTSWaveAddPhoneSingle (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, BOOL fRightHalf, PCMTTS pTTS,
                                      PCM3DWave pWavePCM)
{
   // get the phoneme
   PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(dwWaveNum);
   if (!ppwa)
      return FALSE;
   PCWaveAn pwa = ppwa[0];
   if (!pwa)
      return FALSE;

   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(dwPhoneIndex);
   if (!ppa)
      return FALSE;

   // if this is silence then don't add
   PCMLexicon pLex = Lexicon();
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   if (ppa->bPhone == bSilence)
      return TRUE;   // ignore

   DWORD dwFeatureStart = ppa->dwTimeStart;
   DWORD dwFeatureEnd = ppa->dwTimeEnd;
   // BUGFIX - decided to ignore fRight since when do this not enough data around
   // the phoneme to create a good blend
   // BUGFIX - Re-enable since fixed click problems
   DWORD dwMid = (dwFeatureStart + dwFeatureEnd) / 2;
   if (fRightHalf)
      dwFeatureStart = dwMid;
   else
      dwFeatureEnd = dwMid;
   if (dwFeatureEnd <= dwFeatureStart)
      return TRUE;   // nothing to add


   // see if it already exists
   PCTTSWave pTW = pTTS->TTSFindWave (dwWaveNum);
   if (pTW && pTW->Find (dwFeatureStart, dwFeatureEnd))
      return TRUE;   // already exists


   // flags depending on setting
   // PCMLexicon pLex = Lexicon();
   PLEXPHONE plp = pLex->PhonemeGetUnsort (ppa->bPhone);
   PLEXENGLISHPHONE ple = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
   BOOL fPlosive = (ple->dwCategory & PIC_PLOSIVE) ? TRUE : FALSE;
   WORD wFlags = 0;
   switch (m_dwFreqCompress) {
   case 0:
   default:
      wFlags |= TPMML_FREQCOMPRESS_0;
      break;
   case 1:
      wFlags |= TPMML_FREQCOMPRESS_1;
      break;
   case 2:
      wFlags |= TPMML_FREQCOMPRESS_2;
      break;
   } // timecompress

   switch (m_dwPCMCompress) {
   case 0:  // none
   default:
      wFlags |= TPMML_PCMCOMPRESS_0;
      _ASSERT(!pWavePCM);
      break;
   case 1:  // ADPCM
      wFlags |= TPMML_PCMCOMPRESS_1;
      _ASSERT(pWavePCM);
      break;
   case 2:  // full PCM
      wFlags |= TPMML_PCMCOMPRESS_2;
      _ASSERT(pWavePCM);
      break;
   }


   DWORD dwCompress = m_dwTimeCompress;
   // if (fPlosive && dwCompress)
   //   dwCompress--;     // don't compress plosives as muchh
   switch (dwCompress) {
   case 0:
   default:
      wFlags |= TPMML_TIMECOMPRESS_0;
      break;
   case 1:
      wFlags |= TPMML_TIMECOMPRESS_1;
      break;
   case 2:
      wFlags |= TPMML_TIMECOMPRESS_2;
      break;
   case 3:
      wFlags |= TPMML_TIMECOMPRESS_3;
      break;
   } // switch

   // cache the features
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, dwFeatureStart, dwFeatureEnd, ppa->fIsVoiced,
      pwa->m_fAvgEnergyForVoiced, pTTS, 0);
   if (!psrCache)
      return FALSE;

   // BUGFIX - If creating a voice with a phase model, don't add in the original phases
   // Blank them to zero so they will compress better
   DWORD i;
   if (pTTS->PhaseModelGet(0, FALSE)) {
      for (i = 0; i < dwFeatureEnd - dwFeatureStart; i++)
         memset (psrCache[i].abPhase, 0, sizeof(psrCache[i].abPhase));
   }

   CMem memScratch;
   if (!CleanSRFEATURE (
      psrCache /*pWave->m_paSRFeature + ptr[dwBest].dwTimeStart*/,
      dwFeatureEnd - dwFeatureStart,
      ppa->fIsVoiced,
      &memScratch))
      return FALSE;

   // try to add this
   return pTTS->TTSWaveAdd (wFlags, pWavePCM ? pWavePCM : pwa->m_pWave, dwWaveNum, dwFeatureStart, dwFeatureEnd,
      (PSRFEATURE) memScratch.p);
}


/*************************************************************************************
CTTSWork::TTSWaveAddPhone - Adds a phoneme, and the phonemes that surround it, to the TTS wave.

inputs
   PTTANAL     pAnal - Analysis information
   DWORD       dwWaveNum - Wave number (from the list of wave files loaded)
   DWORD       dwPhoneIndex - Phoneme number into the wave
   PCMTTS      pTTS - TTS to add it to
   PCM3DWave   pWavePCM - Version of the wave loaded that has PCM in it. Only needed
               if (m_dwPCMCompress != 0)
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::TTSWaveAddPhone (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, PCMTTS pTTS,
                                PCM3DWave pWavePCM)
{
   // add the main one, both left and right halvs
   if (!TTSWaveAddPhoneSingle (pAnal, dwWaveNum, dwPhoneIndex, FALSE, pTTS, pWavePCM))
      return FALSE;
   if (!TTSWaveAddPhoneSingle (pAnal, dwWaveNum, dwPhoneIndex, TRUE, pTTS, pWavePCM))
      return FALSE;

   // add to the left
   if (dwPhoneIndex)
      TTSWaveAddPhoneSingle (pAnal, dwWaveNum, dwPhoneIndex-1, TRUE, pTTS, pWavePCM);

   // add to the right
   TTSWaveAddPhoneSingle (pAnal, dwWaveNum, dwPhoneIndex+1, FALSE, pTTS, pWavePCM);

   return TRUE;
}

/*************************************************************************************
CTTSWork::UsePHONEAN - Depending upon the flag sent in, this either marks a phoneme
in the audio as being needed for the final model, or it adds it immediately.
Marking it for the final model is faster (in the long run) when creating a
model with 50K+ units.

inputs
   PTTANAL     pAnal - Analysis information
   DWORD       dwWaveNum - Wave number (from the list of wave files loaded)
   DWORD       dwPhoneIndex - Phoneme number into the wave
   PVOID       pPHONEAN - Phoneme analysis structure
   PVOID       pPHONEANPrev - Previous PHONEAN structure, or NULL if doesnt exist
   PCMTTS      pTTS - TTS to add it to
   BOOL        fNow - If TRUE then add it now, else wait
   DWORD       dwLexWord - Word number in the lexicon, or -1 if not associated with a word
   BOOL        fRemoveExisting - Set to TRUE if there might be an existing triphone
   PCM3DWave      pWavePCM - Wave to use for original PCM. Only need if (m_dwPCMCompress != 0)
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::UsePHONEAN (PTTSANAL pAnal, DWORD dwWaveNum, DWORD dwPhoneIndex, PVOID pPHONEAN, PVOID pPHONEANPrev, PCMTTS pTTS, BOOL fNow,
                           DWORD dwLexWord, BOOL fRemoveExisting, PCM3DWave pWavePCM)
{
   PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(dwWaveNum);
   PCWaveAn pwa = ppwa[0];
   PPHONEAN ppa = (PPHONEAN) pPHONEAN;
   PPHONEAN ppaPrev = (PPHONEAN) pPHONEANPrev;

   // if not setting now, then can set some flags
   if (!fNow) {
      // BUGFIX - Disabled because fWantInFinal is handled elsewhere
      // ppa->fWantInFinal = TRUE;     // shouldnt get called
      if (ppa->dwLexWord == -1)
         ppa->dwLexWord = dwLexWord;
      return TRUE;
   }

   // if the score is too high then don't add
   if (ppa->afRankCompare[0][0][0][0] >= UNDEFINEDRANK)
      return FALSE;

   // else, need to add

   // get the infor for the unit with PIS_PHONEGROUPNUM x #units x PIS_PHONEGROUPNUM triphones,
   // so can get some averages
   PTRIPHONETRAIN ptpt = (PTRIPHONETRAIN)pAnal->plTRIPHONETRAIN->Get(ppa->dwTRIPHONETRAINIndex);
   PCMLexicon pLex = Lexicon();
   if (!ptpt)
      ptpt = GetSpecificTRIPHONETRAIN (this, pAnal, ppa, m_dwTriPhoneGroup, pLex);  // backoff to unknown triphone

   //WORD wAvgDuration = ptpt->dwDurationSRFeat;
   // DWORD dwAvgDurSamples = ptpt->dwDuration;
   // fp fAvgEnergy = ptpt->fEnergyAvg;
   //int iAvgPitch = ptpt->iPitch;
   //int iAvgPitchDelta = ptpt->iPitchDelta;

   // adjust the energy, so take the desired ratio times the average word energy
   // then, divide by what have so know how much to scake by
   // before doing this, was scaling this energy to match the average energy
   // provide by all the phonemes of the unit, ignoring the relative word volume
   fp fEnergyWant = ptpt->fEnergyRatio * m_fWordEnergyAvg;
   fp fAvgEnergy = fEnergyWant / max(ppa->fEnergyAvg, 1);

   // BUGFIX - Was +/- 6 dB, but increase to 12
   // NOTE: This min/max has a slight chance of messing up some of the volume
   // calculations since all units of a triphone are supposed to have the
   // same energy BUT, very unlikely, and better to put this safety check in
   fAvgEnergy = max(fAvgEnergy, .25);   // no more than +/- 12 db change
   fAvgEnergy = min(fAvgEnergy, 4);

   // make sure this is added
   if (!TTSWaveAddPhone (pAnal, dwWaveNum, dwPhoneIndex, pTTS, pWavePCM))
      return FALSE;

#if 0 // no longer used
   // flags depending on setting
   // PCMLexicon pLex = Lexicon();
   PLEXPHONE plp = pLex->PhonemeGetUnsort (ppa->bPhone);
   PLEXENGLISHPHONE ple = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
   BOOL fPlosive = (ple->dwCategory & PIC_PLOSIVE) ? TRUE : FALSE;
   WORD wFlags = 0;
   switch (m_dwFreqCompress) {
   case 0:
   default:
      wFlags |= TPMML_FREQCOMPRESS_0;
      break;
   case 1:
      wFlags |= TPMML_FREQCOMPRESS_1;
      break;
   case 2:
      wFlags |= TPMML_FREQCOMPRESS_2;
      break;
   } // timecompress

   DWORD dwCompress = m_dwTimeCompress;
   if (fPlosive && dwCompress)
      dwCompress--;     // don't compress plosives as muchh
   switch (dwCompress) {
   case 0:
   default:
      wFlags |= TPMML_TIMECOMPRESS_0;
      break;
   case 1:
      wFlags |= TPMML_TIMECOMPRESS_1;
      break;
   case 2:
      wFlags |= TPMML_TIMECOMPRESS_2;
      break;
   case 3:
      wFlags |= TPMML_TIMECOMPRESS_3;
      break;
   } // switch

   // cache the features
   DWORD dwCacheStart = ppa->dwTimeStart ? (ppa->dwTimeStart - 1) : ppa->dwTimeStart;
   DWORD dwCacheEnd = ((ppa->dwTimeEnd+1) < pwa->m_pWave->m_dwSRSamples) ? (ppa->dwTimeEnd+1) : ppa->dwTimeEnd;
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, dwCacheStart, dwCacheEnd, ppa->fIsVoiced,
      pwa->m_fAvgEnergyForVoiced, pTTS, 0);
   if (!psrCache)
      return FALSE;

   CMem memScratch;
   if (!CleanSRFEATURE (
      psrCache /*pWave->m_paSRFeature + ptr[dwBest].dwTimeStart*/,
      dwCacheEnd - dwCacheStart,
      ppa->fIsVoiced,
      &memScratch))
      return FALSE;
#endif // 0 no longer used

   // fill in the future phonemes
   BYTE abFuturePhone[8];
   memset (abFuturePhone, 0, sizeof(abFuturePhone));
   PPHONEAN ppaLookAhead = (PPHONEAN)pwa->m_lPHONEAN.Get(0);
   DWORD i;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   for (i = 0; i < sizeof(abFuturePhone); i++) {
      DWORD dwCur = dwPhoneIndex + 1 + i;
      if (dwCur >= pwa->m_lPHONEAN.Num())
         break;

      if (ppaLookAhead[dwCur].bPhone == bSilence)
         break;   // stop on silence

      // must want in final
      if (!ppaLookAhead[dwCur].dwWantInFinal || ppaLookAhead[dwCur].fBad)
         break;

      // store
      abFuturePhone[i] = ppaLookAhead[dwCur].bPhone+1;
   } // i

#if 0 // no longer used
   // get all the pitch points
   CListFixed lPitch;
   float fPitch;
   lPitch.Init (sizeof(float));
   for (i = ppa->dwTimeStart; i < ppa->dwTimeEnd; i++) {
      fPitch = pwa->m_pWave->PitchAtSample (i * pwa->m_pWave->m_dwSRSkip, 0);
      lPitch.Add (&fPitch);
   }
#endif // 0 no logner used

   // if previous is silence, then null
   if (ppaPrev && (ppaPrev->bPhone == bSilence))
      ppaPrev = NULL;

   if (!pTTS->TriPhoneAudioSet (dwWaveNum, dwPhoneIndex, ppa->bPhone, dwLexWord,
      m_fWordStartEndCombine ? 0 : ppa->bWordPos, ppa->bPhoneLeft, ppa->bPhoneRight,
      ppa->pWord ? ppa->pWord->dwFuncWordGroup : 0,
      &ppa->abRankAdd[0],
      &ppa->aMMIAdd[0], ppa->dwSpecificMismatchAccuracy,
      // wFlags,
      //wAvgDuration, iAvgPitch, iAvgPitchDelta,
      ppa->fPitch, ppa->fPitchDelta, ppa->fPitchBulge,
      ppa->fEnergyAvg, 
      ppaPrev ? ppaPrev->fPitch : 0.0, 
      ppaPrev ? ppaPrev->fEnergyAvg : 0.0, 
      ppaPrev ? (ppaPrev->dwTimeEnd - ppaPrev->dwTimeStart ) : 0,
      ppa->dwTimeStart, ppa->dwTimeEnd,
      //(PSRFEATURE) memScratch.p + (ppa->dwTimeStart - dwCacheStart),
      //(float*)lPitch.Get(0),
      //ppa->dwTimeEnd - ppa->dwTimeStart,
      // dwCacheStart < ppa->dwTimeStart,
      // dwCacheEnd > ppa->dwTimeEnd,
      ppa->dwTrimLeft, ppa->dwTrimRight,
      pwa->m_fMaxEnergy,
      pwa->m_fMaxEnergy / fAvgEnergy,
      abFuturePhone,
      fRemoveExisting))

      return FALSE;

      // BUGFIX - Divide fMaxEnergy by fAvgEnergy so that if the phoneme is louder than
      // the typical instance, it will make it quieter. That way stored unit has
      // the energy typical for all the units

   // note that have added
   m_dwUnitsAdded++;

   // else done
   return TRUE;
}


/*************************************************************************************
CTTSWork::CleanSRFEATURE - This takes a list of SRFEATUREs, copies them to
a new location, and either weights them towards noise or towards voiced, based
on the noised/voiced setting of the phoneme. It makes voiced phonems more
voiced, and un-voiced phonemes more noisy.

inputs
   PSRFEATURE        pasrf - List of features
   DWORD             dwNum - Number of features
   BOOL              fVoiced - If TRUE then weight towards voiced, else towards noise
   PCMem             pMem - Fill this in with dwNum SRFEATUREs that are weighted
returns
   BOOL - TRUE if usccess, FALSE if error
*/
BOOL CTTSWork::CleanSRFEATURE (PSRFEATURE pasrf, DWORD dwNum, BOOL fVoiced, PCMem pMem)
{
   if (!pMem->Required (dwNum * sizeof(SRFEATURE)))
      return FALSE;
   PSRFEATURE pasrfNew = (PSRFEATURE) pMem->p;
   memcpy (pasrfNew, pasrf, dwNum * sizeof(SRFEATURE));

#ifdef NOMODS_CLEANSRFEATURE
   return TRUE;
#endif

   // loop over features
   DWORD i, j, k;
   fp afVoice[SRDATAPOINTS], afNoise[SRDATAPOINTS];
   fp afVoiceOctave[SROCTAVE], afNoiseOctave[SROCTAVE];
   for (i = 0; i < dwNum; i++, pasrfNew++) {
      // BUGFIX - moved these into loop since were erroniously outside
      memset (afVoiceOctave, 0, sizeof(afVoiceOctave));
      memset (afNoiseOctave, 0, sizeof(afNoiseOctave));

      // convert to floating point values
      for (j = 0; j < SRDATAPOINTS; j++) {
         afVoice[j] = DbToAmplitude (pasrfNew->acVoiceEnergy[j]);
         afVoiceOctave[j / SRPOINTSPEROCTAVE] += afVoice[j];
         afNoise[j] = DbToAmplitude (pasrfNew->acNoiseEnergy[j]);
         afNoiseOctave[j / SRPOINTSPEROCTAVE] += afNoise[j];
      } // j

      // distribute noise/voice to areas where no energy
      fp afVoiceOctaveOrig[SROCTAVE], afNoiseOctaveOrig[SROCTAVE];
      memcpy (afVoiceOctaveOrig, afVoiceOctave, sizeof(afVoiceOctave));
      memcpy (afNoiseOctaveOrig, afNoiseOctave, sizeof(afNoiseOctave));
      for (j = 0; j < SROCTAVE; j++) {
         afVoiceOctave[j] = afNoiseOctave[j] = 0;
         for (k = 0; k < SROCTAVE; k++) {
            fp fDist = pow (.5, abs((int)k - (int)j) * 2);  // so de-emphasize when far away
            afVoiceOctave[j] += afVoiceOctaveOrig[k] * fDist;
            afNoiseOctave[j] += afNoiseOctaveOrig[k] * fDist;
         } // k

         // if both null then 50/50
         fp fSum = afVoiceOctave[j] + afNoiseOctave[j];
         if (fSum) {
            afVoiceOctave[j] /= fSum;
            afNoiseOctave[j] /= fSum;
         }
         else
            afVoiceOctave[j] = afNoiseOctave[j] = 0.5;
      } // j

      // go through and re-align noise vs. voice
      for (j = 0; j < SRDATAPOINTS; j++) {
         fp fTotal = afVoice[j] + afNoise[j];
         fp fOctave = (fp)j / (fp)SRPOINTSPEROCTAVE - 0.5;
         fOctave = max(fOctave, 0);
         DWORD dwOctave = (DWORD)fOctave;
         dwOctave = min(dwOctave, SROCTAVE-1);
         DWORD dwOctaveNext = min(dwOctave+1, SROCTAVE-1);
         fOctave -= (fp) dwOctave; // so get alpha

         fp fRatio = (1.0 - fOctave) * afVoiceOctave[dwOctave] + fOctave* afVoiceOctave[dwOctaveNext];
#define CLEANVOICETRIGGER        0.75        // if voiced amount >= this then make all voiced
         if (fVoiced) {
            // BUGFIX - If mostly voiced then make all voiced
            fRatio /= CLEANVOICETRIGGER;
            fRatio = sqrt(fRatio);  // emphasize voiced
            fRatio = min(fRatio, 1.0);
         }
         else
            fRatio = 1 - sqrt(1 - fRatio);  // emphasize noise

         pasrfNew->acVoiceEnergy[j] = AmplitudeToDb (fTotal * fRatio);
         pasrfNew->acNoiseEnergy[j] = AmplitudeToDb (fTotal * (1.0 - fRatio));
      } // j, over SRDATAPOINTs
   } // i

   return TRUE;
}


/*************************************************************************************
CTTSWork::AnalysisTriPhone - This scans the training information and created a
new unit for a triphone, based on the highest score. The tri-phone is considered
word independent.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   DWORD          dwPhone - Phoneme to train
   WORD           wWordPos - Word position, 0 for center, 1 for start of word, 2 for end of word
   WORD           wTriPhone - Tri-phone ID
   BOOL           fRemoveExisting - Set to TRUE if there might be an existing triphone
   BOOL           fNow - If TRUE then write the phoneme write away, otherwise make a note
                  that has been changed and do in bulk
returns
   BOOL - TRUE if success
*/

static int _cdecl UNITRANKSort (const void *elem1, const void *elem2)
{
   UNITRANK *pdw1, *pdw2;
   pdw1 = (UNITRANK*) elem1;
   pdw2 = (UNITRANK*) elem2;

   if (pdw1->fCompare < pdw2->fCompare)
      return -1;
   else if (pdw1->fCompare > pdw2->fCompare)
      return 1;
   else
      return 0;
}

BOOL CTTSWork::AnalysisTriPhone (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwPhone, WORD wWordPos,
                                 WORD wTriPhone, BOOL fRemoveExisting, BOOL fNow)
{
   if (m_fWordStartEndCombine)
      wWordPos = 0;

   // get the list it's in
   PCListFixed plLook = pAnal->paplTriPhone[m_fWordStartEndCombine ? 0 : wWordPos][dwPhone];
   if (!plLook)
      return FALSE;  // not in existing list
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;

   // BUGFIX - If merging with prosody then keep track of matches
   DWORD dwPros;
   PCMTTSTriPhonePros papTPP[NUMPROSODYTTS];

   // find all instances of the triphone
   DWORD i, j, k, l;
   PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
   CListFixed lFound, lFoundMany;
   lFound.Init (sizeof(UNITRANK));
   lFoundMany.Init (sizeof(UNITRANK));
   UNITRANK tp;
   CListFixed lEnergy;
   lEnergy.Init (sizeof(float));
   PPHONEAN ppa = NULL;
   DWORD dwDemi;
   for (i = 0; i < plLook->Num(); i++, pti++) {
      // skip
      if (pti->wTriPhone != wTriPhone)
         continue;
      if (pti->pPHONEAN->dwTimeEnd <= pti->pPHONEAN->dwTimeStart)
         continue;   // no length

      memset (&tp, 0, sizeof(tp));
      
      // get the duration
      PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(pti->dwWave);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa->m_pWave;
      PWVPHONEME pwv1 = (PWVPHONEME) pWave->m_lWVPHONEME.Get(pti->dwPhoneIndex);
      PWVPHONEME pwv2 = (PWVPHONEME) pWave->m_lWVPHONEME.Get(pti->dwPhoneIndex+1);

      tp.dwTimeStart = pti->pPHONEAN->dwTimeStart;
      tp.dwTimeEnd = pti->pPHONEAN->dwTimeEnd;
      tp.fEnergy = pti->pPHONEAN->fEnergyAvg;
      tp.fPitch = pti->pPHONEAN->fPitch;
      tp.fPitchVar = pti->pPHONEAN->fPitchDelta;
      tp.iPitchDelta = pti->pPHONEAN->iPitchDelta;
      tp.iPitchBulge = pti->pPHONEAN->iPitchBulge;
      tp.fVoicedEnergy = pti->pPHONEAN->fVoicedEnergy;
      tp.fPlosiveness = pti->pPHONEAN->fPlosiveness;
      tp.fBrightness = pti->pPHONEAN->fBrightness;
      tp.fIsPlosive = pti->pPHONEAN->fIsPlosive;
      tp.fIsVoiced = pti->pPHONEAN->fIsVoiced;
      tp.dwDuration = pti->pPHONEAN->dwDuration;

      tp.fSRScoreGeneral = pti->pPHONEAN->fSRScoreGeneral;
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         tp.afSRScorePhone[dwDemi] = pti->pPHONEAN->afSRScorePhone[dwDemi];
         tp.afSRScoreMegaPhone[dwDemi] = pti->pPHONEAN->afSRScoreMegaPhone[dwDemi];
         tp.afSRScoreGroupTriPhone[dwDemi] = pti->pPHONEAN->afSRScoreGroupTriPhone[dwDemi];
         tp.afSRScoreSpecificTriPhone[dwDemi] = pti->pPHONEAN->afSRScoreSpecificTriPhone[dwDemi];
      } // dwDEmi

      ppa = pti->pPHONEAN; // just to remember

      // set the inital total score based on blacklisted
      memset (tp.afRankCompare, 0, sizeof(tp.afRankCompare));
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         tp.afRankAdd[dwDemi] = 0;
      if (BlacklistExist (pti->dwWave, TRUE, pti->dwPhoneIndex))
         for (j = 0; j < PITCHFIDELITY; j++) for (k = 0; k < DURATIONFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            tp.afRankCompare[j][k][l][dwDemi] += 100; // so less likely to have

      // add
      tp.pTPInst = pti;
      lFound.Add (&tp);
      
      // add weighted versions for calculatnig medians
      DWORD dwAdd;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
      dwAdd = NUMFUNCWORDGROUP+1;
      if (ppa->pWord)
         dwAdd = ppa->pWord->dwFuncWordGroup+1;
#else
      dwAdd = 1;
#endif
      for (; dwAdd; dwAdd--)
         lFoundMany.Add (&tp);
   } // i

   // get the units
   PUNITRANK ptr = (PUNITRANK)lFound.Get(0);
   PUNITRANK ptrMany = (PUNITRANK)lFoundMany.Get(0);
   DWORD dwNum = lFound.Num();
   DWORD dwNumMany = lFoundMany.Num();
   DWORD dwWeightMany = dwNumMany;
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
   dwWeightMany /= (NUMFUNCWORDGROUP+1);
#endif
   dwWeightMany = max(dwWeightMany, 1);

   // determine a weighting based on the number of trphones, used for SR, so determine
   // if emphasize fSRScoreMegaPhone or fSRScoreTriPhone more,
   // and write the weighted SR

   fp fWeightThisSR = (fp)dwNum / (fp)(dwNum + PARENTCATEGORYWEIGHT);
   for (i = 0; i < dwNum; i++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
      ptr[i].afSRScoreWeighted[dwDemi] = ptr[i].afSRScoreSpecificTriPhone[dwDemi];

#if 0 // no longer used because moved weighting immediately after ->Compare calls
      ptr[i].afSRScoreWeighted[dwDemi] = fWeightThisSR * ptr[i].afSRScoreSpecificTriPhone[dwDemi] + (1.0 - fWeightThisSR) * ptr[i].afSRScoreMegaPhone[dwDemi];
      ptr[i].pTPInst->pPHONEAN->afSRScoreWeighted[dwDemi] = ptr[i].afSRScoreWeighted[dwDemi];

      // weight all the mismatched scores
      for (j = 0; j < TRIPHONESPECIFICMISMATCH; j++) {
         if (!ptr[i].pTPInst->pPHONEAN->aMMSSpecific[j].pVoid)
            continue;   // not set

         fp fScore = ptr[i].pTPInst->pPHONEAN->aMMSSpecific[j].afScore[dwDemi];
         fp fWeightWith = ptr[i].pTPInst->pPHONEAN->aMMSSpecific[j].afScoreWeightWith[dwDemi];
         fScore = fWeightThisSR * fScore + (1.0 - fWeightThisSR) * fWeightWith;
         fScore -= ptr[i].afSRScoreWeighted[dwDemi];  // so end up storing a delta in score

         ptr[i].pTPInst->pPHONEAN->aMMSSpecific[j].afScore[dwDemi] = fScore;
      } // j, mismatch
#endif // 0
   }

   if (!lFound.Num() || !ppa)
      return FALSE;


   // BUGFIX - Move minexamples later so set the fRankCompare

   // get the infor for the unit with PIS_PHONEGROUPNUM x #units x PIS_PHONEGROUPNUM triphones,
   // so can get some averages
   PTRIPHONETRAIN ptpt = GetSpecificTRIPHONETRAIN (this, pAnal, ppa, m_dwTriPhoneGroup, pLex);

   
   // loop through all the units, do calculations, and create a score
   ptr = (PUNITRANK)lFound.Get(0);
   // DWORD dwDemi;
   for (i = 0; i < dwNum; i++, ptr++) {
      // start out with SR score
      fp afRankCompareTemp[TTSDEMIPHONES];
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         afRankCompareTemp[dwDemi] = ptr->afRankCompare[0][0][0][dwDemi];
         afRankCompareTemp[dwDemi] += ptr->afSRScoreWeighted[dwDemi];
         ptr->afRankAdd[dwDemi] += ptr->afSRScoreWeighted[dwDemi]; // BUGFIX - Was fSRScoreMegaPhone, which isn't right
      }


      // energy penalty
      // BUGFIX - Energy penalty tries to favor higher energy units
      fp fEnergyPenalty = log((fp)ptr->fEnergy / (fp)ptpt->fEnergyMedianHigh) / log((fp)2);
      fEnergyPenalty = fabs(fEnergyPenalty) * UnitScoreEnergy(pTTS, (BYTE) dwPhone, pLex, ptr->fEnergy > ptpt->fEnergyMedianHigh);
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         afRankCompareTemp[dwDemi] += fEnergyPenalty;

      // NOTE: ignoring energy ratio, which was energy of this unit compared to average enery of word
      
#ifdef NOMODS_MISCSCOREHACKS
      // voiced energy
      fp fVoicedEnergyPenalty = (1.0 - ptr->fVoicedEnergy) * HYPVOICEDENERGYPENALTY;
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         afRankCompareTemp[dwDemi] += fVoicedEnergyPenalty;
#endif

#ifdef NOMODS_MISCSCOREHACKS
      // include plosiveness
      fp fPlosivenessPenalty = ptr->fPlosiveness * HYPPLOSIVENESSPENALTY;
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         afRankCompareTemp[dwDemi] += fPlosivenessPenalty;
#endif

#ifdef NOMODS_MISCSCOREHACKS  // not NOMODS_UNDERWEIGHTFUNCTIONWORDS because includes score
      // include a penalty for units from function words
      fp fFuncWordPenalty;
      if (ptr->pTPInst->pPHONEAN->pWord)
         fFuncWordPenalty = (fp) 1 - ptr->pTPInst->pPHONEAN->pWord->fFuncWordWeight;
      else
         fFuncWordPenalty = 0;
      fFuncWordPenalty *= HYPFUNCWORDPENALTY;
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         afRankCompareTemp[dwDemi] += fFuncWordPenalty;
         ptr->afRankAdd[dwDemi] += fFuncWordPenalty; // including this so doesn't want to include units from function words
      }
#endif

      // pitch penalty for pitch variation
      // BUGFIX - Pitch penalty regardless of voied or unvoiced
      // if (ptr->fIsVoiced) {
         // pitch variation
         fp fPitchPenalty;
         fPitchPenalty = (fp)abs(ptr->iPitchDelta - ptpt->iPitchDelta) / (fp)1000;
         fPitchPenalty = fabs(fPitchPenalty) * UnitScorePitch(pTTS, (BYTE)dwPhone, pLex, ptr->iPitchDelta > ptpt->iPitchDelta,
            m_dwPCMCompress || ((m_dwFreqCompress == 0) && m_fFullPCM) );
               // BUGFIX - Only do fullPCM is not compressing frequency
         fPitchPenalty = max(0, fPitchPenalty - HYPPITCHPENALTYPEROCTAVEFORGIVE);   // forgive pitch close to range
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            afRankCompareTemp[dwDemi] += fPitchPenalty;

         // NOTE: Ignoring fPitchDelta since what really care about is to have
         // a wave whose pitchdelta is as similar to the typical as possible,
         // and fPitchDelta is the same as iPitchDelta
      //}

      // need to make several copies of fRankCompareTemp for each
      // of the pitches
      for (j = 0; j < PITCHFIDELITY; j++) for (k = 0; k < DURATIONFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         ptr->afRankCompare[j][k][l][dwDemi] = afRankCompareTemp[dwDemi];

      // BUGFIX - Was only doing for ptr->fIsVoiced, but do for unvoiced too since somre residual pitch: if (ptr->fIsVoiced)
      for (j = 0; j < PITCHFIDELITY; j++) {
         // absolute pitch
         fp fPitchWant = (fp)ptpt->fPitch * pow ((fp)OCTAVESPERPITCHFILDELITY, (fp)j - (fp)PITCHFIDELITYCENTER);
         fp fPitchPenalty = log((fp)ptr->fPitch / fPitchWant) / log((fp)2);
         fPitchPenalty = fabs(fPitchPenalty) * HYPPITCHPENALTYPEROCTAVEBUILD(UnitScorePitch(pTTS, (BYTE)dwPhone, pLex, ptr->fPitch > fPitchWant,
           m_dwPCMCompress || ((m_dwFreqCompress == 0) && m_fFullPCM) ));
            // BUGFIX - Only do fullPCM is not compressing frequency
         fPitchPenalty = max(0, fPitchPenalty - HYPPITCHPENALTYPEROCTAVEBUILD(HYPPITCHPENALTYPEROCTAVEFORGIVE));   // forgive pitch close to range

         for (k = 0; k < DURATIONFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            ptr->afRankCompare[j][k][l][dwDemi] += fPitchPenalty;

      }

      for (j = 0; j < DURATIONFIDELITY; j++) {
         fp fDurationWant = (fp)ptpt->dwDuration * pow((fp)SCALEPERDURATIONFIDELITY, (fp)j - (fp)DURATIONFIDELITYCENTER);
         fDurationWant *= 1.0 + HYPDURATIONLONGERLESS(1.0);
               // BUGFIX - Try to pick units that are slightly longer than normal since shrinking
               // down doesn't sound as bad as stretching out.
               // NOTE: There is an equivalent scale by 1.5 in CMTTS ScoreCalcSelf

         // duration penalty
         // BUGFIX - allow to be slightly longer without penalty
         fp fPenalty = UnitScoreDuration (pTTS, (BYTE)dwPhone, pLex, ptr->dwDuration > fDurationWant,
            m_dwPCMCompress || ((m_dwFreqCompress == 0) && m_fFullPCM) );
         //DWORD dwPenalty = ptr->fIsPlosive ? HYPDURATIONPENALTYPLOSIVE(HYPDURATIONPENALTYPERDOUBLE) :
         //   (ptr->fIsVoiced ? HYPDURATIONPENALTYPERDOUBLE : HYPDURATIONPENALTYUNVOICED(HYPDURATIONPENALTYPERDOUBLE) );

         fp fDurationPenalty = log((fp)ptr->dwDuration / fDurationWant ) / log((fp)2) * fPenalty;
         if (fDurationPenalty > 0.0)
            fDurationPenalty = max(0.0, fDurationPenalty - (fp)HYPDURATIONLONGERLESS(dwPenalty) );
         else
            fDurationPenalty *= -1; // since was negative

         for (k = 0; k < PITCHFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            ptr->afRankCompare[k][j][l][dwDemi] += fDurationPenalty;
      } // j


      for (j = 0; j < ENERGYFIDELITY; j++) {
         fp fEnergyWant = (fp)ptpt->fEnergyMedian * pow((fp)SCALEPERENERGYFIDELITY, (fp)j - (fp)ENERGYFIDELITYCENTER);

         // energy penalty
         // BUGFIX - allow to be slightly longer without penalty
         fp fPenalty = UnitScoreEnergy (pTTS, (BYTE)dwPhone, pLex, ptr->fEnergy > fEnergyWant);

         fp fEnergyPenalty = log(max(ptr->fEnergy,CLOSE) / max(fEnergyWant,CLOSE) ) / log((fp)2) * fPenalty;
         if (fEnergyPenalty > 0.0)
            fEnergyPenalty = max(0.0, fEnergyPenalty);
         else
            fEnergyPenalty *= -1; // since was negative

         for (k = 0; k < PITCHFIDELITY; k++) for (l = 0; l < DURATIONFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
            ptr->afRankCompare[k][l][j][dwDemi] += fEnergyPenalty;
      } // j
   } // i

   // if there aren't enough versions of this triphone then don't add
   if (lFound.Num() < MINNUMEXAMPLES)
      return TRUE;


   // sort by brightness and give brighter values (higher) a better rank (lower)
#if 0    // BUGFIX - Get rid of brightness hack since changed SRFEATUREDistortCompare
   ptr = (PUNITRANK)lFound.Get(0);
   for (i = 0; i < dwNum; i++)
      ptr[i].fCompare = ptr[i].fBrightness;
   qsort (ptr, dwNum, sizeof(UNITRANK), UNITRANKSort);
   for (i = 0; i < dwNum; i++) {
      ptr[i].fRankCompare += (fp)(dwNum-i-1) / (fp)dwNum * HYPBRIGHTNESSPENALTY;
      ptr[i].fRankAdd += (fp)(dwNum-i-1) / (fp)dwNum * HYPBRIGHTNESSPENALTY;
         // BUGFIX - Add brightness penalty to this so chooses brighter phonemes
   }
#endif // 0


   // fill in the final rank info since afRankCompare may have been modified by brightness hack
   ptr = (PUNITRANK)lFound.Get(0);
   fp f;
   for (i = 0; i < dwNum; i++, ptr++) {
      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
         // store away the total score
         for (j = 0; j < PITCHFIDELITY; j++) for (k = 0; k < DURATIONFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++)
            ptr->pTPInst->pPHONEAN->afRankCompare[j][k][l][dwDemi] = ptr->afRankCompare[j][k][l][dwDemi];
         ptr->pTPInst->pPHONEAN->afRankAdd[dwDemi] = ptr->afRankAdd[dwDemi];

         f = ptr->afRankAdd[dwDemi] * (fp)RANKDBSCALE + (fp)RANKDBOFFSET;
            // BUGFIX - Add RANKDBOFFSET so can include some negative scores
         f = max(f, 0);
         f = min(f, 255);
         ptr->pTPInst->pPHONEAN->abRankAdd[dwDemi] = (BYTE)(int)f;
      } // dwDemi

      // also transfer the mismatched
      for (j = 0; j < TRIPHONESPECIFICMISMATCH; j++) {
         PMISMATCHSTRUCT pMMS = &ptr->pTPInst->pPHONEAN->aMMSSpecific[j];
         PMISMATCHINFO pMMI = &ptr->pTPInst->pPHONEAN->aMMIAdd[j];
         if (!pMMS->pVoid) {
            pMMI->bLeft = pMMI->bRight = 0;  // for good compressin
            for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
               pMMI->abRank[dwDemi] = 0;  // to indicate no match
            continue;   // nothing to set
         }

         // else, set
         pMMI->bLeft = (BYTE) pMMS->dwValue;
         pMMI->bRight = (BYTE) (pMMS->dwValue >> 8);
         for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
            f = pMMS->afScore[dwDemi] * (fp)RANKDBSCALE + (fp)RANKDBOFFSET;
               // BUGFIX - Add RANKDBOFFSET so can include some negative scores
            f = max(f, 1); // NOTE: Intentionally doing 1 so that 0 indicates an invalid entry
            f = min(f, 255);
            pMMI->abRank[dwDemi] = (BYTE)(int)f;
         } // dwDemi
      } // j
   } // i
   ptr = (PUNITRANK)lFound.Get(0);  // to restore

   // calculate some specific info
   // BUGIX - Before used to weight all the values specific to this triphone
   // and average those in with the scores from ptpt (larger triphones), but
   // not doing this now since the biggest problem is just not enough units
   // keep statistics
   TRIPHONETRAIN tptThis;
   memset (&tptThis, 0, sizeof(tptThis));
   tptThis.dwCountScale = dwNumMany;

   tptThis.dwDurationSRFeat = ptpt->dwDurationSRFeat;
   tptThis.dwDuration = ptpt->dwDuration;

   // BUGFIX - If getting prosody from another TTS voice then get some prosody info
   // from there. This way will look for units as long as an average of the typical
   // unit here, and compared to what want for voice whose prosody is merged in
   memset (papTPP, 0, sizeof(papTPP));
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++) if (pAnal->apTTS[dwPros])
      papTPP[dwPros] = pAnal->apTTS[dwPros]->SynthDetermineTriPhonePros ((BYTE) dwPhone,
         m_fWordStartEndCombine ? 0 : wWordPos, ptr->pTPInst->bPhoneLeft, ptr->pTPInst->bPhoneRight);

   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros]) {
         DWORD dwScale = tptThis.dwDurationSRFeat ? (tptThis.dwDuration / tptThis.dwDurationSRFeat) : 1;
         tptThis.dwDurationSRFeat = (tptThis.dwDurationSRFeat + (DWORD)papTPP[dwPros]->m_wDuration + 1) / 2; // average duration
         tptThis.dwDuration = tptThis.dwDurationSRFeat * dwScale;
      }

   tptThis.fEnergyMedian = ptpt->fEnergyMedian; // keep median volume units

   // BUGFIX - If getting prosody from another TTS voice then get some prosody info
   // from there. This way will look for units as long as an average of the typical
   // unit here, and compared to what want for voice whose prosody is merged in
   // DWORD dwPros;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros])
         tptThis.fEnergyMedian = (tptThis.fEnergyMedian + papTPP[dwPros]->m_fEnergyAvg) / 2; // average energy

   tptThis.fEnergyRatio = ptpt->fEnergyRatio;

   tptThis.fEnergyRatio = (tptThis.fEnergyRatio * (fp) dwWeightMany +
      ptpt->fEnergyRatio * (fp)PARENTCATEGORYWEIGHT) /
      (fp)(dwWeightMany + PARENTCATEGORYWEIGHT);

   // BUGFIX - If getting prosody from another TTS voice then get some prosody info
   // from there. This way will look for units as long as an average of the typical
   // unit here, and compared to what want for voice whose prosody is merged in
   // DWORD dwPros;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros])
         tptThis.fEnergyRatio = (tptThis.fEnergyRatio + papTPP[dwPros]->m_fEnergyAvg / m_fWordEnergyAvg) / 2; // average energy

   tptThis.iPitch = ptpt->iPitch;

   // BUGFIX - If getting prosody from another TTS voice then get some prosody info
   // from there. This way will look for units as long as an average of the typical
   // unit here, and compared to what want for voice whose prosody is merged in
   // DWORD dwPros;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros])
         tptThis.iPitch = (tptThis.iPitch + papTPP[dwPros]->m_iPitch) / 2; // average energy

   tptThis.iPitchDelta = ptpt->iPitchDelta;

   // BUGFIX - If getting prosody from another TTS voice then get some prosody info
   // from there. This way will look for units as long as an average of the typical
   // unit here, and compared to what want for voice whose prosody is merged in
   // DWORD dwPros;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros])
         tptThis.iPitchDelta = (tptThis.iPitchDelta + papTPP[dwPros]->m_iPitchDelta) / 2; // average energy

   tptThis.iPitchBulge = ptpt->iPitchBulge;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++)
      if (papTPP[dwPros])
         tptThis.iPitchBulge = (tptThis.iPitchBulge + papTPP[dwPros]->m_iPitchBulge) / 2; // average energy


   // BUGFIX - calculate the average pitch and pitchdelta, so dont get phonemes all
   // over the place
   // BUGFIX - Use from TRIPHONETRAIN info
   //int iAvgPitch = 0, iAvgPitchDelta = 0;
   //for (i = 0; i < dwNum; i++) {
   //   iAvgPitch += ptr[i].pTPInst->pPHONEAN->iPitch;
   //   iAvgPitchDelta += ptr[i].pTPInst->pPHONEAN->iPitchDelta;
   //} // i
   //iAvgPitch /= (int)dwNum;
   //iAvgPitchDelta /= (int)dwNum;
   //int iAvgPitch = ptpt->iPitch;
   //int iAvgPitchDelta = ptpt->iPitchDelta;

   //WORD wAvgDuration = ptpt->dwDurationSRFeat;
   //DWORD dwAvgDurSamples = ptpt->dwDuration;
   //fp fAvgEnergy = ptpt->fEnergyAvg;


   // add this triphone information
   DWORD dwTPTINDEX = pAnal->plTRIPHONETRAIN->Num();
   pAnal->plTRIPHONETRAIN->Add (&tptThis);

   // write this triphone prosody info, but only if first time throught
   if (!fRemoveExisting) {
      // BUGFIX - Used to be code for adjustind tptThisDuration here, but moved above
      pTTS->TriPhoneProsSet (dwPhone,
         m_fWordStartEndCombine ? 0 : wWordPos,
         ptr->pTPInst->bPhoneLeft, ptr->pTPInst->bPhoneRight,
         tptThis.dwDurationSRFeat,
         tptThis.iPitch,
         tptThis.iPitchDelta,
         tptThis.iPitchBulge,
         tptThis.fEnergyRatio * m_fWordEnergyAvg, fRemoveExisting);
   }


   // need to sort by the final score
   // NOTE: Sorting by the mid pitch
   for (i = 0; i < dwNum; i++) {
      ptr[i].fCompare = 0;

      for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
         ptr[i].fCompare += ptr[i].afRankCompare[PITCHFIDELITYCENTER][DURATIONFIDELITYCENTER][ENERGYFIDELITYCENTER][dwDemi] / (fp)TTSDEMIPHONES;
   } // i
            // using the mid pitch since used to determine what units are bad
   qsort (ptr, dwNum, sizeof(UNITRANK), UNITRANKSort);
   // This will make element 0 be the best

   // find thehighest score... which is a combination of all the factors
   for (i = 0; i < dwNum; i++)
      ptr[i].pTPInst->pPHONEAN->dwTRIPHONETRAINIndex = dwTPTINDEX;

#ifdef NOMODS_DISABLEBOTTOMSCORES
   // mark the worst ones as bad
#define BADPERCENT         4     // one out of four
   DWORD dwNumBad = (dwNum + BADPERCENT/2) / BADPERCENT;
   if (dwNum <= 1)
      dwNumBad = 0;  // dont mark any as bad
   else {
      // else, at least one bad
      dwNumBad = max(dwNumBad, 1);
      dwNumBad = min(dwNumBad, dwNum-1);  // can't all be bad
   }
   for (i = dwNum-dwNumBad; i < dwNum; i++) {
      pti = ptr[i].pTPInst;
      pti->pPHONEAN->fBad = TRUE;
   }
#endif

   // not used anymore because handled by chosing top N units
   // slight chance of an issue because phonean() stores a word number, but
   // i don't think this is being used anymore anhow
#if 0
   // what's the acceptable distance
   fp fPitchMax = 5; // m_dwTriPhonePitch ? pow (PITCHMAXFEW, 1.0 / (fp)m_dwTriPhonePitch) : 5;
   fp fPitchMin = 1.0 / fPitchMax;
   fp fDurMax =  5; // m_dwTriPhonePitch ? pow (DURATIONMAXFEW, 1.0 / (fp)m_dwTriPhonePitch) : 5;
   fp fDurMin = 1.0 / fDurMax;

   // loop through the top 1/4 and potentially add
   CListFixed lAdded;
   lAdded.Init (sizeof(UNITRANK));
   PUNITRANK pAdded = (PUNITRANK) lAdded.Get(0);
   for (i = 0; i < (dwNum+3)/4; i++) {
      // determine the L and R pitch of this
      fp fDelta = sqrt(ptr[i].fPitchVar);
      fp fPitchLeft = ptr[i].fPitch / fDelta;
      fp fPitchRight = ptr[i].fPitch * fDelta;
      fp fDuration = ptr[i].dwDuration;

      // compare to what already have
      for (j = 0; j < lAdded.Num(); j++) {
         fDelta = sqrt(pAdded[j].fPitchVar);
         fp fPitchLeft2 = pAdded[j].fPitch / fDelta;
         fp fPitchRight2 = pAdded[j].fPitch * fDelta;
         fp fDuration2 = pAdded[j].dwDuration;

         if ((fPitchLeft < fPitchLeft2 * fPitchMin) || (fPitchLeft > fPitchLeft2 * fPitchMax))
            continue;   // lower or higher pitch
         if ((fPitchRight < fPitchRight2 * fPitchMin) || (fPitchRight > fPitchRight2 * fPitchMax))
            continue;   // lower or higher pitch
         if ((fDuration < fDuration2 * fDurMin) || (fDuration > fDuration2 * fDurMax))
            continue;   // lower or higher pitch
         break;   // found one with close match so dont want to add
      } // j
      if (i && (j < lAdded.Num()))
         continue;   // already added

      // if get here then add it
      lAdded.Add (&ptr[i]);
      pAdded = (PUNITRANK) lAdded.Get(0); // just in case realloced

      // else, found phoneme with least error
      pti = ptr[i].pTPInst;
      if (!UsePHONEAN (pAnal, pti->dwWave, pti->dwPhoneIndex, pti->pPHONEAN, pTTS,
         fNow, -1, FALSE))
         return FALSE;
   } // i
#endif // 0


// #define WRITERESULTS       // set this to write triphone results to a file
#ifdef WRITERESULTS
   WCHAR szName[256];
   if (!dwNum)
      return TRUE;   // shouldnt happen
   PLEXPHONE plpLeft = pLex->PhonemeGetUnsort (ptr[0].pTPInst->bPhoneLeft);
   PLEXPHONE plpRight = pLex->PhonemeGetUnsort (ptr[0].pTPInst->bPhoneRight);
   PLEXPHONE plpCenter = pLex->PhonemeGetUnsort ((BYTE)dwPhone);
   swprintf (szName, L"c:\\temp\\TriPhone-%s-%s-%s-%d.wav",
      plpCenter->szPhone, plpLeft->szPhone, plpRight->szPhone, (int)wWordPos);
   for (i = 0; szName[i]; i++) {
      if ((szName[i] == L'<') || (szName[i] == L'>'))
         szName[i] = L's'; // around <s>
   }
   CM3DWave Wave;
   WideCharToMultiByte (CP_ACP, 0, szName, -1, Wave.m_szFile, sizeof(Wave.m_szFile), 0, 0);
   Wave.ConvertSamplesAndChannels (22050, 1, NULL);
   Wave.m_dwSRSkip = Wave.m_adwPitchSkip[PITCH_F0] = Wave.m_dwSamplesPerSec / Wave.m_dwSRSAMPLESPERSEC;
   Wave.BlankSRFeatures();

   // loop through all the candidates and paste them in
#define EXTRASILENCE       2
   BYTE abBuf[sizeof(WVWORD)+256];
   DWORD k;
   PLEXPHONE plpSilence = pLex->PhonemeGetUnsort(pLex->PhonemeFindUnsort(pLex->PhonemeSilence()));
   for (i = 0; i < min(dwNum, 50); i++) {
      pti = ptr[i].pTPInst;

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(pti->dwWave));

      PSRFEATURE psrf = CacheSRFeatures (pwa->m_pWave, ptr[i].dwTimeStart, ptr[i].dwTimeEnd);
      if (!psrf)
         continue;
      DWORD dwNumSRF = ptr[i].dwTimeEnd - ptr[i].dwTimeStart;
      DWORD dwNumSamples = dwNumSRF * Wave.m_dwSRSkip;
      DWORD dwStartWriting = Wave.m_dwSamples / Wave.m_dwSRSkip;

      // allocate enough
      if (!Wave.AppendPCMAudio (NULL, dwNumSamples, TRUE))
         continue;

      // copy over
      memcpy (Wave.m_paSRFeature + dwStartWriting, psrf, dwNumSRF * sizeof(SRFEATURE));
      memcpy (Wave.m_apPitch[PITCH_F0] + dwStartWriting, pwa->m_pWave->m_apPitch[PITCH_F0] + ptr[i].dwTimeStart, dwNumSRF * sizeof(WVPITCH));

      // add some extra silence
      DWORD dwSilenceLoc = Wave.m_dwSamples / Wave.m_dwSRSkip;
      Wave.AppendPCMAudio (NULL, Wave.m_dwSRSkip * EXTRASILENCE, TRUE);
      for (j = dwSilenceLoc; j < Wave.m_dwSRSamples; j++) {
         Wave.m_apPitch[PITCH_F0][j].fFreq = pAnal->fAvgPitch;
         Wave.m_apPitch[PITCH_F0]m_apPitch[PITCH_F0][j].fStrength = 1;

         for (k = 0; k < SRDATAPOINTS; k++)
            Wave.m_paSRFeature[j].acVoiceEnergy[k] = Wave.m_paSRFeature[j].acNoiseEnergy[k] = -110;
      }

      // add the word
      PWVWORD pww = (PWVWORD)&abBuf[0];
      memset (pww, 0, sizeof(*pww));
      pww->dwSample = dwStartWriting * Wave.m_dwSRSkip;
      PWSTR psz = (PWSTR)(pww+1);
      swprintf (psz, L"s=%.3g w=%d p=%d", (double)ptr[i].fRankCompare,
         (int)pti->dwWave, (int)pti->dwPhoneIndex);
      Wave.m_lWVWORD.Add (pww, sizeof(*pww) + (wcslen(psz)+1)*sizeof(WCHAR));

      memset (pww, 0, sizeof(*pww)+sizeof(WCHAR));
      pww->dwSample = dwSilenceLoc * Wave.m_dwSRSkip;
      Wave.m_lWVWORD.Add (pww, sizeof(*pww) + (wcslen(psz)+1)*sizeof(WCHAR));

      // add the phoneme
      WVPHONEME wp;
      memset (&wp, 0, sizeof(wp));
      wp.dwSample = dwStartWriting * Wave.m_dwSRSkip;
      wp.dwEnglishPhone = plpCenter->bEnglishPhone;
      memcpy (wp.awcName, plpCenter->szPhone, min(sizeof(wp.awcName), wcslen(plpCenter->szPhone)*sizeof(WCHAR)));
      Wave.m_lWVPHONEME.Add (&wp);

      memset (&wp, 0, sizeof(wp));
      wp.dwSample = dwSilenceLoc * Wave.m_dwSRSkip;
      wp.dwEnglishPhone = plpSilence->bEnglishPhone;
      memcpy (wp.awcName, plpSilence->szPhone, min(sizeof(wp.awcName), wcslen(plpSilence->szPhone)*sizeof(WCHAR)));
      Wave.m_lWVPHONEME.Add (&wp);
   } // i

   // actually synthesize
   CVoiceSynthesize vs;
   vs.SynthesizeFromSRFeature (&Wave, NULL, NULL, NULL, FALSE);

   // save
   Wave.Save (TRUE, NULL);
#endif // WRITERESULTS

   // else done
   return TRUE;
}



/*************************************************************************************
CTTSWork::AnalysisAllTriPhones - This scans training information to learn all triphones.
It assumes the current CMTTS is blank and has no triphones stored in it.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisAllTriPhones (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k, l;
   CListFixed lDone;
   lDone.Init (sizeof(WORD));
   for (i = 0; i < dwNumPhone; i++) for (j = 0; j < 4; j++) {
      PCListFixed plLook = pAnal->paplTriPhone[j][i];
      _ASSERTE (!m_fWordStartEndCombine || !j || !plLook || !plLook->Num());
      if (!plLook)
         continue;

      // update
      pProgress->Update ((fp)(i * 3 + j) / (fp)(dwNumPhone*3));

      // loop through all phone instances listed and see if want to use
      lDone.Clear();
      PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
      for (k = 0; k < plLook->Num(); k++, pti++) {
         // see if already done
         WORD *paw = (WORD*) lDone.Get(0);
         for (l = 0; l < lDone.Num(); l++)
            if (paw[l] == pti->wTriPhone)
               break;
         if (l < lDone.Num())
            continue;   // already done

         // else add to done list
         lDone.Add (&pti->wTriPhone);

         // do it
         if (!AnalysisTriPhone (pAnal, pTTS, i, (WORD)j, pti->wTriPhone, FALSE, FALSE))
            return FALSE;
      } // k, over plLook
   } // i, j

   return TRUE;
}


/*************************************************************************************
CTTSWork::AnalysisWriteTriPhones - Looks through all the wave files and writes any
triphones that are marked as being desired.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisWriteTriPhones (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress)
{
   DWORD i, j;
   CM3DWave WavePCM;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      CacheSRFeatures (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples); // note: dont need to counteract pitch brightness here

      if (m_dwPCMCompress) {
         if (!WavePCM.Open (NULL, pwa->m_pWave->m_szFile))
            return FALSE;  // error
      }

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if (!ppa->dwWantInFinal)
            continue;

         // else want it
         if (!UsePHONEAN (pAnal, i, j, ppa, j ? &ppa[-1] : NULL, pTTS, TRUE, ppa->dwLexWord, FALSE,
            m_dwPCMCompress ? &WavePCM : NULL))
            return FALSE;
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i

   // compress TTS
   pTTS->TTSWaveCompress();

   return TRUE;
}


#if 0 // not used anymore
/*************************************************************************************
CTTSWork::AnalysisWord - Analyzes and produces the triphones for a particular
word.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   DWORD          dwWord - Word index, into m_pLexWords->WordGet()
   BOOL           fRemoveExisting - Set to TRUE if there might be an existing triphone
   BOOL           fNow - If TRUE then modify the triphone database right away.
                  Otherwise, set a flag as to what phoneme is needed (which is faster)
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisWord (PTTSANAL pAnal, PCMTTS pTTS, DWORD dwWord, BOOL fRemoveExisting,
                             BOOL fNow)
{
   if (fRemoveExisting)
      pTTS->TriPhoneClearWord (dwWord);

   // the the list with the word
   PCListFixed plLook = pAnal->paplWords[dwWord];
   if (!plLook)
      return FALSE;
   PWORDINST pwi = (PWORDINST) plLook->Get(0);

   // find the most common form
#define MAXFORM      10
   DWORD adwFormCount[MAXFORM];
   memset (adwFormCount, 0, sizeof(adwFormCount));
   DWORD i;
   for (i = 0; i < plLook->Num(); i++)
      if (pwi[i].pWORDAN->dwForm < MAXFORM)
         adwFormCount[pwi[i].pWORDAN->dwForm]++;
   DWORD dwBestForm = 0;
   for (i = 1; i < MAXFORM; i++)
      if (adwFormCount[i] > adwFormCount[dwBestForm])
         dwBestForm = i;
   if (!adwFormCount[dwBestForm])
      return FALSE;  // none, shouldnt happen

   // BUGFIX - If there aren't enough copies of the word then done
   if (adwFormCount[dwBestForm] < MINNUMEXAMPLES)
      return TRUE;


   // get this pronunciation...
   WCHAR szWord[256];
   if (!m_pLexWords->WordGet (dwWord, szWord, sizeof(szWord), NULL))
      return FALSE;  // shouldnt happen
   PCMLexicon pLex = Lexicon();
   CListVariable lForm, lDontRecurse;
   pLex->WordPronunciation (szWord, &lForm, FALSE, NULL, &lDontRecurse);
   PBYTE pbForm = (PBYTE)lForm.Get(dwBestForm);
   if (!pbForm)
      return FALSE;  // shouldnt happen
   pbForm++;   // so skip pos
   DWORD dwLen = strlen((char*)pbForm);

   // figure out the word score for each instance...
   CListFixed lUNITRANK;
   lUNITRANK.Init (sizeof(UNITRANK));
   DWORD dwInst;
   UNITRANK ur;
   memset (&ur, 0, sizeof(ur));
   double f;
   for (dwInst = 0; dwInst < plLook->Num(); dwInst++) {
      // BUGFIX - Mark as being not best
      pwi[dwInst].fUsedAsBest = FALSE;

      if (pwi[dwInst].pWORDAN->dwForm != dwBestForm)
         continue;
      if (pwi[dwInst].pWORDAN->dwPhoneEnd - pwi[dwInst].pWORDAN->dwPhoneStart != dwLen)
         continue;   // shouldn happen, but test

      // get the wave
      PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(pwi[dwInst].dwWave);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      if (!pwa)
         continue;

      // dont add the word if any of the phonemes are missing triphpone information
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (i = pwi[dwInst].pWORDAN->dwPhoneStart; i < pwi[dwInst].pWORDAN->dwPhoneEnd; i++)
         if (ppa[i].dwTRIPHONETRAINIndex == -1)
            break;
      if (i < pwi[dwInst].pWORDAN->dwPhoneEnd)
         continue;

      // the score of the sum of all the scores of the phonemes
      f = 0;
      ur.fPitch = 0;
      ur.dwDuration = 0;
      BOOL fTooHigh = FALSE;
      for (i = pwi[dwInst].pWORDAN->dwPhoneStart; i < pwi[dwInst].pWORDAN->dwPhoneEnd; i++) {
         f += ppa[i].fRankCompare;
            // BUGFIX - Was multiplying final ranks together, but cos of value works better
         if (ppa[i].fRankCompare >= UNDEFINEDRANK)
            fTooHigh = TRUE;
         ur.fPitch += ppa[i].fPitch;
         ur.dwDuration += ppa[i].dwDuration;
      }
      if (fTooHigh)
         continue;
      ur.fPitch /= (fp)(pwi[dwInst].pWORDAN->dwPhoneEnd - pwi[dwInst].pWORDAN->dwPhoneStart);

      if (BlacklistExist (pwi[dwInst].dwWave, FALSE, pwi[dwInst].pWORDAN->dwPhoneStart))
         f += 1000;

      ur.fCompare = f;
      ur.pWordInst = &pwi[dwInst];

      lUNITRANK.Add (&ur);

   } // dwInst

   // sort the list
   PUNITRANK ptr = (PUNITRANK)lUNITRANK.Get(0);
   DWORD dwNum = lUNITRANK.Num();
   qsort (ptr, dwNum, sizeof(UNITRANK), UNITRANKSort);

   // calculate amount of pitch up/down and duration that's acceptable
   fp fPitchMax = 5; // m_dwTriPhonePitch ? pow (PITCHMAXFEW, 1.0 / (fp)m_dwTriPhonePitch) : 5;
   fPitchMax = sqrt(fPitchMax);  // since will contain several phones
   fp fPitchMin = 1.0 / fPitchMax;
   fp fDurMax =  5; // m_dwTriPhonePitch ? pow (DURATIONMAXFEW, 1.0 / (fp)m_dwTriPhonePitch) : 5;
   fDurMax = sqrt(fDurMax);   // since will contain several phones
   fp fDurMin = 1.0 / fDurMax;

   CListFixed lAdded;
   DWORD j;
   lAdded.Init (sizeof(UNITRANK));
   PUNITRANK pAdded = (PUNITRANK) lAdded.Get(0);

   // add all the ones with enough difference
   for (i = 0; i < (dwNum+3)/4; i++) {
      // compare to what already have
      for (j = 0; j < lAdded.Num(); j++) {
         if ((ptr[i].fPitch < pAdded[j].fPitch * fPitchMin) || (ptr[i].fPitch > pAdded[j].fPitch * fPitchMax))
            continue;   // lower or higher pitch
         if (((fp)ptr[i].dwDuration < (fp)pAdded[j].dwDuration * fDurMin) || ((fp)ptr[i].dwDuration > (fp)pAdded[j].dwDuration * fDurMax))
            continue;   // lower or higher duration
         break;   // found a close match, so dont add
      } // j
      if (i && (j < lAdded.Num()))
         continue;   // already added

      // if get here then add it
      lAdded.Add (&ptr[i]);
      pAdded = (PUNITRANK) lAdded.Get(0); // just in case realloced

      PWORDINST pw = ptr[i].pWordInst;

      // get the wave
      PCWaveAn *ppwa = (PCWaveAn*)pAnal->plPCWaveAn->Get(pw->dwWave);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      if (!pwa)
         return FALSE;

      // loop over all the phonemes and add them
      // NOTE: This is adding the phonemes from the same word recording so dont
      // end up with a disjoint in quality
      DWORD dwPhone;
      PPHONEAN ppa = (PPHONEAN)pwa->m_lPHONEAN.Get(0);
      for (dwPhone = pw->pWORDAN->dwPhoneStart; dwPhone < pw->pWORDAN->dwPhoneEnd; dwPhone++) {
         if (!UsePHONEAN (pAnal, pw->dwWave, dwPhone, ppa + dwPhone, pTTS, fNow, dwWord,
            fRemoveExisting))
            return FALSE;
      } // dwPhone

      // BUGFIX - Mark as used word
      pw->fUsedAsBest = TRUE;
   } // i ones to add


   // done
   return TRUE;
}
#endif // 0


#if 0 // not used anymore
/*************************************************************************************
CTTSWork::AnalysisAllWords - This scans training information to learn all words.
It assumes the current CMTTS is blank and has no triphones stored in it.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::AnalysisAllWords (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress)
{
   DWORD dwNum = m_pLexWords->WordNum();
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      if (pProgress && ((i % 5) == 0))
         pProgress->Update ((fp)i / (fp)dwNum);

      // if nothing there ignore
      PCListFixed plLook = pAnal->paplWords[i];
      if (!plLook)
         continue;   // not found in recording

      // else, go for it
      if (!AnalysisWord (pAnal, pTTS, i, FALSE, FALSE))
         return FALSE;
   } // i

   return TRUE;
}
#endif // 0



#if 0 // no longer used

/*************************************************************************************
CTTSWork::AnalysisMultiUnit - Looks through the voice for good multiple units
to keep.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/

// ANMU - for sorting units
typedef struct {
   float          fScore;           // total score, higher is better
   PPHONEAN       pPhoneAn;         // phoneme where the word starts
   WORD           wWaveIndex;       // wave number
   WORD           wPhoneIndex;      // phoneme index
   WORD           wUnits;           // number of units, 2 or 3
} ANMU, *PANMU;

static int _cdecl ANMUSort (const void *elem1, const void *elem2)
{
   ANMU *pdw1, *pdw2;
   pdw1 = (ANMU*) elem1;
   pdw2 = (ANMU*) elem2;

   if (pdw1->fScore < pdw2->fScore)
      return 1;
   else if (pdw1->fScore > pdw2->fScore)
      return -1;
   else
      return 0;
}

BOOL CTTSWork::AnalysisMultiUnit (PTTSANAL pAnal, PCMTTS pTTS)
{
   if (!m_dwMultiUnit)
      return TRUE;   // nothing to test

   // figure out how much memory need to allocated
   DWORD dwTotalPhone = 0;
   DWORD i, j;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      dwTotalPhone += pwa->m_lPHONEAN.Num();
   } // i
   dwTotalPhone = dwTotalPhone * 2 + // since will have 2 units per
      dwTotalPhone * 3; // for tripple units

   // allocate enough memory
   CMem mem;
   if (!mem.Required (dwTotalPhone * sizeof(ANMU)))
      return FALSE;
   PANMU pa = (PANMU)mem.p;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // loop through and fill them in
   DWORD dwCount = 0;
   DWORD k, dwUnits;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j < dwNum; j++, ppa++) {
         for (dwUnits = 2; dwUnits < 4; dwUnits++) {
            if (j + dwUnits > dwNum)
               continue;   // not long enough sequence for this wave

            // if there aren't enough versions then don't add
            PTRIPHONETRAIN pt;
            DWORD dwKeeper = 0;
            BOOL fFoundPlosive = FALSE;
            fp fScore = 0;
            for (k = 0; k < dwUnits; k++) {
               // make not silence
               if ((ppa[k].bPhone == bSilence) || !ppa[k].pWord)
                  break;

               // must be entirely within a word
               // if bad word location then error
               if (k && !m_fWordStartEndCombine && (ppa[k].bWordPos & 0x01)) // start of word anyplace but beginning
                  break;
               if ((k < dwUnits-1) && !m_fWordStartEndCombine && (ppa[k].bWordPos & 0x02))   // end of word anyplace but end
                  break;

               pt = GetTRIPHONETRAIN (this, pAnal, ppa + k);
               if (pt->dwCountScale < MINNUMEXAMPLES * (NUMFUNCWORDGROUP+1))
                  break;

               // if score too high then exit
               if (ppa[k].fRankCompare >= UNDEFINEDRANK)
                  break;

               if (ppa[k].fWantInFinal)
                  dwKeeper++;

               fScore += ppa[k].fRankCompare;

               // BUGFIX - produce slightly higher score if found a plosive
               // in the lot, so that encourage units with plosives, since
               // plosives tend to be a problem
               if (!fFoundPlosive) {
                  PLEXPHONE plp = pLex->PhonemeGetUnsort (ppa[k].bPhone);
                  PLEXENGLISHPHONE pe = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
                  if (pe && (pe->dwCategory & PIC_PLOSIVE))
                     fFoundPlosive = TRUE;
               } // check for plosives
            } // k

            if (k < dwUnits)
               continue;   // doesn't meet conditions

            // if both are marked as keepers don't bother
            if (dwKeeper == dwUnits)
               continue;

            // keep this
            pa->pPhoneAn = ppa;
            pa->wWaveIndex = (WORD) i;
            pa->wPhoneIndex = (WORD) j;
            pa->wUnits = (WORD)dwUnits;
            pa->fScore = -fScore / (fp)dwUnits;
            
            // if found a plosive then increase the score so more likely to be chosen
            if (fFoundPlosive)
               pa->fScore += 6;

            pa++;
            dwCount++;
         } // dwUnits
      } // j
   } // i

   // sort this list
   pa = (PANMU)mem.p;
   qsort (pa, dwCount, sizeof(ANMU), ANMUSort);

   // loop through again, setting flags
   DWORD dwLeft = m_dwMultiUnit;
   for (i = 0; dwLeft && (i < dwCount); i++, pa++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(pa->wWaveIndex);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(pa->wPhoneIndex);

      // if both marked already then dont bother
      BOOL fKeeper = FALSE;
      for (k = 0; k < (WORD)pa->wUnits; k++) {
         if (ppa[k].fWantInFinal)
            continue;   // already flagged as true

         // set to TRUE
         ppa[k].fWantInFinal = TRUE;
         fKeeper = TRUE;
      } // k

      // else, note that used up one slot
      if (fKeeper)
         dwLeft--;
   } // i

   return TRUE;
}
#endif // 0



#if 0 // no longer used
/*************************************************************************************
CTTSWork::AnalysisWordSyllableUnit - Makes sure the individual syllables from the
required words are included.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/


BOOL CTTSWork::AnalysisWordSyllableUnit (PTTSANAL pAnal, PCMTTS pTTS)
{
   DWORD dwNum = m_pLexWords->WordNum();
   DWORD dwWord, dwForm, dwSyllable, i, j, k;
   WCHAR szWord[64];
   CListVariable lForm, lDontRecurse;
   CListFixed lBoundary;
   PCMLexicon pLex = pTTS->Lexicon();
   for (dwWord = 0; dwWord < dwNum; dwWord++) {
      lForm.Clear();
      if (!m_pLexWords->WordGet (dwWord, szWord, sizeof(szWord), &lForm))
         continue;

      lDontRecurse.Clear();
      lForm.Clear();
      pLex->WordPronunciation (szWord, &lForm, FALSE, NULL, &lDontRecurse);

      // look over all the forms
      for (dwForm = 0; dwForm < lForm.Num(); dwForm++) {
         PBYTE pabPron = ((PBYTE)lForm.Get(dwForm))+1;
         lBoundary.Clear();
         pLex->WordSyllables (pabPron, szWord, &lBoundary);

         DWORD *padwBoundary = (DWORD*)lBoundary.Get(0);
         for (dwSyllable = 0; dwSyllable < lBoundary.Num(); dwSyllable++) {
            DWORD dwStart = dwSyllable ? (padwBoundary[dwSyllable-1] & 0xffff) : 0;
            PBYTE pbPronSyllable = pabPron + dwStart;
            DWORD dwSyllableLen = (padwBoundary[dwSyllable] & 0xffff) - dwStart;
            
            // if the syllable is only one unit then dont bother
            if (dwSyllableLen <= 1)
               continue;

            PPHONEAN ppaBest = NULL;
            fp fScoreBest = 0;
            DWORD dwCount = 0;

            // loop through all the waves looking for this
            for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
               PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
               PCWaveAn pwa = ppwa[0];

               PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
               PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
               DWORD dwNumSyl = pwa->m_lSYLAN.Num();
               if (!dwNumSyl)
                  continue;

               for (j = 0; j < dwNumSyl; j++, psa++) {
                  // must be right length
                  if (psa->dwPhoneEnd - psa->dwPhoneStart != dwSyllableLen)
                     continue;

                  // make sure there's a match
                  for (k = psa->dwPhoneStart; k < psa->dwPhoneEnd; k++) {
                     if (ppa[k].bPhone != pbPronSyllable[k-psa->dwPhoneStart])
                        break;

                     // BUGFIX - if rank too highthen not tested, so toss out
                     if (ppa[k].fRankCompare >= UNDEFINEDRANK)
                        break;
                  }
                  if (k < psa->dwPhoneEnd)
                     continue;   // no match

                  // else, match, so determine score
                  fp fScore = 0;
                  for (k = psa->dwPhoneStart; k < psa->dwPhoneEnd; k++)
                     fScore += ppa[k].fRankCompare;
                  dwCount++;  // so know how many

                  // see if it's a best match
                  if (!ppaBest || (fScore < fScoreBest)) {
                     // found good match
                     ppaBest = &ppa[psa->dwPhoneStart];
                     fScoreBest = fScore;
                  }
               } // j
            } // i, over all waves

            // if there's a best match then mark it
            if (ppaBest && (dwCount >= MINNUMEXAMPLES)) for (i = 0; i < dwSyllableLen; i++)
               ppaBest[i].fWantInFinal = TRUE;
         } // dwSyllable
      } // dwForm

   } // dwWord


   return TRUE;
}

#endif // 0



#if 0 // no longer used
/*************************************************************************************
CTTSWork::AnalysisSyllableUnit - Looks through the voice for the top N syllables
to keep.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/

static int _cdecl ANMUSortSyllable (const void *elem1, const void *elem2)
{
   ANMU *pdw1, *pdw2;
   pdw1 = (ANMU*) elem1;
   pdw2 = (ANMU*) elem2;

   // first, sort by syllable name
   if (pdw1->wUnits != pdw2->wUnits)
      return (int)pdw1->wUnits - (int)pdw2->wUnits;

   // else, compare phonemes, so must all match
   DWORD i;
   for (i = 0; i < (DWORD)pdw1->wUnits; i++)
      if (pdw1->pPhoneAn[i].bPhone != pdw2->pPhoneAn[i].bPhone)
         return (int)(WORD)pdw1->pPhoneAn[i].bPhone - (int)(WORD)pdw2->pPhoneAn[i].bPhone;

   // now compare score
   if (pdw1->fScore < pdw2->fScore)
      return 1;
   else if (pdw1->fScore > pdw2->fScore)
      return -1;
   else
      return 0;
}

BOOL CTTSWork::AnalysisSyllableUnit (PTTSANAL pAnal, PCMTTS pTTS)
{
   if (!m_dwMultiSyllableUnit)
      return TRUE;   // nothing to test

   // figure out how much memory need to allocated
   DWORD dwTotalPhone = 0;
   DWORD i, j;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      dwTotalPhone += pwa->m_lSYLAN.Num();
   } // i

   // allocate enough memory
   CMem mem;
   if (!mem.Required (dwTotalPhone * sizeof(ANMU)))
      return FALSE;
   PANMU pa = (PANMU)mem.p;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // loop through and fill them in
   DWORD dwCount = 0;
   DWORD k;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      DWORD dwNum = pwa->m_lSYLAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j < dwNum; j++, psa++) {
         // if only one unit then dont bother
         if (psa->dwPhoneEnd <= psa->dwPhoneStart+1)
            continue;

         fp fScore = 0;
         for (k = psa->dwPhoneStart; k < psa->dwPhoneEnd; k++)
            fScore += ppa[k].fRankCompare;

         // BUGFIX - not if uncommon
         if (fScore >= UNDEFINEDRANK)
            continue;

         // if ends up being 0 score then skip
         //if (!fScore)
         //   continue;

         // keep this
         pa->pPhoneAn = ppa + psa->dwPhoneStart;
         pa->wWaveIndex = (WORD) i;
         pa->wPhoneIndex = (WORD) psa->dwPhoneStart;
         pa->wUnits = (WORD)(psa->dwPhoneEnd - psa->dwPhoneStart);
         pa->fScore = -fScore / (fp)pa->wUnits;
         
         pa++;
         dwCount++;
      } // j
   } // i

   // sort this list
   pa = (PANMU)mem.p;
   qsort (pa, dwCount, sizeof(ANMU), ANMUSortSyllable);

   // loop through the sorted list and get rid of duplicates
   PANMU pFrom = (PANMU)mem.p;
   PANMU pTo = pFrom;
   DWORD dwKept = 0;
   for (i = 0; i < dwCount; i++, pFrom++) {
      // find how how many times this is duplicated
      for (j = 1; j < dwCount-i; j++) {
         // make sure that same units
         if (pFrom[j].wUnits != pFrom->wUnits)
            break;

         for (k = 0; k < pFrom->wUnits; k++)
            if (pFrom[j].pPhoneAn[k].bPhone != pFrom->pPhoneAn[k].bPhone)
               break;      // found a mismatch in phonemes
         if (k < pFrom->wUnits)
            break;   // found a mismatch in phonemes
      } // j

      // when get here, j indicates number of times repeated
      if (j < MINIMUMSYLLABLES) {
         // this doesnt occur enough times to be statistically repliable
         i += (j-1);
         pFrom += (j-1);
         continue;
      }

      // else, keep this
      if (pFrom != pTo)
         memcpy (pTo, pFrom, sizeof(*pFrom));
      pTo->fScore = j;  // score based on number of occurences, not how good a quality
      pTo++;
      dwKept++;

      // increase pointers
      pFrom += (j-1);
      i += (j-1);
   } // i
   dwCount = dwKept;

   // resort based on score
   qsort (pa, dwCount, sizeof(ANMU), ANMUSort);

   // loop through again, setting flags
   for (i = 0; i < min(m_dwMultiSyllableUnit, dwCount); i++, pa++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(pa->wWaveIndex);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(pa->wPhoneIndex);

      // if both marked already then dont bother
      BOOL fKeeper = FALSE;
      for (k = 0; k < (WORD)pa->wUnits; k++) {
         // set to TRUE
         ppa[k].fWantInFinal = TRUE;
      } // k
   } // i

   return TRUE;
}
#endif // 0



#if 0 // no longer used
/*************************************************************************************
CTTSWork::AnalysisConnectUnits - Finds units in between kept units and keeps them
to keep.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCProgressSocket pProgress - To indicate progress
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisConnectUnits (PTTSANAL pAnal, PCMTTS pTTS)
{
   if (!m_dwConnectUnits)
      return TRUE;   // nothing to test

   // connect them
   DWORD i, j;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // loop through and fill them in
   DWORD k, m;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j+1 < dwNum; j++, ppa++) { // NOTE: using j+1 so safe to test for next one
         // if NOT marked as keeper or is silence then dont bother
         if ((ppa->bPhone == bSilence) || !ppa->fWantInFinal)
            continue;

         // see how long until get to next keeper
         DWORD dwMax = min(dwNum-j, m_dwConnectUnits+1);
         BOOL fCantUse = FALSE;
         for (k = 1; k < dwMax; k++) {
            if (ppa[k].bPhone == bSilence) {
               // found silence
               fCantUse = TRUE;
               continue;
            }
            
            // if found want in final then break
            if (ppa[k].fWantInFinal)
               break;

            // if score too low then cant use
            // NOTE: Just picking 30 dB as a cutoff. Not too much testing went into this
            if (ppa[k].fRankCompare > 15) {  // BUGFIX - Changed to 15, from 30
               fCantUse = TRUE;
               break;   // too low a score for the unit, so skip
            }
         } // k
         if ((k >= dwMax) || (k <= 1))
            fCantUse = TRUE;  // nothing found before the end, or no separation


         // if can use it, then set to want
         if (!fCantUse) for (m = 1; m < k; m++)
            ppa[m].fWantInFinal = TRUE;

         // next
         j += (k-1);
         ppa += (k-1);
      } // j
   } // i

   return TRUE;
}
#endif // 0



#if 0 // no longer used

/*************************************************************************************
CTTSWork::AnalysisDiphones - Finds the best match for each diphone and keeps

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
returns
   BOOL - TRUE if success
*/

// ANDI - Diphone analysis info. best one
typedef struct {
   DWORD       dwCount;    // number found
   fp          fScore;     // best score
   PPHONEAN    ppa;        // phonean for the first best element. Must be a second at ppa[1]
} ANDI, *PANDI;

BOOL CTTSWork::AnalysisDiphones (PTTSANAL pAnal, PCMTTS pTTS)
{
   // connect them
   DWORD i, j;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   DWORD dwNumPhone = pLex->PhonemeNum();
   DWORD dwNumPhonePlusOne = dwNumPhone + 1;

   // allocate enough memory
   DWORD dwNeed = dwNumPhonePlusOne * dwNumPhonePlusOne * 2 * sizeof(ANDI);
   CMem mem;
   if (!mem.Required (dwNeed))
      return FALSE;
   PANDI pandi = (PANDI) mem.p;
   memset (pandi, 0, dwNeed);

   // loop through and fill them in
   PANDI pandiCur;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j+1 < dwNum; j++, ppa++) { // NOTE: using j+1 so safe to test for next one
         // figure out which index
         pandiCur = pandi + ((min(ppa[0].bPhone,dwNumPhone) * dwNumPhonePlusOne + min(ppa[1].bPhone,dwNumPhone)) * 2
            + ((ppa->bWordPos & 0x02) ? 1 : 0));

         // average the scores
         fp fScore = 0;
         if (ppa[0].bPhone != bSilence)
            fScore += ppa[0].fRankCompare;
         if (ppa[1].bPhone != bSilence)
            fScore += ppa[1].fRankCompare;

         // BUGFIX - if ranked too high then skip
         if (fScore >= UNDEFINEDRANK)
            continue;

         // increase counter
         pandiCur->dwCount += 1;

         // remember best
         if (!pandiCur->ppa || (fScore < pandiCur->fScore)) {
            pandiCur->ppa = ppa;
            pandiCur->fScore = fScore;
         }
      } // j
   } // i

   // loop over all the diphone entries, and if there are enough, mark as keep
   for (i = 0, pandiCur = pandi; i < dwNumPhonePlusOne * dwNumPhonePlusOne * 2; i++, pandiCur++) {
      if ((pandiCur->dwCount < MINNUMEXAMPLES) || !pandiCur->ppa)
         continue;

      // mark as want
      if (pandiCur->ppa[0].bPhone != bSilence)
         pandiCur->ppa[0].fWantInFinal = TRUE;
      if (pandiCur->ppa[1].bPhone != bSilence)
         pandiCur->ppa[1].fWantInFinal = TRUE;
   } // i

   return TRUE;
}
#endif // 0



#if 0 // no longer used
/*************************************************************************************
CTTSWork::AnalysisTriPhoneGroup - Finds the best triphone using the generalized
groups/units. Do this in case chose higher quality TTS voice with triphones using
(un)stressed phonemes, and end up missing some of the larger group.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisTriPhoneGroup (PTTSANAL pAnal, PCMTTS pTTS)
{
   // connect them
   DWORD i, j;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD dwNumEntries = dwNumPhone * PIS_PHONEGROUPNUM * PIS_PHONEGROUPNUM * 4;

   // allocate enough memory
   DWORD dwNeed = dwNumEntries * sizeof(ANDI);
   CMem mem;
   if (!mem.Required (dwNeed))
      return FALSE;
   PANDI pandi = (PANDI) mem.p;
   memset (pandi, 0, dwNeed);

   // loop through and fill them in
   PANDI pandiCur;
   PLEXPHONE plp;
   PLEXENGLISHPHONE ple;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(1);  // NOTE - getting 1, since start at 1
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 1; j+1 < dwNum; j++, ppa++) { // NOTE: using j+1 so safe to test for next one, j=1 to start out
         // if this is silence then conitnue
         if (ppa[0].bPhone == bSilence)
            continue;

         // left group
         plp = pLex->PhonemeGetUnsort (ppa[-1].bPhone);
         if (!plp)
            continue;   // shouldnt happen
         ple = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
         if (!ple)
            continue;
         DWORD dwLeftGroup = PIS_FROMPHONEGROUP(ple->dwShape);

         // right group
         plp = pLex->PhonemeGetUnsort (ppa[1].bPhone);
         if (!plp)
            continue;   // shouldnt happen
         ple = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
         if (!ple)
            continue;
         DWORD dwRightGroup = PIS_FROMPHONEGROUP(ple->dwShape);

         // figure out which index
         pandiCur = pandi + (
            ((((DWORD)ppa[0].bPhone * PIS_PHONEGROUPNUM +
            dwLeftGroup) * PIS_PHONEGROUPNUM) +
            dwRightGroup) * 4 +
            (m_fWordStartEndCombine ? 0 : ((DWORD)ppa->bWordPos & 0x03)) );

         // BUGFIX - If score too high then continue
         if (ppa[0].fRankCompare >= UNDEFINEDRANK)
            continue;

         // increase counter
         pandiCur->dwCount += 1;

         // score is the center one
         fp fScore = ppa[0].fRankCompare;

         // remember best
         if (!pandiCur->ppa || (fScore < pandiCur->fScore)) {
            pandiCur->ppa = ppa;
            pandiCur->fScore = fScore;
         }
      } // j
   } // i

   // loop over all the diphone entries, and if there are enough, mark as keep
   for (i = 0, pandiCur = pandi; i < dwNumEntries; i++, pandiCur++) {
      if ((pandiCur->dwCount < MINNUMEXAMPLES) || !pandiCur->ppa)
         continue;

      // mark center one as want
      // BUGFIX - Disable this because handled elsehwere
      // pandiCur->ppa[0].fWantInFinal = TRUE;
   } // i

   return TRUE;
}
#endif // 0


/*************************************************************************************
CTTSWork::BlacklistRemove - Removes the given entry from the blacklist (if
it exists)

inputs
   DWORD          dwWave - Wave number
   BOOL           fIsPhone - Set to TRUE if it's a phoneme, FALSE if is a word
   DWORD          dwUnit - Unit number. if fIsPhone then it's a phoneme index into
                  the wave, if dwUnit==0 then it's a phoneme index to start of word
returns
   BOOL - TRUE if exists
*/
BOOL CTTSWork::BlacklistRemove (WORD wWave, BOOL fIsPhone, WORD wUnit)
{
   // structure
   PHONEBLACK pb;
   pb.wPhoneIndex = wUnit | (fIsPhone ? 0x8000 : 0);
   pb.wWaveIndex = wWave;

   DWORD i;
   DWORD dwNum = m_lPHONEBLACK.Num();
   PPHONEBLACK ppb = (PPHONEBLACK)m_lPHONEBLACK.Get(0);
   for (i = 0; i < dwNum; i++, ppb++)
      if (!memcmp(&pb, ppb, sizeof(pb))) {
         m_lPHONEBLACK.Remove (i);
         return TRUE;  // already there
      }

   return FALSE;
}



/*************************************************************************************
CTTSWork::BlacklistExist - Sees if an entry for the blacklist already exists (and hence
is blacklisted)

inputs
   DWORD          dwWave - Wave number
   BOOL           fIsPhone - Set to TRUE if it's a phoneme, FALSE if is a word
   DWORD          dwUnit - Unit number. if fIsPhone then it's a phoneme index into
                  the wave, if dwUnit==0 then it's a phoneme index to start of word
returns
   BOOL - TRUE if exists
*/
BOOL CTTSWork::BlacklistExist (WORD wWave, BOOL fIsPhone, WORD wUnit)
{
   // structure
   PHONEBLACK pb;
   pb.wPhoneIndex = wUnit | (fIsPhone ? 0x8000 : 0);
   pb.wWaveIndex = wWave;

   DWORD i;
   DWORD dwNum = m_lPHONEBLACK.Num();
   PPHONEBLACK ppb = (PPHONEBLACK)m_lPHONEBLACK.Get(0);
   for (i = 0; i < dwNum; i++, ppb++)
      if (!memcmp(&pb, ppb, sizeof(pb)))
         return TRUE;  // already there

   return FALSE;
}


/*************************************************************************************
CTTSWork::BlacklistAdd - Adds a new entry to the blacklist.

inputs
   DWORD          dwWave - Wave number
   BOOL           fIsPhone - Set to TRUE if it's a phoneme, FALSE if is a word
   DWORD          dwUnit - Unit number. if fIsPhone then it's a phoneme index into
                  the wave, if dwUnit==0 then it's a phoneme index to start of word
returns
   none
*/
void CTTSWork::BlacklistAdd (WORD wWave, BOOL fIsPhone, WORD wUnit)
{
   // if exist then ignore
   if (BlacklistExist (wWave, fIsPhone, wUnit))
      return;

   // structure
   PHONEBLACK pb;
   pb.wPhoneIndex = wUnit | (fIsPhone ? 0x8000 : 0);
   pb.wWaveIndex = wWave;

   // else add
   m_lPHONEBLACK.Add (&pb);
}


/*************************************************************************************
CTTSWork::BlacklistClearTriPhone - Clears a blacklist for a specific triphone.

inputs
   PTTSANAL       pAnal - Analysis information
   WORD           wPhone - Phoneme
   WORD           wWordPos - If at start/end of word
   WORD           wTriPhone - Triphone
*/
void CTTSWork::BlacklistClearTriPhone (PTTSANAL pAnal, WORD wPhone, WORD wWordPos, WORD wTriPhone)
{
   if (m_fWordStartEndCombine)
      wWordPos = 0;

   PCListFixed plLook = pAnal->paplTriPhone[m_fWordStartEndCombine ? 0 : (wWordPos%4)][(BYTE)wPhone];
   if (!plLook)
      return;

   PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
   DWORD k;
   for (k = 0; k < plLook->Num(); k++, pti++) {
      if (pti->wTriPhone != wTriPhone)
         continue;

      // else, know wave and location so clear
      BlacklistRemove ((WORD)pti->dwWave, TRUE, (WORD)pti->dwPhoneIndex);
   }

}


/*************************************************************************************
CTTSWork::BlacklistClearWord - Clears the blacklist of entries for the given word.

inputs
   PTTSANAL       pAnal - Analysis information
   DWORD          dwWord - Index into m_plLexWords
*/
void CTTSWork::BlacklistClearWord (PTTSANAL pAnal, DWORD dwWord)
{
   PCListFixed plLook = pAnal->paplWords[dwWord];
   if (!plLook)
      return;

   PWORDINST pti = (PWORDINST) plLook->Get(0);
   DWORD k;
   for (k = 0; k < plLook->Num(); k++, pti++) {
      BlacklistRemove ((WORD)pti->dwWave, FALSE, (WORD)pti->pWORDAN->dwPhoneStart);
   }
}


/*************************************************************************************
CTTSWork::BlacklistNumTriPhone - Returns the number of triphones on the blacklist.

inputs
   PTTSANAL       pAnal - Analysis information
   WORD           wPhone - Phoneme number, unsorted lex number
   WORD           wWordPos - 0 if middle, 1 if start, 2 if end, 3 if entire word
   BYTE           bPhoneLeft - Left phoneme
   BYTE           bPhoneRight - Right phoneme
   DWORD          *pdwCount - Filled in with the total count
*/
DWORD CTTSWork::BlacklistNumTriPhone (PTTSANAL pAnal, WORD wPhone, WORD wWordPos,
                                      BYTE bPhoneLeft, BYTE bPhoneRight,
                                      DWORD *pdwCount)
{
   if (m_fWordStartEndCombine)
      wWordPos = 0;

   *pdwCount = 0;
   DWORD dwCount = 0;

   PCListFixed plLook = pAnal->paplTriPhone[m_fWordStartEndCombine ? 0 : (wWordPos%4)][(BYTE)wPhone];
   if (!plLook)
      return 0;

   PTPHONEINST pti = (PTPHONEINST) plLook->Get(0);
   DWORD k;
   for (k = 0; k < plLook->Num(); k++, pti++) {
      if ((pti->bPhoneLeft != bPhoneLeft) || (pti->bPhoneRight != bPhoneRight))
         continue;

      // keep track of total
      pdwCount[0] = pdwCount[0] + 1;

      // else, know wave and location so clear
      if (BlacklistExist ((WORD)pti->dwWave, TRUE, (WORD)pti->dwPhoneIndex))
         dwCount++;
   }

   return dwCount;
}



/*************************************************************************************
CTTSWork::BlacklistNumWord - Returns the number of words on the blacklist.

inputs
   PTTSANAL       pAnal - Analysis information
   DWORD          dwWord - Index into m_plLexWords
   DWORD          *pdwCount - Filled in with the total count
*/
DWORD CTTSWork::BlacklistNumWord (PTTSANAL pAnal, DWORD dwWord, DWORD *pdwCount)
{
   pdwCount[0] = 0;
   DWORD dwCount = 0;

   PCListFixed plLook = pAnal->paplWords[dwWord];
   if (!plLook)
      return 0;

   PWORDINST pti = (PWORDINST) plLook->Get(0);
   DWORD k;
   for (k = 0; k < plLook->Num(); k++, pti++) {
      if (BlacklistExist ((WORD)pti->dwWave, FALSE, (WORD)pti->pWORDAN->dwPhoneStart))
         dwCount++;
   }
   pdwCount[0] = plLook->Num();

   return dwCount;
}


/*************************************************************************************
FindFileNameMinusPath - Find the file name withing the path.

inputs
   PWSTR          pszFile - File name
returns
   PWSTR - File name only, or pszFile if can't find a file name only
*/
PWSTR FindFileNameMinusPath (PWSTR pszFile)
{
   while (TRUE) {
      PWSTR psz = wcschr (pszFile, L'\\');
      if (!psz)
         break;
      // else
      pszFile = psz+1;
   }

   return pszFile;
}

/*************************************************************************************
ResolvePathIfNotExist - Tries to find the lexicon (or whatever) file. Because the file is hardcoded,
if it can't find the file then it looks in the directory where the work/sr is.

inputs
   PWSTR          pszLexicon - Lexicon file as stored away. If this fails then
                     several other locationes are tried. This will be modified
                     if successful
   PWSTR          pszSrc - Source file (such as .tts). The path of this is used.
returns
   BOOL - TRUE if found file, FALSE if not
*/
BOOL ResolvePathIfNotExist (PWSTR pszLexicon, PWSTR pszSrc)
{
   PMEGAFILE f;

   // do check so dont get assertion when do fopen
   if (!pszLexicon[0])
      return FALSE;

   // see if file exists straight off
   PCMegaFile pmf = MegaFileGet();
   if (pmf) {
      // NOTE: Not tested
      if (pmf->Exists (pszLexicon))
         // NOTE: not tested
         return TRUE;
   }

   // BUGFIX - Whether or not megafile, try opening it (ignoring directory)
   f = MegaFileOpen (pszLexicon, TRUE, MFO_IGNOREDIR);
   if (f) {
      MegaFileClose (f);
      return TRUE;
   }

   // find where the lexicon's file really starts
   PWSTR pszLexFile = FindFileNameMinusPath (pszLexicon);
   //while (TRUE) {
   //   PWSTR psz = wcschr (pszLexFile, L'\\');
   //   if (!psz)
   //      break;
   //   // else
   //   pszLexFile = psz+1;
   //}

   // look in the source directory
   WCHAR szw[256];
   wcscpy (szw, pszSrc);
   PWSTR pszSrcFile = FindFileNameMinusPath(szw);
   //while (TRUE) {
   //   PWSTR psz = wcschr (pszSrcFile, L'\\');
   //   if (!psz)
   //      break;
   //   // else
   //   pszSrcFile = psz+1;
   //}

   // make sure have enough space
   DWORD dwLen = (DWORD)wcslen(szw) - (DWORD)wcslen(pszSrcFile);
   if (dwLen + wcslen(pszLexFile) + 1 < sizeof(szw)/sizeof(WCHAR)) {
      wcscpy (pszSrcFile, pszLexFile);


      if (pmf) {
         // NOTE - not tested
         if (pmf->Exists (szw)) {
            // NOTE: not tested
            wcscpy (pszLexicon, szw);
            return TRUE;
         }
      }
      else {
         f = MegaFileOpen (szw);
         if (f) {
            MegaFileClose (f);
            wcscpy (pszLexicon, szw);
            return TRUE;
         }
      }
      //WideCharToMultiByte (CP_ACP, 0, szw, -1, szTemp, sizeof(szTemp), 0 ,0);
      //f = fopen (szTemp, "rb");
      //if (f) {
      //   fclose (f);
      //   wcscpy (pszLexicon, szw);
      //   return TRUE;
      //}
   }

   // else, try app dir
   MultiByteToWideChar (CP_ACP, 0, gszAppDir, -1, szw, sizeof(szw)/sizeof(WCHAR));
   if (wcslen(szw) + wcslen(pszLexFile)+1 < sizeof(szw)/sizeof(WCHAR)) {
      wcscat (szw, pszLexFile);

      if (pmf) {
         // NOTE - not tested
         if (pmf->Exists (szw)) {
            // NOTE: not tested
            wcscpy (pszLexicon, szw);
            return TRUE;
         }
      }
      else {
         f = MegaFileOpen (szw);
         if (f) {
            MegaFileClose (f);
            wcscpy (pszLexicon, szw);
            return TRUE;
         }
      }

      //WideCharToMultiByte (CP_ACP, 0, szw, -1, szTemp, sizeof(szTemp), 0 ,0);
      //f = fopen (szTemp, "rb");
      //if (f) {
      //   fclose (f);
      //   wcscpy (pszLexicon, szw);
      //   return TRUE;
      //}
   }

   // else fail
   return FALSE;
}

/*************************************************************************************
LexiconExists - Tries to find the lexicon file. Because the file is hardcoded,
if it can't find the file then it looks in the directory where the work/sr is.

inputs
   PWSTR          pszLexicon - Lexicon file as stored away. If this fails then
                     several other locationes are tried. This will be modified
                     if successful
   PWSTR          pszSrc - Source file (such as .tts). The path of this is used.
returns
   BOOL - TRUE if found file, FALSE if not
*/
BOOL LexiconExists (PWSTR pszLexicon, PWSTR pszSrc)
{
   if (ResolvePathIfNotExist(pszLexicon, pszSrc))
      return TRUE;

   // else try default lexicon...

   // get the default lexicon file
   // char szTemp[256];
   WCHAR szw[256];
   PMEGAFILE f;
   PCMegaFile pmf = MegaFileGet();

   char szTemp[256];
   szTemp[0] = 0;
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dw, dwType;
      LONG lRet;
      dw = sizeof(szTemp);
      lRet = RegQueryValueEx (hKey, gszKeyLexFile, NULL, &dwType, (LPBYTE) szTemp, &dw);
      RegCloseKey (hKey);

      if (lRet != ERROR_SUCCESS)
         szTemp[0] = 0;
   }
   if (!szTemp[0]) {
      strcpy (szTemp, gszAppDir);
      strcat (szTemp, "EnglishInstalled.mlx");
   }


   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));
   if (pmf) {
      // NOTE - not tested
      if (pmf->Exists (szw)) {
         wcscpy (pszLexicon, szw);
         return TRUE;
      }
   }

   // BUGFIX - whether or not megafile exists, try opening
   f = MegaFileOpen (szw, TRUE, MFO_IGNOREDIR);
   if (f) {
      MegaFileClose (f);
      wcscpy (pszLexicon, szw);
      return TRUE;
   }
   //MultiByteToWideChar (CP_ACP, 0, szTemp, -1, szw, sizeof(szw)/sizeof(WCHAR));

   //f = fopen (szTemp, "rb");
   //if (f) {
   //   fclose (f);
   //   wcscpy (pszLexicon, szw);
   //   return TRUE;
   //}

   // else, no luck
   return FALSE;
}



/*************************************************************************************
CTTSWork::WordEmphPhone - Calculates the word emphasis (pitch, volume, and duration)
of a word compared to what's predicted by just using the triphones.

NOTE: This does NOT fill in pSylEmph[i].Emph.XXX. That's done in WordEmphPhone2()

inputs
   PCWaveAn          pWaveAn - Analyzed wave that getting word emphasis for
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
   DWORD             dwWord - Word index into the wave's m_lWVWORD
   //PCMTTSTriPhone    *pptp - Triphone information for each phone in the wave
   PCPoint           pEmph - Filled with .p[0] = pitch scale, .p[1] = volume scale,
                              and .p[2] = duration scale.
   DWORD             *pdwPhoneStart - Filled with the starting phoneme
   DWORD             *pdwPhoneEnd - Filled with the ending phoneme
   DWORD             *pdwNumSyl - Filled in with the number of syllables.
   PSYLEMPH          pSylEmph - This in an array of SYLEMPH that are filled in for
                        each of the syllables. The number of syllables is dictated
                        by the word (specified by dwWord).
returns
   PWSTR - Pointer to the name string (from m_lWVWORD), or NULL if error
*/
PWSTR CTTSWork::WordEmphPhone (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord,
                               PCPoint pEmph,
                               DWORD *pdwPhoneStart, DWORD *pdwPhoneEnd,
                               DWORD *pdwNumSyl, PSYLEMPH pSylEmph)

{
   PCM3DWave pWave = pWaveAn->m_pWave;

   *pdwNumSyl = 0;

   // get the word, previous and neext
   PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(dwWord);
   if (!pw)
      return NULL;

   // make sure word was analyzed
   DWORD i;
   PWORDAN pwa = (PWORDAN) pWaveAn->m_lWORDAN.Get(0);
   for (i = 0; i < pWaveAn->m_lWORDAN.Num(); i++, pwa++)
      if (pwa->dwIndexInWave == dwWord)
         break;
   if (i >= pWaveAn->m_lWORDAN.Num())
      return NULL;
   PSYLAN psa = (PSYLAN) pWaveAn->m_lSYLAN.Get(0);
   psa += pwa->dwSylStart;

   PWSTR psz = (PWSTR)(pw+1);
   PWVWORD pwPrev = (dwWord ? (PWVWORD)pWave->m_lWVWORD.Get(dwWord-1) : NULL);
   PWSTR pszPrev = pwPrev ? (PWSTR)(pwPrev+1) : NULL;
   PWVWORD pwNext = (PWVWORD)pWave->m_lWVWORD.Get(dwWord+1);
   PWSTR pszNext = pwNext ? (PWSTR)(pwNext+1) : NULL;

   // word start and end
   DWORD dwWordStart = pw->dwSample;
   DWORD dwWordEnd = pwNext ? pwNext->dwSample : pWave->m_dwSamples;

   // find which phoneme this starts and ends on
   DWORD dwPhoneStart, dwPhoneEnd;
   DWORD dwNum = pWave->m_lWVPHONEME.Num();
   PWVPHONEME pwp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(0);
   dwPhoneStart = pwa->dwPhoneStart;
   dwPhoneEnd = pwa->dwPhoneEnd;
   if (dwPhoneEnd <= dwPhoneStart)
      return NULL;

   *pdwPhoneStart = dwPhoneStart;
   *pdwPhoneEnd = dwPhoneEnd;

#if 0  // not used, copied to WordEmphTrainSyl()
   // get the lexicon
   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return NULL;

   // loop through all the phonemes from one before to one after and
   // get the phoneme number
   DWORD dwStart = (dwPhoneStart ? (dwPhoneStart-1) : dwPhoneStart);
   DWORD dwEnd = min(dwPhoneEnd+1, dwNum);
   CListFixed lPhone;
   lPhone.Init (sizeof(BYTE));
   PPHONEAN ppa = (PPHONEAN)pWaveAn->m_lPHONEAN.Get(0);
   for (i = dwStart; i < dwEnd; i++) {
      BYTE bPhone = ppa[i].bPhone;
      lPhone.Add (&bPhone);
   }
   PBYTE pabPhone = (PBYTE)lPhone.Get(0);
   BYTE bSilence = (BYTE)pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // see if word is in most common
   DWORD dwWordCom = pwa->dwWordIndex; //pTTS->LexWordsGet()->WordFind (psz);

   // syllable duration
   fp fAvgSyllableDur = pTTS->AvgSyllableDurGet();



   // now loop through all the phonemes and sum up the expected
   // duration, pitch, and volume
   CPoint apExpect[MAXSYLLABLES+1];
   // DWORD j;
   DWORD adwCountEnergy[MAXSYLLABLES+1];
   DWORD adwCountPitch[MAXSYLLABLES+1];
   for (i = 0; i < MAXSYLLABLES+1; i++)
      apExpect[i].Zero();
   memset (adwCountEnergy, 0 ,sizeof(adwCountEnergy));
   memset (adwCountPitch, 0, sizeof(adwCountPitch));

   DWORD dwSyllable = 0;
   fp f;
   for (i = dwPhoneStart; i < dwPhoneEnd; i++) {
      // move to correct syllable
      while (i >= psa[dwSyllable].dwPhoneEnd)
         dwSyllable++;

      PCMTTSTriPhonePros ptp = pptp[i];
      //BYTE bLeft = (i > dwStart) ? pabPhone[i-dwStart-1] : bSilence;
      //BYTE bRight = (i+1 - dwStart < lPhone.Num()) ? pabPhone[i+1 - dwStart] : bSilence;
      //BYTE bFarLeft = (i > dwStart+1) ? pabPhone[i-dwStart-2] : bSilence;
      //BYTE bFarRight = (i+2 - dwStart < lPhone.Num()) ? pabPhone[i+2 - dwStart] : bSilence;
      //BYTE bWordPos = ((i == dwPhoneStart) ? 1 : 0) + ((i+1 == dwPhoneEnd) ? 2 : 0);

      //PCMTTSTriPhone ptp = pTTS->TriPhoneMatch (pabPhone[i-dwStart], bWordPos, bLeft, bRight,
      //   bFarLeft, bFarRight, dwWordCom);
      if (!ptp) // BUGFIX - Dont use since using prosody Triphone || !ptp->Decompress())
         continue;

      // get its english lexicon
      PLEXPHONE plp = pLex->PhonemeGetUnsort (pabPhone[i-dwStart]);
      PLEXENGLISHPHONE pe = NULL;
      if (plp)
         pe = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (pe && (pe->dwCategory & PIC_VOICED)) {
         // BUGFIX - Only count pitch when have voiced phoneme. ignore for univoiced
         f = pTTS->AvgPitchGet() * pow (2, (fp)ptp->m_iPitch / 1000.0) *
            (fp)ptp->m_wDuration;

         // include for whole word
         apExpect[MAXSYLLABLES].p[0] += f;
         adwCountPitch[MAXSYLLABLES] += ptp->m_wDuration;

         // include for syllable
         apExpect[dwSyllable].p[0] += f;
         adwCountPitch[dwSyllable] += ptp->m_wDuration;
      }

      // both whole word and syllable
      adwCountEnergy[MAXSYLLABLES] += ptp->m_wDuration;
      apExpect[MAXSYLLABLES].p[2] += (fp)ptp->m_wDuration;
      adwCountEnergy[dwSyllable] += ptp->m_wDuration;
      apExpect[dwSyllable].p[2] += (fp)ptp->m_wDuration;

      // figure out the average energy
      // BUGFIX - Since energy in prosody triphone, dont calc
      f = ptp->m_fEnergyAvg * ptp->m_wDuration;
      apExpect[MAXSYLLABLES].p[1] += f;
      apExpect[dwSyllable].p[1] += f;
      //PSRFEATURE psr = (PSRFEATURE)ptp->m_pmemSRFEATURE->p;
      //for (j = 0; j < ptp->m_dwNumSRFEATURE; j++, psr++)
      //   pExpect.p[1] += SRFEATUREEnergy (psr);
      // NOTE: Leave the phoneme decompressed since this will be called lots
      // of time while working on it
   } // i
   if (!adwCountEnergy[MAXSYLLABLES])
      return NULL;   // error

   for (i = 0; i < MAXSYLLABLES+1; i++) {
      if (!adwCountEnergy[i])
         continue;

      if (adwCountPitch[i])
         apExpect[i].p[0] /= (fp)adwCountPitch[i];
      else
         apExpect[i].p[0] = 1;
      apExpect[i].p[1] /= (fp)adwCountEnergy[i];
   } // i


   // now, calculate the actual pitch, energy, and duration of the word
   CPoint pFind;
   pFind.Zero();

   // BUGFIX - Use what's in wave analysy
   // pFind.p[0] = pWave->PitchOverRange (dwWordStart, dwWordEnd, 0);
   pFind.p[0] = pwa->fPitch;


   pFind.p[2] = (fp)(dwWordEnd - dwWordStart) / (fp)pWave->m_dwSRSkip;
   dwWordStart = (dwWordStart + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
   dwWordEnd = (dwWordEnd + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
   if (dwWordEnd <= dwWordStart)
      return NULL;   // too short

   // cache the features
   // BUGFIX - Just get from word info
   pFind.p[1] = pwa->fEnergyAvg;
      // NOTE: Should be OK to do this since energy from actual triphonemodel,
      // which have already adjusted. Normally would have used energyratio

   //PSRFEATURE psrCache = CacheSRFeatures (pWave, dwWordStart, dwWordEnd);
   //if (!psrCache)
   //   return NULL;

   //for (i = dwWordStart; i < dwWordEnd; i++)
   //   pFind.p[1] += SRFEATUREEnergy (psrCache + (i - dwWordStart)/*pWave->m_paSRFeature + i*/);
   //pFind.p[1] /= (fp)(dwWordEnd - dwWordStart);
#endif // 0, not used



#if 0 // handled in WordEmphPhone2
   // do this on a per syllable basis
   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();
   TYPICALSYLINFO TSI;
   for (i = 0; i < pwa->dwSylEnd - pwa->dwSylStart; i++) {
      // get the typical info
      pTTSProsody->TypicalSylInfoGet (psa[i].dwIndexIntoSubSentence, psa[i].dwSyllablesInSubSentence, &TSI);

      // these values already stored in psa
      pSylEmph[i].Emph.fPitch = exp(psa[i].TSI.fPitchSum -
         (TSI.fCount ? (TSI.fPitchSum / TSI.fCount) : 0) );

      pSylEmph[i].Emph.fVolume = exp(psa[i].TSI.fVolumeSum -
         (TSI.fCount ? (TSI.fVolumeSum / TSI.fCount) : 0) );

      pSylEmph[i].Emph.fDurPhone = exp(psa[i].TSI.fDurPhoneSum -
         (TSI.fCount ? (TSI.fDurPhoneSum / TSI.fCount) : 0) );

      pSylEmph[i].Emph.fDurSyl = exp(psa[i].TSI.fDurSylSum -
         (TSI.fCount ? (TSI.fDurSylSum / TSI.fCount) : 0) );

      NOTE: Not during fDurSkew

      pSylEmph[i].Emph.fPitchSweep = psa[i].TSI.fPitchSweepSum -
         (TSI.fCount ? (TSI.fPitchSweepSum / TSI.fCount) : 0);

      pSylEmph[i].Emph.fPitchBulge = psa[i].TSI.fPitchBulgeSum -
         (TSI.fCount ? (TSI.fPitchBulgeSum / TSI.fCount) : 0);

      pSylEmph[i].fMisc = (fp)psa[i].bSylNum + (psa[i].bStress ? 256.0 : 0);
      pSylEmph[i].dwNumPhones = psa[i].dwPhoneEnd - psa[i].dwPhoneStart;
   } // i
#endif // 0

   // BUGFIX - get pEmph from the word an
   pEmph->Copy (&pwa->pWrdEmph);
   *pdwNumSyl = pwa->dwSylEnd - pwa->dwSylStart;

   // finally
   return psz;
}



/*************************************************************************************
CTTSWork::WordEmphPhone2 - Fills in pSylEmph[i].Emph.XXX. That's done in WordEmphPhone2()

inputs
   PCWaveAn          pWaveAn - Analyzed wave that getting word emphasis for
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
   DWORD             dwWord - Word index into the wave's m_lWVWORD
   DWORD             dwNumSyl - Number of tyllables in pSylEmph
   PSYLEMPH          pSylEmph - This in an array of SYLEMPH that are filled in for
                        each of the syllables. The number of syllables is dictated
                        by the word (specified by dwWord).
returns
   none
*/
void CTTSWork::WordEmphPhone2 (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord, DWORD dwNumSyl, PSYLEMPH pSylEmph)

{
   PCM3DWave pWave = pWaveAn->m_pWave;

   // get the word, previous and neext
   //PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(dwWord);
   //if (!pw)
   //   return NULL;

   // make sure word was analyzed
   DWORD i;
   PWORDAN pwa = (PWORDAN) pWaveAn->m_lWORDAN.Get(0);
   for (i = 0; i < pWaveAn->m_lWORDAN.Num(); i++, pwa++)
      if (pwa->dwIndexInWave == dwWord)
         break;
   if (i >= pWaveAn->m_lWORDAN.Num())
      return;
   PSYLAN psa = (PSYLAN) pWaveAn->m_lSYLAN.Get(0);
   psa += pwa->dwSylStart;



   // do this on a per syllable basis
   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();
   for (i = 0; i < pwa->dwSylEnd - pwa->dwSylStart; i++) {
      // these values already stored in psa
      pSylEmph[i].Emph.fPitch = exp(psa[i].TSI.fPitchSum);
      pSylEmph[i].Emph.fVolume = exp(psa[i].TSI.fVolumeSum);
      pSylEmph[i].Emph.fDurPhone = exp(psa[i].TSI.fDurPhoneSum);
      pSylEmph[i].Emph.fDurSyl = exp(psa[i].TSI.fDurSylSum);
      pSylEmph[i].Emph.fDurSkew = psa[i].TSI.fDurSkewSum;
      pSylEmph[i].Emph.fPitchSweep = psa[i].TSI.fPitchSweepSum;
      pSylEmph[i].Emph.fPitchBulge = psa[i].TSI.fPitchBulgeSum;

      pSylEmph[i].fMultiMisc = (fp)psa[i].bSylNum + (fp)psa[i].bMultiStress * 256.0;
      pSylEmph[i].dwNumPhones = psa[i].dwPhoneEnd - psa[i].dwPhoneStart;
   } // i

}


/*************************************************************************************
CTTSWork::WordEmphTrainFromWavePrep - Prepares the TYPICALSYLINFO in the prosody
model from the given wave.

This also fills in the each SYLAN.TSI.

inputs
   PCWaveAn          pWaveAn - Analysis oject
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
returns
   none
*/
void CTTSWork::WordEmphTrainFromWavePrep (PCWaveAn pWaveAn, PCMTTS pTTS)
{
   PCM3DWave pWave = pWaveAn->m_pWave;

   // make a list of all the words and note if they're punctuation or not
   CListFixed lPunct;
   lPunct.Init (sizeof(WCHAR));
   WCHAR cPunct;
   DWORD i;
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      if (!pw)
         continue;
      PWSTR psz = (PWSTR)(pw+1);

      cPunct = 0; // assume it's not punctuation
      if (!psz[0])
         cPunct = L' '; // indicate space
      else if (psz[0] == L' ')
         cPunct = psz[0];
      else if (!psz[1] && iswpunct(psz[0]))
         cPunct = psz[0];

      lPunct.Add (&cPunct);
   } // i
   WCHAR *pawPunct = (WCHAR*)lPunct.Get(0);
   DWORD dwNumPunct = lPunct.Num();

   // create a list of all the triphones used for the wave
   CMem memTriPhone;
   CListFixed lPCMTTSTriPhone;
   DWORD dwNumPhone = pWaveAn->m_lPHONEAN.Num();
   if (!memTriPhone.Required (dwNumPhone * 2 * sizeof(DWORD)))
      return;
   DWORD *padwTriPhone = (DWORD*)memTriPhone.p;
   DWORD *padwTriWord = padwTriPhone + dwNumPhone;
   PPHONEAN ppa = (PPHONEAN) pWaveAn->m_lPHONEAN.Get(0);
   for (i = 0; i < dwNumPhone; i++, ppa++) {
      padwTriPhone[i] = ppa->bPhone + (m_fWordStartEndCombine ? 0 : (((DWORD)ppa->bWordPos) << 24));
      padwTriWord[i] = ppa->dwWord;
   } // i
   pTTS->SynthDetermineTriPhonePros (padwTriPhone, /*padwTriWord,*/ dwNumPhone, &lPCMTTSTriPhone);
   PCMTTSTriPhonePros *pptp = (PCMTTSTriPhonePros*) lPCMTTSTriPhone.Get(0);

   // train on each of the words
   // loop through all the words
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      if (pawPunct[i])
         continue;   // not word so skip

      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      if (!pw)
         continue;

      // fill in the structure
      WordEmphTrainSylPrep (pWaveAn, pTTS, i, pptp);
   } // i

}


/*************************************************************************************
CTTSWork::WordEmphTrainFromWave - Trains the TYPICALSYLINFO in the prosody
model from the given wave.


inputs
   PCWaveAn          pWaveAn - Analysis oject
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
returns
   none
*/
void CTTSWork::WordEmphTrainFromWave (PCWaveAn pWaveAn, PCMTTS pTTS)
{
   PCM3DWave pWave = pWaveAn->m_pWave;
   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();

   PSYLAN psa = (PSYLAN) pWaveAn->m_lSYLAN.Get(0);
   DWORD i;
   for (i = 0; i < pWaveAn->m_lSYLAN.Num(); i++)
      pTTSProsody->TypicalSylInfoTrain (psa[i].dwSentenceType, &psa[i].TSI, psa[i].dwIndexIntoSubSentence, psa[i].dwSyllablesInSubSentence);

}

/*************************************************************************************
CTTSWork::WordEmphTrainSylPrep - Fill in SYLAN.TSI.

inputs
   PCWaveAn          pWaveAn - Analyzed wave that getting word emphasis for
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
   DWORD             dwWord - Word index into the wave's m_lWVWORD
   PCMTTSTriPhone    *pptp - Triphone information for each phone in the wave

returns
   none
*/
void CTTSWork::WordEmphTrainSylPrep (PCWaveAn pWaveAn, PCMTTS pTTS, DWORD dwWord,
                               PCMTTSTriPhonePros *pptp)

{
   PCM3DWave pWave = pWaveAn->m_pWave;
   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();

   // get the word, previous and neext
   PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(dwWord);
   if (!pw)
      return;

   // make sure word was analyzed
   DWORD i;
   PWORDAN pwa = (PWORDAN) pWaveAn->m_lWORDAN.Get(0);
   for (i = 0; i < pWaveAn->m_lWORDAN.Num(); i++, pwa++)
      if (pwa->dwIndexInWave == dwWord)
         break;
   if (i >= pWaveAn->m_lWORDAN.Num())
      return;
   PSYLAN psa = (PSYLAN) pWaveAn->m_lSYLAN.Get(0);
   psa += pwa->dwSylStart;

   PWSTR psz = (PWSTR)(pw+1);
   PWVWORD pwPrev = (dwWord ? (PWVWORD)pWave->m_lWVWORD.Get(dwWord-1) : NULL);
   PWSTR pszPrev = pwPrev ? (PWSTR)(pwPrev+1) : NULL;
   PWVWORD pwNext = (PWVWORD)pWave->m_lWVWORD.Get(dwWord+1);
   PWSTR pszNext = pwNext ? (PWSTR)(pwNext+1) : NULL;

   // word start and end
   DWORD dwWordStart = pw->dwSample;
   DWORD dwWordEnd = pwNext ? pwNext->dwSample : pWave->m_dwSamples;

   // find which phoneme this starts and ends on
   DWORD dwPhoneStart, dwPhoneEnd;
   DWORD dwNum = pWave->m_lWVPHONEME.Num();
   PWVPHONEME pwp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(0);
   dwPhoneStart = pwa->dwPhoneStart;
   dwPhoneEnd = pwa->dwPhoneEnd;
   if (dwPhoneEnd <= dwPhoneStart)
      return;

   // get the lexicon
   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return;

   // loop through all the phonemes from one before to one after and
   // get the phoneme number
   DWORD dwStart = (dwPhoneStart ? (dwPhoneStart-1) : dwPhoneStart);
   DWORD dwEnd = min(dwPhoneEnd+1, dwNum);
   CListFixed lPhone;
   lPhone.Init (sizeof(BYTE));
   PPHONEAN ppa = (PPHONEAN)pWaveAn->m_lPHONEAN.Get(0);
   if (dwEnd > dwStart)
      lPhone.Required (dwEnd - dwStart);
   for (i = dwStart; i < dwEnd; i++) {
      BYTE bPhone = ppa[i].bPhone;
      lPhone.Add (&bPhone);
   }
   PBYTE pabPhone = (PBYTE)lPhone.Get(0);
   BYTE bSilence = (BYTE)pLex->PhonemeFindUnsort (pLex->PhonemeSilence());

   // see if word is in most common
   DWORD dwWordCom = pwa->dwWordIndex; //pTTS->LexWordsGet()->WordFind (psz);

   // syllable duration
   fp fAvgSyllableDur = pTTS->AvgSyllableDurGet();


   // now loop through all the phonemes and sum up the expected
   // duration, pitch, and volume
   CPoint apExpect[MAXSYLLABLES+1];
   // DWORD j;
   DWORD adwCountEnergy[MAXSYLLABLES+1];
   DWORD adwCountPitch[MAXSYLLABLES+1];
   for (i = 0; i < MAXSYLLABLES+1; i++)
      apExpect[i].Zero();
   memset (adwCountEnergy, 0 ,sizeof(adwCountEnergy));
   memset (adwCountPitch, 0, sizeof(adwCountPitch));

   PCMTTSTriPhonePros ptp;
   DWORD dwSyllable = 0;
   fp f;
   for (i = dwPhoneStart; i < dwPhoneEnd; i++) {
      // move to correct syllable
      while (i >= psa[dwSyllable].dwPhoneEnd)
         dwSyllable++;

      ptp = pptp[i];
      //BYTE bLeft = (i > dwStart) ? pabPhone[i-dwStart-1] : bSilence;
      //BYTE bRight = (i+1 - dwStart < lPhone.Num()) ? pabPhone[i+1 - dwStart] : bSilence;
      //BYTE bFarLeft = (i > dwStart+1) ? pabPhone[i-dwStart-2] : bSilence;
      //BYTE bFarRight = (i+2 - dwStart < lPhone.Num()) ? pabPhone[i+2 - dwStart] : bSilence;
      //BYTE bWordPos = ((i == dwPhoneStart) ? 1 : 0) + ((i+1 == dwPhoneEnd) ? 2 : 0);

      //PCMTTSTriPhone ptp = pTTS->TriPhoneMatch (pabPhone[i-dwStart], bWordPos, bLeft, bRight,
      //   bFarLeft, bFarRight, dwWordCom);
      if (!ptp) // BUGFIX - Dont use since using prosody Triphone || !ptp->Decompress())
         continue;

      // get its english lexicon
      PLEXPHONE plp = pLex->PhonemeGetUnsort (pabPhone[i-dwStart]);
      PLEXENGLISHPHONE pe = NULL;
      if (plp)
         pe = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (pe && (pe->dwCategory & PIC_VOICED)) {
         // BUGFIX - Only count pitch when have voiced phoneme. ignore for univoiced
         f = pTTS->AvgPitchGet() * pow (2, (fp)ptp->m_iPitch / 1000.0) *
            (fp)ptp->m_wDuration;

         // include for whole word
         apExpect[MAXSYLLABLES].p[0] += f;
         adwCountPitch[MAXSYLLABLES] += ptp->m_wDuration;

         // include for syllable
         apExpect[dwSyllable].p[0] += f;
         adwCountPitch[dwSyllable] += ptp->m_wDuration;
      }

      // both whole word and syllable
      adwCountEnergy[MAXSYLLABLES] += ptp->m_wDuration;
      apExpect[MAXSYLLABLES].p[2] += (fp)ptp->m_wDuration;
      adwCountEnergy[dwSyllable] += ptp->m_wDuration;
      apExpect[dwSyllable].p[2] += (fp)ptp->m_wDuration;

      // figure out the average energy
      // BUGFIX - Since energy in prosody triphone, dont calc
      f = ptp->m_fEnergyAvg * ptp->m_wDuration;
      apExpect[MAXSYLLABLES].p[1] += f;
      apExpect[dwSyllable].p[1] += f;
      //PSRFEATURE psr = (PSRFEATURE)ptp->m_pmemSRFEATURE->p;
      //for (j = 0; j < ptp->m_dwNumSRFEATURE; j++, psr++)
      //   pExpect.p[1] += SRFEATUREEnergy (psr);
      // NOTE: Leave the phoneme decompressed since this will be called lots
      // of time while working on it
   } // i
   if (!adwCountEnergy[MAXSYLLABLES])
      return;   // error

   for (i = 0; i < MAXSYLLABLES+1; i++) {
      if (!adwCountEnergy[i])
         continue;

      if (adwCountPitch[i])
         apExpect[i].p[0] /= (fp)adwCountPitch[i];
      else
         apExpect[i].p[0] = 1;
      apExpect[i].p[1] /= (fp)adwCountEnergy[i];
   } // i


   // now, calculate the actual pitch, energy, and duration of the word
   CPoint pFind;
   pFind.Zero();

   // BUGFIX - Use what's in wave analysy
   // pFind.p[0] = pWave->PitchOverRange (dwWordStart, dwWordEnd, 0);
   pFind.p[0] = pwa->fPitch;


   pFind.p[2] = (fp)(dwWordEnd - dwWordStart) / (fp)pWave->m_dwSRSkip;
   dwWordStart = (dwWordStart + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
   dwWordEnd = (dwWordEnd + pWave->m_dwSRSkip/2) / pWave->m_dwSRSkip;
   if (dwWordEnd <= dwWordStart)
      return;   // too short

   // cache the features
   // BUGFIX - Just get from word info
   pFind.p[1] = pwa->fEnergyAvg;
      // NOTE: Should be OK to do this since energy from actual triphonemodel,
      // which have already adjusted. Normally would have used energyratio

   //PSRFEATURE psrCache = CacheSRFeatures (pWave, dwWordStart, dwWordEnd);
   //if (!psrCache)
   //   return NULL;

   //for (i = dwWordStart; i < dwWordEnd; i++)
   //   pFind.p[1] += SRFEATUREEnergy (psrCache + (i - dwWordStart)/*pWave->m_paSRFeature + i*/);
   //pFind.p[1] /= (fp)(dwWordEnd - dwWordStart);

   // fill in the relative amount
   if (adwCountPitch[MAXSYLLABLES])
      pwa->pWrdEmph.p[0] = pFind.p[0] / apExpect[MAXSYLLABLES].p[0];
   else
      pwa->pWrdEmph.p[0] = 1;  // no pitch detected at all
#if 0 // def _DEBUG
      if ( _isnan(pwa->pWrdEmph.p[0]) || !_finite(pwa->pWrdEmph.p[0]))
         pwa->pWrdEmph.p[0] += 0.00001;
#endif
   pwa->pWrdEmph.p[1] = pFind.p[1] / apExpect[MAXSYLLABLES].p[1];
   pwa->pWrdEmph.p[2] = pFind.p[2] / apExpect[MAXSYLLABLES].p[2];


   // do this on a per syllable basis
   DWORD j;
   for (i = 0; i < pwa->dwSylEnd - pwa->dwSylStart; i++) {
      // get the values
      pFind.p[0] = psa[i].fPitch;
      pFind.p[1] = psa[i].fEnergyAvg;
      pFind.p[2] = (fp)(psa[i].dwTimeEnd - psa[i].dwTimeStart) * psa[i].fDurationScale;
         // BUGFIX - Scale the duration so that is effectively normalized


      // determine the pitch change, in 
      if (adwCountPitch[i] && pFind.p[0] && apExpect[i].p[0])
         psa[i].TSI.fPitchSum = log(pFind.p[0] / apExpect[i].p[0]);
               // BUGFIX - Was NOT storing log() before
      else
         psa[i].TSI.fPitchSum = 0;  // no pitch detected at all
      if (pFind.p[1] && apExpect[i].p[1])
         psa[i].TSI.fVolumeSum = log(pFind.p[1] / apExpect[i].p[1]);
      else
         psa[i].TSI.fVolumeSum = 0;
         // BUGFIX - was NOT storing volume before
      if (pFind.p[2] && apExpect[i].p[2])
         psa[i].TSI.fDurPhoneSum = log(pFind.p[2] / apExpect[i].p[2]);
      else
         psa[i].TSI.fDurPhoneSum = 0;
         // BUGFIX - was NOT storing volume before
      if (pFind.p[2])
         psa[i].TSI.fDurSylSum = log(pFind.p[2] / (fp)pWave->m_dwSRSAMPLESPERSEC / fAvgSyllableDur);
      else
         psa[i].TSI.fDurSylSum = 0;
         // BUGFIX - Was NOT storing volume before

      // figure out the middle
      DWORD dwTheorDur = 0, dwActualDur = 0;
      for (j = psa[i].dwPhoneStart; j < psa[i].dwPhoneEnd; j++) {
         ptp = pptp[j];
         dwTheorDur += ptp ? ptp->m_wDuration : 1;
         dwActualDur += max(psa[i].dwTimeEnd - psa[i].dwTimeStart, 1);
      }
      dwActualDur = max(dwActualDur, 2);

      // find mid-point in theoretical
      DWORD dwTheorDurHalf = dwTheorDur / 2;
      DWORD dwDur = 1;
      for (j = psa[i].dwPhoneStart; j < psa[i].dwPhoneEnd; j++) {
         ptp = pptp[j];
         dwDur = ptp ? ptp->m_wDuration : 1;
         if (dwTheorDurHalf < dwDur)
            break;

         dwTheorDurHalf -= dwDur;
      }
      DWORD dwMidPhone = j;
      fp fMidPhonePercent = (fp)dwTheorDurHalf / (fp)dwDur;

      // find this mid-phoneme in the actual phoneme
      DWORD dwDurAfterMid = 0;
      for (j = dwMidPhone; j < psa[i].dwPhoneEnd; j++) {
         dwDurAfterMid += (DWORD)((1.0 - fMidPhonePercent) * (fp)(psa[i].dwTimeEnd - psa[i].dwTimeStart));
         fMidPhonePercent = 0;   // so include all of the rest of the phonemes
      }
      dwDurAfterMid = max(dwDurAfterMid, 1);
      dwDurAfterMid = min(dwDurAfterMid, dwActualDur-1);
      psa[i].TSI.fDurSkewSum = log((fp)dwDurAfterMid / (fp)(dwActualDur - dwDurAfterMid)) / log((fp)2);

      // determine the pitch sweep of the syllable
      // BUGFIX - much easier was of doing this now
      fp fPitchAtSylStart = 1;
      fp fPitchAtSylEnd = psa[i].fPitchDelta;
      fp fPitchBulge = psa[i].fPitchBulge;

      if (fPitchAtSylStart && fPitchAtSylEnd)
         psa[i].TSI.fPitchSweepSum = log(fPitchAtSylEnd / fPitchAtSylStart) / log((fp)2);
      else
         psa[i].TSI.fPitchSweepSum = 0; // can't tell
      if (fPitchBulge)
         psa[i].TSI.fPitchBulgeSum = log(fPitchBulge) / log((fp)2);
      else
         psa[i].TSI.fPitchBulgeSum = 0;

      // train this
      // BUGFIX - Done in WordEmphTrainFromWave
      // NOTE: Doing this elsewhere pTTSProsody->TypicalSylInfoTrain (&psa[i].TSI, psa[i].dwIndexIntoSubSentence, psa[i].dwSyllablesInSubSentence);
   } // i

   // done
}


/*************************************************************************************
CTTSWork::WordEmphExtractFromWave - Extract word emphasis information from a wave
file.

inputs
   PCWaveAn          pWaveAn - Analysis oject
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
returns
   PCListFixed - List containing an array of WEMPH structures that are filled in. This list
         must be eventually freed.
*/
PCListFixed CTTSWork::WordEmphExtractFromWave (PCWaveAn pWaveAn, PCMTTS pTTS)
{
   PCM3DWave pWave = pWaveAn->m_pWave;

   // make a list of all the words and note if they're punctuation or not
   CListFixed lPunct;
   lPunct.Init (sizeof(WCHAR));
   WCHAR cPunct;
   DWORD i;
   lPunct.Required (pWave->m_lWVWORD.Num());
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      if (!pw)
         continue;
      PWSTR psz = (PWSTR)(pw+1);

      cPunct = 0; // assume it's not punctuation
      if (!psz[0])
         cPunct = L' '; // indicate space
      else if (psz[0] == L' ')
         cPunct = psz[0];
      else if (!psz[1] && iswpunct(psz[0]))
         cPunct = psz[0];

      lPunct.Add (&cPunct);
   } // i
   WCHAR *pawPunct = (WCHAR*)lPunct.Get(0);
   DWORD dwNumPunct = lPunct.Num();

   // make the list
   PCListFixed pl = new CListFixed;
   if (!pl)
      return NULL;
   pl->Init (sizeof(WEMPH));


#if 0 // move to WordEmphTrainFromWavePrep
   // make a list of triphones for each phoneme entry
   CMem memTriPhone;
   CListFixed lPCMTTSTriPhone;
   DWORD dwNumPhone = pWaveAn->m_lPHONEAN.Num();
   if (!memTriPhone.Required (dwNumPhone * 2 * sizeof(DWORD)))
      return NULL;
   DWORD *padwTriPhone = (DWORD*)memTriPhone.p;
   DWORD *padwTriWord = padwTriPhone + dwNumPhone;
   PPHONEAN ppa = (PPHONEAN) pWaveAn->m_lPHONEAN.Get(0);
   for (i = 0; i < dwNumPhone; i++, ppa++) {
      padwTriPhone[i] = ppa->bPhone + (m_fWordStartEndCombine ? 0 : (((DWORD)ppa->bWordPos) << 24));
      padwTriWord[i] = ppa->dwWord;
   } // i
   pTTS->SynthDetermineTriPhonePros (padwTriPhone, /*padwTriWord,*/ dwNumPhone, &lPCMTTSTriPhone);
   PCMTTSTriPhonePros *pptp = (PCMTTSTriPhonePros*) lPCMTTSTriPhone.Get(0);
#endif // 0

   // loop through all the words
   for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
      if (pawPunct[i])
         continue;   // not word so skip

      PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
      if (!pw)
         continue;

      // fill in the structure
      WEMPH we;
      memset (&we, 0, sizeof(we));
      we.pszWord = WordEmphPhone (pWaveAn, pTTS, i, &we.pWrdEmph, &we.dwPhoneStart, &we.dwPhoneEnd,
         &we.dwNumSyl, we.aSYLEMPH);
      we.dwWordIndex = i;

      // get the WORDAN info
      PWORDAN pwa = (PWORDAN) pWaveAn->m_lWORDAN.Get(0);
      DWORD j;
      for (j = 0; j < pWaveAn->m_lWORDAN.Num(); j++, pwa++)
         if (pwa->dwIndexInWave == i)
            break;
      if (j >= pWaveAn->m_lWORDAN.Num())
         we.pwa = NULL;
      else
         we.pwa = pwa;

      // find the punctuation to the left
      int iLook;
      we.wPunctLeft = L'.';   // in case dont find anything
      for (iLook = (int)i-1; iLook >= 0; iLook--) {
         // skip spaces without penalty
         if (pawPunct[iLook] == L' ')
            continue;

         // skip words but increase disntace
         if (!pawPunct[iLook]) {
            we.dwPunctLeftDist++;
            continue;
         }

         // else found
         we.wPunctLeft = pawPunct[iLook];
         break;
      }

      // and on right
      we.wPunctRight = L'.';   // in case dont find anything
      for (iLook = (int)i+1; iLook < (int)dwNumPunct; iLook++) {
         // skip spaces without penalty
         if (pawPunct[iLook] == L' ')
            continue;

         // skip words but increase disntace
         if (!pawPunct[iLook]) {
            we.dwPunctRightDist++;
            continue;
         }

         // else found
         we.wPunctRight = pawPunct[iLook];
         break;
      }

      // add this
      pl->Add (&we);
   } // i


   // return the list
   return pl;
}



/*************************************************************************************
CTTSWork::WordEmphExtractFromWave2 - Basically calls WordEmphPhone2.

inputs
   PCWaveAn          pWaveAn - Analysis oject
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
   PCListFixed       plWEMPH - Returned from WordEmphExtractFromWave().
returns
   none
*/
void CTTSWork::WordEmphExtractFromWave2 (PCWaveAn pWaveAn, PCMTTS pTTS, PCListFixed plWEMPH)
{
   PCM3DWave pWave = pWaveAn->m_pWave;

   // loop through all the words
   DWORD i;
   PWEMPH pwe = (PWEMPH)plWEMPH->Get(0);
   for (i = 0; i < plWEMPH->Num(); i++, pwe++)
      WordEmphPhone2 (pWaveAn, pTTS, pwe->dwWordIndex, pwe->dwNumSyl, pwe->aSYLEMPH);

}


/*************************************************************************************
PopulateNGrams - This populates the N-gram table using the sentence info
and analysis information.

inputs
   PTTSANAL       pAnal - Analysis information
   PWEMPH         pwe - Pointer to an array of word info
   DWORD          dwNum - Number of words in list
   DWORD          dwWord - Word, indexed into pwe, to look at
   DWORD          dwLookLeft - Number of words can look left (and hence populate)
   DWORD          dwLookRight - Number of words can look right (and populate)
returns
   none
*/
#if 0 // old prosody
static void PopulateNGrams (PTTSANAL pAnal, PWEMPH pwe, DWORD dwNum, DWORD dwWord,
                            DWORD dwLookLeft, DWORD dwLookRight)
{
   // buffer to fill in POS
   BYTE abPOS[(TTSPROSNGRAMBIT+TTSPROSNGRAM)*2+1];
   int i;
   for (i = -(TTSPROSNGRAM+TTSPROSNGRAMBIT); i <= (TTSPROSNGRAM+TTSPROSNGRAMBIT); i++) {
      DWORD dwIndex = (DWORD)(i + TTSPROSNGRAM + TTSPROSNGRAMBIT);

      // if beyond what looking for the indicate it's uncertain
      if ((i < -(int)dwLookLeft) || (i > (int)dwLookRight)) {
         abPOS[dwIndex] = 0;  // indicate it's uncertain
         continue;
      }


      // if go beyond edge of data indicate punctuation
      if ((i + (int)dwWord < 0) || (i + (int)dwWord >= (int)dwNum)) {
         abPOS[dwIndex] = POS_MAJOR_NUM;  // indicate it's punctuation
         continue;
      }

      // if beyond punctiation then indicate that
      if ((i < -(int)pwe[dwWord].dwPunctLeftDist) || (i > (int)pwe[dwWord].dwPunctRightDist)) {
         abPOS[dwIndex] = POS_MAJOR_NUM;  // indicate it's punctuation
         continue;
      }

      // else, fill in with punct
      abPOS[dwIndex] = pwe[i + (int)dwWord].bPOS;
   } // i

   // figure out the bin to put it in
   DWORD dwBin = 0;
   for (i = 0; i < (TTSPROSNGRAMBIT+TTSPROSNGRAM)*2+1; i++) {
      if (!i || (i == (TTSPROSNGRAMBIT+TTSPROSNGRAM)*2)) {
         // trigram at very start or end, only care about unknown, word, or punctuation
         dwBin *= 3;
         if (!abPOS[i])
            dwBin += 0;
         else if (abPOS[i] == POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION))
            dwBin += 2;
         else
            dwBin += 1;
      }
      else {
         // main trigram
         dwBin *= (POS_MAJOR_NUM+1);
         dwBin += abPOS[i];
      }
   }

   // get the bin
   PTTSWORKNGRAM ptn = pAnal->paTTSNGram + dwBin;

   // include
   ptn->pProsody.Add (&pwe[dwWord].pWrdEmph);
   ptn->pProsody.p[3] += 1;   // so keep count
   if (dwWord) {
      ptn->dwPauseTotal++;
      if (pwe[dwWord].bPauseLeft)
         ptn->dwPauseLeft++;
#ifdef _DEBUG
      else
         ptn->pProsody.p[3] += 0;
#endif
   }
}
#endif // 0, old prosody


/*************************************************************************************
CTTSWork::WordEmphProduceSentSyl - Examines all the word emphasis structures and
produces the sentence syllable inforamtion that's written into the TTS engine.


inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
*/
void CTTSWork::WordEmphProduceSentSyl (PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all the waves and find out all the word strings used to train
   DWORD dwWordID, i, j, k;
   PCMLexicon pLexSet;
   pLexSet = pTTS->LexTrainingWordsGet ();
   pLexSet->Clear();
   CListVariable lForm;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PWEMPH pwe = pAnal->paplWEMPH[i] ? (PWEMPH)pAnal->paplWEMPH[i]->Get(0) : NULL;
      DWORD dwNum = pAnal->paplWEMPH[i] ? pAnal->paplWEMPH[i]->Num() : 0;
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      for (j = 0; j < dwNum; j++, pwe++) {
         // if there's a word then add it
         if (pwe->pszWord) {
            if (pLexSet->WordExists(pwe->pszWord))
               continue;
            pLexSet->WordSet (pwe->pszWord, &lForm);
         }

         // if there's a punctuation after this then add it
         if (!pwe->dwPunctRightDist) {
            WCHAR szPunct[2];
            szPunct[0] = pwe->wPunctRight;
            szPunct[1] = 0;

            // need to get word ID for punctuation
            if (!pLexSet->WordExists (szPunct))
               pLexSet->WordSet (szPunct, &lForm);
         }
      } // j
   } // i

   // loop through all the words and create the training senteces
   PCSentenceSyllable pss;
   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();
   PCMLexicon pLex = pTTS->Lexicon ();
   CListFixed lPARSERULEDEPTH;
   lPARSERULEDEPTH.Init (sizeof(PARSERULEDEPTH));
   if (!pTTSProsody)
      return;  // error
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PWEMPH pwe = pAnal->paplWEMPH[i] ? (PWEMPH)pAnal->paplWEMPH[i]->Get(0) : NULL;
      DWORD dwNum = pAnal->paplWEMPH[i] ? pAnal->paplWEMPH[i]->Num() : 0;
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwPhoneNum = pwa->m_lPHONEAN.Num();

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      pss = new CSentenceSyllable;
      if (!pss)
         continue; // error

      // clear the word rank history for each sentence
      pTTS->WordRankHistory (NULL);
      BYTE abPhone[50];
      lPARSERULEDEPTH.Clear();
      PARSERULEDEPTH PRD;

      for (j = 0; j < dwNum; j++, pwe++) {
         // determine the word ID
         dwWordID = pwe->pszWord ? pLexSet->WordFind (pwe->pszWord) : (DWORD)-1;

         DWORD dwWordRank = pTTS->WordRank (pwe->pszWord, TRUE);
         if (pwe->pszWord) // BUGFIX - Only call word-rank history is not NULL
            pTTS->WordRankHistory (pwe->pszWord);  // to remember this

         DWORD dwPhoneInFile = pwe->dwPhoneStart;
         DWORD dwSyllablesAdded = 0;
         for (k = 0; k < pwe->dwNumSyl; dwPhoneInFile += pwe->aSYLEMPH[k++].dwNumPhones) {
            fp fPauseProb = 0;
            if (!k && pwe->bPauseLeft && j /* word num */) {
               DWORD dwPhoneLeft, dwPhoneRight;
               if ((pwe[-1].dwPhoneStart < pwe[-1].dwPhoneEnd) && pwe[-1].dwPhoneEnd && (pwe[-1].dwPhoneEnd <= dwPhoneNum))
                  dwPhoneLeft = ppa[pwe[-1].dwPhoneEnd-1].bPhone;
               else
                  dwPhoneLeft = 255;
               if ((pwe[0].dwPhoneStart < pwe[0].dwPhoneEnd) && pwe[0].dwPhoneEnd && (pwe[0].dwPhoneEnd <= dwPhoneNum))
                  dwPhoneRight = ppa[pwe[0].dwPhoneEnd-1].bPhone;
               else
                  dwPhoneRight = 255;
               DWORD *padw = pTTSProsody->PhonemePauseGet (dwPhoneLeft, dwPhoneRight, FALSE, pLex);
               if (padw) {
                  // probability is 1, except must remove probability of silence between
                  // phones of this type
                  if (padw[0])
                     fPauseProb = 1.0 - (fp)padw[1] / (fp)padw[0];
                  else
                     fPauseProb = 0;
                  fPauseProb = max(fPauseProb, 0.0);
                  fPauseProb = min(fPauseProb, 1.0);
               }
            }

            // figure out the phoneme bits
            DWORD dwPhoneNum = min(pwe->aSYLEMPH[k].dwNumPhones, sizeof(abPhone));
            DWORD q;
            for (q = 0; q < dwPhoneNum; q++)
               abPhone[q] = ppa[dwPhoneInFile+q].bPhone;
            DWORD dwPhoneGroupBits = pss->PhoneGroupBitsCalc (abPhone, dwPhoneNum, pLex);

            pss->Add (
              dwWordID,
              pwe->bPOS,
              (BYTE) ((DWORD)pwe->aSYLEMPH[k].fMultiMisc & 0xff),
              pwe->bRuleDepthLowDetail,
              &pwe->aSYLEMPH[k].Emph,
              (BYTE) floor(pwe->aSYLEMPH[k].fMultiMisc / 256),
              // (DWORD) pwe->aSYLEMPH[k].fMultiMisc & 0xff,
              pwe->aSYLEMPH[k].dwNumPhones,
              dwWordRank,
              (BYTE)(fPauseProb * 15.0 + 0.5),
              dwPhoneGroupBits);

            // remember how many syllables added
            dwSyllablesAdded++;
         } // k

         // add a word entry for each syllable
         for (k = 0; k < dwSyllablesAdded; k++) {
            PRD = pwe->ParseRuleDepth;
            if (dwSyllablesAdded >= 2) {
               if (k)
                  PRD.bBefore = PRD.bDuring;
               if (k+1 < dwSyllablesAdded)
                  PRD.bAfter = PRD.bDuring;
            }
            lPARSERULEDEPTH.Add (&PRD);
         } // k


         // if there's a punctuation after this then add it
         if (!pwe->dwPunctRightDist) {
            WCHAR szPunct[2];
            SENTSYLEMPH Emph;
            szPunct[0] = pwe->wPunctRight;
            szPunct[1] = 0;
            memset (&Emph, 0, sizeof(Emph));
            // NOTE: Dont need to do fDurSkew because should be 0
            Emph.fDurPhone = Emph.fDurSyl = Emph.fPitch = Emph.fVolume  = 1;

            // need to get word ID for punctuation
            dwWordID = pLexSet->WordFind (szPunct);
            pss->Add (
               dwWordID,
               POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION),
               0, // sylindex
               0, // rule depth
               &Emph,
               0, // stress
               // 0,
               0, // NOTE: with punctuation must be syllable 0, number of phonemes o f0
               0, // NOTE: with punctuation use 0 for most common
               0,
               0);

            // use ParseRuleDepthNext to determine the depth for this one
            PRD = pwe->ParseRuleDepthNext;
            lPARSERULEDEPTH.Add (&PRD);
         }
      } // j, all words

      // remember the parse rule depths
      pss->SetPARSERULEDEPTH ((PPARSERULEDEPTH) lPARSERULEDEPTH.Get(0), lPARSERULEDEPTH.Num());

      // make sure no unknown words
      // BUGFIX - make sure there are no "unknown" parts of speech in the sentence syllable
      // because wont match. Particular problem with chinese, but would have been
      // a sizable (but unnoticed) problem with english
      pss->ReplaceUnknownPOSWithNoun ();

      // should add this to TTS, but instead just deleting it??? think adding ti
      pTTSProsody->SentenceAdd (pLex, pss, pLexSet);
   } // i, over all waves

   // see if there's a tts voice to get from. if not then done
   DWORD dwPros;
   for (dwPros = 0; dwPros < NUMPROSODYTTS; dwPros++) if (pAnal->apTTSProsody[dwPros])
      pTTSProsody->Merge (pLex, pAnal->apTTSProsody[dwPros]);
}


/*************************************************************************************
CTTSWork::GeneratePOSList - Given a syllable index into a wave, this fills in an
array of parts-of-speech that can then be used to train based on POS

inputs
   PTTSANAL       pAnal - Analysis information
   PCWaveAn       pwa - Wave
   DWORD          dwWaveIndex - Wave index number (for paplWEMPH)
   DWORD          dwSylIndex - Index into m_lSYLAN
   PCListFixed    plPOS - Filled in with list of POS bytes to pass into ProsodyNGramInfoGet
returns
   DWORD - Index into plPOS where this syllable occurs. Or -1 if error
*/
DWORD CTTSWork::GeneratePOSList (PTTSANAL pAnal, PCWaveAn pwa, DWORD dwWaveIndex, DWORD dwSylIndex, PCListFixed plPOS)
{
   DWORD dwFound = (DWORD)-1;

   plPOS->Init (sizeof(BYTE));

   PWORDAN pWordAn;
   PWEMPH pwe = (PWEMPH)pAnal->paplWEMPH[dwWaveIndex]->Get(0);
   PSYLAN psa = (PSYLAN)pwa->m_lSYLAN.Get(0);
   DWORD dwNum = pAnal->paplWEMPH[dwWaveIndex]->Num();
   DWORD i, j;
   BYTE bPOS;
   for (i = 0; i < dwNum; i++) {
      bPOS = pwe[i].bPOS;

      // find the word analsys that matches the wordemph
      for (j = 0; j < pwa->m_lWORDAN.Num(); j++) {
         pWordAn = (PWORDAN)pwa->m_lWORDAN.Get(j);
         if (!pWordAn)
            continue;
         if (pWordAn->dwIndexInWave == pwe[i].dwWordIndex)
            break;
      }
      if (j >= pwa->m_lWORDAN.Num())
         continue;


      for (j = pWordAn->dwSylStart; j < pWordAn->dwSylEnd; j++) {
         if (dwSylIndex == j)
            dwFound = plPOS->Num();

         bPOS = (bPOS & 0x0f) | (psa[j].bMultiStress << 4);
         //if (psa[j].bStress)
         //   bPOS |= 0x80;
         //else
         //   bPOS &= 0x7f;  // remove 0x80

         plPOS->Add (&bPOS);
      }

      // if punctuation after this then add separator
      if (!pwe[i].dwPunctRightDist) {
         bPOS = POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION);
         plPOS->Add (&bPOS);
      }
   } // i


   return dwFound;
}



/*************************************************************************************
CTTSWork::AdjustSYLANPitch - This recalculates the SYLAN pitch after subtracting
the expected triphone pitch from it. Useful for chinese so tones stay in the
phonemes, and NOT the sentence prosody.

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
returns
   none
*/
void CTTSWork::AdjustSYLANPitch (PTTSANAL pAnal, PCMTTS pTTS)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;

   // loop through all the waves and prepare for training
   CMem memPitchCache, memLowPass;
   DWORD dwNeed;
   DWORD i, j, k;
   fp *pafLowPass;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;
      PSYLAN psa = (PSYLAN)pwa->m_lSYLAN.Get(0);
      PPHONEAN ppa = (PPHONEAN)pwa->m_lPHONEAN.Get(0);

      if (!pWave->m_adwPitchSamples[PITCH_F0])
         continue;   // shouldnt happen
      // BUGBUG - may want PITCH_SUB in this function

      // memory for average pitch
      if (!memLowPass.Required (pWave->m_adwPitchSamples[PITCH_F0] * sizeof(fp)))
         return;   //error
      pafLowPass = (fp*)memLowPass.p;
      for (j = 0; j < pWave->m_adwPitchSamples[PITCH_F0]; j++)
         pafLowPass[j] = -1;  // uninitialized

      // copy over the pitch information
      dwNeed = pWave->m_adwPitchSamples[PITCH_F0] * sizeof(WVPITCH);
      if (!memPitchCache.Required (dwNeed))
         return;  // error
      memcpy (memPitchCache.p, pWave->m_apPitch[PITCH_F0], dwNeed);

      // find the vowels
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         PLEXPHONE plp = pLex->PhonemeGetUnsort (ppa->bPhone);
         PLEXENGLISHPHONE ple = plp ? MLexiconEnglishPhoneGet(plp->bEnglishPhone) : NULL;
         if (!ple)
            continue;   // assume not a vowel

         // only want vowels
         if ((ple->dwCategory & PIC_MAJORTYPE) != PIC_VOWEL)
            continue;

         // get the triphone
         PCMTTSTriPhonePros pTP = pTTS->SynthDetermineTriPhonePros (ppa->bPhone, ppa->bWordPos,
            ppa->bPhoneLeft, ppa->bPhoneRight);
         if (!pTP)
            continue;   // shouldnt happen, but might

         // calculate the pitch
         int iPitchCenter, iPitchLeft, iPitchRight;
         iPitchCenter = pTP->m_iPitch;
         iPitchLeft = iPitchCenter - pTP->m_iPitchDelta/2;
         iPitchRight = iPitchCenter + pTP->m_iPitchDelta/2;
         iPitchCenter += pTP->m_iPitchBulge;
         iPitchLeft -= pTP->m_iPitchBulge;
         iPitchRight -= pTP->m_iPitchBulge;

         // set info
         for (k = 0; k < 3; k++) {
            // figure out the time
            DWORD dwTime = ppa->dwTimeStart + (ppa->dwTimeEnd - ppa->dwTimeStart) * (k*2+1) / (3*2);
            dwTime = dwTime * pWave->m_dwSRSkip / pWave->m_adwPitchSkip[PITCH_F0];
            dwTime = min(pWave->m_adwPitchSamples[PITCH_F0]-1, dwTime);   // so not over

            // what value to use
            int iValue;
            switch (k) {
               case 0:  // left
               default:
                  iValue = iPitchLeft;
                  break;
               case 1:  // center
                  iValue = iPitchCenter;
                  break;
               case 2:  // right
                  iValue = iPitchRight;
                  break;
            } // switch

            iValue += 10000;  // just to make sure large
            iValue = max(iValue, 1);   // so doesn't go negative

            pafLowPass[dwTime] = iValue;
         } // k

      } // j, all phonemes

      // low-pass
      PitchLowPass (pafLowPass, pWave->m_adwPitchSamples[PITCH_F0], pWave->m_dwSamplesPerSec / 20 / pWave->m_adwPitchSkip[PITCH_F0], 10000.0);
         // default is 10,000 because add that to pitch adjust

      // loop over all the pitch points and scale by the low-pass
      for (j = 0; j < pWave->m_adwPitchSamples[PITCH_F0]; j++)
         pWave->m_apPitch[PITCH_F0][j].fFreq *= pow(2.0, (pafLowPass[j] - 10000.0) / 1000.0);

      // fix the syllables, counteracting by the pitch of the units
      ppa = (PPHONEAN)pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lSYLAN.Num(); j++, psa++) {
         DWORD dwPhonemeStart = ppa[psa->dwVoiceStart].dwTimeStart * pWave->m_dwSRSkip;
         DWORD dwPhonemeEnd = ppa[psa->dwVoiceEnd-1].dwTimeEnd * pWave->m_dwSRSkip;
         psa->fPitch = pWave->PitchOverRange (PITCH_F0, dwPhonemeStart, dwPhonemeEnd, 0, NULL, &psa->fPitchDelta, &psa->fPitchBulge,
            TRUE);
         // BUGBUG - May need PITCH_SUB
      } // j

      // restore pitch
      memcpy (pWave->m_apPitch[PITCH_F0], memPitchCache.p, dwNeed);
   } // i, all waves
}

/*************************************************************************************
CTTSWork::WordEmphExtractAllWaves - This takes an analysis structure and fills in the
paplWEMPH items. It is assumed that paplWEMPH already points to a buffer of the
right size and all blanked to 0.

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS            pTTS - TTS to use for converting phonemes. Must have triphones
                        and average pitch filled in
   PCProgressSocket pProgress
*/
void CTTSWork::WordEmphExtractAllWaves (PTTSANAL pAnal, PCMTTS pTTS, PCProgressSocket pProgress)
{
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;

   PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();
   if (!pTTSProsody)
      return;  // error

   CTextParse TextParse;
   TextParse.Init (pLex->LangIDGet(), pLex);

   DWORD i, j, k;

   // loop through all the waves and prepare for training
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      WordEmphTrainFromWavePrep (pwa, pTTS);
   } // i

   // BUGFIX - loop through all the words and normalize the volumes
   // was a problem before because consistently calculating the average volume
   // about 1.2x the desired volume, so would let another part of the system
   // do the volume normalization, causing problems
   //if (dwVolumeCount)
   //   fVolumeSum /= (fp)dwVolumeCount;
   //fVolumeSum = 1.0 / fVolumeSum;
   double fPitchSum, fVolumeSum;
   // BUGFIX - Fixed so modified TSA
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PSYLAN psa = (PSYLAN)pwa->m_lSYLAN.Get(0);
      DWORD dwNum = pwa->m_lSYLAN.Num();

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      // BUGFIX - Rather than calculate energy globally, do on a per-sentence
      // basis since some recordings might be quieter than others
      // Also average out the pitch change
      fVolumeSum = 0;
      fPitchSum = 0;
      for (j = 0; j < dwNum; j++) {
         fVolumeSum += psa[j].TSI.fVolumeSum;
         fPitchSum += psa[j].TSI.fPitchSum;
      }
      if (dwNum) {
         fVolumeSum /= (double)dwNum;
         fPitchSum /= (double)dwNum;
      }

      // normalize volume and pitch
      for (j = 0; j < dwNum; j++) {
         psa[j].TSI.fVolumeSum -= fVolumeSum;
         psa[j].TSI.fPitchSum -= fPitchSum;
      } // j
   } // i

   // loop through all the waves and train the prosody model based on them
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;



      WordEmphTrainFromWave (pwa, pTTS);
   } // i

   // calculate the wordemph
   CListVariable lForm;
   CListFixed lLEXPOSGUESS;
   // fVolumeSum = 0;
   // DWORD dwVolumeCount = 0;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      if ( ((i%10) == 0) && pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      // findo
      pAnal->paplWEMPH[i] = WordEmphExtractFromWave (pwa, pTTS);
      PWEMPH pwe = (PWEMPH)pAnal->paplWEMPH[i]->Get(0);
      DWORD dwNum = pAnal->paplWEMPH[i]->Num();

      // determine the part of speech
      pLex->POSWaveToLEXPOSGUESS (pWave, &lLEXPOSGUESS, &TextParse);
      PLEXPOSGUESS pLPG = (PLEXPOSGUESS) lLEXPOSGUESS.Get(0);
      DWORD dwLPGNum = lLEXPOSGUESS.Num();

      for (j = 0; j < dwNum; j++) {
         // while at it, include volume
         //fVolumeSum += pwe[j].pWrdEmph.p[1];
         //dwVolumeCount++;

         // try to find the word...
         for (k = 0; k < dwLPGNum; k++)
            if (pwe[j].dwWordIndex == (DWORD)(size_t)pLPG[k].pvUserData)
               break;
         if (k < dwLPGNum) {
            pwe[j].bPOS = POS_MAJOR_EXTRACT(pLPG[k].bPOS);
            pwe[j].bRuleDepthLowDetail = pLPG[k].bRuleDepthLowDetail;   // BUGFIX - Was j, should be k
            pwe[j].ParseRuleDepth = pLPG[k].ParseRuleDepth;

            if (k+1 < dwLPGNum)
               pwe[j].ParseRuleDepthNext = pLPG[k+1].ParseRuleDepth;
            else
               memset (&pwe[j].ParseRuleDepthNext, 0, sizeof(pwe[j].ParseRuleDepthNext));

            continue;
         }

         // else
         pwe[j].bPOS = POS_MAJOR_EXTRACT(POS_MAJOR_NOUN);; // assume noun
         pwe[j].bRuleDepthLowDetail = 0;
         memset (&pwe[j].ParseRuleDepth, 0, sizeof(pwe[j].ParseRuleDepth));
         memset (&pwe[j].ParseRuleDepthNext, 0, sizeof(pwe[j].ParseRuleDepthNext));

         lForm.Clear();
         if (!pLex->WordGet (pwe[j].pszWord, &lForm))
            continue;
         if (!lForm.Num())
            continue;

         PBYTE pb = (PBYTE)lForm.Get(0);
         if (POS_MAJOR_ISOLATE(pb[0]))
            pwe[j].bPOS = POS_MAJOR_EXTRACT(pb[0]);
         // NOTE: pwe[j].bRuleDepth set above
      }

      // while here, go through and figure out how many microbreaks there
      // are between the phonemes
      if (dwNum)
         pwe[0].bPauseLeft = FALSE; // remeber if pause to left
      for (j = 1; j < dwNum; j++) {
         pwe[j].bPauseLeft = FALSE; // remember if pause to left

         // get the phonemes
         PWVPHONEME pwp1 = (PWVPHONEME) pWave->m_lWVPHONEME.Get(pwe[j-1].dwPhoneEnd-1);
         PWVPHONEME pwp2 = (PWVPHONEME) pWave->m_lWVPHONEME.Get(pwe[j].dwPhoneStart);
         if (!pwp1 || !pwp2)
            continue;   // shouldnt happen

         // get their numbers
         DWORD dw1, dw2;
         WCHAR szTemp[16];
         memset (szTemp, 0, sizeof(szTemp));
         memcpy (szTemp, pwp1->awcNameLong, sizeof(pwp1->awcNameLong));
         dw1 = pLex->PhonemeFindUnsort (szTemp);
         memset (szTemp, 0, sizeof(szTemp));
         memcpy (szTemp, pwp2->awcNameLong, sizeof(pwp2->awcNameLong));
         dw2 = pLex->PhonemeFindUnsort (szTemp);
         if ((dw1 >= pLex->PhonemeNum()) || (dw2 >= pLex->PhonemeNum()))
            continue;   // shouldnt happen

         if (pwe[j-1].dwPhoneEnd != pwe[j].dwPhoneStart)
            pwe[j].bPauseLeft = TRUE; // remember if pause to left

         // BUGFIX - if punctuation separates the words then ignore them
         // since cant get valid information on whether or not a pause
         // occurs between words
         if (!pwe[j-1].dwPunctRightDist || !pwe[j].dwPunctLeftDist)
            continue;

         // store the pause left/right away
         DWORD *padw = pTTSProsody->PhonemePauseGet (dw1, dw2, TRUE, pLex);
         //DWORD *padw = pAnal->padwMicroPause + (dw1 * pLex->PhonemeNum() + dw2)*2;
         if (padw) {
            padw[0]++;  // total
            if (pwe[j].bPauseLeft)
               padw[1]++;
         }
      } // j

   } // i

   // now that trained, loop through all the waves and REMOVE the main prosody
   // model from the training
   // PCTTSProsody pTTSProsody = pTTS->TTSProsodyGet();
   CListFixed lPOS;
   DWORD dwSylIndex;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      PWORDAN pWordAn = (PWORDAN) pwa->m_lWORDAN.Get(0);
      TYPICALSYLINFO TSI;
      for (j = 0; j < pwa->m_lSYLAN.Num(); j++) {
         pTTSProsody->TypicalSylInfoGet (psa[j].dwSentenceType, psa[j].dwIndexIntoSubSentence, psa[j].dwSyllablesInSubSentence, &TSI);

         if (TSI.fCount) {
            psa[j].TSI.fDurPhoneSum -= TSI.fDurPhoneSum / TSI.fCount;
            psa[j].TSI.fDurSylSum -= TSI.fDurSylSum / TSI.fCount;
            psa[j].TSI.fDurSkewSum -= TSI.fDurSkewSum / TSI.fCount;
            psa[j].TSI.fPitchBulgeSum -= TSI.fPitchBulgeSum / TSI.fCount;
            psa[j].TSI.fPitchSum -= TSI.fPitchSum / TSI.fCount;
            psa[j].TSI.fPitchSweepSum -= TSI.fPitchSweepSum / TSI.fCount;
            psa[j].TSI.fVolumeSum -= TSI.fVolumeSum / TSI.fCount;
         }

         // train this for the ngram
         dwSylIndex = GeneratePOSList (pAnal, pwa, i, j, &lPOS);;
         if (dwSylIndex != (DWORD)-1)
            pTTSProsody->ProsodyNGramInfoTrain (&psa[j].TSI, (PBYTE)lPOS.Get(0), lPOS.Num(), dwSylIndex);
      } // j

   } // i

   // train per word info
   TYPICALSYLINFO aTSI[16];
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      // PWORDAN pWordAn = (PWORDAN) pwa->m_lWORDAN.Get(0);
      PCListFixed plWEMPH = pAnal->paplWEMPH[i];
      PWEMPH pwe = (PWEMPH) plWEMPH->Get(0);
      for (j = 0; j < plWEMPH->Num(); j++, pwe++) {
         PWORDAN pWordAn = pwe->pwa;
         if (!pWordAn)
            continue;   // not a word

         if (pWordAn->dwSylStart >= pWordAn->dwSylEnd)
            continue;   // too short
         DWORD dwSyllables = pWordAn->dwSylEnd - pWordAn->dwSylStart;
         if (dwSyllables > sizeof(aTSI) / sizeof(aTSI[0]))
            continue;   // too many syllables

         // fill in the TYPICALSYLINFO
         DWORD dwStressBitsMulti = 0;
         DWORD dwScale = 1;
         for (k = 0; k < dwSyllables; k++, dwScale *= pLex->Stresses()) {
            aTSI[k] = psa[pWordAn->dwSylStart + k].TSI;
            if (!aTSI[k].fCount)
               aTSI[k].fCount = 1.0;
            dwStressBitsMulti += (DWORD)psa[pWordAn->dwSylStart + k].bMultiStress * dwScale;
            // if (psa[pWordAn->dwSylStart + k].bStress)
            //   dwStressBitsMulti |= (1 << k);
         }

         // train this
         if (pwe->pszWord)
            pTTSProsody->WordSylTrain (pLex, dwSyllables, aTSI, pwe->pszWord, pwe->bPOS, dwStressBitsMulti);
         pTTSProsody->WordSylTrain (pLex, dwSyllables, aTSI, NULL, pwe->bPOS, dwStressBitsMulti);
      } // j
   } // i

   // remove word training
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      // PWORDAN pWordAn = (PWORDAN) pwa->m_lWORDAN.Get(0);
      PCListFixed plWEMPH = pAnal->paplWEMPH[i];
      PWEMPH pwe = (PWEMPH) plWEMPH->Get(0);
      for (j = 0; j < plWEMPH->Num(); j++, pwe++) {
         PWORDAN pWordAn = pwe->pwa;
         if (!pWordAn)
            continue;   // not a word

         if (pWordAn->dwSylStart >= pWordAn->dwSylEnd)
            continue;   // too short
         DWORD dwSyllables = pWordAn->dwSylEnd - pWordAn->dwSylStart;
         if (dwSyllables > sizeof(aTSI) / sizeof(aTSI[0]))
            continue;   // too many syllables

         // stress bits
         DWORD dwStressBitsMulti = 0;
         DWORD dwScale = 1;
         for (k = 0; k < dwSyllables; k++, dwScale *= pLex->Stresses())
            dwStressBitsMulti += (DWORD)psa[pWordAn->dwSylStart + k].bMultiStress * dwScale;
            //if (psa[pWordAn->dwSylStart + k].bStress)
            //   dwStressBitsMulti |= (1 << k);

         // get the info
         if (!pTTSProsody->WordSylGetTYPICALSYLINFO (pLex, pwe->pszWord, pwe->bPOS, dwSyllables, dwStressBitsMulti,
            &pTTSProsody, 1, aTSI))
            continue;

         // remove these
         // NOTE: Know that aTSI[x].fCount == 1
         for (k = 0; k < dwSyllables; k++) {
            PTYPICALSYLINFO pTSI = &psa[pWordAn->dwSylStart + k].TSI;
            fp fCount = pTSI->fCount;
            if (!fCount)
               fCount = 1;

            pTSI->fDurPhoneSum -= aTSI[k].fDurPhoneSum * fCount;
            pTSI->fDurSkewSum -= aTSI[k].fDurSkewSum * fCount;
            pTSI->fDurSylSum -= aTSI[k].fDurSylSum * fCount;
            pTSI->fPitchBulgeSum -= aTSI[k].fPitchBulgeSum * fCount;
            pTSI->fPitchSum -= aTSI[k].fPitchSum * fCount;
            pTSI->fPitchSweepSum -= aTSI[k].fPitchSweepSum * fCount;
            pTSI->fVolumeSum -= aTSI[k].fVolumeSum * fCount;
         } // k
      } // j
   } // i

   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      PWORDAN pWordAn = (PWORDAN) pwa->m_lWORDAN.Get(0);
      for (j = 0; j < pwa->m_lSYLAN.Num(); j++) {
         // train this for the ngram
         dwSylIndex = GeneratePOSList (pAnal, pwa, i, j, &lPOS);;
         if (dwSylIndex != (DWORD)-1)
            pTTSProsody->ProsodyNGramInfoTrain (&psa[j].TSI, (PBYTE)lPOS.Get(0), lPOS.Num(), dwSylIndex);
      } // j
   } // i


   // now, loop through all the waves and remove the N-gram training
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      PSYLAN psa = (PSYLAN) pwa->m_lSYLAN.Get(0);
      PWORDAN pWordAn = (PWORDAN) pwa->m_lWORDAN.Get(0);
      TYPICALSYLINFO TSI;
      for (j = 0; j < pwa->m_lSYLAN.Num(); j++) {
         dwSylIndex = GeneratePOSList (pAnal, pwa, i, j, &lPOS);;
         if (dwSylIndex == (DWORD)-1)
            continue;

         pTTSProsody->ProsodyNGramInfoGet ((PBYTE) lPOS.Get(0), lPOS.Num(), dwSylIndex, &TSI);

         if (TSI.fCount) {
            psa[j].TSI.fDurPhoneSum -= TSI.fDurPhoneSum / TSI.fCount;
            psa[j].TSI.fDurSylSum -= TSI.fDurSylSum / TSI.fCount;
            psa[j].TSI.fDurSkewSum -= TSI.fDurSkewSum / TSI.fCount;
            psa[j].TSI.fPitchBulgeSum -= TSI.fPitchBulgeSum / TSI.fCount;
            psa[j].TSI.fPitchSum -= TSI.fPitchSum / TSI.fCount;
            psa[j].TSI.fPitchSweepSum -= TSI.fPitchSweepSum / TSI.fCount;
            psa[j].TSI.fVolumeSum -= TSI.fVolumeSum / TSI.fCount;
         }

      } // j

   } // i


   // fill in final WEMPH information
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa ? ppwa[0] : NULL;
      PCM3DWave pWave = pwa ? pwa->m_pWave : NULL;
      if (!pWave)
         continue;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;

      // findo
      WordEmphExtractFromWave2 (pwa, pTTS, pAnal->paplWEMPH[i]);
   } // i

   // loop through the micropauses and write them into the tts engine
#if 0 // Dont do this anymore since micropauses part of prosody
   DWORD dwNeed = (pLex->PhonemeNum() * pLex->PhonemeNum() + 7) / 8;
   PCMem pMicroPause = pTTS->MemMicroPauseGet();
   if (!pMicroPause->Required (dwNeed)) {
      pMicroPause->m_dwCurPosn = 0;
      return;
   }
   pMicroPause->m_dwCurPosn = dwNeed;
   PBYTE pb = (PBYTE)pMicroPause->p;
   memset (pb, 0, dwNeed);
#ifdef _DEBUG
   DWORD dwTotal = 0, dwCantTell = 0, dwPause = 0;
#endif
   for (i = 0; i < pLex->PhonemeNum(); i++) for (j = 0; j < pLex->PhonemeNum(); j++) {
      DWORD dwBit = (i * pLex->PhonemeNum() + j);
      DWORD *padw = pAnal->padwMicroPause + dwBit*2;
      BOOL fSetBit = FALSE;
      if ((padw[0] > 3) && (padw[1] > MINPAUSECOUNT(padw[0])))
         fSetBit = TRUE;
         // BUGFIX - was 1/2, but made it 2/3 so a bit harder before put pause in
         // BUGFIX - Made 3/4 because still seem to have too many pauses
         // BUGFIX - Put minimum count in

#ifdef _DEBUG
      dwTotal++;
#endif
      if (!padw[0]) {
         // BUGFIX - If not enought information assume that connect toeghether
         fSetBit = FALSE;
#ifdef _DEBUG
         dwCantTell++;
#endif
         continue;
      }

      if (!fSetBit)
         continue;
      PBYTE pMod = pb + (dwBit/8);
      pMod[0] = pMod[0] | (1 << (dwBit%8));
#ifdef _DEBUG
      dwPause++;
#endif
   } // i,j

#ifdef _DEBUG
   char szTemp[256];
   sprintf (szTemp, "Micropause, total=%d, canttell=%d, pause=%d\r\n",
      (int)dwTotal, (int)dwCantTell, (int)dwPause);
   OutputDebugString (szTemp);
#endif
#endif // 0 - writing micropauses in
}


/*************************************************************************************
CTTSWork::WordEmphFromCommon - Looks theough the paplWEMPH list in the analysis
information and determines how much to emphasize a word based on how common it is
(which m_apLexWordEmph list it fits in). This fills in pAnal.

inputs
   PTTSANAL       pAnal - Analysis information
returns
   none
*/
#if 0 // old prosody
void CTTSWork::WordEmphFromCommon (PTTSANAL pAnal)
{
   // fill in default information, assuming that have already have 10 instances
   // without any change to pitch, volume, duration. That way if don't have
   // much of a dataset wont get wildly varying emphasis
   DWORD adwCount[NUMLEXWORDEMPH+1][3];
   DWORD i;
   for (i = 0; i < NUMLEXWORDEMPH+1; i++) {
      adwCount[i][0] = adwCount[i][1] = adwCount[i][2] = 10;
      pAnal->apLexWordEmphScale[i].Zero();
      pAnal->apLexWordEmphScale[i].p[0] = pAnal->apLexWordEmphScale[i].p[1] = 
         pAnal->apLexWordEmphScale[i].p[2] = adwCount[i][0];
   } // i

   // loop through all the senteces
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      DWORD j, k;
      for (j = 0; j < dwNum; j++, pw++) {
         // find out what common-group this word belongs to
         for (k = 0; k < NUMLEXWORDEMPH; k++)
            if (m_apLexWordEmph[k] && (-1 != m_apLexWordEmph[k]->WordFind (pw->pszWord)))
               break;
         // NOTE: if goes beyond end of list ok, since have extra one

         // see if this word is part of the exceptions. If it is, don't bother including
         // their duration and energy in the calculations since these are already
         // accounted for by the special units
         // BUGFIX - Since changed the way that code work, no exception
         //BOOL fException = (-1 != m_pLexWords->WordFind(pw->pszWord));

         // combine scores
#if 0 // def _DEBUG
         if ( _isnan(pw->pEmph.p[0]) || !_finite(pw->pEmph.p[0]))
            pw->pEmph.p[0] += 0.0001;
#endif
         pAnal->apLexWordEmphScale[k].p[0] += pw->pWrdEmph.p[0];
         adwCount[k][0]++;
         //if (!fException) {
            pAnal->apLexWordEmphScale[k].p[1] += pw->pWrdEmph.p[1];
            adwCount[k][1]++;
            pAnal->apLexWordEmphScale[k].p[2] += pw->pWrdEmph.p[2];
            adwCount[k][2]++;
         //}
      } // j, over words
   } // i, over all files

   // scale the score by bound
   //fp fSumVol = 0;
   for (i = 0; i < NUMLEXWORDEMPH+1; i++) {
      pAnal->apLexWordEmphScale[i].p[0] /= (fp) adwCount[i][0];
      pAnal->apLexWordEmphScale[i].p[1] /= (fp) adwCount[i][1];
      pAnal->apLexWordEmphScale[i].p[2] /= (fp) adwCount[i][2];

      // fSumVol +=  pAnal->apLexWordEmphScale[i].p[1];
   }

#if 0 // BUGFIX - Moved volume normalization elsewhere, since this was buseted anyway
   // need to normalize the volume otherwise it might end up
   // clipping
   fSumVol /= (fp)(NUMLEXWORDEMPH+1);
   for (i = 0; i < NUMLEXWORDEMPH+1; i++)
      pAnal->apLexWordEmphScale[i].p[1] /= fSumVol;
#endif // 0

   // loop back through all the words and invert their emphasis by what
   // have calculated. That way not double-emphasizing words when add more
   // than one emphasis
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      DWORD j, k;
      for (j = 0; j < dwNum; j++, pw++) {
         // find out what common-group this word belongs to
         for (k = 0; k < NUMLEXWORDEMPH; k++)
            if (m_apLexWordEmph[k] && (-1 != m_apLexWordEmph[k]->WordFind (pw->pszWord)))
               break;
         // NOTE: if goes beyond end of list ok, since have extra one

         // see if this word is part of the exceptions. If it is, don't bother including
         // their duration and energy in the calculations since these are already
         // accounted for by the special units
         // BUGFIX - Since changed the way that code works, no exception
         //BOOL fException = (-1 != m_pLexWords->WordFind(pw->pszWord));

         // undo change
         pw->pWrdEmph.p[0] /= pAnal->apLexWordEmphScale[k].p[0];
#if 0 // def _DEBUG
         if ( _isnan(pw->pWrdEmph.p[0]) || !_finite(pw->pWrdEmph.p[0]))
            pw->pEmph.p[0] += 0.0001;
#endif
         //if (!fException) {
            pw->pWrdEmph.p[1] /= pAnal->apLexWordEmphScale[k].p[1];
            pw->pWrdEmph.p[2] /= pAnal->apLexWordEmphScale[k].p[2];
         //}
      } // j, over words
   } // i, over all files

   // done
}
#endif // 0, old prosody

#if 0 // old prosody

/*************************************************************************************
CTTSWork::WordEmphFromWordLength - Looks theough the paplWEMPH list in the analysis
information and determines how much to emphasize a word based on how common it is
(which m_apLexWordEmph list it fits in). This fills in pAnal.

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS         pTTS - TTS to add the PCTTSPunctPros to
returns
   none
*/
void CTTSWork::WordEmphFromWordLength (PTTSANAL pAnal, PCMTTS pTTS)
{
   // fill in default information, assuming that have already have 10 instances
   // without any change to pitch, volume, duration. That way if don't have
   // much of a dataset wont get wildly varying emphasis
   DWORD adwCount[NUMPROSWORDLENGTH+1];
   DWORD i;
   for (i = 0; i < NUMPROSWORDLENGTH+1; i++) {
      adwCount[i] = 10;
      pAnal->apWordEmphFromWordLength[i].Zero();
      pAnal->apWordEmphFromWordLength[i].p[0] = pAnal->apWordEmphFromWordLength[i].p[1] = 
         pAnal->apWordEmphFromWordLength[i].p[2] = adwCount[i];
   } // i

   // loop through all the senteces
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();
      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));
      PPHONEAN ppa = (PPHONEAN)pwa->m_lPHONEAN.Get(0);

      DWORD j, k;
      for (j = 0; j < dwNum; j++, pw++) {
         DWORD dwLen = pw->dwPhoneEnd - pw->dwPhoneStart;
         if (dwLen < 1)
            continue; // shouldn happen
         if (dwLen >= NUMPROSWORDLENGTH)
            dwLen = NUMPROSWORDLENGTH-1;  // unlikely to happen

         // sum up the expected length in SR units
         CPoint pExpected, pReal;
         pExpected.Zero();
         pReal.Zero();
         for (k = pw->dwPhoneStart; k < pw->dwPhoneEnd; k++) {
            PTRIPHONETRAIN pt = (PTRIPHONETRAIN)pAnal->plTRIPHONETRAIN->Get(ppa[k].dwTRIPHONETRAINIndex);
            if (!pt) // backoff to unknown triphone
               pt = GetTRIPHONETRAIN (this, pAnal, ppa + k);

            pReal.p[0] += pow (2.0, (fp)ppa[k].iPitch / 1000.0);
            pExpected.p[0] += pow (2.0, (fp)pt->iPitch / 1000.0);

            pReal.p[1] += ppa[k].fEnergyRatio;
            pExpected.p[1] += pt->fEnergyRatio;
            //pReal.p[1] += ppa[k].fEnergyAvg;
            //pExpected.p[1] += pt->fEnergyAvg;

            pReal.p[2] += ppa[k].dwTimeEnd - ppa[k].dwTimeStart;
            pExpected.p[2] += pt->dwDurationSRFeat;
         } // k

         // figure out how much more real is than expected
         pReal.p[0] /= pExpected.p[0];
         pReal.p[1] /= pExpected.p[1];
         pReal.p[2] /= pExpected.p[2];

         // combine scores
         pAnal->apWordEmphFromWordLength[dwLen].Add (&pReal);
         adwCount[dwLen]++;

         // also keep track of all values
         pAnal->apWordEmphFromWordLength[0].Add (&pReal);
         adwCount[0]++;
      } // j, over words
   } // i, over all files

   // scale the score by bound
   //fp fSumVol = 0;
   for (i = 0; i < NUMPROSWORDLENGTH+1; i++) {
      pAnal->apWordEmphFromWordLength[i].Scale (1.0 / (fp) adwCount[i]);

      // if this is not the first one (the sum) then remove any error for
      // over-all scaling
      if (i) {
         pAnal->apWordEmphFromWordLength[i].p[0] /= pAnal->apWordEmphFromWordLength[0].p[0];
         pAnal->apWordEmphFromWordLength[i].p[1] /= pAnal->apWordEmphFromWordLength[0].p[1];
         pAnal->apWordEmphFromWordLength[i].p[2] /= pAnal->apWordEmphFromWordLength[0].p[2];
      }
   } // i


   // loop back through all the words and invert their emphasis by what
   // have calculated. That way not double-emphasizing words when add more
   // than one emphasis
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      DWORD j;
      for (j = 0; j < dwNum; j++, pw++) {
         // undo change
         DWORD dwLen = pw->dwPhoneEnd - pw->dwPhoneStart;
         if (dwLen < 1)
            continue; // shouldn happen
         if (dwLen >= NUMPROSWORDLENGTH)
            dwLen = NUMPROSWORDLENGTH-1;  // unlikely to happen

         pw->pWrdEmph.p[0] /= pAnal->apWordEmphFromWordLength[dwLen].p[0];

#if 0 // def _DEBUG
      if ( _isnan(pw->pWrdEmph.p[0]) || !_finite(pw->pWrdEmph.p[0]))
         pw->pWrdEmph.p[0] += 0.00001;
#endif
         pw->pWrdEmph.p[1] /= pAnal->apWordEmphFromWordLength[dwLen].p[1];
         pw->pWrdEmph.p[2] /= pAnal->apWordEmphFromWordLength[dwLen].p[2];
      } // j, over words
   } // i, over all files

   // copy over learned info
   pTTS->ProsWordEmphFromWordLength (pAnal->apWordEmphFromWordLength + 1);
}
#endif // 0


/*************************************************************************************
CTTSWork::PhoneEmph - Figures out how much phonemes near the edge of the word
are stretched/shrunk, given the POS to the left and right.

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS         pTTS - TTS to add the PCTTSPunctPros to
returns
   none
*/
void CTTSWork::PhoneEmph (PTTSANAL pAnal, PCMTTS pTTS)
{
   PCMLexicon pLex = Lexicon();

   // fill in default information, assuming that have already have 10 instances
   // without any change to pitch, volume, duration. That way if don't have
   // much of a dataset wont get wildly varying emphasis
   DWORD adwCount[NUMPHONEEMPH*2][PHONEPOSBIN];
   DWORD adwCountNULL[PHONEPOSBIN];
   CPoint apPhoneEmphNULL[PHONEPOSBIN];
   DWORD i, j;
   for (i = 0; i < NUMPHONEEMPH*2; i++) for (j = 0; j < PHONEPOSBIN; j++) {
      adwCount[i][j] = 10;
      pAnal->apPhoneEmph[i][j].Zero();
      pAnal->apPhoneEmph[i][j].p[0] = pAnal->apPhoneEmph[i][j].p[1] = 
         pAnal->apPhoneEmph[i][j].p[2] = adwCount[i][j];
   } // i
   // also include NULL so dont overcompensate based on Ngram
   for (j = 0; j < PHONEPOSBIN; j++) {
      adwCountNULL[j] = 10;
      apPhoneEmphNULL[j].p[0] = apPhoneEmphNULL[j].p[1] = apPhoneEmphNULL[j].p[2] = 
         adwCountNULL[j];
   }

   // loop through all the senteces
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();
      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));
      PPHONEAN ppa = (PPHONEAN)pwa->m_lPHONEAN.Get(0);

      DWORD j, k;
      CPoint pReal, pExpected;

      // potentially exclude from prosody model
      if (pwa->m_fExcludeFromProsodyModel)
         continue;


      for (j = 0; j < dwNum; j++, pw++) {
         // figure out part of speech to left and right
         BYTE bPOSLeft, bPOSRight;
         bPOSLeft = bPOSRight = POS_MAJOR_EXTRACT(POS_MAJOR_PUNCTUATION);
         if (j && pw->dwPunctLeftDist)
            bPOSLeft = pw[-1].bPOS;
         if ((j+1 < dwNum) && pw->dwPunctRightDist)
            bPOSRight = pw[1].bPOS;
         bPOSLeft = min(bPOSLeft, PHONEPOSBIN-1);
         bPOSRight = min(bPOSRight, PHONEPOSBIN-1);

         // sum up the expected length in SR units
         DWORD dwNumPhone = pw->dwPhoneEnd - pw->dwPhoneStart;
         for (k = pw->dwPhoneStart; k < pw->dwPhoneEnd; k++) {
            // which index is this from the start or end of the phone
            DWORD dwIndexL = k - pw->dwPhoneStart;
            DWORD dwIndexR = pw->dwPhoneEnd - k - 1;
            BOOL fFromStart = (dwIndexL < NUMPHONEEMPH);
            BOOL fFromEnd = (dwIndexR < NUMPHONEEMPH);
            dwIndexR = NUMPHONEEMPH*2-dwIndexR-1; // so index from 0..NUMPHONEEMPH*2-1

            // see how this phoneme differs from what expect
            PTRIPHONETRAIN pt = (PTRIPHONETRAIN)pAnal->plTRIPHONETRAIN->Get(ppa[k].dwTRIPHONETRAINIndex);
            if (!pt) // backoff to unknown triphone
               pt = GetSpecificTRIPHONETRAIN (this, pAnal, ppa + k, m_dwTriPhoneGroup, pLex);

            pReal.p[0] = pow (2.0, (fp)ppa[k].iPitch / 1000.0);
            pExpected.p[0] = pow (2.0, (fp)pt->iPitch / 1000.0);

            pReal.p[1] = ppa[k].fEnergyRatio;
            pExpected.p[1] = pt->fEnergyRatio;
            //pReal.p[1] = ppa[k].fEnergyAvg;
            //pExpected.p[1] = pt->fEnergyAvg;

            pReal.p[2] = ppa[k].dwTimeEnd - ppa[k].dwTimeStart;
            pExpected.p[2] = pt->dwDurationSRFeat;

            pReal.p[0] /= pExpected.p[0];
            pReal.p[1] /= pExpected.p[1];
            pReal.p[2] /= pExpected.p[2];

            // BUGFIX - Was getting a divide by zero, with nan energyratio
            if (_isnan(pReal.p[1]))
               pReal.p[1] = 1.0; // so no divide by zero

            // figure out POS to the left
            DWORD dwRight;
            for (dwRight = 0; dwRight < 2; dwRight++) {
               // figure out part of speech
               BYTE bPOS = dwRight ? bPOSRight : bPOSLeft;

               // include  count in NULL
               adwCountNULL[bPOS]++;
               apPhoneEmphNULL[bPOS].Add (&pReal);

               // and include in part of 
               BOOL fOK = dwRight ? fFromEnd : fFromStart;
               if (!fOK)
                  continue;   // out of range

               DWORD dwIndex = dwRight ? dwIndexR : dwIndexL;
               DWORD dwPunctDist = dwRight ? pw->dwPunctRightDist : pw->dwPunctLeftDist;

               pAnal->apPhoneEmph[dwIndex][bPOS].Add (&pReal);
               adwCount[dwIndex][bPOS]++;
            } // dwRight
         } // k
      } // j, over words
   } // i, over all files

   // scale these
   for (j = 0; j < PHONEPOSBIN; j++)
      apPhoneEmphNULL[j].Scale (1.0 /  adwCountNULL[j]);
   for (i = 0; i < NUMPHONEEMPH*2; i++) for (j = 0; j < PHONEPOSBIN; j++) {
      pAnal->apPhoneEmph[i][j].Scale (1.0 / adwCount[i][j]);

      // remove generic influence that's regardless of where phoneme is in word
      // If don't do this, will get interference with N-gram
      pAnal->apPhoneEmph[i][j].p[0] /= apPhoneEmphNULL[j].p[0];
      pAnal->apPhoneEmph[i][j].p[1] /= apPhoneEmphNULL[j].p[1];
      pAnal->apPhoneEmph[i][j].p[2] /= apPhoneEmphNULL[j].p[2];

#ifdef _DEBUG
      if (pAnal->apPhoneEmph[i][j].p[1] > 10)
         pAnal->apPhoneEmph[i][j].p[1] = 90; 
      if (!apPhoneEmphNULL[j].p[1] || (fabs(apPhoneEmphNULL[j].p[1]) < CLOSE) || !adwCount[i][j] || _isnan(apPhoneEmphNULL[j].p[1]) )
         pAnal->apPhoneEmph[i][j].p[1] = 90; 
#endif
   } // i,j


   // NOTE: Not applying the phone length affects elsewhere because not really sure
   // the best way to include the math. This will introduce some error to the word
   // lengths, but hopefully not much

   // copy over learned info
   pTTS->ProsPhoneEmph (pAnal->apPhoneEmph);

   return;
}

/*************************************************************************************
CTTSWork::WordEmphFromFuncWord - Looks theough the paplWEMPH list in the analysis
information and determines how much to emphasize a word based on a function word.
This then adds all the function word modifiers to pTTS

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS         pTTS - TTS to add the PCTTSPunctPros to
returns
   none
*/
typedef struct {
   DWORD    adwBefore[PUNCTPROS];   // weight of before
   DWORD    adwAfter[PUNCTPROS];    // weight of after
} PPCOUNT, *PPPCOUNT;

#if 0 // old prosody
void CTTSWork::WordEmphFromFuncWord (PTTSANAL pAnal, PCMTTS pTTS)
{
   // if no function words exist
   if (!m_pLexFuncWords)
      return;

   // maintain a list of CTTSPunctPros to add adn their weights
   CListFixed lPP, lCount;
   lPP.Init (sizeof(PCTTSPunctPros));
   lCount.Init (sizeof(PPCOUNT));

   // loop through all the senteces
   DWORD i, j, k, l;
   PCTTSPunctPros *ppp;
   PCTTSPunctPros pp;
   PPPCOUNT ppc;
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      for (j = 0; j < dwNum; j++) {
         // if this isn't one of the keywords then ignore
         WORD wFuncWord = (WORD) m_pLexFuncWords->WordFind (pw[j].pszWord);
         if (-1 == wFuncWord)
            continue;

         // see if can find it
         ppp = (PCTTSPunctPros*)lPP.Get(0);
         for (k = 0; k < lPP.Num(); k++)
            if (ppp[k]->m_wFuncWord == wFuncWord)
               break;
         if (k >= lPP.Num()) {
            // addd
            PPCOUNT pc;
            memset (&pc, 0, sizeof(pc));
            for (l = 0; l < PUNCTPROS; l++)
               pc.adwAfter[l] = pc.adwBefore[l] = 10; // so if not enough points no problem
            pp = new CTTSPunctPros;
            if (!pp)
               continue;   // error
            for (l = 0; l < PUNCTPROS; l++) {
               pp->m_apAfter[l].p[0] = pp->m_apAfter[l].p[1] = pp->m_apAfter[l].p[2] = pc.adwAfter[l];
               pp->m_apBefore[l].p[0] = pp->m_apBefore[l].p[1] = pp->m_apBefore[l].p[2] = pc.adwBefore[l];
            }
            pp->m_wFuncWord = wFuncWord;
            pp->m_wPunct = 0;
            lPP.Add (&pp);
            lCount.Add (&pc);
            k = lCount.Num()-1;
         }
         else
            pp = ppp[k];   // point directly there

         // get the count
         ppc = (PPPCOUNT) lCount.Get(k);

         // look left of function word and see how emphasis varies
         int iLoc;
         for (k = 0; k < PUNCTPROS; k++) {
            iLoc = (int)j - (int)k - 1;
            if (iLoc < 0)
               continue;

            pp->m_apBefore[k].Add (&pw[iLoc].pWrdEmph);
            ppc->adwBefore[k]++;
         } // k

         // look right of function word and see how emphasis varies
         for (k = 0; k < PUNCTPROS; k++) {
            iLoc = (int)j + (int)k + 1;
            if (iLoc >= (int)dwNum)
               continue;

            pp->m_apAfter[k].Add (&pw[iLoc].pWrdEmph);
            ppc->adwAfter[k]++;
         } // k
      } // j, over words
   } // i, over all files


   // loop through and divide by count
   ppp = (PCTTSPunctPros*)lPP.Get(0);
   ppc = (PPPCOUNT) lCount.Get(0);
   for (i = 0; i < lPP.Num(); i++) for (j = 0; j < PUNCTPROS; j++) {
      ppp[i]->m_apAfter[j].Scale (1.0 / (fp)ppc[i].adwAfter[j]);
      ppp[i]->m_apBefore[j].Scale (1.0 / (fp)ppc[i].adwBefore[j]);
   }


   // go back through and reverse the emphasis in the words so don't
   // count emphasis from different methods twice
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      for (j = 0; j < dwNum; j++) {
         // if this isn't one of the keywords then ignore
         WORD wFuncWord = (WORD) m_pLexFuncWords->WordFind (pw[j].pszWord);
         if (-1 == wFuncWord)
            continue;

         // see if can find it
         for (k = 0; k < lPP.Num(); k++)
            if (ppp[k]->m_wFuncWord == wFuncWord)
               break;
         if (k >= lPP.Num())
            continue;
         pp = ppp[k];

         // look left of function word and adjust emphasis
         int iLoc;
         for (k = 0; k < PUNCTPROS; k++) {
            iLoc = (int)j - (int)k - 1;
            if (iLoc < 0)
               continue;

            for (l = 0; l < 3; l++)
               pw[iLoc].pWrdEmph.p[l] /= pp->m_apBefore[k].p[l];
         } // k

         // look right of function word and see how emphasis varies
         for (k = 0; k < PUNCTPROS; k++) {
            iLoc = (int)j + (int)k + 1;
            if (iLoc >= (int)dwNum)
               continue;

            for (l = 0; l < 3; l++)
               pw[iLoc].pWrdEmph.p[l] /= pp->m_apAfter[k].p[l];
         } // k
      } // j, over words
   } // i, over all files

   // add to tts engine
   for (i = 0; i < lPP.Num(); i++)
      pTTS->PunctProsAdd (ppp[i]);
}
#endif // 0


/*************************************************************************************
CTTSWork::WordEmphFromPunct - Looks theough the paplWEMPH list in the analysis
information and determines how much to emphasize a word based on punctuation word.
This then adds all the punctuation word modifiers to pTTS

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS         pTTS - TTS to add the PCTTSPunctPros to
returns
   none
*/

#if 0 // old prosody
void CTTSWork::WordEmphFromPunct (PTTSANAL pAnal, PCMTTS pTTS)
{
   // if no function words exist
   if (!m_pLexFuncWords)
      return;

   // maintain a list of CTTSPunctPros to add adn their weights
   CListFixed lPP, lCount;
   lPP.Init (sizeof(PCTTSPunctPros));
   lCount.Init (sizeof(PPCOUNT));

   // loop through all the senteces
   DWORD i, j, k, l, dwLeft;
   PCTTSPunctPros *ppp;
   PCTTSPunctPros pp;
   PPPCOUNT ppc;
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      for (j = 0; j < dwNum; j++) {
         // find the left/right
         for (dwLeft = 0; dwLeft < 2; dwLeft++) {
            // if too far away then ignore
            DWORD dwDist = (dwLeft ? pw[j].dwPunctLeftDist : pw[j].dwPunctRightDist);
            if (dwDist >= PUNCTPROS)
               continue;

            WORD wPunct = (dwLeft ? pw[j].wPunctLeft : pw[j].wPunctRight);
            if (!wPunct)
               continue;

            // see if can find it
            ppp = (PCTTSPunctPros*)lPP.Get(0);
            for (k = 0; k < lPP.Num(); k++)
               if (ppp[k]->m_wPunct == wPunct)
                  break;
            if (k >= lPP.Num()) {
               // addd
               PPCOUNT pc;
               memset (&pc, 0, sizeof(pc));
               for (l = 0; l < PUNCTPROS; l++)
                  pc.adwAfter[l] = pc.adwBefore[l] = 10; // so if not enough points no problem
               pp = new CTTSPunctPros;
               if (!pp)
                  continue;   // error
               for (l = 0; l < PUNCTPROS; l++) {
                  pp->m_apAfter[l].p[0] = pp->m_apAfter[l].p[1] = pp->m_apAfter[l].p[2] = pc.adwAfter[l];
                  pp->m_apBefore[l].p[0] = pp->m_apBefore[l].p[1] = pp->m_apBefore[l].p[2] = pc.adwBefore[l];
               }
               pp->m_wFuncWord = 0;
               pp->m_wPunct = wPunct;
               lPP.Add (&pp);
               lCount.Add (&pc);
               k = lCount.Num()-1;
            }
            else
               pp = ppp[k];   // point directly there

            // get the count
            ppc = (PPPCOUNT) lCount.Get(k);

            // include this
            if (dwLeft) {
               pp->m_apBefore[dwDist].Add (&pw[j].pWrdEmph);
               ppc->adwBefore[dwDist]++;
            }
            else {
               pp->m_apAfter[dwDist].Add (&pw[j].pWrdEmph);
               ppc->adwAfter[dwDist]++;
            }
         } // dwLeft

      } // j, over words
   } // i, over all files


   // loop through and divide by count
   ppp = (PCTTSPunctPros*)lPP.Get(0);
   ppc = (PPPCOUNT) lCount.Get(0);
   for (i = 0; i < lPP.Num(); i++) for (j = 0; j < PUNCTPROS; j++) {
      ppp[i]->m_apAfter[j].Scale (1.0 / (fp)ppc[i].adwAfter[j]);
      ppp[i]->m_apBefore[j].Scale (1.0 / (fp)ppc[i].adwBefore[j]);
   }


   // go back through and reverse the emphasis in the words so don't
   // count emphasis from different methods twice
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      for (j = 0; j < dwNum; j++) {
         // find the left/right
         for (dwLeft = 0; dwLeft < 2; dwLeft++) {
            // if too far away then ignore
            DWORD dwDist = (dwLeft ? pw[j].dwPunctLeftDist : pw[j].dwPunctRightDist);
            if (dwDist >= PUNCTPROS)
               continue;

            WORD wPunct = (dwLeft ? pw[j].wPunctLeft : pw[j].wPunctRight);
            if (!wPunct)
               continue;

            // see if can find it
            ppp = (PCTTSPunctPros*)lPP.Get(0);
            for (k = 0; k < lPP.Num(); k++)
               if (ppp[k]->m_wPunct == wPunct)
                  break;
            if (k >= lPP.Num())
               continue;   // too far
            pp = ppp[k];

            // include this
            if (dwLeft) {
               for (l = 0; l < 3; l++)
                  pw[j].pWrdEmph.p[l] /= pp->m_apBefore[dwDist].p[l];
            }
            else {
               for (l = 0; l < 3; l++)
                  pw[j].pWrdEmph.p[l] /= pp->m_apAfter[dwDist].p[l];
            }
         } // dwLeft
      } // j, over words
   } // i, over all files

   // add to tts engine
   for (i = 0; i < lPP.Num(); i++)
      pTTS->PunctProsAdd (ppp[i]);
}
#endif // 0

/*************************************************************************************
CTTSWork::WordEmphFromNGram - Looks theough the paplWEMPH list in the analysis
information and determines how much to emphasize a word based on the ngram.
This then adds all the punctuation word modifiers to pTTS

inputs
   PTTSANAL       pAnal - Analysis information
   PCMTTS         pTTS - TTS to add the ngram to
returns
   none
*/

#if 0 // old prosody
void CTTSWork::WordEmphFromNGram (PTTSANAL pAnal, PCMTTS pTTS)
{
   // loop through all the senteces
   DWORD i, j, k;
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCListFixed pl = pAnal->paplWEMPH[i];
      if (!pl)
         continue;

      PWEMPH pw = (PWEMPH) pl->Get(0);
      DWORD dwNum = pl->Num();

      for (j = 0; j < dwNum; j++) {
         // BUGFIX - Since increase # of N-grams, changed to loop
         for (k = TTSPROSNGRAM + TTSPROSNGRAMBIT; k; k--) {
            PopulateNGrams (pAnal, pw, dwNum, j, k, k);
            PopulateNGrams (pAnal, pw, dwNum, j, k, k-1);
            PopulateNGrams (pAnal, pw, dwNum, j, k-1, k);
         } // k

         //PopulateNGrams (pAnal, pw, dwNum, j, 2, 2);
         //PopulateNGrams (pAnal, pw, dwNum, j, 2, 1);  // backoff
         //PopulateNGrams (pAnal, pw, dwNum, j, 1, 2);  // backoff
         //PopulateNGrams (pAnal, pw, dwNum, j, 1, 1);  // backoff
         //PopulateNGrams (pAnal, pw, dwNum, j, 1, 0);  // backoff
         //PopulateNGrams (pAnal, pw, dwNum, j, 0, 1);  // backoff

         PopulateNGrams (pAnal, pw, dwNum, j, 0, 0);  // backoff
      } // j, over words
   } // i, over all files

   // allocate space in tts for this
   DWORD dwTotal = (DWORD)pow(POS_MAJOR_NUM+1, TTSPROSNGRAM*2+1) * (DWORD)pow(3, TTSPROSNGRAMBIT*2);
   DWORD dwSize = dwTotal * sizeof(TTSNGRAM);
   PCMem pNGram = pTTS->MemNGramGet ();
   if (!pNGram->Required (dwSize)) {
      pNGram->m_dwCurPosn = 0;
      return;
   }
   pNGram->m_dwCurPosn = dwSize;

   // loop through all the ngrams and transfer over info
   PTTSWORKNGRAM pwng = pAnal->paTTSNGram;
   PTTSNGRAM png = (PTTSNGRAM)pNGram->p;
   for (i = 0; i < dwTotal; i++, pwng++, png++) {
      // if there arent enough entries then ignore
      if (pwng->pProsody.p[3] < (fp)MINNUMEXAMPLES) {   // BUGFIX - If less than 3 uses then not statitically signficant
         png->bDur = png->bFlags = png->bPitch = png->bVol = 0;
         continue;
      }

      // scale...
      pwng->pProsody.Scale (100.0 / pwng->pProsody.p[3]);
      for (j = 0; j < 3; j++) {
         pwng->pProsody.p[j] = max(pwng->pProsody.p[j], 50);
         pwng->pProsody.p[j] = min(pwng->pProsody.p[j], 200);
      }
      png->bPitch = (BYTE)pwng->pProsody.p[0];
      png->bVol = (BYTE)pwng->pProsody.p[1];
      png->bDur = (BYTE)pwng->pProsody.p[2];
      // BUGFIX - Was 1/2 but now require 2/3 of cases before put in pause
      // BUGFIX - Made 3/4 because still seem to have too many pauses
      // BUGFIX - Put minimum count in
      if ((pwng->dwPauseTotal > 3) && (pwng->dwPauseLeft > MINPAUSECOUNT(pwng->dwPauseTotal)))
         png->bFlags = 0x01;  // for pause to left
      else
         png->bFlags = 0;  // no pause
   } // i


   // done
}
#endif // 0, old prosody




/*************************************************************************************
CTTSWork::FuncWordWeight - Returns a weighting for the word.

inputs
   PWSTR       psz - String of the word
   DWORD       *pdwGroup - Filled in with the current group, 0..NUMFUNCWORDGROUP(inclusive)
returns
   fp - 0.001 to 1.0. Low values are for very common function words. 1.0 is for
      non-function words
*/
fp CTTSWork::FuncWordWeight (PWSTR psz, DWORD *pdwGroup)
{
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
   DWORD i;
   for (i = 0; i < NUMFUNCWORDGROUP; i++)
      if (m_apLexFuncWord[i] && m_apLexFuncWord[i]->WordExists (psz)) {
         //DWORD dwOld = i ? (16 << (i-1)) : 0;
         //DWORD dwNew = (16 << i);
         //DWORD dwMax = (16 << (NUMFUNCWORDGROUP-1));
         *pdwGroup = i;
         return (fp)i / (fp)NUMFUNCWORDGROUP * 0.9 + 0.1;   // BUGFIX - so gentler slope
         //return (fp)((dwNew-dwOld)/2 + dwOld) / (fp)dwMax;
      }

   // else not
#endif // 0
   *pdwGroup = NUMFUNCWORDGROUP;
   return 1.0;
}

/*************************************************************************************
CWaveAn::Constructor and destructor
*/
CWaveAn::CWaveAn (void)
{
   m_lWORDAN.Init (sizeof(WORDAN));
   m_lPHONEAN.Init (sizeof(PHONEAN));
   m_lSYLAN.Init (sizeof(SYLAN));
   m_pWave = NULL;
   m_fMaxEnergy = 0;
   m_fAvgEneryPerPhone = 0;
   m_fAvgEnergyForVoiced = 0;
   m_fAvgPitch = 0;
   m_dwAvgPitchCount = 0;
   m_fAvgSyllableDur = 0;
   m_dwSyllableCount = 0;
   m_szFile[0] = 0;
   m_fExcludeFromProsodyModel = FALSE;
}

CWaveAn::~CWaveAn (void)
{
   if (m_pWave)
      delete m_pWave;
}


/*************************************************************************************
CWaveAn::AnalyzeWave - Analyzes the wave

inputs
   PWSTR             pszFile - Wave file to open
   // PCProgress        pProgress - Progress to use
   DWORD             dwWaveNum - Number of the wave, to store in the TPHONEINST and WORDINST info
   PCListFixed       paplTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] entries for storing
                        the tri-phone info extracted info. New lists will be created as necessary
                        to store TPHONELIST structures.
   PCListFixed       paplWord[] - Pointer to an array of m_pLexWords.WordNum() entries which contain
                        pointers to lists of word instances. New lists will be created as necessary
                        to store WORDINST structures
   PCTTSWork         pTTS - TTS working set to use
   PCVoiceFile       pVF - Speech recognizer to use
   double            *pafEnergyPerPitch - From TTSANAL
   double            *pafEnergyPerVolume - From TTSANAL
   LPCRITICAL_SECTION lpcs - Critical section to use for important info
   double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount - As per TTSANAL
returns
   BOOL - TRUE if success
*/
BOOL CWaveAn::AnalyzeWave (PWSTR pszFile, /*PCProgressSocket pProgress,*/ DWORD dwWaveNum,
                            PCListFixed *paplTriPhone /*[][PHONEGROUPSQUARE]*/, PCListFixed paplWord[],
                            PCTTSWork pTTS, PCVoiceFile pVF, double *pafEnergyPerPitch, double *pafEnergyPerVolume,
                            LPCRITICAL_SECTION lpcs,
                            double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount)
{
   if (m_pWave)
      return FALSE;

   m_pWave = NULL;

   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0,0);
   m_pWave = new CM3DWave;
   if (!m_pWave)
      return FALSE;
   if (!m_pWave->Open (NULL, szTemp, FALSE))
      return FALSE;

   // make sure there are srfeatures in here
   PCProgressSocket pProgress = NULL;
   if (!m_pWave->m_dwSRSamples || !m_pWave->m_adwPitchSamples[PITCH_F0] || !m_pWave->m_adwPitchSamples[PITCH_SUB]) {
         // BUGBUG - May want to test for PITCH_SUB
      m_pWave->RequireWave(NULL);

      // calculate pitch
      if (pProgress)
         pProgress->Push (0, .33);
      m_pWave->CalcPitch (pTTS->m_fFullPCM ? WAVECALC_TTS_FULLPCM : WAVECALC_TTS_PARTIALPCM, pProgress);
      if (pProgress)
         pProgress->Pop ();

      // calculate SR features
      if (pProgress)
         pProgress->Push (.33, .5);
      m_pWave->CalcSRFeaturesIfNeeded (pTTS->m_fFullPCM ? WAVECALC_TTS_FULLPCM : WAVECALC_TTS_PARTIALPCM, NULL, pProgress);
      if (pProgress)
         pProgress->Pop ();

      // may need to calc PITCH_SUB
      if (!m_pWave->m_adwPitchSamples[PITCH_SUB])
         m_pWave->CalcPitchIfNeeded (WAVECALC_TTS_FULLPCM, NULL);

      // save so dont need to do again
      m_pWave->Save (TRUE, NULL);

      // dont do because frees up sr features - m_pWave->ReleaseWave();
   }

   // make sure there's sr
   if (!m_pWave->m_lWVPHONEME.Num() || !m_pWave->m_lWVWORD.Num()) {
      m_pWave->RequireWave(NULL);

      if (pProgress)
         pProgress->Push (.5, 1);
      pVF->Recognize ((PWSTR)m_pWave->m_memSpoken.p, m_pWave, FALSE, pProgress);
      if (pProgress)
         pProgress->Pop ();

      // may need to calc PITCH_SUB
      if (!m_pWave->m_adwPitchSamples[PITCH_SUB])
         m_pWave->CalcPitchIfNeeded (WAVECALC_TTS_FULLPCM, NULL);

      // save so dont need to do again
      m_pWave->Save (TRUE, NULL);

      // dont do because frees up srfeatures- m_pWave->ReleaseWave();
   }

   // calculate the energy for the totla wave
   DWORD j;
   m_fMaxEnergy = 0;
   m_fAvgEneryPerPhone = 0;
   m_fAvgEnergyForVoiced = 0;
   // NOTE: Leavining m_fMaxEnergy calculations in here, but will recalc in analyzewaveint
   for (j = 0; j < m_pWave->m_dwSRSamples; j++) {
      fp fEnergy = SRFEATUREEnergy (FALSE, &m_pWave->m_paSRFeature[j]); // note: cant counteract brightness here, so dont bother
         // accessing m_paSRFeature here is OK since guaranteed to have features here
      m_fMaxEnergy = max(m_fMaxEnergy, fEnergy);
   }

   // pull out phones and words
   if (!AnalyzeWaveInt (dwWaveNum, paplTriPhone, paplWord, pTTS, pVF, pafEnergyPerPitch, pafEnergyPerVolume, lpcs,
      pafPhonemePitchSum, pafPhonemeDurationSum, pafPhonemeEnergySum, padwPhonemeCount))
      return FALSE;

   // BUGFIX - Move calculating the average pitch to after calculate pitch
   // for words and phonemes

   // figure out average pitch
   // BUGFIX - Was just getting for the whole wave, but the latter is more accurate
   // m_fAvgPitch = m_pWave->PitchOverRange (0, m_pWave->m_dwSamples, 0);
   m_fAvgPitch = 0;
   m_dwAvgPitchCount =0;
   m_fAvgSyllableDur = 0;
   m_dwSyllableCount = 0;

   // syllable durations
   PSYLAN psa = (PSYLAN) m_lSYLAN.Get(0);
   for (j = 0; j < m_lSYLAN.Num(); j++) {
      m_fAvgPitch += log(max(1.0, psa[j].fPitch) / SRBASEPITCH);
      m_dwAvgPitchCount++;
      m_fAvgSyllableDur += (fp)(psa[j].dwTimeEnd - psa[j].dwTimeStart) / (fp)m_pWave->m_dwSRSAMPLESPERSEC;
      m_dwSyllableCount++;
   } // j
   if (m_dwAvgPitchCount)
      m_fAvgPitch /= (fp) m_dwAvgPitchCount;
   m_fAvgPitch = exp (m_fAvgPitch) * SRBASEPITCH;
   if (m_dwSyllableCount)
      m_fAvgSyllableDur /= (fp)m_dwSyllableCount;

   // release the SR features so don't use too much memory
   m_pWave->ReleaseSRFeatures();

   return TRUE;
}


/*************************************************************************************
CWaveAn::ENERGYPERPITCHIncorporate - Incorporate the SRFEATURE in the energy-per-pitch
sum so that can counteract this later.

inputs
   PSRFEATURE        psrf - SRFEATURE of a voiced phoneme to incorproate
   fp                fPitch - Pitch at the sample
   double            *pafEnergyPerPitch - From TTSANAL
returns
   none
*/
void CWaveAn::ENERGYPERPITCHIncorporate (PSRFEATURE psrf, fp fPitch, double *pafEnergyPerPitch)
{
   // convert the pitch to index into pafEnergyPerPitch
   if (fPitch <= CLOSE)
      return;  // error
   fPitch = log(fPitch / (fp)ENERGYPERPITCHBASE) / log((fp)2) * (fp)ENERGYPERPITCHPOINTSPEROCTAVE + 0.5;
   fPitch = max(fPitch, 0.0);
   fPitch = min(fPitch, (fp)ENERGYPERPITCHNUM-1);

   DWORD dwIndex = (DWORD)fPitch;
   double fAlpha = (double) fPitch - (double)dwIndex;
   double fOneMinus = 1.0 - fAlpha;
   DWORD dwIndex2 = min(dwIndex+1, ENERGYPERPITCHNUM-1);

   // find out where stores
   double *paf = pafEnergyPerPitch + (dwIndex * (SRDATAPOINTS+1));
   double *paf2 = pafEnergyPerPitch + (dwIndex2 * (SRDATAPOINTS+1));

   // add in
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      double f = DbToAmplitude (psrf->acVoiceEnergy[i]);
      paf[i] += f * fOneMinus;
      paf2[i] += f * fAlpha;
   } // i

   // increaste found
   paf[SRDATAPOINTS] += (double)fOneMinus;
   paf2[SRDATAPOINTS] += (double)fAlpha;
}


/*************************************************************************************
CWaveAn::ENERGYPERVOLUMEIncorporate - Incorporate the SRFEATURE in the energy-per-volume
sum so that can counteract this later.

inputs
   PSRFEATURE        psrf - SRFEATURE of a voiced phoneme to incorproate
   fp                fEnergyRatio - Ratio of energy compared to average energy of voiced phonemes
   double            *pafEnergyPerVolume - From TTSANAL
returns
   none
*/
void CWaveAn::ENERGYPERVOLUMEIncorporate (PSRFEATURE psrf, fp fEnergyRatio, double *pafEnergyPerVolume)
{
   // convert the volume to index into pafEnergyPerVolume
   fEnergyRatio = max(fEnergyRatio, CLOSE);
   fEnergyRatio = log(fEnergyRatio) / log((fp)2) * (fp)ENERGYPERVOLUMEPOINTSPEROCTAVE + 0.5;
   fEnergyRatio += (fp)ENERGYPERVOLUMECENTER;
   fEnergyRatio = max(fEnergyRatio, 0.0);
   fEnergyRatio = min(fEnergyRatio, (fp)ENERGYPERVOLUMENUM-1);

   DWORD dwIndex = (DWORD)fEnergyRatio;
   double fAlpha = (double) fEnergyRatio - (double)dwIndex;
   double fOneMinus = 1.0 - fAlpha;
   DWORD dwIndex2 = min(dwIndex+1, ENERGYPERVOLUMENUM-1);

   // find out where stores
   double *paf = pafEnergyPerVolume + (dwIndex * (SRDATAPOINTS+1));
   double *paf2 = pafEnergyPerVolume + (dwIndex2 * (SRDATAPOINTS+1));

   // add in
   DWORD i;
   for (i = 0; i < SRDATAPOINTS; i++) {
      double f = DbToAmplitude (psrf->acVoiceEnergy[i]);
      paf[i] += f * fOneMinus;
      paf2[i] += f * fAlpha;
   } // i

   // increaste found
   paf[SRDATAPOINTS] += (double)fOneMinus;
   paf2[SRDATAPOINTS] += (double)fAlpha;
}


/*************************************************************************************
CWaveAn::AnalyzeWaveInt - This takes a wave file and analyzes is to determine what
triphones and words are in it. It then fills in the appropriate structures to
keep track of the triphone.

NOTE: m_pWave isave file that analyzing. The wave must already contain
                        SRFEATURE information and segmentation.

inputs
   DWORD             dwWaveNum - Number of the wave, to store in the TPHONEINST and WORDINST info
   PCListFixed       paplTriPhone[][PHONEGROUPSQUARE] - Pointer to an array of [4][PHONEGROUPSQUARE] entries for storing
                        the tri-phone info extracted info. New lists will be created as necessary
                        to store TPHONELIST structures.
   PCListFixed       paplWord[] - Pointer to an array of m_pLexWords.WordNum() entries which contain
                        pointers to lists of word instances. New lists will be created as necessary
                        to store WORDINST structures
   PCTTSWork         pTTS - TTS working set to use
   PCVoiceFile       pVF - Speech recognizer to use
   double            *pafEnergyPerPitch - From TTSANAL
   double            *pafEnergyPerVolume - From TTSANAL
   LPCRITICAL_SECTION   lpcs - Critical section for paplTriPhone, paplWord, and pafEnergyPerPitch
   double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount - As per TTSANAL
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CWaveAn::AnalyzeWaveInt (DWORD dwWaveNum,
                            PCListFixed *paplTriPhone /*[][PHONEGROUPSQUARE]*/, PCListFixed paplWord[],
                            PCTTSWork pTTS, PCVoiceFile pVF, double *pafEnergyPerPitch, double *pafEnergyPerVolume,
                            LPCRITICAL_SECTION lpcs,
                            double *pafPhonemePitchSum, double *pafPhonemeDurationSum, double *pafPhonemeEnergySum, DWORD *padwPhonemeCount)
{
   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   PCWSTR pszSilence = pLex->PhonemeSilence();
   BYTE bSilence = pLex->PhonemeFindUnsort (pszSilence);
   DWORD dwNumPhone = min(pLex->PhonemeNum(), 255);

#ifndef OLDSR
   CSRAnal SRAnal;
   PSRANALBLOCK psab;
   PSRFEATURE psrCache = CacheSRFeatures (m_pWave, 0, m_pWave->m_dwSRSamples); // note: cant counteract pitch brightness here so dont bother
   psab = SRAnal.Init (psrCache, m_pWave->m_dwSRSamples, FALSE, &m_fMaxEnergy);
#endif

   // keep track of phones converted to DWORDs
   CListFixed lPhone, lWord, lPitchWord, lPitchPhone, lPitchPhoneDelta, lPitchPhoneBulge;
   lPhone.Init (sizeof(DWORD));
   lWord.Init (sizeof(DWORD));
   lPitchWord.Init (sizeof(fp));
   lPitchPhone.Init (sizeof(fp));
   lPitchPhoneDelta.Init (sizeof(fp));
   lPitchPhoneBulge.Init (sizeof(fp));


   // look for phonemes
   DWORD dwNum = m_pWave->m_lWVPHONEME.Num();
   PWVPHONEME pwp = (PWVPHONEME) m_pWave->m_lWVPHONEME.Get(0);
   // if (!dwNum)
   //   return FALSE;   // BUGFIX - Was FALSE, but dont want to crash and burn
   
   WORDAN wa;
   PHONEAN pa;
   DWORD i, j, k, l;
   DWORD dwDemi;
   memset (&wa, 0, sizeof(wa));
   wa.dwIndexInWave = -1;
   wa.dwPhoneStart = -1;
   memset (&pa, 0, sizeof(pa));
   pa.dwLexWord = -1;   // so all new phonemes start with no lex associated
   pa.dwTRIPHONETRAINIndex = -1; // so no association with triphone train info
   for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
      pa.afRankAdd[dwDemi] = UNDEFINEDRANK;// so dont get used
   for (i = 0; i < PITCHFIDELITY; i++) for(k = 0; k < DURATIONFIDELITY; k++) for (l = 0; l < ENERGYFIDELITY; l++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
      pa.afRankCompare[i][k][l][dwDemi] = UNDEFINEDRANK;  // so dont get used
   for (i = 0; i < TTSDEMIPHONES; i++)
      pa.abRankAdd[i] = 255;

   // just in case
   if (!m_pWave->m_dwSRSkip)
      m_pWave->m_dwSRSkip = m_pWave->m_dwSamplesPerSec / m_pWave->m_dwSRSAMPLESPERSEC;

   // conver the phoneme strings to numbers
   DWORD dwPhone;
   DWORD dwSubSentenceNum = 0;
   m_lPHONEAN.Required (dwNum);
   fp fPitchStrengthSum = 0;
   DWORD dwPitchStrengthSumCount = 0;
   for (j = 0; j < dwNum; j++) {
      WCHAR szPhone[16];
      memset (szPhone, 0, sizeof(szPhone));
      memcpy (szPhone, pwp[j].awcNameLong, sizeof(pwp[j].awcNameLong));

      dwPhone = pLex->PhonemeFindUnsort (szPhone);

      // see if it has an unstressed version
      PLEXPHONE plp = pLex->PhonemeGetUnsort (dwPhone);
      if (!plp)
         dwPhone = 255;
      pa.bPhone = (BYTE) dwPhone;   // BUGFIX

      // BUGFIX - See if matches the start of a word
      DWORD dwWord;
      dwWord = -1;
      DWORD dwPhonemeStart, dwPhonemeEnd;
      dwPhonemeStart = pwp[j].dwSample;
      dwPhonemeEnd = ((j+1) < dwNum) ? pwp[j+1].dwSample : m_pWave->m_dwSamples;
      pa.dwTimeStart = (dwPhonemeStart + m_pWave->m_dwSRSkip/2) / m_pWave->m_dwSRSkip;
      pa.dwTimeEnd = (dwPhonemeEnd + m_pWave->m_dwSRSkip/2) / m_pWave->m_dwSRSkip;
      pa.dwDuration = dwPhonemeEnd - dwPhonemeStart;  // so know in samples

      fp fPitch = 1.0;  // so have something
      for (k = 0; k < m_pWave->m_lWVWORD.Num(); k++) {
         PWVWORD pwv = (PWVWORD)m_pWave->m_lWVWORD.Get(k);
         PWVWORD pwv2 = (PWVWORD)m_pWave->m_lWVWORD.Get(k+1);
         DWORD dwWordStart = pwv->dwSample;
         DWORD dwWordEnd = pwv2 ? pwv2->dwSample : m_pWave->m_dwSamples;

         if (dwWordEnd <= dwPhonemeStart)
            continue;   // out of range
         else if (dwWordStart > dwPhonemeEnd)
            break;   // gone too far

         // else, within word
         dwWord = k;

         if (dwWordStart == dwPhonemeStart)
            dwPhone |= (1 << 24);   // not that at start of word
         if (dwWordEnd == dwPhonemeEnd)
            dwPhone |= (2 << 24);   // note that at end of word

         // keep track of the pitch of the word
         fPitch = 1; // for now, since will calc later
         //fPitch = m_pWave->PitchOverRange (dwWordStart, dwWordEnd, 0);
         //fPitch = max(fPitch, 1);   // so have something

         // remember this for the word
         // leave till later wa.dwTimeStart = dwWordStart;
         // leave till later wa.dwTimeEnd = dwWordEnd;
         wa.pszWord = (PWSTR)(pwv+1);
         wa.fPitch = fPitch;

         break;
      }

      lPhone.Add (&dwPhone);
      lWord.Add (&dwWord);
      lPitchWord.Add (&fPitch);

      // get the pitch of the phoneme
      fp fPitchDelta, fPitchBulge, fStrength;
      fPitch = m_pWave->PitchOverRange (PITCH_F0, dwPhonemeStart, dwPhonemeEnd, 0, &fStrength, &fPitchDelta, &fPitchBulge,
         TRUE);
         // BUGBUG - may need PITCH_SUB
            // BUGFIX - Ignore strength
         // NOTE: This may cause problems in the future because NOT removing the syllable
         // pitchdelta and pitch over range, which could cause an additive affect.
         // dont expect much of an issue since have many more copies of a phoneme than syllable
      if (fPitch < 1)
         fPitch = max(fPitch, 1);
      lPitchPhone.Add (&fPitch);
      pa.fPitch = fPitch;
      pa.fPitchStrength = fStrength;

      // if it's a voiced phoneme note this
      PLEXENGLISHPHONE pe = NULL;
      if (plp)
         pe = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      pa.fIsVoiced = (pe && (pe->dwCategory & PIC_VOICED));

      // if it's voiced remember this for average strength
      if (pa.fIsVoiced) {
         fPitchStrengthSum += pa.fPitchStrength;
         dwPitchStrengthSumCount++;
      }

      // figure out delta in pitch
      // BUGFIX - Figured it out in PitchOverRange
      //fp fLeft, fRight;
      //fLeft = m_pWave->PitchAtSample (dwPhonemeStart, 0);
      //fRight = m_pWave->PitchAtSample (dwPhonemeEnd, 0);
      //fPitchDelta = max(fRight,1) / max(fLeft,1);
      lPitchPhoneDelta.Add (&fPitchDelta);
      lPitchPhoneBulge.Add (&fPitchBulge);

      // add this word?
      if ((BYTE)dwPhone == bSilence) {
         pa.pWord = NULL;  // since silence phone

         // see if this is punctuation
         PWSTR pszPunct = wa.pszWord ? wa.pszWord : L" ";
         DWORD dwSentenceType = TYPICALSYLINFO_STATEMENT;
         BOOL fNewSentenceNum = TRUE;
         if (pszPunct[0] == L'?')
            dwSentenceType = TYPICALSYLINFO_QUESTION;
         else if (pszPunct[0] == L'!')
            dwSentenceType = TYPICALSYLINFO_EXCLAMATION;
         else if (pszPunct[0] == L'.')
            dwSentenceType = TYPICALSYLINFO_STATEMENT;
         else
            fNewSentenceNum = FALSE;

         // if new sentence that change type of old one
         if (fNewSentenceNum) {
            // loop back over what have written and change the sentence type
            PWORDAN pwa = (PWORDAN) m_lWORDAN.Get(0);
            for (k = 0; k < m_lWORDAN.Num(); k++, pwa++)
               if (pwa->dwSubSentenceNum == dwSubSentenceNum)
                  pwa->dwSentenceType = dwSentenceType;

            // new number
            dwSubSentenceNum++;
         }
      }
      else {
         if ((wa.dwIndexInWave != dwWord) || !m_lWORDAN.Num()) {
            // add it
            wa.dwIndexInWave = dwWord;

            // add the phoneme weight
            wa.fFuncWordWeight = 1.0;
            wa.dwFuncWordGroup = NUMFUNCWORDGROUP;
            if (wa.pszWord)
               wa.fFuncWordWeight = pTTS->FuncWordWeight (wa.pszWord, &wa.dwFuncWordGroup);


            // see if have switched to a new sentence
            // NOTE: The following "new sentence" code isn't foolproof. Also need (dwPhone == bSilence) above
            DWORD dwSentenceType = TYPICALSYLINFO_STATEMENT;
            BOOL fNewSentenceNum = FALSE;
            if (m_lWORDAN.Num()) {
               PWORDAN pwa = (PWORDAN) m_lWORDAN.Get(m_lWORDAN.Num()-1);
               for (k = pwa->dwIndexInWave + 1; k < wa.dwIndexInWave; k++) {
                  PWVWORD pwv = (PWVWORD)m_pWave->m_lWVWORD.Get(k);
                  PWSTR pszPunct = (PWSTR)(pwv+1);

                  if ((pszPunct[0] == L'.') || (pszPunct[0] == L'!') || (pszPunct[0] == L'?')) {
                     if (pszPunct[0] == L'?')
                        dwSentenceType = TYPICALSYLINFO_QUESTION;
                     else if (pszPunct[0] == L'!')
                        dwSentenceType = TYPICALSYLINFO_EXCLAMATION;
                     break;   // found an official sentence break
                  }
               } // k
               if (k < wa.dwIndexInWave) {
                  // loop back over what have written and change the sentence type
                  PWORDAN pwa = (PWORDAN) m_lWORDAN.Get(0);
                  for (k = 0; k < m_lWORDAN.Num(); k++, pwa++)
                     if (pwa->dwSubSentenceNum == dwSubSentenceNum)
                        pwa->dwSentenceType = dwSentenceType;

                  fNewSentenceNum = TRUE; // so add this as part of the sentence
               }
            } // if m_lWORDAN.Num()

            wa.dwSubSentenceNum = dwSubSentenceNum;
            wa.dwSentenceType = dwSentenceType;
            m_lWORDAN.Add (&wa);

            if (fNewSentenceNum)
               dwSubSentenceNum++;
         }

         pa.pWord = (PWORDAN)(size_t)m_lWORDAN.Num();
      }

      // add this phoneme
      m_lPHONEAN.Add (&pa);
   } // j
   if (dwPitchStrengthSumCount)
      fPitchStrengthSum /= (fp)dwPitchStrengthSumCount;
   fPitchStrengthSum = max(fPitchStrengthSum, CLOSE);

   // loop over all the phonean and adjust the word pointer since all words now added
   PWORDAN pwa = (PWORDAN)m_lWORDAN.Get(0);
   PPHONEAN ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   m_fAvgEneryPerPhone = 0;
   m_fAvgEnergyForVoiced = 0;
   DWORD dwARPPCount = 0;
   DWORD dwAEFVCount = 0;
   for (i = 0; i < m_lPHONEAN.Num(); i++, ppa++) {
      // readjust the pitch strength
      if (ppa->fIsVoiced) {
         // calculate how strong the pitch detect was compared to the average for the wave
         ppa->fPitchStrength = sqrt(ppa->fPitchStrength / fPitchStrengthSum);
         ppa->fPitchStrength = max(0.25, ppa->fPitchStrength); // at least something
      }
      else
         ppa->fPitchStrength = 1.0; // just in case use

      if (ppa->pWord) {
         ppa->pWord = pwa + ((DWORD)(size_t)ppa->pWord-1);

         ppa->pWord->dwPhoneStart = min(ppa->pWord->dwPhoneStart, i);
         ppa->pWord->dwPhoneEnd = max(ppa->pWord->dwPhoneEnd, i+1);
      }

      // calculate the enrgy for the phoneme
      ppa->fEnergyAvg = 0;
      ppa->fVoicedEnergy = 0;
      for (j = ppa->dwTimeStart; j < ppa->dwTimeEnd; j++) {
         ppa->fEnergyAvg += SRFEATUREEnergy (FALSE, &m_pWave->m_paSRFeature[j]); // note: cant counteract pitch brightness here so dont bother
            // accessing m_paSRFeature here is OK since guaranteed to have features here

         // also, voiced score
         ppa->fVoicedEnergy += SRFEATUREEnergy (FALSE, &m_pWave->m_paSRFeature[j], TRUE); // note: cant counteract pitch brightness here so dont bother
      } // j
      ppa->fEnergyAvg /= (fp)(ppa->dwTimeEnd - ppa->dwTimeStart);
      ppa->fVoicedEnergy /= (fp)(ppa->dwTimeEnd - ppa->dwTimeStart);
      dwARPPCount++;
      m_fAvgEneryPerPhone += ppa->fEnergyAvg;

      // voiced phoneme energy
      if (ppa->fIsVoiced) {
         m_fAvgEnergyForVoiced += (fp)(ppa->dwTimeEnd - ppa->dwTimeStart) * ppa->fEnergyAvg;
         dwAEFVCount += (ppa->dwTimeEnd - ppa->dwTimeStart);
      }

      EnterCriticalSection (lpcs);
      // sum of pitch, energy, and duration
      pafPhonemePitchSum[ppa->bPhone] += ppa->fPitch;
      pafPhonemeDurationSum[ppa->bPhone] += (double) ppa->dwDuration;
      pafPhonemeEnergySum[ppa->bPhone] += ppa->fEnergyAvg;
      padwPhonemeCount[ppa->bPhone] += 1;
      LeaveCriticalSection (lpcs);

   } // i
   if (dwARPPCount)
      m_fAvgEneryPerPhone /= (fp)dwARPPCount;
   if (dwAEFVCount)
      m_fAvgEnergyForVoiced /= (fp)dwAEFVCount;

   // loop over and store away the spectrums based on the energies of the phoneme compared to the overall
   ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   for (i = 0; i < m_lPHONEAN.Num(); i++, ppa++) {
      // only care about voiced
      if (!ppa->fIsVoiced)
         continue;

      // average this audio into pafEnergyPerPitch, and pafEnergyPerVolume
      EnterCriticalSection (lpcs);
      for (i = ppa->dwTimeStart; i < ppa->dwTimeEnd; i++) {
         ENERGYPERPITCHIncorporate (psrCache + i,
            m_pWave->PitchAtSample(PITCH_F0, i * m_pWave->m_dwSRSkip, 0), pafEnergyPerPitch);
               // BUBUG - may need PITCH_SUB

         fp fEnergy = SRFEATUREEnergy (FALSE, psrCache + i);

         ENERGYPERVOLUMEIncorporate (psrCache + i, fEnergy / m_fAvgEnergyForVoiced, pafEnergyPerVolume);
      }
      LeaveCriticalSection (lpcs);

   } // i
   

   // loop over the words and figure out info
   fp *pafPitchWord = (fp*)lPitchWord.Get(0);
   pwa = (PWORDAN)m_lWORDAN.Get(0);
   ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   fp fPitchSumAllWords = 0, fPitchSumWeightAllWords = 0;
   for (i = 0; i < m_lWORDAN.Num(); i++, pwa++) {
      // readjust phoneme start and stop, just in case
      pwa->dwTimeStart = ppa[pwa->dwPhoneStart].dwTimeStart;
      pwa->dwTimeEnd = ppa[pwa->dwPhoneEnd-1].dwTimeEnd;

      // BUGFIX - Energy average overweights voiced phonemes
      pwa->fEnergyAvg = 0;
      fp fWeightSum = 0;
      fp fWeight;
      for (j = pwa->dwPhoneStart; j < pwa->dwPhoneEnd; j++) {
         fWeight = (!ppa[j].fIsVoiced) ? ENERGYAVGWEIGHT_UNVOICED : (ppa[j].fIsPlosive ? ENERGYAVGWEIGHT_PLOSIVE : ENERGYAVGWEIGHT_VOICED);
         fWeight *= (fp)ppa[j].dwDuration;

         pwa->fEnergyAvg += ppa[j].fEnergyAvg * fWeight;
         fWeightSum += fWeight;
      } // j
      if (fWeightSum)
         pwa->fEnergyAvg /= fWeightSum;
      //for (j = pwa->dwTimeStart; j < pwa->dwTimeEnd; j++)
      //   pwa->fEnergyAvg += SRFEATUREEnergy (&m_pWave->m_paSRFeature[j]); // note: cant counteract pitch brightness here so dont bother
            // accessing m_paSRFeature here is OK since guaranteed to have features here
      //pwa->fEnergyAvg /= (fp)(pwa->dwTimeEnd - pwa->dwTimeStart);

      // keep track of average word energy
      pTTS->m_fWordEnergyAvg += pwa->fEnergyAvg;
      pTTS->m_dwWordCount++;

      // BUGFIX - Dont do by phoneme, but weight
      //fp fWeight = 0, fWeightThis;
      pwa->fPitch = m_pWave->PitchOverRange (PITCH_F0, pwa->dwTimeStart * m_pWave->m_dwSRSkip, pwa->dwTimeEnd * m_pWave->m_dwSRSkip, 0,
         NULL, NULL, NULL, FALSE);
         // BUGBUG - May need PITCH_SUB
         // NOTE: Since this is not by phoneme, pitch weight taken into account

      // figure out average of all words
      fWeight = (pwa->dwTimeEnd - pwa->dwTimeStart);
      fPitchSumWeightAllWords += fWeight;
      fPitchSumAllWords += fWeight * log(pwa->fPitch);

      for (j = pwa->dwPhoneStart; j < pwa->dwPhoneEnd; j++) {
         // include pitch from phonemes, weighted by if voiced
         //fWeightThis = (ppa[j].fIsVoiced ? 1.0 : WEIGHTUNVOICEDFORPITCH) * ppa[j].dwDuration;
         //fWeight += fWeightThis;
         //pwa->fPitch += log(max(1.0,ppa[j].fPitch) / SRBASEPITCH) * fWeightThis;

         // modify the phonemes with the energy ratio
         // BUGFIX - Avoid divide by 0
         if (pwa->fEnergyAvg < CLOSE)
            ppa[j].fEnergyRatio = 1;
         else
            ppa[j].fEnergyRatio = ppa[j].fEnergyAvg / pwa->fEnergyAvg;
      }
      //pwa->fPitch /= fWeight;
      //pwa->fPitch = exp(pwa->fPitch) * SRBASEPITCH;

      // BUGFIX - write in the pitch of the word
      for (j = pwa->dwPhoneStart; j < pwa->dwPhoneEnd; j++)
         pafPitchWord[j] = pwa->fPitch;
   } // i, words
   if (fPitchSumWeightAllWords)
      fPitchSumAllWords /= fPitchSumWeightAllWords;
   fPitchSumAllWords = exp(fPitchSumAllWords);

   // calculate the energy for each...
   CListFixed lEnergy;
   lEnergy.Init (sizeof(fp));

   // loop through and create tri-phones...
   DWORD *padwPhone = (DWORD*)lPhone.Get(0);
   DWORD *padwWord = (DWORD*)lWord.Get(0);
   fp *pafPitchPhone = (fp*)lPitchPhone.Get(0);
   fp *pafPitchPhoneDelta = (fp*)lPitchPhoneDelta.Get(0);
   fp *pafPitchPhoneBulge = (fp*)lPitchPhoneBulge.Get(0);
   CListVariable lForm, lDontRecurse;
   ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   for (i = 0; i < m_lPHONEAN.Num(); i++, ppa++) {
      // no triphones for silence
      if ((LOWORD(padwPhone[i]) == bSilence) || (ppa->dwTimeStart == ppa->dwTimeEnd))
         continue;

      BYTE bCenter = (BYTE)LOWORD(padwPhone[i]);
      BYTE bWordPos = pTTS->m_fWordStartEndCombine ? 0 : (BYTE)(padwPhone[i] >> 24);

      // left and right phonemes...
      BYTE bLeft = (i ? (BYTE)LOWORD(padwPhone[i-1]) : bSilence);
      BYTE bRight = (((i+1) < dwNum) ? (BYTE)LOWORD(padwPhone[i+1]) : bSilence);

      // figure out the triphone number
      for (j = 0; j < NUMTRIPHONEGROUP; j++)
         ppa->awTriPhone[j] = PhoneToTriPhoneNumber (bLeft, bRight, pLex, j);
      ppa->bPhoneLeft = bLeft;
      ppa->bPhoneRight = bRight;

      // find the word...
      ppa->dwWord = -1;
      PWSTR psz = ppa->pWord ? ppa->pWord->pszWord : NULL;
      if (psz && psz[0])
         ppa->dwWord = pTTS->m_pLexWords->WordFind (psz);
      ppa->bPhone = (BYTE)padwPhone[i];
      ppa->bWordPos = pTTS->m_fWordStartEndCombine ? 0 : bWordPos;

      ppa->iPitch = (short)(log(pafPitchPhone[i] / fPitchSumAllWords) / log((fp)2) * 1000.0);
         // BUGFIX - Was dividing by pafPitchWord[i], but caused problems with
         // chinese, so do for entire sentence

#ifdef _DEBUG
      if (pafPitchWord[i] == 1.0)
         OutputDebugString ("\r\npagPitchWord[0] = 1.0");
#endif
      ppa->iPitch = max(ppa->iPitch, -MAXPITCHRANGE);  // BUGFIX - So pitch isn't too far off
      ppa->iPitch = min(ppa->iPitch, MAXPITCHRANGE);
         // BUGFIX - Changed min and max to 1/2 an octave, from one octave since if too large must be error
      ppa->iPitchDelta = (short)(log(pafPitchPhoneDelta[i]) / log((fp)2) * 1000.0);
      ppa->iPitchDelta = max(ppa->iPitchDelta, -MAXPITCHRANGE);  // BUGFIX - So pitch isn't too far off
      ppa->iPitchDelta = min(ppa->iPitchDelta, MAXPITCHRANGE);
      ppa->iPitchBulge = (short)(log(pafPitchPhoneBulge[i]) / log((fp)2) * 1000.0);
      ppa->iPitchBulge = max(ppa->iPitchBulge, -MAXPITCHRANGE);  // BUGFIX - So pitch isn't too far off
      ppa->iPitchBulge = min(ppa->iPitchBulge, MAXPITCHRANGE);
         // BUGFIX - Went from +/-500 to +/-1000 for chinese

      DWORD dwMaxTime = min(ppa->dwTimeEnd, m_pWave->m_adwPitchSamples[PITCH_F0]-1);
      // BUGFIX - Use pitchdelta from linear fit
      ppa->fPitchDelta = pafPitchPhoneDelta[i];
      // BUGFIX - Different way of calculating pitch delta
      //ppa->fPitchDelta = m_pWave->m_apPitch[PITCH_F0][dwMaxTime].fFreq / m_pWave->m_apPitch[PITCH_F0][ppa->dwTimeStart].fFreq;
      //ppa->fPitchVar = fabs(m_pWave->m_apPitch[PITCH_F0][ppa->dwTimeStart].fFreq - m_pWave->m_apPitch[PITCH_F0][dwMaxTime].fFreq) /
      //   (m_pWave->m_apPitch[PITCH_F0][ppa->dwTimeStart].fFreq + m_pWave->m_apPitch[PITCH_F0][dwMaxTime].fFreq);
      ppa->fPitchBulge = pafPitchPhoneBulge[i];

      ppa->fPlosiveness = Plosiveness(m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd) /
         (fp)(ppa->dwTimeEnd - ppa->dwTimeStart);

      // adjust the voiced energy here based on the actual phoneme
      // make this value lower for the type of energy we want
      if (ppa->fEnergyAvg) {
         ppa->fVoicedEnergy /= ppa->fEnergyAvg;
         ppa->fPlosiveness /= ppa->fEnergyAvg;
      }
      PLEXPHONE plp = pLex->PhonemeGetUnsort (ppa->bPhone);
      PLEXENGLISHPHONE ple = NULL;
      if (plp)
         ple = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
      if (ple && !(ple->dwCategory & PIC_VOICED))
         ppa->fVoicedEnergy = 1 - ppa->fVoicedEnergy; // if want unvoiced, then make a low voiced energy a high number

      // Will want amount of plosiveness too
      if (ple && (ple->dwCategory & PIC_PLOSIVE)) {
         ppa->fPlosiveness *= -1;  // lower numbers are better
         ppa->fIsPlosive = TRUE;
      }
      else
         ppa->fIsPlosive = FALSE;

      // brightness
      ppa->fBrightness = FormantBrightness (m_pWave, ppa->dwTimeStart, ppa->dwTimeEnd, ppa->fIsPlosive);

      // BUGFIX - determine the maximum variation that can cut out of the unit
      DWORD dwMaxCut = ((ppa->dwTimeEnd - ppa->dwTimeStart) * MAXPHONETRIM + 50) / 100;
      if (ppa->fIsPlosive)
         dwMaxCut = 0;  // cant trim off plosive

      // figure out the SR score against the main SR voice
      // NOTE: plp set from previous call
      PCPhoneme pPhone = (plp && pVF) ? pVF->PhonemeGet (plp->szPhoneLong, FALSE /*, TRUE*/) : NULL;
#ifdef OLDSR
      lEnergy.Clear();
      for (j = ppa->dwTimeStart; j < ppa->dwTimeEnd; j++) {
         fp fEnergy = SRFEATUREEnergy (m_pWave->m_paSRFeature + j);
         lEnergy.Add (&fEnergy);
      } // j
#endif // oldsr
      ppa->fSRScoreGeneral = 0;
      if (pPhone) {
         fp fScoreCur, fScoreBest = -1;
         DWORD dwTrimLeftCur, dwTrimRightCur, dwTrimLeftBest, dwTrimRightBest;

         for (dwTrimLeftCur = 0; dwTrimLeftCur <= dwMaxCut; dwTrimLeftCur++) {
            for (dwTrimRightCur = 0; dwTrimRightCur + dwTrimLeftCur <= dwMaxCut; dwTrimRightCur++) {
      #ifdef OLDSR
               fScoreCur = pPhone->Compare (
                  m_pWave->m_paSRFeature + (ppa->dwTimeStart + dwTrimLeftCur),
                  (fp*)lEnergy.Get(0) + dwTrimLeftCur, ppa->dwTimeEnd - ppa->dwTimeStart - (dwTrimLeftCur + dwTrimRightCur),
                  m_fMaxEnergy,
                  pLex->PhonemeGetUnsort(bLeft)->szPhone, // is previous silence
                  pLex->PhonemeGetUnsort(bRight)->szPhone, pLex); // is next silence
      #else
               fScoreCur = pPhone->Compare (
                  psab + (ppa->dwTimeStart + dwTrimLeftCur),
                  ppa->dwTimeEnd - ppa->dwTimeStart - (dwTrimLeftCur + dwTrimRightCur),
                  m_fMaxEnergy,
                  pLex->PhonemeGetUnsort(bLeft)->szPhoneLong, // is previous silence
                  pLex->PhonemeGetUnsort(bRight)->szPhoneLong, pLex,
                  FALSE, TRUE /* feature distortion */, FALSE,
                  FALSE /* slow */, FALSE /* all examplars */); // is next silence
                     // BUGFIX - Last param was TRUE, indicating it was for unit selection, but
                     // in this case, want a very broad model for the general score,
                     // but not for the others. Related to OLDUSEALLSRSCORE
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But not really doing anything

               fScoreCur += pPhone->Compare (
                  psab + (ppa->dwTimeStart + dwTrimLeftCur),
                  ppa->dwTimeEnd - ppa->dwTimeStart - (dwTrimLeftCur + dwTrimRightCur),
                  m_fMaxEnergy,
                  pLex->PhonemeGetUnsort(bLeft)->szPhoneLong, // is previous silence
                  pLex->PhonemeGetUnsort(bRight)->szPhoneLong, pLex,
                  FALSE, TRUE /* feature distortion */, TRUE,
                  FALSE /* slow */, FALSE /* all examplars */); // is next silence
                     // BUGFIX - Add in CI model to score, so weight both CD and CI
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But not really doing anything
      #endif

               if (fScoreBest < 0) {
                  fScoreBest = fScoreCur;
                  dwTrimLeftBest = dwTrimLeftCur;
                  dwTrimRightBest = dwTrimRightCur;
               }
               else if (fScoreCur < fScoreBest) {
                  fScoreBest = fScoreCur;
                  dwTrimLeftBest = dwTrimLeftCur;
                  dwTrimRightBest = dwTrimRightCur;
               }
            } // dwTrimRightCur
         } // dwTrimLeftCur

         ppa->fSRScoreGeneral = fScoreBest;
         ppa->dwTrimLeft = dwTrimLeftBest;
         ppa->dwTrimRight = dwTrimRightBest;

         // BUGFIX - Weight SRScoreGeneral higher if it's a function word, so less
         // likely to include in final voice, and less likely to include in
         // SR model
         if (ppa->pWord)
            ppa->fSRScoreGeneral *= (2.0 - ppa->pWord->fFuncWordWeight);
      } // if pPhone

      // fill this into a structure
      TPHONEINST pi;
      memset (&pi, 0, sizeof(pi));
      pi.dwPhoneIndex = i;
      pi.dwWave = dwWaveNum;
      pi.pPHONEAN = ppa;
      //pi.dwWord = ppa->dwWord;
      pi.wTriPhone = ppa->awTriPhone[pTTS->m_dwTriPhoneGroup];
      pi.bPhoneLeft = ppa->bPhoneLeft;
      pi.bPhoneRight = ppa->bPhoneRight;

      //pi.iPitch = ppa->iPitch;
      //pi.iPitchDelta = ppa->iPitchDelta;

      // if triphone list doesnt exist then add
      // if there isn't a  list then add
      EnterCriticalSection (lpcs);
      if (!paplTriPhone[(DWORD)(pTTS->m_fWordStartEndCombine ? 0 : bWordPos) * PHONEGROUPSQUARE + (DWORD)bCenter]) {
         paplTriPhone[(DWORD)(pTTS->m_fWordStartEndCombine ? 0 : bWordPos) * PHONEGROUPSQUARE + (DWORD)bCenter] = new CListFixed;
         paplTriPhone[(DWORD)(pTTS->m_fWordStartEndCombine ? 0 : bWordPos) * PHONEGROUPSQUARE + (DWORD)bCenter]->Init (sizeof(TPHONEINST));
      }
      paplTriPhone[(DWORD)(pTTS->m_fWordStartEndCombine ? 0 : bWordPos) * PHONEGROUPSQUARE + (DWORD)bCenter]->Add (&pi);
      LeaveCriticalSection (lpcs);
   } // i

   // create syllables for all the words
   pwa = (PWORDAN)m_lWORDAN.Get(0);
   ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   CListFixed lPron, lBoundary;
   BYTE bPhone;
   SYLAN sa;
   memset (&sa, 0, sizeof(sa));
   lPron.Init (sizeof(BYTE));
#ifdef _DEBUG
   WCHAR szTemp[128];
   swprintf (szTemp, L"\r\nAnalyzeWaveInt %d", (int)dwWaveNum);
   EscOutputDebugString (szTemp);
#endif
   DWORD dwIndexIntoSubSentence = 0;
   DWORD dwLastSubSentence = 0;
   for (i = 0; i < m_lWORDAN.Num(); i++, pwa++) {
      // create the pronunciaton
      lPron.Clear();
      if (pwa->dwPhoneEnd >= pwa->dwPhoneStart)
         lPron.Required (pwa->dwPhoneEnd - pwa->dwPhoneStart + 1);
      for (j = pwa->dwPhoneStart; j < pwa->dwPhoneEnd; j++) {
         bPhone = ppa[j].bPhone+1;
         lPron.Add (&bPhone);
      } // j
      bPhone = 0;
      lPron.Add (&bPhone);


      // find out how many syllables
      // BUGFIX - If no word string then dont bother with syllables
      if (!pwa->pszWord || !pLex->WordSyllables ((BYTE*)lPron.Get(0), pwa->pszWord, &lBoundary) || !lBoundary.Num()) {
         // fill with something
         DWORD dwLen = lPron.Num()-1;
         lBoundary.Init (sizeof(DWORD), &dwLen, 1);
      }
      DWORD dwLast = 0;
      DWORD dwCur;
      DWORD dwNumSyl = lBoundary.Num();
      DWORD dwSylStart = m_lSYLAN.Num();
      for (j = 0; j < dwNumSyl; j++, dwLast = dwCur) {
         DWORD dwVal = *((DWORD*)lBoundary.Get(j));
         dwCur = (WORD)dwVal; // to get rid of stress marker

         // fill in info
         sa.dwIndexInWave = m_lSYLAN.Num();
         sa.dwPhoneStart = pwa->dwPhoneStart + dwLast;
         sa.dwPhoneEnd = pwa->dwPhoneStart + dwCur;
         sa.dwTimeStart = ppa[sa.dwPhoneStart].dwTimeStart;
         sa.dwTimeEnd = ppa[sa.dwPhoneEnd-1].dwTimeEnd;
         sa.dwWordIndex = i;
         sa.bMultiStress = (BYTE)(dwVal >> 24);
         sa.bSylNum = (BYTE) j;

         // NOTE: if single syllable word always stress
         // BUGFIX - Can't do this if chinese
         if ((dwNumSyl <= 1) && !pLex->ChineseUse())
            sa.bMultiStress = 1;   // BUGFIX - Was TRUE, but more accurate to assume primary stress

         // BUGFIX - Energy average overweights voiced phonemes
         sa.fEnergyAvg = 0;
         fp fWeightSum = 0;
         fp fWeight;
         for (k = sa.dwPhoneStart; k < sa.dwPhoneEnd; k++) {
            fWeight = (!ppa[k].fIsVoiced) ? ENERGYAVGWEIGHT_UNVOICED : (ppa[k].fIsPlosive ? ENERGYAVGWEIGHT_PLOSIVE : ENERGYAVGWEIGHT_VOICED);
            fWeight *= (fp)ppa[k].dwDuration;

            sa.fEnergyAvg += ppa[k].fEnergyAvg * fWeight;
            fWeightSum += fWeight;
         } // j
         if (fWeightSum)
            sa.fEnergyAvg /= fWeightSum;
         //for (k = sa.dwTimeStart; k < sa.dwTimeEnd; k++)
         //   sa.fEnergyAvg += SRFEATUREEnergy (&m_pWave->m_paSRFeature[k]); // note: cant counteract pitch brightness here so dont bother
               // accessing m_paSRFeature here is OK since guaranteed to have features here
         //sa.fEnergyAvg /= (fp)(sa.dwTimeEnd - sa.dwTimeStart);

         // figure out where the voiced portion of the syllable begins and ends
         DWORD dwVoiceStart = (DWORD)-1, dwVoiceEnd = 0;
         for (k = sa.dwPhoneStart; k < sa.dwPhoneEnd; k++) {
            if (ppa[k].fIsVoiced)
               dwVoiceEnd = k + 1;
            else
               continue;   // not voiced

            if (dwVoiceStart == (DWORD)-1)
               dwVoiceStart = k;
         } // k
         if (dwVoiceStart == (DWORD)-1) {
            // not viced section so use whole thing
            dwVoiceStart = sa.dwPhoneStart;
            dwVoiceEnd = sa.dwPhoneEnd;
         }

#if 0 // replaced by bugfix
         fp fWeight = 0, fWeightThis;
         sa.fPitch = 0;
         for (k = sa.dwPhoneStart; k < sa.dwPhoneEnd; k++) {
            // include pitch from phonemes, weighted by if voiced
            fWeightThis = (ppa[k].fIsVoiced ? 1.0 : WEIGHTUNVOICEDFORPITCH) * ppa[k].dwDuration;
            fWeight += fWeightThis;
            sa.fPitch += ppa[k].fPitch * fWeightThis;
         }
         sa.fPitch /= fWeight;
#endif // 0

         // BUGFIX - more accurate way of getting
         DWORD dwPhonemeStart = ppa[dwVoiceStart].dwTimeStart * m_pWave->m_dwSRSkip;
         DWORD dwPhonemeEnd = ppa[dwVoiceEnd-1].dwTimeEnd * m_pWave->m_dwSRSkip;
         sa.dwVoiceStart = dwVoiceStart;
         sa.dwVoiceEnd = dwVoiceEnd;
         sa.fPitch = m_pWave->PitchOverRange (PITCH_F0, dwPhonemeStart, dwPhonemeEnd, 0, NULL, &sa.fPitchDelta, &sa.fPitchBulge,
            TRUE);
               // BUGFIX - Ignore pitch weight for syllable
               // BUGBUG - may need PITCH_SUB


         // store away the sub-sentence number and index into it
         sa.dwSubSentenceNum = pwa->dwSubSentenceNum;
         sa.dwSentenceType = pwa->dwSentenceType;
         if (dwLastSubSentence != pwa->dwSubSentenceNum) {
            dwLastSubSentence = pwa->dwSubSentenceNum;
            dwIndexIntoSubSentence = 0;
         }
         sa.dwIndexIntoSubSentence = dwIndexIntoSubSentence;
         dwIndexIntoSubSentence++;
         sa.fDurationScale = sa.dwTimeEnd - sa.dwTimeStart;

         // add it
         m_lSYLAN.Add (&sa);
      } // j, over all syllables

      // fill in the word info
      pwa->dwSylStart = dwSylStart;
      pwa->dwSylEnd = m_lSYLAN.Num();
   } // i


   // go back over syllables and write # syllables in the sentence
   PSYLAN psa = (PSYLAN)m_lSYLAN.Get(0);
   for (i = 0; i < m_lSYLAN.Num(); i++) {
      // figure out the number of syllables in the sub-sentence
      DWORD dwCount = 0;
      for (j = 0; j < m_lSYLAN.Num(); j++)
         if (psa[i].dwSubSentenceNum == psa[j].dwSubSentenceNum)
            dwCount++;
      psa[i].dwSyllablesInSubSentence = dwCount;
   } // i

   // now, loop through all the words and see where they start/stop
   pwa = (PWORDAN)m_lWORDAN.Get(0);
   ppa = (PPHONEAN)m_lPHONEAN.Get(0);
   for (i = 0; i < m_lWORDAN.Num(); i++, pwa++) {
      // see if the word is one we're looking for
      pwa->dwWordIndex = -1;
      PWSTR psz = pwa->pszWord;
      if (psz && psz[0])   // BUGFIX - check for psz
         pwa->dwWordIndex = pTTS->m_pLexWords->WordFind (psz);
      if (pwa->dwWordIndex == -1)
         continue;   // nope, dont want this word...

      // figure out the pronunciation form
      lForm.Clear();
      lDontRecurse.Clear();
      if (!pLex->WordPronunciation (psz, &lForm, FALSE, NULL, &lDontRecurse))
         continue;
      for (j = 0; j < lForm.Num(); j++) {
         PBYTE pb = (PBYTE)lForm.Get(j);
         pb++; // so skip POS
         DWORD dwLen = (DWORD)strlen((char*)pb);

         // if wrong number of phonemes continue
         if (dwLen != pwa->dwPhoneEnd - pwa->dwPhoneStart)
            continue;

         // else compare
         for (k = 0; k < dwLen; k++)
            if ((DWORD)pb[k]-1 != LOWORD(padwPhone[pwa->dwPhoneStart+k]))
               break;
         if (k < dwLen)
            continue;   // no match

         // else match
         break;
      } // j
      if (j >= lForm.Num())
         continue;   // not a valid form
      pwa->dwForm = j;

      // else want it
      WORDINST wi;
      memset (&wi, 0, sizeof(wi));
      wi.dwWave = dwWaveNum;
      wi.pWORDAN = pwa;
      //wi.dwWordIndex = pwa->dwIndexInWave; // was: padwWord[pwa->dwPhoneStart];
      //wi.dwPhoneStart = pwa->dwPhoneStart;
      //wi.dwPhoneEnd = pwa->dwPhoneEnd;
      //wi.dwForm = pwa->dwForm;

      // add it
      DWORD dwWord = pwa->dwWordIndex;
      EnterCriticalSection (lpcs);
      if (!paplWord[dwWord]) {
         paplWord[dwWord] = new CListFixed;
         paplWord[dwWord]->Init (sizeof(WORDINST));
      }
      paplWord[dwWord]->Add (&wi);
      LeaveCriticalSection (lpcs);
   } // i

   return TRUE;
}



/*************************************************************************************
CCoverageSentence */

// CCoverageSentence - For storing information about a sentence used to determine
// which would have the best coverage

class CCoverageSentence : public CEscObject {
public:
   CCoverageSentence (void);
   ~CCoverageSentence (void);

   BOOL InitFromWave (PWSTR pszFile, PCMLexicon pLex, PCTextParse pTextParse);
   fp CoverageSentenceIncrement (
                              float *pafSmallToInc, float *pafLargeToInc,
                              float *pafSmallRef, float *pafLargeRef,
                              PCMLexicon pLex);

   CMem           m_memText;        // memory with the sentence text
   CMem           m_memFile;        // memory with the sentence file
   CListFixed     m_lPhones;        // memory with the sentence bones. DWORD values for phoneme numbers.
                                    // as per SentenceToPhonemeString() in high byte, 1 for start of word, 2 for end of word, or both
   fp             m_fDuration;      // duration in seconds

private:
};
typedef CCoverageSentence *PCCoverageSentence;



/*************************************************************************************
CCoverageSentence::Constructor and destructor */
CCoverageSentence::CCoverageSentence (void)
{
   MemZero (&m_memText);
   MemZero (&m_memFile);
   m_lPhones.Init (sizeof(DWORD));
   m_fDuration = 0;
}

CCoverageSentence::~CCoverageSentence (void)
{
   // do nothing for now
}


/*************************************************************************************
CCoverageSentence::InitFromWave - Initializes a coverage sentence from a wave file.

inputs
   PWSTR             pszFile - Wave file
   PCM3DLexicon      pLex - Lexicon to use
   PCTextParse       pTextParse - Text parser to use. Must also be using pLex
returns
   BOOL - TRUE if success. FALSE if fail
*/
BOOL CCoverageSentence::InitFromWave (PWSTR pszFile, PCMLexicon pLex, PCTextParse pTextParse)
{
   CM3DWave Wave;
   char szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);

   if (!Wave.Open (NULL, szTemp))
      return FALSE;

   // make sure there's text and phonemes
   PWSTR pszText = (PWSTR)Wave.m_memSpoken.p;
   if (!pszText || !pszText[0])
      return FALSE;
   if (!Wave.m_lWVPHONEME.Num())
      return FALSE;

   // else, clear out
   MemZero (&m_memText);
   MemCat (&m_memText, (PWSTR)Wave.m_memSpoken.p);
   MemZero (&m_memFile);
   MemCat (&m_memFile, pszFile);
   m_lPhones.Clear();
   m_fDuration = 0;

   // use lex and get phonemes
   if (!SentenceToPhonemeString ((PWSTR)m_memText.p, pLex, pTextParse, &m_lPhones, NULL))
      return FALSE;

   DWORD dwSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

   // figure out the duration
   fp fFirstPhoneme = -0.01;
   fp fLastPhoneme = fFirstPhoneme;
   PWVPHONEME pwp = (PWVPHONEME) Wave.m_lWVPHONEME.Get(0);
   WCHAR szPhone[16];
   DWORD i;
   memset (szPhone, 0, sizeof(szPhone));
   for (i = 0; i < Wave.m_lWVPHONEME.Num(); i++, pwp++) {
      memcpy (szPhone, pwp->awcNameLong, sizeof(pwp->awcNameLong));

      // see what this phoneme is
      DWORD dwPhone = pLex->PhonemeFindUnsort (szPhone);
      if ((dwPhone == (DWORD)-1) || (dwPhone == dwSilence))
         continue;

      if (fFirstPhoneme < 0)
         fFirstPhoneme = (fp)pwp->dwSample / (fp)Wave.m_dwSamplesPerSec;
      if (i+1 < Wave.m_lWVPHONEME.Num())
         fLastPhoneme = (fp)pwp[1].dwSample / (fp)Wave.m_dwSamplesPerSec;
   } // i
   m_fDuration = fLastPhoneme - fFirstPhoneme;
   m_fDuration += 0.2;  // BUGFIX - Since blizzard numbers seem to be about 0.2 higher

   return TRUE;
}


/*************************************************************************************
CoverageSentenceIndex - This indexes into the coverage sentence memory.

inputs
   DWORD          dwLeft - Left phoneme. Only the lower byte is used
   DWORD          dwCenter - Center phoneme. Only the lower byte is used
   DWORD          dwRight - Right phoneme. Only the lower byte is used
   BOOL           fSmall - If TRUE, then the small PIS_PHONEGROUPNUM x (phonenum+1) x PIS_PHONEGROUPNUM group
                        is used. If FALSE then the lerge one of (phonenum+1) x (phonenum+1) x (phonenum+1) is used
   float          *pfArray - Start of the array. Must be large enough, based on fSmall
   PCMLexicon      pLex - Lexicon
returns
   float * - Index into the list
*/
float *CoverageSentenceIndex (DWORD dwLeft, DWORD dwCenter, DWORD dwRight, BOOL fSmall,
                              float *pfArray, PCMLexicon pLex)
{
   DWORD dwPhones = pLex->PhonemeNum() + 1;
   dwLeft &= 0xff;
   dwCenter &= 0xff;
   dwRight &= 0xff;
   dwCenter = min(dwCenter, dwPhones-1);  // so silence gets capped

   if (fSmall) {
      PLEXPHONE plp = pLex->PhonemeGetUnsort (dwLeft);
      if (!plp)   // shouldnt happen
         plp = pLex->PhonemeGetUnsort(pLex->PhonemeFind (pLex->PhonemeSilence()));
      PLEXENGLISHPHONE pep = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
      if (pep)
         dwLeft = PIS_FROMPHONEGROUP(pep->dwShape);
      else
         dwLeft = 0; // shoulnt happen

      plp = pLex->PhonemeGetUnsort (dwRight);
      if (!plp)   // shouldnt happen
         plp = pLex->PhonemeGetUnsort(pLex->PhonemeFind (pLex->PhonemeSilence()));
      pep = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
      if (pep)
         dwRight = PIS_FROMPHONEGROUP(pep->dwShape);
      else
         dwRight = 0; // shoulnt happen
   }
   else {
      dwLeft = min(dwLeft, dwPhones-1);  // so silence gets capped
      dwRight = min(dwRight, dwPhones-1);  // so silence gets capped
   }

   if (fSmall)
      return pfArray + ((dwLeft * PIS_PHONEGROUPNUM + dwRight) * dwPhones + dwCenter);
   else
      return pfArray + ((dwLeft * dwPhones + dwRight) * dwPhones + dwCenter);
}



/*************************************************************************************
CoverageSentenceScore - Determine the score given a new and old count.

inputs
   float          fOldValue - Original count before increment
   float          fNewValue - New count before increment
   float          fReference - Reference count
returns
   float - Score
*/

#define PROBOFGOODUNIT        (0.7)       // probability of a specific unit being good

__inline float CoverageSentenceScore (float fOldValue, float fNewValue, float fReference)
{
   if (fOldValue)
      fOldValue = 1.0 - pow((fp)(1.0 - PROBOFGOODUNIT), (fp)fOldValue);
   else
      fOldValue = 0;

   if (fNewValue)
      fNewValue = 1.0 - pow((fp)(1.0 - PROBOFGOODUNIT), (fp)fNewValue);
   else
      fNewValue = 0;

   return (fNewValue - fOldValue) * fReference;
}

/*************************************************************************************
CoverageSentenceIncrement - Increases the coverage sentence scores.

inputs
   DWORD          dwLeft - Left phoneme. Only low-byte used
   DWORD          dwCenter - Left phoneme. Hi-byte has info about start/end of word
   DWORD          dwRight - Right phoneme. Only low-byte used
   float          *pafSmallToInc - Small array to increase
   float          *pafLargeInInc - Large araryy to increase
   float          *pafSmallRef - Small array for reference. Can be NULL. Needed to generate a score
   float          *pafLargeRef - Large array for reference. Can be NULL. Needed to generate a score
   PCMLexicon      pLex - Lexicon
   DWORD          dwSilence - Silence phoneme
returns
   fp - Score if pfSmallRef and pfLargeRef. If not, this is 0
*/
fp CoverageSentenceIncrement (DWORD dwLeft, DWORD dwCenter, DWORD dwRight,
                              float *pafSmallToInc, float *pafLargeToInc,
                              float *pafSmallRef, float *pafLargeRef,
                              PCMLexicon pLex, DWORD dwSilence)
{
   DWORD dwSmall, dwLeftSil, dwRightSil;
   DWORD dwSilInfo = dwCenter >> 24;
   DWORD dwNumLeft = ((dwSilInfo & 0x01) ? 2 : 1);
   DWORD dwNumRight = ((dwSilInfo & 0x02) ? 2 : 1);
   DWORD dwNumTotal = dwNumRight * dwNumLeft;
   float fInc = 1.0 / (float)dwNumTotal;
   fp fScore = 0;
   for (dwSmall = 0; dwSmall < 2; dwSmall++) {
      float *pafToInc = dwSmall ? pafSmallToInc : pafLargeToInc;
      float *pafRef = dwSmall ? pafSmallRef : pafLargeRef;

      // loop over left/right, hypothesizing left/right silence
      for (dwLeftSil = 0; dwLeftSil < dwNumLeft; dwLeftSil++)
         for (dwRightSil = 0; dwRightSil < dwNumRight; dwRightSil++) {
            // get the index to increase
            float *pfInc = CoverageSentenceIndex (dwLeftSil ? dwSilence : dwLeft, dwCenter,
               dwRightSil ? dwSilence : dwRight, dwSmall ? TRUE : FALSE, pafToInc, pLex);

            // if only increasing then easy
            if (!pafRef) {
               *pfInc += fInc;
               continue;
            }

            // get index for reference
            float *pfRef = CoverageSentenceIndex (dwLeftSil ? dwSilence : dwLeft, dwCenter,
               dwRightSil ? dwSilence : dwRight, dwSmall ? TRUE : FALSE, pafRef, pLex);

            // remember original and new value
            float fOrig = *pfInc;
            *pfInc += fInc;
            float fNew = *pfInc;

            fScore += CoverageSentenceScore (fOrig, fNew, *pfRef);
         } // dwLeftSil, dwRightSil
   } // i

   return fScore;
}




/*************************************************************************************
CCoverageSentence::CoverageSentenceIncrement - Increases the coverage sentence scores.

inputs
   float          *pafSmallToInc - Small array to increase
   float          *pafLargeInInc - Large araryy to increase
   float          *pafSmallRef - Small array for reference. Can be NULL. Needed to generate a score
   float          *pafLargeRef - Large array for reference. Can be NULL. Needed to generate a score
   PCMLexicon      pLex - Lexicon
returns
   fp - Score for the entire sentence, if pfSmallRef and pfLargeRef. If not, this is 0.
            The score is divided by the overall sentence length
*/
fp CCoverageSentence::CoverageSentenceIncrement (
                              float *pafSmallToInc, float *pafLargeToInc,
                              float *pafSmallRef, float *pafLargeRef,
                              PCMLexicon pLex)
{
   DWORD i;
   DWORD dwSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());
   DWORD *padw = (DWORD*)m_lPhones.Get(0);
   float fScore = 0;
   for (i = 0; i < m_lPhones.Num(); i++, padw++) {
      DWORD dwLeft = i ? padw[i-1] : dwSilence;
      DWORD dwRight = ((i+1) < m_lPhones.Num()) ? padw[i+1] : dwSilence;

      fScore += ::CoverageSentenceIncrement (
         dwLeft, padw[i], dwRight,
         pafSmallToInc, pafLargeToInc, pafSmallRef, pafLargeRef, pLex, dwSilence);
   } // i

   if (m_fDuration)
      fScore /= m_fDuration;  // so know score based on length

   return fScore;
}


/*************************************************************************************
CTTSWork::BlizzardSmallVoice - This scans through the existing list of
sentences and determines which are to be kept for optimium units.

inputs
   HWND           hWnd - To show progress
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::BlizzardSmallVoice (HWND hWnd)
{
   BOOL fRet = TRUE;
   CProgress Progress;
   CListFixed lPCCoverageSentence, lKeep;
   PCCoverageSentence *ppc;
   DWORD i;
   lPCCoverageSentence.Init (sizeof(PCCoverageSentence));
   lKeep.Init (sizeof(PCCoverageSentence));
   Progress.Start (hWnd, "Processing...", TRUE);

   // get lexicon
   CTextParse TextParse;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   TextParse.Init (pLex->LangIDGet(), pLex);

   // how much memory is needed for large and small buffers
   DWORD dwNumPhones = pLex->PhonemeNum()+1;
   DWORD dwForSmall = dwNumPhones * PIS_PHONEGROUPNUM * PIS_PHONEGROUPNUM;
   DWORD dwForLarge = dwNumPhones * dwNumPhones * dwNumPhones;
   CMem mem;
   if (!mem.Required ((dwForSmall + dwForLarge) * 4 * sizeof(float)))
      return FALSE;
   float *pafRef = (float*)mem.p;
   float *pafTotal = pafRef + (dwForSmall + dwForLarge);
   float *pafBest = pafTotal + (dwForSmall + dwForLarge);
   float *pafScratch = pafBest + (dwForSmall + dwForLarge);
   memset (pafRef, 0, (dwForSmall + dwForLarge) * sizeof(float)); // to clear out reference
   memset (pafTotal, 0, (dwForSmall + dwForLarge) * sizeof(float)); // to clear out total count

   // scan the existing files
   Progress.Push (0, 0.5);
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(0);
   double fStartingTotalTime = 0;
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      Progress.Update ((fp)i / (fp)m_pWaveDir->m_lPCWaveFileInfo.Num());
      if (!ppfi[i]->m_pszFile || !ppfi[i]->m_pszFile[0])
         continue;

      // create a new coverag sentence
      PCCoverageSentence pc = new CCoverageSentence;
      if (!pc)
         continue;
      if (!pc->InitFromWave (ppfi[i]->m_pszFile, pLex, &TextParse)) {
         delete pc;
         continue;
      }

      // add
      lPCCoverageSentence.Add (&pc);

      // add to refernce
      pc->CoverageSentenceIncrement (pafRef, pafRef + dwForSmall, NULL, NULL, pLex);
      fStartingTotalTime += pc->m_fDuration;
   } // i
   Progress.Pop ();

   // choose
   Progress.Push (0.5, 1);
   double fTotalTimeUsed = 0;
#define MAXNUMSECCANKEEP         2914.0      // for blizzard challenge 2007
   // repeat until have used
   while ((fTotalTimeUsed < MAXNUMSECCANKEEP) && lPCCoverageSentence.Num()) {
      Progress.Update (fTotalTimeUsed / MAXNUMSECCANKEEP);

      // remember the best
      DWORD dwBest = (DWORD)-1;
      fp fBestScore = 0;

      // loop through all possibilities
      ppc = (PCCoverageSentence*) lPCCoverageSentence.Get(0);
      for (i = 0; i < lPCCoverageSentence.Num(); i++) {
         if (ppc[i]->m_fDuration > (MAXNUMSECCANKEEP - fTotalTimeUsed))
            continue;   // too long

         // copy over current values to scratch so can see how much to increment
         memcpy (pafScratch, pafTotal, (dwForSmall + dwForLarge)*sizeof(float));

         // increment the scratch
         fp fScore = ppc[i]->CoverageSentenceIncrement (pafScratch, pafScratch + dwForSmall,
            pafRef, pafRef + dwForSmall, pLex);

         // if this score is better than anything else then keep
         if ((dwBest == (DWORD)-1) || (fScore > fBestScore)) {
            fBestScore = fScore;
            dwBest = i;
            memcpy (pafBest, pafScratch, (dwForSmall + dwForLarge)*sizeof(float));
         }
      } // i

      // if nothing found then break here
      if (dwBest == (DWORD)-1)
         break;

      // keep this
      memcpy (pafTotal, pafBest, (dwForSmall + dwForLarge)*sizeof(float));
      fTotalTimeUsed += ppc[dwBest]->m_fDuration;
      PCCoverageSentence pcs = ppc[dwBest];
      lKeep.Add (&pcs);
      lPCCoverageSentence.Remove (dwBest);
   } // while
   Progress.Pop ();

   // fill in the wave list
   m_pWaveDir->ClearFiles();
   ppc = (PCCoverageSentence*) lKeep.Get(0);
   m_pWaveDir->m_lPCWaveFileInfo.Required (lKeep.Num());
   for (i = 0; i < lKeep.Num(); i++) {
      // else, add
      PCWaveFileInfo pNew = new CWaveFileInfo;
      if (!pNew)
         continue;
      pNew->SetText ((PWSTR)ppc[i]->m_memFile.p, (PWSTR)ppc[i]->m_memText.p, NULL);
      m_pWaveDir->m_lPCWaveFileInfo.Add (&pNew);
   } // i

//done:
   // free up
   ppc = (PCCoverageSentence*) lPCCoverageSentence.Get(0);
   for (i = 0; i < lPCCoverageSentence.Num(); i++)
      if (ppc[i])
         delete ppc[i];
   ppc = (PCCoverageSentence*) lKeep.Get(0);
   for (i = 0; i < lKeep.Num(); i++)
      if (ppc[i])
         delete ppc[i];

   return fRet;
}



/*************************************************************************************
UNITGROUPCOUNTInsertByScore - Given a UNITGROUPCOUNT, this inserts an entry
into the aUGCS field, assuming that the fScore is acceptable.

inputs
   PUNITGROUPCOUNT         pUGC - To insert into
   fp                      fScore - Score
   WORD                    wWave - Wave number
   WORD                    wPhoneIndex - Phoneme index into the wave
returns
   BOOL - TRUE if was added, FALSE if wasn't good enough to be added.
*/
BOOL UNITGROUPCOUNTInsertByScore (PUNITGROUPCOUNT pUGC, fp fScore, WORD wWave, WORD wPhoneIndex)
{
   DWORD dwInsertBefore;
   if (!pUGC->dwUGCSCount)
      // nothing, so fast insert
      dwInsertBefore = 0;
   else if (fScore >= pUGC->aUGCS[pUGC->dwUGCSCount-1].fScore)
      dwInsertBefore = pUGC->dwUGCSCount; // higher score than last element
   else for (dwInsertBefore = 0; dwInsertBefore < pUGC->dwUGCSCount; dwInsertBefore++)
      if (fScore < pUGC->aUGCS[dwInsertBefore].fScore)
         break;   // found something to insert before

   if (dwInsertBefore >= MAXUGCS)
      return FALSE;  // not good enough

   // move
   pUGC->dwUGCSCount = min(pUGC->dwUGCSCount, MAXUGCS-1);   // forget about the last one
   memmove (&pUGC->aUGCS[dwInsertBefore+1], &pUGC->aUGCS[dwInsertBefore], (pUGC->dwUGCSCount - dwInsertBefore) * sizeof(pUGC->aUGCS[dwInsertBefore]));

   // keep
   pUGC->aUGCS[dwInsertBefore].fScore = fScore;
   pUGC->aUGCS[dwInsertBefore].wWave = wWave;
   pUGC->aUGCS[dwInsertBefore].wPhoneIndex = wPhoneIndex;
   pUGC->dwUGCSCount++;
   return TRUE;
}

/*************************************************************************************
UNITGROUPCOUNTInsert - This inserts a blank entry in UNITGROUPCOUNT list. If an
entry already exists then the prexisting one is returned.

inputs
   DWORD             dwPitchFidelity - Pitch fidelity bin, from 0..PITCHFIDELITYPLUSONE-1
   DWORD             dwDurationFidelity - Duration fidelity bin, from 0..DURATIONFIDELITYPLUSONE-1
   DWORD             dwEnergyFidelity - Energy fidelity bin, from 0..ENERGYFIDELITYPLUSONE-1
   DWORD             dwNumPhones - For comparison of bNumPhones.
   QWORD             qwPhonemes - Phonemes to insert
   QWORD             qwWordPos - Word position for each of the phonemes, from bWordPos or wWordPos.
                        Bit 0x01 in each byte indicates at start of word, 0x02 at the end.
                        NOTE: If m_fWordStartEndCombine then pass in 0!
   PCListFixed       pl - List of UNITGROUPCOUNT
returns
   PUNITGROUPCOUNT - Pointer to the location in pl where the UNITGROUPCOUNT is. This may
      be a preexisting entry, or may have been inserted
*/
int UNITGROUPCOUNTCompare (PUNITGROUPCOUNT p1, PUNITGROUPCOUNT p2)
{
   // compare by pitch group
   int iCompare = (int)p1->bPitchFidelity - (int)p2->bPitchFidelity;
   if (iCompare)
      return iCompare;

   // compare by duration fidelity group
   iCompare = (int)p1->bDurationFidelity - (int)p2->bDurationFidelity;
   if (iCompare)
      return iCompare;

   // compare by energy fidelity group
   iCompare = (int)p1->bEnergyFidelity - (int)p2->bEnergyFidelity;
   if (iCompare)
      return iCompare;

   // compare by phonemes
   if (p1->qwPhonemes > p2->qwPhonemes)
      return 1;
   else if (p1->qwPhonemes < p2->qwPhonemes)
      return -1;

   // compare word pos
   if (p1->qwWordPos > p2->qwWordPos)
      return 1;
   else if (p1->qwWordPos < p2->qwWordPos)
      return -1;

   // BUGFIX - Also, need to compare number of phones actually used
   // from list so AnalysisSingleUnits() works properly
   if (p1->bNumPhones != p2->bNumPhones)
      return (int)p1->bNumPhones - (int)p2->bNumPhones;

   // else, ok
   return 0;
}

PUNITGROUPCOUNT UNITGROUPCOUNTInsert (DWORD dwPitchFidelity, DWORD dwDurationFidelity, DWORD dwEnergyFidelity,
                                      DWORD dwNumPhones, QWORD qwPhonemes,
                                      QWORD qwWordPos, PCListFixed pl)
{
   // try to find it
   DWORD dwCur, dwTest;
   DWORD dwNum = pl->Num();
   int iCompare;
   PUNITGROUPCOUNT pug = (PUNITGROUPCOUNT)pl->Get(0);

   UNITGROUPCOUNT ug;
   memset (&ug, 0, sizeof(ug));
   ug.qwPhonemes = qwPhonemes;
   ug.qwWordPos = qwWordPos;
   ug.bPitchFidelity = (BYTE)dwPitchFidelity;
   ug.bDurationFidelity = (BYTE)dwDurationFidelity;
   ug.bEnergyFidelity = (BYTE)dwEnergyFidelity;
   ug.bNumPhones = (BYTE)dwNumPhones;

   for (dwTest = 1; dwTest < dwNum; dwTest *= 2);
   for (dwCur = 0; dwTest; dwTest /= 2) {
      DWORD dwTry = dwCur + dwTest;
      if (dwTry >= dwNum)
         continue;

      int iCompare = UNITGROUPCOUNTCompare (&ug, pug + dwTry);

      // see how compares
      if (iCompare > 0) {
         // string occurs after this
         dwCur = dwTry;
         continue;
      }

      if (iCompare == 0)
         return pug + dwTry;
      
      // else, string occurs before. so throw out dwTry
      continue;
   } // dwTest

   // if get here, need to insert

   if (dwCur >= dwNum) // beyond end => not found
      dwCur = dwNum;
   else {
      // else, might be a match
      iCompare = UNITGROUPCOUNTCompare (&ug, pug + dwCur);
      if (iCompare > 0)
         dwCur++;
      else if (iCompare == 0)
         return pug + dwCur;
      // else, leave as is
   }

   // insert
   // ug.fScore = UNDEFINEDRANK;
   pl->Insert (dwCur, &ug);

   return (PUNITGROUPCOUNT) pl->Get(dwCur);
}


/*************************************************************************************
UNITGROUPCOUNTSort - Sort compare, by dwScoreCount
*/
static int _cdecl UNITGROUPCOUNTSort (const void *elem1, const void *elem2)
{
   UNITGROUPCOUNT *pdw1, *pdw2;
   pdw1 = (UNITGROUPCOUNT*) elem1;
   pdw2 = (UNITGROUPCOUNT*) elem2;

   if (pdw2->fScoreCount > pdw1->fScoreCount)
      return 1;
   else if (pdw2->fScoreCount < pdw1->fScoreCount)
      return -1;
   else
      return 0;
   // return (int)pdw2->dwScoreCount - (int)pdw1->dwScoreCount;
}


/*************************************************************************************
CTTSWork::AnalysisSingleUnits - Finds the best match for single units and
fills in a list with them.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCListFixed    plUNITGROUPCOUNT - Filled with the unit group information (appended)
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisSingleUnits (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT)
{
   DWORD i, j;

   // BUGFIX - If keeping all units then save some time and don't bother
   if (m_dwTotalUnits >= INCLUDEALLUNITS)
      return TRUE;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   DWORD dwNumPhone = pLex->PhonemeNum();
   DWORD dwNumPhonePlusOne = dwNumPhone + 1;
   PLEXENGLISHPHONE pe;
   PLEXPHONE plp;

   // find the conversion from phonemes to 16-size group, and to unstressed
   BYTE abPhoneToGroup[256], abPhoneToMegaGroup[256], abPhoneToUnstressed[256];
   BYTE abPhoneGroupToPhone[PIS_PHONEGROUPNUM], abPhoneMegaGroupToPhone[PIS_PHONEMEGAGROUPNUM];
   for (i = 0; i < 256; i++)
      abPhoneToGroup[i] = abPhoneToMegaGroup[i] = 255;
   for (i = 0; i < 256; i++)
      abPhoneToUnstressed[i] = 255;
   for (i = 0; i < PIS_PHONEGROUPNUM; i++)
      abPhoneGroupToPhone[i] = 255;
   for (i = 0; i < PIS_PHONEMEGAGROUPNUM; i++)
      abPhoneMegaGroupToPhone[i] = 255;
   for (i = 0; i <= dwNumPhone; i++) {
      BYTE bPhone = (i < dwNumPhone) ? (BYTE)i : bSilence;
      plp = pLex->PhonemeGetUnsort (bPhone);
      if (!plp)
         continue;

      // unstressed/stressed
      if (plp->bStress)
         abPhoneToUnstressed[bPhone] = (BYTE) plp->wPhoneOtherStress;
      else
         abPhoneToUnstressed[bPhone] = (BYTE)bPhone;

      // phoneme group
      pe = MLexiconEnglishPhoneGet (plp->bEnglishPhone);
      if (!pe)
         continue;
      abPhoneToGroup[bPhone] = (BYTE)PIS_FROMPHONEGROUP(pe->dwShape);
      abPhoneToMegaGroup[bPhone] = LexPhoneGroupToMega(abPhoneToGroup[bPhone]);

      // convert this back to a phoneme
      if (abPhoneGroupToPhone[abPhoneToGroup[bPhone]] == 255)
         abPhoneGroupToPhone[abPhoneToGroup[bPhone]] = bPhone;   // first one of the group type
      if (abPhoneMegaGroupToPhone[abPhoneToMegaGroup[bPhone]] == 255)
         abPhoneMegaGroupToPhone[abPhoneToMegaGroup[bPhone]] = bPhone;   // first one of the group type
      abPhoneToGroup[bPhone] = abPhoneGroupToPhone[abPhoneToGroup[bPhone]]; // to turn back into phoneme
      abPhoneToMegaGroup[bPhone] = abPhoneMegaGroupToPhone[abPhoneToMegaGroup[bPhone]]; // to turn back into phoneme
   } // i

   // create three lists, one for phone only, one with triphone based on group, the other triphone based on unstressed
#define NUMSUGROUP      5     // 5 groups, 0=no mismatch, 1=stress, 2=within group, 3=bad group, 4=bad mega
   CListFixed  alPhone[NUMSUGROUP][NUMSUGROUP];    // [left context][right context]
                                                   // [0][0] is special, use to ensure copy of each phoneme
   DWORD dwLeft, dwRight;
   for (dwLeft = 0; dwLeft < NUMSUGROUP; dwLeft++)
      for (dwRight = 0; dwRight < NUMSUGROUP; dwRight++)
         alPhone[dwLeft][dwRight].Init (sizeof(UNITGROUPCOUNT));

   // loop through and fill them in
   PUNITGROUPCOUNT pug;
   DWORD dwDemi;
   DWORD dwPitchFidelityAll, dwDurationFidelityAll, dwEnergyFidelityAll;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j < dwNum; j++, ppa++) { // NOTE: using j+1 so safe to test for next one
         // make sure not silence
         if (ppa->bPhone == bSilence)
            continue;

         if (ppa->fBad)
            continue;   // bad phoneme

         // determine which pitch bin this is in
         // BUGFIX - Was checking IsVoiced, but shouldnt do that
         DWORD dwPitchFidelity = PitchToFidelity (ppa->fPitch, pAnal->fAvgPitch);
         PTRIPHONETRAIN ptpt = (PTRIPHONETRAIN)pAnal->plTRIPHONETRAIN->Get(ppa->dwTRIPHONETRAINIndex);
         DWORD dwDurationFidelity = ptpt ?
            DurationToFidelity (ppa->dwDuration, ptpt->dwDuration) :
            DURATIONFIDELITYCENTER;
         DWORD dwEnergyFidelity = ptpt ?
            EnergyToFidelity (ppa->fEnergyAvg, ptpt->fEnergyMedian) :
            ENERGYFIDELITYCENTER;

         // BUGFIX - Keep track of an "all" fidelity group for small voices
         fp afRank[2][2][2];
         for (dwPitchFidelityAll = 0; dwPitchFidelityAll < 2; dwPitchFidelityAll++)
            for (dwDurationFidelityAll = 0; dwDurationFidelityAll < 2; dwDurationFidelityAll++)
               for (dwEnergyFidelityAll = 0; dwEnergyFidelityAll < 2; dwEnergyFidelityAll++) {
                  afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll] = 0;

                  for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++)
                     afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll] += ppa->afRankCompare
                        [dwPitchFidelityAll ? PITCHFIDELITYCENTER : dwPitchFidelity]
                        [dwDurationFidelityAll ? DURATIONFIDELITYCENTER : dwDurationFidelity]
                        [dwEnergyFidelityAll ? ENERGYFIDELITYCENTER : dwEnergyFidelity]
                        [dwDemi] / (fp)TTSDEMIPHONES;
               } // all fidelities
         if (afRank[0][0][0] >= UNDEFINEDRANK)
            continue;      // no score


         // BUGFIX - There's an alternate score. Assume that the pitch-independent, energy-independent,
         // duration-indepenent best has been selected already. Then, the score benefit of the unit
         // is based on the F0, duration, and energy penalty
         fp fF0Penalty = 0, fEnergyPenalty = 0, fDurationPenalty = 0;
         if (dwPitchFidelity != PITCHFIDELITYCENTER)
            fF0Penalty = UnitScorePitch(pTTS, ppa->bPhone, pLex, dwPitchFidelity > PITCHFIDELITYCENTER, pTTS->FullPCMGet()) *
               log(OCTAVESPERPITCHFILDELITY) / log(2.0);
         if (dwDurationFidelity != DURATIONFIDELITYCENTER)
            fDurationPenalty = UnitScoreDuration (pTTS, ppa->bPhone, pLex, dwDurationFidelity > DURATIONFIDELITYCENTER, pTTS->FullPCMGet()) *
               log(SCALEPERDURATIONFIDELITY) / log(2.0);
         if (dwEnergyFidelity != ENERGYFIDELITYCENTER)
            fEnergyPenalty = UnitScoreEnergy (pTTS, ppa->bPhone, pLex, dwEnergyFidelity > ENERGYFIDELITYCENTER) *
               log(SCALEPERENERGYFIDELITY) / log(2.0);
         

         // find phoneme to left and right
         BYTE bLeft = j ? ppa[-1].bPhone : bSilence;
         BYTE bRight = (j+1 < dwNum) ? ppa[1].bPhone : bSilence;
         BYTE bWordPosLeft = j ? ppa[-1].bWordPos : 0;
         BYTE bWordPosRight = (j+1 < dwNum) ? ppa[1].bWordPos : 0;

         // loop over 3 forms
         for (dwLeft = 0; dwLeft < NUMSUGROUP; dwLeft++) for (dwRight = 0; dwRight < NUMSUGROUP; dwRight++) {
            PCListFixed pl;
            pl = &alPhone[dwLeft][dwRight];

            DWORD qwWordPos =
               0 | // BUGFIX - Was this, but left word position doesn't matter((DWORD)bWordPosLeft) |
               ((DWORD)ppa->bWordPos << 8) |
               0; // BUGFIX - Was this but right word position doesn't matter((DWORD)bWordPosRight << 16);

            DWORD qwPhonemes = 0;
            double fScoreCount = 0;
            BYTE *pab = (BYTE*)&qwPhonemes;
            pab[1] = ppa->bPhone+1; // center phoneme

            switch (dwLeft) {
               case 4:  // megagroup mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bLeft, pLex, FALSE, 5);
                  pab[0] = 0 + 1;   // hack to use phoneme 0
                  break;
               case 3:  // group mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bLeft, pLex, FALSE, 4);
                  pab[0] = abPhoneToMegaGroup[bLeft] + 1;
                  break;
               case 2:  // in-group mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bLeft, pLex, FALSE, 3);
                  pab[0] = abPhoneToGroup[bLeft] + 1;
                  break;
               case 1:  // stress mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bLeft, pLex, FALSE, 2);
                  pab[0] = abPhoneToUnstressed[bLeft] + 1;
                  break;
               case 0:  // no mismatch
               default:
                  // do nothing to fScoreCount
                  pab[0] = bLeft + 1;
                  break;
            } // dwLeft



            switch (dwRight) {
               case 4:  // megagroup mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bRight, pLex, TRUE, 5);
                  pab[2] = 0 + 1;   // hack to use phoneme 0
                  break;
               case 3:  // group mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bRight, pLex, TRUE, 4);
                  pab[2] = abPhoneToMegaGroup[bRight] + 1;
                  break;
               case 2:  // in-group mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bRight, pLex, TRUE, 3);
                  pab[2] = abPhoneToGroup[bRight] + 1;
                  break;
               case 1:  // stress mismatch
                  fScoreCount += UnitScoreScoreLRMismatch(pTTS, ppa->bPhone, bRight, pLex, TRUE, 2);
                  pab[2] = abPhoneToUnstressed[bRight] + 1;
                  break;
               case 0:  // no mismatch
               default:
                  // do nothing to fScoreCount
                  pab[2] = bRight + 1;
                  break;
            } // dwLeft

            // special case  for [0][0], in which case need to make extra sure at least
            // one example of phoneme exists
            if (!dwLeft && !dwRight) {
               pab[0] = pab[2] = 0 + 1;   // L/R context doesn't matter
               fScoreCount = 1000000000;  // very large to ensure that keep
            }

            for (dwPitchFidelityAll = 0; dwPitchFidelityAll < 2; dwPitchFidelityAll++)
               for (dwDurationFidelityAll = 0; dwDurationFidelityAll < 2; dwDurationFidelityAll++)
                  for (dwEnergyFidelityAll = 0; dwEnergyFidelityAll < 2; dwEnergyFidelityAll++) {
                     pug = UNITGROUPCOUNTInsert (
                        dwPitchFidelityAll ? (PITCHFIDELITYPLUSONE-1) : dwPitchFidelity,
                        dwDurationFidelityAll ? (DURATIONFIDELITYPLUSONE-1) : dwDurationFidelity,
                        dwEnergyFidelityAll ? (ENERGYFIDELITYPLUSONE-1) : dwEnergyFidelity,
                        1, qwPhonemes, m_fWordStartEndCombine ? 0 : qwWordPos, pl);
                     if (!pug)
                        continue;   // shouldnt happen

                     // BUGFIX - alternate penalty for non-primary
                     fp fScoreCountWithAlt = fScoreCount;
                     if (!(dwPitchFidelityAll && dwDurationFidelityAll && dwEnergyFidelityAll)) {
                        // we're NOT doing the core unit, so alternate energy is possible
                        fp fAltCount = 1000000000;
                        if (!dwPitchFidelityAll && fF0Penalty)
                           fAltCount = min(fAltCount, fF0Penalty);
                        if (!dwDurationFidelityAll && fDurationPenalty)
                           fAltCount = min(fAltCount, fDurationPenalty);
                        if (!dwEnergyFidelityAll && fEnergyPenalty)
                           fAltCount = min(fAltCount, fEnergyPenalty);

                        if ((fAltCount < fScoreCountWithAlt) && (fAltCount < 1000000000))
                           fScoreCountWithAlt = fAltCount;
                     }


                     // divide fScoreCount by 3 phonemes, since can't just store one phoneme,
                     // but also need to store surrounding phonemes. Therefore, effectiveness
                     // of keeping this unit it less
                     fScoreCountWithAlt /= 2.0;
                        // BUGFIX - Was 3.0, but only storing half of the audio no left/right

                     // BUGFIX - Was overweighting for units that appeared in XXXCENTER, but shouldnt
                     // bother, because there are more of them, so they should necessarily be added
                     // first
                     //DWORD dwScore =
                     //   UNITGROUPCOUNTSCORE_STANDALONE *
                     //   ( ((dwPitchFidelity == PITCHFIDELITYCENTER) || dwPitchFidelityAll) ? 2 : 1) *
                     //   ( ((dwDurationFidelity == DURATIONFIDELITYCENTER) || dwDurationFidelityAll) ? 2 : 1) *
                     //   ( ((dwEnergyFidelity == ENERGYFIDELITYCENTER) || dwEnergyFidelityAll) ? 2 : 1);
                              // extra score if center pitch
                     if (pug->dwCount) {
                        // already exists
                        pug->dwCount++;
                        pug->fScoreCount += fScoreCountWithAlt;

                        // add this to list
                        UNITGROUPCOUNTInsertByScore (pug, afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll], (WORD)i, (WORD)j);
                        //if (afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll] < pug->fScore) {
                        //   // found a new best
                        //   pug->fScore = ;
                        //   pug->wWave = (WORD) i;
                        //   pug->wPhoneIndex = (WORD)j;
                        //}

                        continue;
                     }  // if count

                     // else, new entry, so fill in with this is best
                     pug->bNumPhones = 1;
                     pug->dwCount = 1;
                     pug->fScoreCount = fScoreCountWithAlt;

                     UNITGROUPCOUNTInsertByScore (pug, afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll], (WORD)i, (WORD)j);
                     //pug->fScore= afRank[dwPitchFidelityAll][dwDurationFidelityAll][dwEnergyFidelityAll];
                     //pug->wPhoneIndex = (WORD)j;
                     //pug->wWave = (WORD)i;
                  } // dwDurationFidelityAll, dwPitchFidelityAll
         } // dwForm
      } // j, phonemes
   } // i, waves

   // loop through and add units that occurred enough times
   for (dwLeft = 0; dwLeft < NUMSUGROUP; dwLeft++) for (dwRight = 0; dwRight < NUMSUGROUP; dwRight++) {
      PCListFixed pl = &alPhone[dwLeft][dwRight];

      // sort, for fun, since really doesn't change anything. makes debug easier
      qsort (pl->Get(0), pl->Num(), sizeof(UNITGROUPCOUNT), UNITGROUPCOUNTSort);

      pug = (PUNITGROUPCOUNT)pl->Get(0);
      for (i = 0; i < pl->Num(); i++, pug++)
         if ((pug->dwCount >= m_dwMinExamples) || (!dwLeft && !dwRight))   // NOTE: Always include at least a single copy of the phoneme
            plUNITGROUPCOUNT->Add (pug);
   } // dwForm

   return TRUE;
}


#define ANALYSISMULTIUNITS_MAXSEQUENCE    5     // maximum number of units that will remember in a sequence
   // BUGFIX - Was 6, but changed to 5 since only really use this functionality for small
   // voices where they won't have such long sequences anyway

/*************************************************************************************
CTTSWork::AnalysisMultiUnits - Finds a match for all units starting with the
given phoneme.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCListFixed    plUNITGROUPCOUNT - Filled with the unit group information (appended)
   BYTE           bStartPhone - Start phoneme must be this. Unsorted phoneme number
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisMultiUnits (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT,
                                   BYTE bStartPhone)
{
   DWORD i, j;

   // BUGFIX - If keeping all units then save some time and don't bother
   if (m_dwTotalUnits >= INCLUDEALLUNITS)
      return TRUE;

   PCMLexicon pLex = pTTS->Lexicon();
   if (!pLex)
      return FALSE;
   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   DWORD dwNumPhone = pLex->PhonemeNum();
   DWORD dwNumPhonePlusOne = dwNumPhone + 1;
   //PLEXENGLISHPHONE pe;
   //PLEXPHONE plp;

   // create temporary list
   CListFixed     lScratch;
   lScratch.Init (sizeof(UNITGROUPCOUNT));

   // loop through and fill them in
   PUNITGROUPCOUNT pug;
   DWORD dwDemi;
   DWORD dwPitchFidelityAll, dwDurationFidelityAll, dwEnergyFidelityAll;
   fp fScoreCount;
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];

      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      DWORD dwNum = pwa->m_lPHONEAN.Num();
      if (!dwNum)
         continue;

      for (j = 0; j < dwNum; j++, ppa++) { // NOTE: using j+1 so safe to test for next one
         // make sure not silence
         if (ppa->bPhone != bStartPhone)
            continue;

         // if bad phoneme then abort
         if (ppa->fBad)
            continue;

         // loop over all lengths and compute a score
         DWORD dwLength;
         QWORD qwPhonemes, qwWordPos;
         fp fJoinCostLeft, fJoinCostRight;
         for (dwLength = 2; dwLength <= min(ANALYSISMULTIUNITS_MAXSEQUENCE,sizeof(qwPhonemes)); dwLength++) {
                  // BUGFIX - Used to be max of 4, now max of 6 phonemes in a unit
            // make sure not too long
            if (j + dwLength > dwNum)
               break;

            qwPhonemes = 0;
            qwWordPos = 0;
            DWORD dwFirstIndex = (DWORD)-1;
            DWORD dwLastIndex = 0;
            fp afScore[PITCHFIDELITY][DURATIONFIDELITY][ENERGYFIDELITY];
            memset (afScore, 0, sizeof(afScore));
            DWORD k, m, n, p;
            fp fPitch = 0;
            DWORD dwPitchCount = 0;
            fp fDurationFidelitySum = 0, fEnergyFidelitySum = 0;
            DWORD dwDurationFidelityCount = 0, dwEnergyFidelityCount = 0;
            for (k = 0; k < dwLength; k++) {
               // if bad phoneme then skip
               if (ppa[k].fBad)
                  break;

               // if this is silence at the wrong place then abort now
               if (ppa[k].bPhone == bSilence) {
                  if ((k > 0) && (k < dwLength-1))
                     break;   // silence in the middle. dont do

                  // else, continue
                  continue;
               }

               // if this score is out of bounds then skip
               // NOTE: Can use [0] because they're all set together
               if (ppa[k].afRankCompare[0][0][0][0] >= UNDEFINEDRANK)
                  break;

               // remember
               dwFirstIndex = min(dwFirstIndex, j+k);
               dwLastIndex = max(dwLastIndex, j+k+1);
               qwPhonemes |= ((QWORD)ppa[k].bPhone+1) << (8 * k);
               qwWordPos |= ((QWORD)ppa[k].bWordPos) << (8 * k);
                  // BUGFIX - phonemes at start/end have lower score
               for (m = 0; m < PITCHFIDELITY; m++) for (n = 0; n < DURATIONFIDELITY; n++) for (p = 0; p < ENERGYFIDELITY; p++) for (dwDemi = 0; dwDemi < TTSDEMIPHONES; dwDemi++) {
                  if (TTSDEMIPHONES >= 2) {
                     if ((k == 0) && (dwDemi == 0))
                        continue;   // very start of first phoneme, so ignore score
                     if ((k == (dwLength-1)) && (dwDemi == 1))
                        continue;   // very end of last phoneme, so ignore score
                  } // if have demiphones

                  afScore[m][n][p] += ppa[k].afRankCompare[m][n][p][dwDemi] / (fp)TTSDEMIPHONES;
               }

               // remember the pitch
               if (ppa[k].fIsVoiced) {
                  // pitch more important for the middle ones
                  fPitch += ppa[k].fPitch;   // not bothering to conver to log
                  dwPitchCount += 1;
               }

               // duration and energy
               PTRIPHONETRAIN ptpt = (PTRIPHONETRAIN)pAnal->plTRIPHONETRAIN->Get(ppa[k].dwTRIPHONETRAINIndex);
               if (ptpt) {
                  fDurationFidelitySum += log((fp)ppa[k].dwDuration / (fp)ptpt->dwDuration);
                  dwDurationFidelityCount += 1;

                  fEnergyFidelitySum += log(max(ppa[k].fEnergyAvg,CLOSE) / max(ptpt->fEnergyMedian,CLOSE));
                  dwEnergyFidelityCount += 1;
               }

            } // k
            if ((k < dwLength) || (dwLastIndex <= dwFirstIndex))
               continue;   // skip because aborted for some reason


            // assume that joins always occur mid phone, which seems to be a fairly accurate
            // guestimate of the way that the viterbi search ends up producing

            // find the join cost of the left phoneme mid. Only calculate once for this range to save CPU
            if (dwLength == 2)
               // must be join cost at the phoneme edge, which is unusal, but do
               fJoinCostLeft = UnitScoreJoinEstimate(pTTS, ppa[0].bPhone, ppa[1].bPhone, FALSE, pLex) / (fp) AMUUNITSPERPHONEME;
            else if (dwLength == 3)
               // saves a join cost in middle of 2nd phoneme
               fJoinCostLeft = UnitScoreJoinEstimate(pTTS, ppa[1].bPhone, ppa[1].bPhone, TRUE, pLex) / (fp) AMUUNITSPERPHONEME;
            // else, keep same fJoinCost Left from before

            // find the join cost of the right phoneme mid
            if (dwLength <= 3)   // == 2 then diphone, == 3 then 3-phones and but ppa[1].bPhone = ppa[dwLength-2].pPhone
               fJoinCostRight = fJoinCostLeft;  // to make fCostNewOnOneSide and weighting to some out correctly
            else
               fJoinCostRight = UnitScoreJoinEstimate(pTTS, ppa[dwLength-2].bPhone, ppa[dwLength-2].bPhone, TRUE, pLex) / (fp) AMUUNITSPERPHONEME;

            // assuming that there's a unit sequence of length, dwLength-1. The reason we want a longer
            // unit sequence length is to eliminate a join-cost penalty. Since this is 1 longer than dwLenth-1,
            // not really sure whether the extra unit is added to left or the right, so average the two
            fp fCostNewOnOneSide = (fJoinCostLeft + fJoinCostRight) / 2.0;
            // fp fCostNewOnTwoSides = fJoinCostLeft + fJoinCostRight;
            
            // what's the probability that this is new only on one side. Else, assume new on both sides
            // fp fNewOnOneSideProb = (fp)(dwLength-1) / (fp)(dwLength);

            // final score is a combination of the probability, being divided by the number of new units
            //fScoreCount =
            //   fNewOnOneSideProb * fCostNewOnOneSide / 1.0 /* units added */ +
            //   (1.0 - fNewOnOneSideProb) * fCostNewOnTwoSides / 2.0 /* units added */;
            // BUGFIX - If do the above math guestimate, the score always comes down to
            fScoreCount = fCostNewOnOneSide / 1.0;

            DWORD dwNum = dwLastIndex - dwFirstIndex;

            // BUGFIX - versions for "all", that are independent of pitch and duration
            for (dwPitchFidelityAll = 0; dwPitchFidelityAll < 2; dwPitchFidelityAll++)
               for (dwDurationFidelityAll = 0; dwDurationFidelityAll < 2; dwDurationFidelityAll++)
                  for (dwEnergyFidelityAll = 0; dwEnergyFidelityAll < 2; dwEnergyFidelityAll++) {
                     // determine the fidelity to use
                     DWORD dwPitchFidelity = (!dwPitchFidelityAll && dwPitchCount) ?
                        PitchToFidelity (fPitch / (fp)dwPitchCount, pAnal->fAvgPitch) :
                        PITCHFIDELITYCENTER;
                     DWORD dwDurationFidelity = (!dwDurationFidelityAll && dwDurationFidelityCount) ?
                        DurationToFidelity (exp(fDurationFidelitySum / (fp)dwDurationFidelityCount), 1.0) :
                        DURATIONFIDELITYCENTER;
                     DWORD dwEnergyFidelity = (!dwEnergyFidelityAll && dwEnergyFidelityCount) ?
                        EnergyToFidelity (exp(fEnergyFidelitySum / (fp)dwEnergyFidelityCount), 1.0) :
                        ENERGYFIDELITYCENTER;

                     // BUGFIX - Used to up weight of phonemes in XXXCENTER, but don't need to do this
                     // since they will already be more statistically common
                     //dwCountScore = dwCountScoreOrig;
                     //if (dwPitchFidelity == PITCHFIDELITYCENTER)
                     //   dwCountScore *= 2;   // center pitch weighted more
                     //if (dwDurationFidelity == DURATIONFIDELITYCENTER)
                     //   dwCountScore *= 2;   // center duration weighted more
                     //if (dwEnergyFidelity == ENERGYFIDELITYCENTER)
                     //   dwCountScore *= 2;   // center energy weighted more

                     // adjust score by number of included phonemes
                     //dwCountScore /= dwNum;
                     //dwCountScore = max(dwCountScore,1); // so at least have something

                     // try to find this one
                     pug = UNITGROUPCOUNTInsert (
                        dwPitchFidelityAll ? (PITCHFIDELITYPLUSONE-1) : dwPitchFidelity,
                        dwDurationFidelityAll ? (DURATIONFIDELITYPLUSONE-1) : dwDurationFidelity,
                        dwEnergyFidelityAll ? (ENERGYFIDELITYPLUSONE-1) : dwEnergyFidelity,
                        dwNum, qwPhonemes, m_fWordStartEndCombine ? 0 : qwWordPos, &lScratch);
                     if (!pug)
                        continue;   // shouldnt happen
                     pug->dwCount++;
                     pug->fScoreCount += fScoreCount;
                     pug->bNumPhones = (BYTE)dwNum;
                     UNITGROUPCOUNTInsertByScore (pug, afScore[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity], (WORD)i, (WORD)dwFirstIndex);
                     //if (afScore[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity] < pug->fScore) {
                     //   // keep as new best
                     //   pug->bNumPhones = (BYTE)dwNum;
                     //   pug->fScore = afScore[dwPitchFidelity][dwDurationFidelity][dwEnergyFidelity];
                     //   pug->wPhoneIndex = (WORD)dwFirstIndex;
                     //   pug->wWave = (WORD)i;
                     //}
                  } // dwPitchFidelityAll, dwDurationFidelityAll
         } // dwLength

      } // j, phonemes
   } // i, waves

   // find the units that occurred enough times and add them
   pug = (PUNITGROUPCOUNT)lScratch.Get(0);
   for (i = 0; i < lScratch.Num(); i++, pug++)
      if (pug->dwCount >= m_dwMinExamples)
         plUNITGROUPCOUNT->Add (pug);

   return TRUE;
}


/*************************************************************************************
UNITGROUPCOUNTKeepPhonemes - Call this to keep a phoneme in a unit group count

inputs
   PTTSANAL       pAnal - Analysis information
   WORD           wWave - Wave number
   WORD           wPhoneIndex - Phoneme index to start
   BYTE           bNumPhones - Number of phonemes
   BYTE           bSilence - Silence phoneme
   BOOL           fJustToTest - If TRUE, just doing this to test. If FALSE, acutally change dwWantInFinal and dwWantInFinalAudio
   DWORD          *pdwAudioAdded - Number of audio units that would be added
   DWORD          *pdwSubscribedTo - Filled in with sum of audio added for all interested phonemes. Used to encourage
                  choice of most popular units already
returns
   DWORD - Number of units that would be added
*/
DWORD UNITGROUPCOUNTKeepPhonemes (PTTSANAL pAnal, WORD wWave, WORD wPhoneIndex, BYTE bNumPhones, BYTE bSilence,
                                  BOOL fJustToTest, DWORD *pdwAudioAdded, DWORD *pdwSubscribedTo)
{
   *pdwAudioAdded = 0;
   *pdwSubscribedTo = 0;

   PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(wWave);
   if (!ppwa)
      return 0;
   PCWaveAn pwa = ppwa[0];
   if (!pwa)
      return 0;
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(wPhoneIndex);
   if (!ppa)
      return 0;

   // see how many want in final
   DWORD dwWantInFinal = 0;
   int i;
   for (i = 0; i < (int)bNumPhones; i++) {
      if (!ppa[i].dwWantInFinal)
         dwWantInFinal++;

      // if actually want to add it then do so
      if (!fJustToTest)
         ppa[i].dwWantInFinal++;
   }

   // loop over audio added
   for (i = -1; i <= (int)bNumPhones; i++) {  // intentionally use <=
      if ((i < 0) && !wPhoneIndex)
         continue;
      if (i + (int)wPhoneIndex >= (int) pwa->m_lPHONEAN.Num())
         continue;   // too far to right

      // if it's silence then skip
      if (ppa[i].bPhone == bSilence)
         continue;

      // add
      if (!ppa[i].dwWantInFinalAudio)
         *pdwAudioAdded += 1;

      // how many want it already
      *pdwSubscribedTo += ppa[i].dwWantInFinalAudio;

      // actuall yadd?
      if (!fJustToTest)
         ppa[i].dwWantInFinalAudio++;
   } // i

   // done
   return dwWantInFinal;
}


/*************************************************************************************
UNITGROUPCOUNTKeepPhonemes - Call this to keep the phonemes in a unitgroup

inputs
   PTTSANAL          pAnal - Analysis information
   PUNITGROUPCOUNT   pug - To keep
   BYTE              bSilence - Precalculated silence phoneme
   DWORD             dwRandom - Random value to ensure that doesn't always take the same score.
returns
   DWORD - Number of units added
*/
DWORD UNITGROUPCOUNTKeepPhonemes (PTTSANAL pAnal, PUNITGROUPCOUNT pug, BYTE bSilence, DWORD dwRandom)
{
   pug->dwUGCSBest = (DWORD)-1;

   // how many can try
   DWORD dwToTry = (pug->dwCount + PUGTOTRY / 2) / PUGTOTRY;
   dwToTry = max(dwToTry, 1);
   dwToTry = min(dwToTry, pug->dwUGCSCount);
   if (!dwToTry)
      return 0;   // not added

   // loop
   DWORD i;
   DWORD dwBest = (DWORD)-1;
   DWORD dwBestAdded = 1000000, dwBestAudioAdded = 1000000, dwBestSubscribed = 0;
   DWORD dwAddedThis, dwAudioAddedThis, dwSubscribedThis;
   for (i = 0; i < dwToTry; i++) {
      // which one acutally try
      DWORD dwTry = (i + dwRandom) % dwToTry;

      dwAddedThis = UNITGROUPCOUNTKeepPhonemes (pAnal, pug->aUGCS[dwTry].wWave, pug->aUGCS[dwTry].wPhoneIndex,
         pug->bNumPhones, bSilence, TRUE, &dwAudioAddedThis, &dwSubscribedThis);

      // eliminate
      if (dwAddedThis > dwBestAdded)
         continue;   // already have something better as far as added
      if (dwAddedThis == dwBestAdded) {
         // match
         if (dwAudioAddedThis > dwBestAudioAdded)
            continue;   // already have something better

         if ((dwAudioAddedThis == dwBestAudioAdded) && (dwSubscribedThis < dwBestSubscribed))
            continue;   // already have something better
      }

      // else, keep
      dwBest = dwTry;
      dwBestAdded = dwAddedThis;
      dwBestAudioAdded = dwAudioAddedThis;
      dwBestSubscribed = dwSubscribedThis;
   } // i

   // if can't find best then exit
   if (dwBest == (DWORD)-1)
      return 0;

   // else, use this
   pug->dwUGCSBest = dwBest;
   return UNITGROUPCOUNTKeepPhonemes (pAnal, pug->aUGCS[dwBest].wWave, pug->aUGCS[dwBest].wPhoneIndex,
         pug->bNumPhones, bSilence, FALSE, &dwAudioAddedThis, &dwSubscribedThis);
}


/*************************************************************************************
UNITGROUPCOUNTReleasePhonemes - Release the phonemes previously used in a UNITGROUPGOUNT,
as remembered in UNITGROUPCOUNTKeepPhonemes().

inputs
   PTTSANAL          pAnal - Analysis information
   PUNITGROUPCOUNT   pug - To keep
   BYTE              bSilence - Precalculated silence phoneme
returns
   DWORD - Number of units released
*/
DWORD UNITGROUPCOUNTReleasePhonemes (PTTSANAL pAnal, PUNITGROUPCOUNT pug, BYTE bSilence)
{
   DWORD dwUse = pug->dwUGCSBest;
   if (dwUse >= pug->dwUGCSCount)
      return 0;   // no change
   pug->dwUGCSBest = (DWORD)-1;
   WORD wWave = pug->aUGCS[dwUse].wWave;
   WORD wPhoneIndex = pug->aUGCS[dwUse].wPhoneIndex;
   BYTE bNumPhones = pug->bNumPhones;

   PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(wWave);
   if (!ppwa)
      return 0;
   PCWaveAn pwa = ppwa[0];
   if (!pwa)
      return 0;
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(wPhoneIndex);
   if (!ppa)
      return 0;

   // see how many want in final
   DWORD dwWantInFinal = 0;
   int i;
   for (i = 0; i < (int)bNumPhones; i++) {
      if (ppa[i].dwWantInFinal)
         ppa[i].dwWantInFinal--;

      if (!ppa[i].dwWantInFinal)
         dwWantInFinal++;
   }

   // loop over audio added
   for (i = -1; i <= (int)bNumPhones; i++) {  // intentionally use <=
      if ((i < 0) && !wPhoneIndex)
         continue;
      if (i + (int)wPhoneIndex >= (int) pwa->m_lPHONEAN.Num())
         continue;   // too far to right

      // if it's silence then skip
      if (ppa[i].bPhone == bSilence)
         continue;

      // remove
      if (ppa[i].dwWantInFinalAudio)
         ppa[i].dwWantInFinalAudio--;
   } // i

   // done
   return dwWantInFinal;
}


/*************************************************************************************
CTTSWork::AnalysisSelectFromUNITGROUPCOUNT - This selects the top units
from UNITGROUPCOUNT.

inputs
   PTTANAL        pAnal - Analysis information
   PCMTTS         pTTS - TTS to write to
   PCListFixed    plUNITGROUPCOUNT - Contains the unit information, unsorted
returns
   BOOL - TRUE if success
*/

BOOL CTTSWork::AnalysisSelectFromUNITGROUPCOUNT (PTTSANAL pAnal, PCMTTS pTTS, PCListFixed plUNITGROUPCOUNT)
{
   DWORD i, j;
   PUNITGROUPCOUNT pug;

   // BUGFIX - loop through all the units and fine-tune the fScoreCount, to couneract some of the weight
   // added by dwCount. Doing so encourages "risky" or variable-sounding unit sequences above unit sequences
   // that occur frequently
   pug = (PUNITGROUPCOUNT)plUNITGROUPCOUNT->Get(0);
   for (i = 0; i < plUNITGROUPCOUNT->Num(); i++, pug++) {
      if (!pug->dwCount)
         continue;   // shouldn't happen

      // basically, if divided by pug->dwCount, then would chose the most variable-sounding units to keep,
      // regardless of how common they are. If set fScoreCount = pug->dwCount, would keep the most ocmmon
      // unit sequences, regardless of how challenging they are. Had fScore count as no change, but I think
      // is not keeping enough challenging units, so removing some of the impact of dwCount by
      // dividing by sqrt(pug->dwCount)
      pug->fScoreCount /= sqrt((fp)pug->dwCount);

   } // i

   // sort the list so the highest countscores are on top
   qsort (plUNITGROUPCOUNT->Get(0), plUNITGROUPCOUNT->Num(), sizeof(UNITGROUPCOUNT), UNITGROUPCOUNTSort);

#if 1 //  0 // for test purposes
   PCMLexicon pLex = pTTS->Lexicon();
   PLEXPHONE plp;
   char szTemp[128];
   if (!pLex)
      return FALSE;
   FILE *pf = fopen("c:\\temp\\UnitGroupCount.txt", "wt");

   if (pf) {
      // write out a text file so know best units
      pug = (PUNITGROUPCOUNT) plUNITGROUPCOUNT->Get(0);
      for (i = 0; i < plUNITGROUPCOUNT->Num(); i++, pug++) {
         // newline
         fputs ("\n", pf);

         // show the phonemes
         for (j = 0; j < sizeof(pug->qwPhonemes); j++) {
            BYTE bPhone = (BYTE)(pug->qwPhonemes >> (j * 8));
            if (!bPhone)
               continue;   // nothing
            bPhone--;

            // NOTE: Not displaying word pos

            plp = pLex->PhonemeGetUnsort (bPhone);
            if (!plp)
               continue;
            WideCharToMultiByte (CP_ACP, 0, plp->szPhoneLong, -1, szTemp, sizeof(szTemp), 0, 0);
            fprintf (pf, "%s ", szTemp);
         } // j

         // count info and stuff
         fprintf (pf, " - ScoreCount=%g Count=%d UGCSCount=%d Score1=%g Score2=%g Score3=%g Wave=%d PhoneIndex=%d Num=%d PitchFid=%d DurFid=%d EnergyFid=%d",
            (double)pug->fScoreCount, (int)pug->dwCount, (int)pug->dwUGCSCount,
            (double)pug->aUGCS[0].fScore,
            (double) ((pug->dwUGCSCount >= 2) ? pug->aUGCS[1].fScore: 0.0),
            (double) ((pug->dwUGCSCount >= 3) ? pug->aUGCS[2].fScore: 0.0),
            (int)pug->aUGCS[0].wWave, (int)pug->aUGCS[0].wPhoneIndex, (int)pug->bNumPhones,
            (int)pug->bPitchFidelity, (int)pug->bDurationFidelity, (int)pug->bEnergyFidelity);
      } // i

      // fclose (pf);
   } // pf
#endif // 1 or 0

   BYTE bSilence = pLex->PhonemeFindUnsort (pLex->PhonemeSilence());
   DWORD dwUnitsUsed = 0, dwLastNumSequences = 0;

   DWORD dwPass;
   DWORD dwMaxPasses = (m_dwTotalUnits >= INCLUDEALLUNITS) ? 1 : UGCPASSES;
      // BUGFIX - If asking for infinite number of units, then only bother with one pass because will
      // automatically choose all units, so not use wasting time
   PUNITGROUPCOUNT *ppug;
   CListFixed lPUNITGROUPCOUNT;

   for (dwPass = 0; dwPass < dwMaxPasses; dwPass++) {
      pug = (PUNITGROUPCOUNT) plUNITGROUPCOUNT->Get(0);

      // loop through the units used last time and randomize their order
      lPUNITGROUPCOUNT.Init (sizeof(PUNITGROUPCOUNT));
      lPUNITGROUPCOUNT.Required (dwLastNumSequences);
      for (i = 0; i < dwLastNumSequences; i++, pug++) {
         lPUNITGROUPCOUNT.Add (&pug);

         // if this happens to be the second pass (dwPass == 1) only, then
         // completely remove the unit
         if (dwPass == 1)
            dwUnitsUsed -= UNITGROUPCOUNTReleasePhonemes (pAnal, pug, bSilence);
      }

      // make sure down to 0 units
      _ASSERTE ((dwPass != 1) || !dwUnitsUsed);

      ppug = (PUNITGROUPCOUNT*)lPUNITGROUPCOUNT.Get(0);
      for (i = 0; i < dwLastNumSequences; i++) {
         // randomly order
         DWORD dwTo = ((DWORD)rand() + (((DWORD)rand()) << 16)) % dwLastNumSequences;
         pug = ppug[dwTo];
         ppug[dwTo] = ppug[i];
         ppug[i] = pug;
      }

      // loop through the randomly ordered units and remove, and re-add, which
      // should lead to a reduced number of units
      for (i = 0; i < dwLastNumSequences; i++) {
         if (dwPass != 1)
            dwUnitsUsed -= UNITGROUPCOUNTReleasePhonemes (pAnal, ppug[i], bSilence);
         dwUnitsUsed += UNITGROUPCOUNTKeepPhonemes (pAnal, ppug[i], bSilence, (DWORD)rand());
      } // i

      // see if can add any more
      pug = (PUNITGROUPCOUNT) plUNITGROUPCOUNT->Get(dwLastNumSequences);
      for (i = dwLastNumSequences; (i < plUNITGROUPCOUNT->Num()) && (dwUnitsUsed < m_dwTotalUnits); i++, pug++)
         dwUnitsUsed += UNITGROUPCOUNTKeepPhonemes (pAnal, pug, bSilence, (DWORD) rand());

      // remember number of units in sequence
      dwLastNumSequences = i;

      // write this info out
#if 1 //  0 // for test purposes
      if (pf) {
         fprintf (pf, "\nUnit optimization pass %d got to sequence %d",
            (int)dwPass, (int)dwLastNumSequences);
      }
#endif // 1 or 0
   } // dwPass

   // add in free phonemes
   DWORD dwUnitsPreFree = dwUnitsUsed;
   CListFixed lFreeToAdd;
   // Test (MAXUGCS > 1) so can debug with original setup and no free units
   if (MAXUGCS > 1) for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);

      lFreeToAdd.Init (sizeof(PPHONEAN));

      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         // if out of range for L/R check then ignore
         if ((j < 1) || ((j+1) >= pwa->m_lPHONEAN.Num()))
            continue;

         // if this is selected then ignore
         if (ppa->dwWantInFinal)
            continue;

         // both left and right must be selected
         if (!ppa[-1].dwWantInFinal || !ppa[1].dwWantInFinal)
            continue;

         // must be an OK unit
         if ((ppa->bPhone != bSilence) && (ppa->afRankCompare[0][0][0][0] < UNDEFINEDRANK) && !ppa->fBad && !ppa->dwWantInFinal)
            lFreeToAdd.Add (&ppa);
      } // j
      
      // add all of these
      PPHONEAN *pppa = (PPHONEAN*)lFreeToAdd.Get(0);
      for (j = 0; j < lFreeToAdd.Num(); j++) {
         pppa[j]->dwWantInFinal++;
         dwUnitsUsed++;
      }
   } // i
#if 1 //  0 // for test purposes
   if (pf)
      fprintf (pf, "\nAdd %d free (no cost) units", (int)(dwUnitsUsed - dwUnitsPreFree));
#endif // 1 or 0


#if 1 //  0 // for test purposes
   if (pf)
      fclose (pf);
#endif // 1 or 0

   // if total units >= 1000000 then add add
   if (m_dwTotalUnits >= INCLUDEALLUNITS) for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn *ppwa = (PCWaveAn*) pAnal->plPCWaveAn->Get(i);
      PCWaveAn pwa = ppwa[0];
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);


      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++)
         if ((ppa[j].bPhone != bSilence) && (ppa[j].afRankCompare[0][0][0][0] < UNDEFINEDRANK) && !ppa[j].fBad && !ppa[j].dwWantInFinal) {
            ppa[j].dwWantInFinal++;
            dwUnitsUsed++;
         }
   } // i

   return TRUE;
}




/*************************************************************************************
CTTSWork::EscMultiThreadedCallback - Standard call
*/
void CTTSWork::EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread)
{
   DWORD *padw = (DWORD*)pParams;
   DWORD dwStart = padw[0];
   DWORD dwEnd = padw[1];
   DWORD dwType = padw[2];
   DWORD i;

   switch (dwType) {
   case 10: // AnalyzeWave
      {
         PEMTCANALYZEWAVE peaw = (PEMTCANALYZEWAVE) pParams;

         for (i = dwStart; i < dwEnd; i++) {
            PCWaveAn *ppwa = (PCWaveAn*)peaw->plWave->Get(i);
            PCWaveAn pwa = *ppwa;

            if (!pwa->AnalyzeWave (pwa->m_szFile, i, peaw->paplTriPhone, peaw->paplWord,
               peaw->pTTS, peaw->pVF, peaw->pafEnergyPerPitch, peaw->pafEnergyPerVolume, peaw->lpcs,
               peaw->pafPhonemePitchSum, peaw->pafPhonemeDurationSum, peaw->pafPhonemeEnergySum, peaw->padwPhonemeCount)) {

                  delete pwa;
                  *ppwa = NULL;  // to send error back
            }
         } // i
      }
      break;


   case 20: // AnalysisPHONETRAINSub
      {
         PEMTCANALYSISPHONETRAIN peapt = (PEMTCANALYSISPHONETRAIN) pParams;

         for (i = dwStart; i < dwEnd; i++)
            AnalysisPHONETRAINSub (peapt->pAnal, peapt->pTTS, i, dwThread);
      }
      break;

   case 30: // AnalysisMegaPHONETRAINSub
      {
         PEMTCANALYSISPHONETRAIN peapt = (PEMTCANALYSISPHONETRAIN) pParams;

         for (i = dwStart; i < dwEnd; i++)
            AnalysisMegaPHONETRAINSub (peapt->pAnal, peapt->pTTS, i, dwThread);
      }
      break;

   case 40: // AnalysisGroupTRIPHONETRAINSub
      {
         PEMTCANALYSISPHONETRAIN peapt = (PEMTCANALYSISPHONETRAIN) pParams;

         for (i = dwStart; i < dwEnd; i++)
            AnalysisGroupTRIPHONETRAINSub (peapt->pAnal, peapt->pTTS, i, dwThread, peapt->dwStartPhone, peapt->dwEndPhone);
      }
      break;

   case 45: // AnalysisSpecificTRIPHONETRAINSub
      {
         PEMTCANALYSISPHONETRAIN peapt = (PEMTCANALYSISPHONETRAIN) pParams;

         for (i = dwStart; i < dwEnd; i++)
            AnalysisSpecificTRIPHONETRAINSub (peapt->pAnal, peapt->pTTS, i, dwThread, peapt->dwStartPhone, peapt->dwEndPhone);
      }
      break;

   case 50: // JoinCosts
      {
         PEMTCANALYSISPHONETRAIN peapt = (PEMTCANALYSISPHONETRAIN) pParams;

         for (i = dwStart; i < dwEnd; i++)
            AnalysisJoinCostsSub (peapt->dwPass, peapt->pAnal, peapt->pJC, peapt->pTTS, i, dwThread);
      }
      break;

   } // switch dwType
}


#ifdef _DEBUG
/*************************************************************************************
FakeSRFEATURE - Fills in fake SRFEATURE based on a hash. Used for testing
that audio gets all the way through

inputs
   PSRFEATURE     psrf - To modify
   DWORD          dwNum - Number to modify
   DWORD          dwHash - Hash number to use
returns
   none
*/
#define FAKESRFEATUREDB       (-40)
static void FakeSRFEATURE (PSRFEATURE psrf, DWORD dwNum, DWORD dwHash)
{
   DWORD i, j;
   for (i = 0; i < dwNum; i++, psrf++) {
      for (j = 0; j < SRDATAPOINTS; j++)
         psrf->acVoiceEnergy[j] = psrf->acNoiseEnergy[j] = -110;
      for (j = 0; j < SRPHASENUM; j++)
         psrf->abPhase[j] = 0;

      // same for all
      for (j = 0; j < SRDATAPOINTS; j++)
         psrf->acVoiceEnergy[j] = FAKESRFEATUREDB + 10 - (char)(j / 3);

      // phase with some alternating
      for (j = 0; j < SRPHASENUM; j++) {
         psrf->abPhase[j] = (BYTE)(j * 256 / SRPHASENUM);

         if (!(j%4))
            psrf->abPhase[j] += 0x40;  // slightly out of phase
      }

      continue;

      // either vertical or horizontal stipes
      if (dwHash & 0x02) {
         DWORD dwBar = i / 2;
         if (dwBar %2)
            continue;   // skip

         dwBar /= 2;
         dwBar = dwBar % 2;
         for (j = 0; j < SRDATAPOINTS; j++)
            if (dwBar)
               psrf->acVoiceEnergy[j] = FAKESRFEATUREDB;
            else
               psrf->acNoiseEnergy[j] = FAKESRFEATUREDB;
      }
      else {
         for (j = 0; j < SRDATAPOINTS; j++) {
            DWORD dwBar = j / (SRPOINTSPEROCTAVE/2);
            if (dwBar % 2)
               continue;   // empty

            dwBar /= 2;
            dwBar = dwBar % 2;
            if (dwBar)
               psrf->acVoiceEnergy[j] = FAKESRFEATUREDB;
            else
               psrf->acNoiseEnergy[j] = FAKESRFEATUREDB;
         } // j
      }

      if (dwHash % 0x04) {
         for (j = 0; j < SRPHASENUM; j++)
            psrf->abPhase[j] = (BYTE)(i * 256 / dwNum);
      }
      else {
         for (j = 0; j < SRPHASENUM; j++)
            psrf->abPhase[j] = (BYTE)(j * 256 / SRDATAPOINTS);
      }
   } // i
}


#endif // _DEBUG




/*************************************************************************************
CTTSWork::SaveRecTranscript - Saves the text of all the recordings into a text
file.

inputs
   PCEscPage         pPage - Current page
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CTTSWork::SaveRecTranscript (PCEscPage pPage)
{
   FILE *f = fopen ("c:\\temp\\SaveRecTranscript.txt", "wt");
   if (!f) {
      pPage->MBError (L"Can't open the file c:\\temp\\SaveRecTranscript.txt.");
      return FALSE;
   }

   // loop
   DWORD i;
   char szHuge[10000];
   PCWaveFileInfo *ppwfi = (PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      if (!ppwfi[i]->m_pszText)
         continue;   // no text

      // save
      WideCharToMultiByte (CP_ACP, 0, ppwfi[i]->m_pszText, -1, szHuge, sizeof(szHuge), 0, 0);

      fputs (szHuge, f);
      fputs ("\n", f);
   } // i

   fclose (f);

   return TRUE;
}


/*************************************************************************************
CTTSWork::Import2008 - Imports blizzard 2008 file format

inputs
   PCEscPage         pPage - Current page
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CTTSWork::Import2008 (PCEscPage pPage)
{
   char szFile[256];
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
   ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Data file (*.data)\0*.data\0\0\0";
   ofn.lpstrFile = szFile;
   ofn.nMaxFile = sizeof(szFile);
   ofn.lpstrTitle = "Load Blizzard 2008 utt.data file";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "data";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return FALSE;


   // open if
   WCHAR szwFile[256];
   MultiByteToWideChar (CP_ACP, 0, szFile, -1, szwFile, sizeof(szwFile)/sizeof(WCHAR));
   FILE *pf = _wfopen (szwFile, L"rt");
   if (!pf) {
      pPage->MBError (L"Can't open the selected text file.", szwFile);
      return FALSE;
   }

   // make the file root
   WCHAR szFileRoot[256];
   wcscpy (szFileRoot, szwFile);
   PWSTR pszSlash = szFileRoot, pszLastSlash = NULL;
   while (TRUE) {
      pszSlash = wcschr (pszSlash, L'\\');
      if (!pszSlash)
         break;

      pszLastSlash = pszSlash;
      pszSlash++;
   }
   if (pszLastSlash)
      pszLastSlash[1] = 0; // to termiante there

   BOOL fNorm = (IDYES == pPage->MBYesNo (L"Do you want to trim silence, remove noise, and normalize? (recommended)"));

   WCHAR szHuge[10000], szHugeCopy[10000];
   DWORD dwWaves = 0;
   {
      CProgress Progress;

      Progress.Start (pPage->m_pWindow->m_hWnd, "Processing", TRUE);

      // repeat
      WCHAR szWaveFile[256];
      char szaWaveFile[256];
      CM3DWave Wave;
      while (TRUE) {
         szHuge[0] = 0;
         if (!fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf))
            break;   // BUGFIX
         wcscpy (szHugeCopy, szHuge);
         WCHAR *pszN = wcschr (szHuge, L'\n');
         if (pszN)
            *pszN = 0;
         pszN = wcschr (szHuge, L'\r');
         if (pszN)
            *pszN = 0;

         if (!wcslen(szHuge))
            continue;   // empty

         Progress.Update ((fp)(dwWaves % 1000) / 1000.0);

         WCHAR *pszStart = wcschr (szHuge, L'(');
         if (!pszStart) {
            pPage->MBError (L"Can't find starting paren.", szHugeCopy);
            goto done;
         }

         // find space
         WCHAR *pszSpace = wcschr (pszStart, L' ');
         if (!pszStart) {
            pPage->MBError (L"Can't find space after file name.", szHugeCopy);
            goto done;
         }
         pszSpace[0] = 0;

         // make the filename
         wcscpy (szWaveFile, szFileRoot);
         wcscat (szWaveFile, pszStart + 1);
         wcscat (szWaveFile, L".wav");
         WideCharToMultiByte (CP_ACP, 0, szWaveFile, -1, szaWaveFile, sizeof(szaWaveFile), 0, 0);
         if (!Wave.Open (NULL, szaWaveFile)) {
            pPage->MBError (L"Can't open the wave file.", szWaveFile);
            goto done;
         }

         // get the text
         WCHAR *pszQuoteFirst = wcschr (pszSpace+1, L'"');
         if (!pszQuoteFirst) {
            pPage->MBError (L"Can't open the starting quote.", szHugeCopy);
            goto done;
         }

         // find the last quote
         WCHAR *pszQuoteLast = NULL;
         WCHAR *pszCur;
         pszCur = pszQuoteFirst+1;
         while (TRUE) {
            pszCur = wcschr (pszCur, L'"');
            if (!pszCur)
               break;
            pszQuoteLast = pszCur;
            pszCur++;
         }
         if (!pszQuoteLast) {
            pPage->MBError (L"Can't find the ending quote.", szHugeCopy);
            goto done;
         }

         // get rid or \"
         pszQuoteLast[0] = 0; // NULL terminate
         pszQuoteFirst++;
         for (pszCur = szHugeCopy; *pszQuoteFirst; pszQuoteFirst++, pszCur++) {
            if ((pszQuoteFirst[0] == '\\') && (pszQuoteFirst[1] == '"')) {
               pszCur[0] = pszQuoteFirst[1];
               pszQuoteFirst++;
            }
            else
               pszCur[0] = pszQuoteFirst[0];
         }
         pszCur[0] = 0;

         pszCur = (PWSTR)Wave.m_memSpoken.p;
         if (_wcsicmp(pszCur, szHugeCopy)) {
            // write in wave file
            MemZero (&Wave.m_memSpoken);
            MemCat (&Wave.m_memSpoken, szHugeCopy);

            // effects
            if (fNorm) {
               // BUGFIX - While at it, remove sub-voice, noise, and noramlize
               Wave.FXRemoveDCOffset (TRUE, NULL);
               short sMax = Wave.FindMax();
               fp f;
               sMax = max(1,sMax);
               f = 32767.0 / (fp)sMax;
               Wave.FXVolume (f, f, NULL);
               Wave.FXNoiseReduce (NULL);
            }
            
            // save
            Wave.Save (FALSE, NULL);
         }
         dwWaves++;
      }
   } // Progress

done:
   fclose (pf);

   swprintf (szHuge, L"%d waves scanned and modified.", (int)dwWaves);
   pPage->MBInformation (szHuge);

   return TRUE;
}



/*************************************************************************************
Blizzard2009ToneCleanup - The blizzard 2009 data has some odd tone numbers. 0, I know,
gets converted to 5. I'm not sure about the others though.

inputs
   PWSTR       psz - Text, with tone numbers
*/
void Blizzard2009ToneCleanup (PWSTR psz)
{
   for (; psz[0]; psz++) {
      if ((psz[0] < L'0') || (psz[0] > L'9'))
         continue;   // not a tone

      // if it's know tone then keep
      if ((psz[0] >= L'1') && (psz[0] <= L'5'))
         continue;

      // if 0, convert to 5
      if (psz[0] == L'0') {
         psz[0] = L'5';
         continue;
      }

      // just convert to 5 since unknown
      psz[0] = L'5';
   }
}

/*************************************************************************************
CTTSWork::Import2009Mandarin - Imports blizzard 2009 mandarin file format

inputs
   PCEscPage         pPage - Current page
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CTTSWork::Import2009Mandarin (PCEscPage pPage)
{
   char szFile[256];
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
   ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
   ofn.hInstance = ghInstance;
   ofn.lpstrFilter = "Data file (*.data)\0*.data\0\0\0";
   ofn.lpstrFile = szFile;
   ofn.nMaxFile = sizeof(szFile);
   ofn.lpstrTitle = "Load Blizzard 2009 ProText.data file";
   ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
   ofn.lpstrDefExt = "data";
   // nFileExtension 

   if (!GetOpenFileName(&ofn))
      return FALSE;


   // open if
   WCHAR szwFile[256];
   MultiByteToWideChar (CP_ACP, 0, szFile, -1, szwFile, sizeof(szwFile)/sizeof(WCHAR));
   FILE *pf = _wfopen (szwFile, L"rt");
   if (!pf) {
      pPage->MBError (L"Can't open the selected text file.", szwFile);
      return FALSE;
   }

   // make the file root
   WCHAR szFileRoot[256];
   wcscpy (szFileRoot, szwFile);
   PWSTR pszSlash = szFileRoot, pszLastSlash = NULL;
   while (TRUE) {
      pszSlash = wcschr (pszSlash, L'\\');
      if (!pszSlash)
         break;

      pszLastSlash = pszSlash;
      pszSlash++;
   }
   if (pszLastSlash)
      pszLastSlash[1] = 0; // to termiante there

   BOOL fNorm = (IDYES == pPage->MBYesNo (L"Do you want to trim silence, remove noise, and normalize? (recommended)"));

   WCHAR szHuge[10000]; //, szHugeCopy[10000];
   DWORD dwWaves = 0;
   {
      CProgress Progress;

      Progress.Start (pPage->m_pWindow->m_hWnd, "Processing", TRUE);

      // repeat
      WCHAR szWaveFile[256];
      char szaWaveFile[256];
      CM3DWave Wave;
      while (TRUE) {
         szHuge[0] = 0;
         if (!fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf))
            break;   // BUGFIX
         size_t iSize = wcslen(szHuge);
         if (!iSize)
            continue;
         if (iSize >= sizeof(szWaveFile) / sizeof(WCHAR) + 6 + wcslen(szFileRoot)) {
            pPage->MBError (L"Wave file name too long.", szHuge);
            break;
         }
         WCHAR *pszCur;
         pszCur = wcschr (szHuge, L'\r');
         if (pszCur)
            pszCur[0] = 0;
         pszCur = wcschr (szHuge, L'\n');
         if (pszCur)
            pszCur[0] = 0;
         wcscpy (szWaveFile, szFileRoot);
         wcscat (szWaveFile, szHuge);
         wcscat (szWaveFile, L".wav");

         // skip the next line
         if (!fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf))
            break;   // BUGFIX

         // read in the pinyin
         if (!fgetws (szHuge, sizeof(szHuge) / sizeof(WCHAR), pf))
            break;   // BUGFIX
         pszCur = wcschr (szHuge, L'\r');
         if (pszCur)
            pszCur[0] = 0;
         pszCur = wcschr (szHuge, L'\n');
         if (pszCur)
            pszCur[0] = 0;

         // loop and insert spaces
         for (pszCur = szHuge; *pszCur; pszCur++) {
            if ((pszCur[0] < L'0') || (pszCur[0] > L'9'))
               continue;   // do nothing
            
            // else, insert after this
            pszCur++;

            iSize = wcslen(pszCur) + 1;
            memmove (pszCur+1, pszCur, iSize * sizeof(WCHAR));

            pszCur[0] = L' ';
         }
         Blizzard2009ToneCleanup (szHuge);

         Progress.Update ((fp)(dwWaves % 1000) / 1000.0);

         // try to open
         WideCharToMultiByte (CP_ACP, 0, szWaveFile, -1, szaWaveFile, sizeof(szaWaveFile), 0, 0);
         if (!Wave.Open (NULL, szaWaveFile)) {
            pPage->MBError (L"Can't open the wave file.", szWaveFile);
            goto done;
         }


         pszCur = (PWSTR)Wave.m_memSpoken.p;
         if (_wcsicmp(pszCur, szHuge)) {
            // write in wave file
            MemZero (&Wave.m_memSpoken);
            MemCat (&Wave.m_memSpoken, szHuge);

            // effects
            if (fNorm) {
               // BUGFIX - While at it, remove sub-voice, noise, and noramlize
               Wave.FXRemoveDCOffset (TRUE, NULL);
               short sMax = Wave.FindMax();
               fp f;
               sMax = max(1,sMax);
               f = 32767.0 / (fp)sMax;
               Wave.FXVolume (f, f, NULL);
               Wave.FXNoiseReduce (NULL);
            }
            
            // save
            Wave.Save (FALSE, NULL);
         }
         dwWaves++;
      }
   } // Progress

done:
   fclose (pf);

   swprintf (szHuge, L"%d waves scanned and modified.", (int)dwWaves);
   pPage->MBInformation (szHuge);

   return TRUE;
}

/*************************************************************************************
CTTSWork::JoinCosts - Calculates the join costs for a TTS voice.

inputs
   HWND           hWnd - Window to bring up progress bar and UI for saving
   PCEscWindow    pWindow - Window where to bring up UI for modifying/testing
returns
   BOOL - TRUE if user pressed back, FALSE if cancels
*/
BOOL CTTSWork::JoinCosts (HWND hWnd, PCEscWindow pWindow)
{
   // BUGFIX - Always combine start/end
   // BGUFIX - Changed back to FALSE in order to improve prosody
   m_fWordStartEndCombine = FALSE;
   DWORD i;


#if 0 // def _DEBUG // to test
   SRFEATURE SRF, SRFNew;
   memset (&SRF, 0, sizeof(SRF));
   float afVoiced[JOINPSOLAHARMONICS], afNoise[JOINPSOLAHARMONICS];
   for (i = 0; i < SRDATAPOINTS; i++) {
      SRF.acVoiceEnergy[i] = (char)(i % 64) - 70;
      SRF.acNoiseEnergy[i] = -110;
   }
   SRFEATUREToHarmonics (&SRF, JOINPSOLAHARMONICS, afVoiced, afNoise, 130);
   SRFEATUREFromHarmonics (JOINPSOLAHARMONICS, afVoiced, afNoise, 130, &SRFNew);
#endif

   CMTTS tts;
   tts.LexiconSet (m_szLexicon);
   tts.TriPhoneGroupSet (m_dwTriPhoneGroup);
   tts.KeepLogSet (m_fKeepLog);
   BOOL fFullPCM = FALSE;
   if (m_dwFreqCompress == 0) // per frame PCM
      fFullPCM = m_fFullPCM;
   if (m_dwPCMCompress)
      fFullPCM = TRUE;
   tts.FullPCMSet (fFullPCM);
   // BUGFIX - Only do fullPCM is not compressing frequency
   if (m_memTTSTARGETCOSTS.m_dwCurPosn == sizeof(TTSTARGETCOSTS))
      tts.TTSTARGETCOSTSSet ((PTTSTARGETCOSTS) m_memTTSTARGETCOSTS.p);
   if (!tts.LexWordsSet (m_pLexWords))
      return TRUE;
   
   // wipe out memory
   if (!m_memTTSTARGETCOSTS.Required (sizeof(TTSTARGETCOSTS)))
      return TRUE;   // shouldnt happen
   m_memTTSTARGETCOSTS.m_dwCurPosn = sizeof(TTSTARGETCOSTS);
   memset (m_memTTSTARGETCOSTS.p, 0, m_memTTSTARGETCOSTS.m_dwCurPosn);

   // set random so reproducable
   srand (1000);


#ifdef _DEBUG
         // BUGFIX - Turn off continual testing so that runs faster

         // Get current flag
         int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

         // Turn on leak-checking bit
         tmpFlag &= ~(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF);
         //tmpFlag |=  _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF;

         tmpFlag = LOWORD(tmpFlag) | (_CRTDBG_CHECK_EVERY_1024_DF << 4); // BUGFIX - So dont check for memory overwrites that often, make things faster

         // Set flag to the new value
         _CrtSetDbgFlag( tmpFlag );
#endif // _DEBUG

   // analysis
   TTSANAL Anal;
   CMem memAnal, memJoinCosts;
   memset (&Anal, 0, sizeof(Anal));

   // join costs into
   TTSJOINCOSTS JC;
   memset (&JC, 0, sizeof(JC));
   JC.pTarget = (PTTSTARGETCOSTS) m_memTTSTARGETCOSTS.p;

   // open the file
   FILE *pf = fopen ("c:\\temp\\JoinCosts.txt", "wt");
   //if (!pf) {
   //   EscMessageBox (hWnd, ASPString(),
   //      L"Analysis failed in JoinCosts.",
   //      L"Can't create c:\\temp\\JoinCosts.txt.",
   //      MB_ICONEXCLAMATION | MB_OK);
   //   return TRUE;
   //}

   {
      CProgress Progress;
      Progress.Start (hWnd, "Calculating target costs...", TRUE);

      if (!JoinCostsInit (pf, &memJoinCosts, &JC)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in JoinCosts.",
            L"JoinCostsInit failed.",
            MB_ICONEXCLAMATION | MB_OK);
         
         goto done;
      }

      Progress.Push (0, .1);
      if (!AnalysisInit (&Progress, &memAnal, &Anal)) {
         EscMessageBox (hWnd, ASPString(),
            L"Analysis failed in JoinCosts.",
            L"You may not have specified a lexicon to use.",
            MB_ICONEXCLAMATION | MB_OK);
         
         goto done;
      }
      Progress.Pop ();

      // pull out pitch from analysys
      tts.AvgPitchSet(Anal.fAvgPitch);
      tts.AvgSyllableDurSet (Anal.fAvgSyllableDur);
      tts.EnergyPerPitchSet (Anal.pacEnergyPerPitch);
      tts.EnergyPerVolumeSet (Anal.pacEnergyPerVolume);
      for (i = 0; i < NUMFUNCWORDGROUP; i++)
         tts.LexFuncWordsSet (m_apLexFuncWord[i], i);

// #define LASTPASSONLY       // to test - disable after recalc this part of join costs
      DWORD dwPass;
      Progress.Push (.1, 1.0);
#ifdef LASTPASSONLY
      for (dwPass = 6; dwPass < 7; dwPass++) {  // as per AnalysisJoinCostsSub
         Progress.Push (0.0, 1.0);
#else
      for (dwPass = 0; dwPass < 7; dwPass++) {  // as per AnalysisJoinCostsSub
         Progress.Push ((fp)dwPass / 7.0, (fp)(dwPass+1) / 7.0);
#endif

         Progress.Push (0, .3);
         if (!JoinCostsTrain (dwPass, &Progress, &Anal, &JC, &tts)) {
            EscMessageBox (hWnd, ASPString(),
               L"Analysis failed in JoinCosts.",
               L"Error in JoinCostsTrain().",
               MB_ICONEXCLAMATION | MB_OK);
            
            goto done;
         }
         Progress.Pop ();

         // NOTE: Average pitch is Anal.fAvgPitch

         // do multithreaded calculations
         Progress.Push (.3, 1.0);
         EMTCANALYSISPHONETRAIN emapt;
         memset (&emapt, 0, sizeof(emapt));
         emapt.dwType = 50;
         emapt.pAnal = &Anal;
         emapt.pJC = &JC;
         emapt.pTTS = &tts;
         emapt.dwPass = dwPass;
         ThreadLoop (0, Anal.plPCWaveAn->Num(), 16, &emapt, sizeof(emapt), &Progress);
         Progress.Pop ();

         // write out the results
         JoinCostsWrite (dwPass, &Anal, &JC, &tts);

         // free up bits
         JoinCostsFree (dwPass, &JC);

         Progress.Pop ();
      } //dwPass
      Progress.Pop();

   }


done:
   // done
   AnalysisFree (&Anal);
   JoinCostsFree ((DWORD)-1, &JC);

   // fclose (pf);
   
   return TRUE;
}



/*************************************************************************************
CTTSWork::JoinCostsInit - Initialize the analysis information.

inputs
   FILE              *pf - Text file to write to. Taken over and closed by JoinCostsFreed()
   PCMem             pmemJoinCosts - Memory to use for pAnal lists.
   PTTSJOINCOSTS        pJC - Fill this with analysis information
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::JoinCostsInit (FILE *pf, PCMem pmemJoinCosts, PTTSJOINCOSTS pJC)
{
   // so can calculate PSOLA
   JoinCalcPSOLASpread (&m_memJoinCalcPSOLASpread, HALFOCTAVEFORPCM);

   pJC->pfJoinCosts = pf;
   InitializeCriticalSection (&pJC->cs);

   // phonemes
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   // memset (pAnal, 0, sizeof(*pAnal)); BUGFIX - dont clear because already zeroed
   pJC->dwPhonemes = pLex->PhonemeNum();

   // loop over these and get info
   DWORD i;
   pJC->plJCPHONEMEINFO = new CListFixed;
   if (!pJC->plJCPHONEMEINFO)
      return FALSE;
   pJC->plJCPHONEMEINFO->Init (sizeof(JCPHONEMEINFO));
   PLEXPHONE plp;
   PLEXENGLISHPHONE ple;
   JCPHONEMEINFO jpi;
   memset (&jpi, 0, sizeof(jpi));
   for (i = 0; i < pLex->PhonemeNum(); i++) {
      plp = pLex->PhonemeGetUnsort (i);
      if (!plp)
         continue;
      ple = MLexiconEnglishPhoneGet (plp->bEnglishPhone);

      jpi.pszName = plp->szPhoneLong;
      WideCharToMultiByte (CP_ACP, 0, jpi.pszName, -1, jpi.szaName, sizeof(jpi.szaName), 0, 0);
      jpi.dwNoStress = plp->bStress ? (DWORD) plp->wPhoneOtherStress : (DWORD)-1;

      jpi.fVoiced = ple ? ((ple->dwCategory & PIC_VOICED) ? TRUE : FALSE) : FALSE;
      jpi.fPlosive = ple ? ((ple->dwCategory & PIC_PLOSIVE) ? TRUE : FALSE) : FALSE;
      jpi.dwGroup = ple ? PIS_FROMPHONEGROUP(ple->dwShape) : 0;
      jpi.dwMegaGroup = LexPhoneGroupToMega(jpi.dwGroup);

      pJC->plJCPHONEMEINFO->Add (&jpi);
   } // i

   // loop through phonemes and keep list of stressed versions
   PJCPHONEMEINFO pjpi = (PJCPHONEMEINFO) pJC->plJCPHONEMEINFO->Get(0);
   for (i = 0; i < pJC->plJCPHONEMEINFO->Num(); i++) {
      if (pjpi[i].dwNoStress != (DWORD)-1) {
         PJCPHONEMEINFO pNoStress = pjpi + pjpi[i].dwNoStress;
         if (pNoStress->dwWithStress >= sizeof(pNoStress->adwWithStress) / sizeof(pNoStress->adwWithStress[0]))
            continue;   // too many already. shouldnt happen
         pNoStress->adwWithStress[pNoStress->dwWithStress] = i;
         pNoStress->dwWithStress++;
      }
   } // i

   // where context indexing is
   pJC->dwContextIndexMegaStart = 0;
   pJC->dwContextIndexGroupStart = pJC->dwContextIndexMegaStart + PIS_PHONEMEGAGROUPNUM;
   // pJC->dwContextIndexUnstressedStart = pJC->dwContextIndexGroupStart + PIS_PHONEGROUPNUM;
   pJC->dwContextIndexStressedStart = pJC->dwContextIndexGroupStart + PIS_PHONEGROUPNUM; // pJC->dwContextIndexUnstressedStart + pJC->dwPhonemes;
   pJC->dwContextIndexNum = pJC->dwContextIndexStressedStart + pJC->dwPhonemes;

   // enough memory to store the words
   DWORD dwSize =
      // pitch
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCLINEARFIT) +

      // duration
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCLINEARFIT) +

      // energy
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCLINEARFIT) +

      // word position
      4 * pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCWORDPOS) +

      // connect
      pJC->dwPhonemes * pJC->dwPhonemes * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * pJC->dwPhonemes * sizeof(JCCONNECT) +

      // connect mid
      pJC->dwPhonemes * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * sizeof(JCCONNECT) +

      // context
      pJC->dwPhonemes * pJC->dwContextIndexNum * pJC->dwContextIndexNum * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCCONTEXT) +

      // function words
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCPHONETRAIN) +
      pJC->dwPhonemes * PHONEGROUPSQUARE * sizeof(JCLINEARFIT) +

      0
      ;

   if (!pmemJoinCosts->Required (dwSize))
      return FALSE;
   memset (pmemJoinCosts->p, 0, dwSize);

   pJC->paJCPHONETRAINPitch = (PJCPHONETRAIN)pmemJoinCosts->p;
   pJC->paJCLINEARFITPitch = (PJCLINEARFIT) (pJC->paJCPHONETRAINPitch + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCPHONETRAINDuration = (PJCPHONETRAIN)(pJC->paJCLINEARFITPitch + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCLINEARFITDuration = (PJCLINEARFIT) (pJC->paJCPHONETRAINDuration + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCPHONETRAINEnergy = (PJCPHONETRAIN)(pJC->paJCLINEARFITDuration + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCLINEARFITEnergy = (PJCLINEARFIT) (pJC->paJCPHONETRAINEnergy + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCPHONETRAINFunc = (PJCPHONETRAIN) (pJC->paJCLINEARFITEnergy + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCLINEARFITFunc = (PJCLINEARFIT) (pJC->paJCPHONETRAINFunc + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCPHONETRAINWordPos = (PJCPHONETRAIN) (pJC->paJCLINEARFITFunc + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCWORDPOSWordPos = (PJCWORDPOS) (pJC->paJCPHONETRAINWordPos + 4 * pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCPHONETRAINConnect = (PJCPHONETRAIN) (pJC->paJCWORDPOSWordPos + pJC->dwPhonemes * PHONEGROUPSQUARE);
   pJC->paJCCONNECTConnect = (PJCCONNECT) (pJC->paJCPHONETRAINConnect + pJC->dwPhonemes * pJC->dwPhonemes);
   pJC->paJCPHONETRAINConnectMid = (PJCPHONETRAIN) (pJC->paJCCONNECTConnect + pJC->dwPhonemes * pJC->dwPhonemes);
   pJC->paJCCONNECTConnectMid = (PJCCONNECT) (pJC->paJCPHONETRAINConnectMid + pJC->dwPhonemes);
   pJC->paJCPHONETRAINContext = (PJCPHONETRAIN) (pJC->paJCCONNECTConnectMid + pJC->dwPhonemes);
   pJC->paJCCONTEXTContext = (PJCCONTEXT) (pJC->paJCPHONETRAINContext + pJC->dwPhonemes * pJC->dwContextIndexNum * pJC->dwContextIndexNum);


   return TRUE;

}



/*************************************************************************************
CTTSWork::JoinCostsFree - Frees all the memory allocated into a PTTSJOINCOSTS structure

inputs
   DWORD             dwPass - Pass to use.
                        0 = pitch comparison
                        1 = energy
                        2 = duration
                        3 = position of unit within word
                        4 = arbitrary border connection
                        5 = context
                        -1 = all
   PTTSJOINCOSTS           pJC - Fill this with analysis information
*/
void CTTSWork::JoinCostsFree (DWORD dwPass, PTTSJOINCOSTS pJC)
{
   if (dwPass == (DWORD)-1)
      DeleteCriticalSection (&pJC->cs);

   // close the file
   if ((dwPass == (DWORD)-1) && pJC->pfJoinCosts)
      fclose (pJC->pfJoinCosts);

   if ((dwPass == (DWORD)-1) && pJC->plJCPHONEMEINFO)
      delete pJC->plJCPHONEMEINFO;

   DWORD i;
   if ( ((dwPass == 0) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINPitch)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCPHONETRAINPitch[i].pPhoneme) {
            delete pJC->paJCPHONETRAINPitch[i].pPhoneme;
            pJC->paJCPHONETRAINPitch[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 0) || (dwPass == (DWORD)-1)) && pJC->paJCLINEARFITPitch)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCLINEARFITPitch[i].plJCLINEARFITPOINT) {
            delete pJC->paJCLINEARFITPitch[i].plJCLINEARFITPOINT;
            pJC->paJCLINEARFITPitch[i].plJCLINEARFITPOINT = NULL;
         }

   if ( ((dwPass == 2) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINDuration)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCPHONETRAINDuration[i].pPhoneme) {
            delete pJC->paJCPHONETRAINDuration[i].pPhoneme;
            pJC->paJCPHONETRAINDuration[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 2) || (dwPass == (DWORD)-1)) && pJC->paJCLINEARFITDuration)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCLINEARFITDuration[i].plJCLINEARFITPOINT) {
            delete pJC->paJCLINEARFITDuration[i].plJCLINEARFITPOINT;
            pJC->paJCLINEARFITDuration[i].plJCLINEARFITPOINT = NULL;
         }

   if ( ((dwPass == 1) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINEnergy)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCPHONETRAINEnergy[i].pPhoneme) {
            delete pJC->paJCPHONETRAINEnergy[i].pPhoneme;
            pJC->paJCPHONETRAINEnergy[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 1) || (dwPass == (DWORD)-1)) && pJC->paJCLINEARFITEnergy)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCLINEARFITEnergy[i].plJCLINEARFITPOINT) {
            delete pJC->paJCLINEARFITEnergy[i].plJCLINEARFITPOINT;
            pJC->paJCLINEARFITEnergy[i].plJCLINEARFITPOINT = NULL;
         }

   if ( ((dwPass == 3) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINWordPos)
      for (i = 0; i < 4 * pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCPHONETRAINWordPos[i].pPhoneme) {
            delete pJC->paJCPHONETRAINWordPos[i].pPhoneme;
            pJC->paJCPHONETRAINWordPos[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 4) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINConnect)
      for (i = 0; i < pJC->dwPhonemes * pJC->dwPhonemes; i++)
         if (pJC->paJCPHONETRAINConnect[i].pPhoneme) {
            delete pJC->paJCPHONETRAINConnect[i].pPhoneme;
            pJC->paJCPHONETRAINConnect[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 4) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINConnectMid)
      for (i = 0; i < pJC->dwPhonemes; i++)
         if (pJC->paJCPHONETRAINConnectMid[i].pPhoneme) {
            delete pJC->paJCPHONETRAINConnectMid[i].pPhoneme;
            pJC->paJCPHONETRAINConnectMid[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 5) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINContext)
      for (i = 0; i < pJC->dwPhonemes * pJC->dwContextIndexNum * pJC->dwContextIndexNum; i++)
         if (pJC->paJCPHONETRAINContext[i].pPhoneme) {
            delete pJC->paJCPHONETRAINContext[i].pPhoneme;
            pJC->paJCPHONETRAINContext[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 6) || (dwPass == (DWORD)-1)) && pJC->paJCPHONETRAINFunc)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCPHONETRAINFunc[i].pPhoneme) {
            delete pJC->paJCPHONETRAINFunc[i].pPhoneme;
            pJC->paJCPHONETRAINFunc[i].pPhoneme = NULL;
         }

   if ( ((dwPass == 6) || (dwPass == (DWORD)-1)) && pJC->paJCLINEARFITFunc)
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
         if (pJC->paJCLINEARFITFunc[i].plJCLINEARFITPOINT) {
            delete pJC->paJCLINEARFITFunc[i].plJCLINEARFITPOINT;
            pJC->paJCLINEARFITFunc[i].plJCLINEARFITPOINT = NULL;
         }

   if (dwPass == (DWORD)-1)
      memset (pJC, 0, sizeof(*pJC));
}


/*************************************************************************************
CTTSWork::JoinCostsTrain - Train SR for JoinCosts calculations

inputs
   DWORD             dwPass - Pass to use.
                        0 = pitch comparison
                        1 = energy
                        2 = duration
                        3 = position of unit within word
                        4 = arbitrary border connection
                        5 = context
   PCProgressSocket  pProgress - Progress to show loading of waves
   PTTSANAL          pAnal - Fill this with analysis information
   PTTSJOINCOSTS     pJC - Join costs info
   PCMTTS            pTTS - TTS used to store pitch-range energy costs
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::JoinCostsTrain (DWORD dwPass, PCProgressSocket pProgress, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS)
{
   // loop through all phonemes and determine what the median SR score would be
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return FALSE;
   DWORD dwNumPhone = pLex->PhonemeNum();

   DWORD i, j, k;
   CSRAnal SRAnalVoiced, SRAnalUnvoiced;
   fp fMaxEnergy;
   PSRANALBLOCK psabVoiced, psabUnvoiced;
   SRANALBLOCK sabBorder[SUBPHONEPERPHONE];

   // loop through all phonemes and make a model from the phonemes of the same
   // type so get a better "average" phoneme
   BYTE bSilence;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());

   PJCPHONETRAIN pjpt;
   PJCPHONEMEINFO pjpi = (PJCPHONEMEINFO) pJC->plJCPHONEMEINFO->Get(0);
   DWORD dwJCPhoneNum = pJC->plJCPHONEMEINFO->Num ();

   // first, loop and figure out average duration and energy for the class
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      if ((dwPass == 1) || (dwPass == 2)) for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // left/right context
         DWORD dwGroupLeft = ((DWORD)ppa->bPhoneLeft < dwJCPhoneNum) ? pjpi[ppa->bPhoneLeft].dwGroup : 0 /* silence */;
         DWORD dwGroupRight = ((DWORD)ppa->bPhoneRight < dwJCPhoneNum) ? pjpi[ppa->bPhoneRight].dwGroup : 0 /* silence */;
         DWORD dwGroupIndex = dwGroupLeft * PIS_PHONEGROUPNUM + dwGroupRight;
         DWORD dwPhoneGroupIndex = (DWORD)ppa->bPhone * PHONEGROUPSQUARE + dwGroupIndex;

         if (dwPass == 2) {
            pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex].dwCount++;
            pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex].fValueSum += log((fp)max(1,ppa->dwDuration));
         }

         if (dwPass == 1) {
            pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex].dwCount++;
            pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex].fValueSum += log((fp)max(CLOSE,ppa->fEnergyAvg));
         }
      } // j
   } // i
   for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
      if ((dwPass == 2) && pJC->paJCPHONETRAINDuration[i].dwCount) {
         pJC->paJCPHONETRAINDuration[i].fValueSum = exp(pJC->paJCPHONETRAINDuration[i].fValueSum / (double)pJC->paJCPHONETRAINDuration[i].dwCount);
         pJC->paJCPHONETRAINDuration[i].dwCount = 0;  // to reset
      }

      if ((dwPass == 1) && pJC->paJCPHONETRAINEnergy[i].dwCount) {
         pJC->paJCPHONETRAINEnergy[i].fValueSum = exp(pJC->paJCPHONETRAINEnergy[i].fValueSum / (double)pJC->paJCPHONETRAINEnergy[i].dwCount);
         pJC->paJCPHONETRAINEnergy[i].dwCount = 0;  // to reset
      }
   } // i

   // average pitch
   fp fPitchMin = pAnal->fAvgPitch / sqrt(OCTAVESPERPITCHFILDELITY);
   fp fPitchMax = pAnal->fAvgPitch * sqrt(OCTAVESPERPITCHFILDELITY);

   // average energy
   fp fEnergyMin, fEnergyMax, fDurationMin, fDurationMax;

   // loop through and train
   for (i = 0; i < pAnal->plPCWaveAn->Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)pAnal->plPCWaveAn->Num());

      PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(i));

      // cache the entire wave since will be accessing it call
      PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);

      psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
      psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
         pwa->m_fAvgEnergyForVoiced, pTTS, 0);
      psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);

      // in each wave loop through all the phonemes
      PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
      for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
         if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
            continue;   // ignore silnce

         // left/right context
         DWORD dwGroupLeft = ((DWORD)ppa->bPhoneLeft < dwJCPhoneNum) ? pjpi[ppa->bPhoneLeft].dwGroup : 0 /* silence */;
         DWORD dwGroupRight = ((DWORD)ppa->bPhoneRight < dwJCPhoneNum) ? pjpi[ppa->bPhoneRight].dwGroup : 0 /* silence */;
         DWORD dwGroupIndex = dwGroupLeft * PIS_PHONEGROUPNUM + dwGroupRight;
         DWORD dwPhoneGroupIndex = (DWORD)ppa->bPhone * PHONEGROUPSQUARE + dwGroupIndex;

         // energy min/max
         fEnergyMin = pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex].fValueSum / sqrt(2.0); // allow energy range of 2.0
         fEnergyMax = pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex].fValueSum * sqrt(2.0);

         // duration min/max
         fDurationMin = pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex].fValueSum / sqrt(SCALEPERDURATIONFIDELITY);
         fDurationMax = pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex].fValueSum * sqrt(SCALEPERDURATIONFIDELITY);

         // if this is within the realms of average pitch then train that
         if ((dwPass == 0) &&(ppa->fPitch >= fPitchMin) && (ppa->fPitch <= fPitchMax)) {
            pjpt = &pJC->paJCPHONETRAINPitch[dwPhoneGroupIndex];
            if (!pjpt->pPhoneme) {
               pjpt->pPhoneme = new CPhoneme;
               if (!pjpt->pPhoneme)
                  continue;   // error
            }

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            if (!pjpt->pPhoneme->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               1.0 ))
               continue;
            
            pjpt->dwCount++;
         }

         // if this is within the realms of average energy then train that
         if ((dwPass == 1) && (ppa->fEnergyAvg >= fEnergyMin) && (ppa->fEnergyAvg <= fEnergyMax)) {
            pjpt = &pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex];
            if (!pjpt->pPhoneme) {
               pjpt->pPhoneme = new CPhoneme;
               if (!pjpt->pPhoneme)
                  continue;   // error
            }

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            if (!pjpt->pPhoneme->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               1.0 ))
               continue;
            
            pjpt->dwCount++;
         }


         // if this is within the realms of average Duration then train that
         if ((dwPass == 2) && ((fp)ppa->dwDuration >= fDurationMin) && ((fp)ppa->dwDuration <= fDurationMax)) {
            pjpt = &pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex];
            if (!pjpt->pPhoneme) {
               pjpt->pPhoneme = new CPhoneme;
               if (!pjpt->pPhoneme)
                  continue;   // error
            }

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            if (!pjpt->pPhoneme->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               1.0 ))
               continue;
            
            pjpt->dwCount++;
         }

         // word position train
         if (dwPass == 3) {
            DWORD dwWordPosIndex = (DWORD) (ppa->bWordPos & 0x03) * pJC->dwPhonemes * PHONEGROUPSQUARE +
               dwPhoneGroupIndex;
            pjpt = &pJC->paJCPHONETRAINWordPos[dwWordPosIndex];
            if (!pjpt->pPhoneme) {
               pjpt->pPhoneme = new CPhoneme;
               if (!pjpt->pPhoneme)
                  continue;   // error
            }

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            if (!pjpt->pPhoneme->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               1.0 ))
               continue;
            pjpt->dwCount++;
         }

         // train connect
         if (dwPass == 4) {
            DWORD dwConnectIndex = min((DWORD)ppa->bPhone, pJC->dwPhonemes-1) * pJC->dwPhonemes +
               min((DWORD)ppa->bPhoneRight, pJC->dwPhonemes-1);
            if (ppa->dwTimeEnd < pwa->m_pWave->m_dwSRSamples) {
               // create hack phoneme starting just after the end
               for (k = 0; k < SUBPHONEPERPHONE; k++)
                  sabBorder[k] = (ppa->fIsVoiced ? psabVoiced : psabUnvoiced)[ppa->dwTimeEnd];

               pjpt = &pJC->paJCPHONETRAINConnect[dwConnectIndex];
               if (!pjpt->pPhoneme) {
                  pjpt->pPhoneme = new CPhoneme;
                  if (!pjpt->pPhoneme)
                     continue;   // error
               }

               if (!pjpt->pPhoneme->Train (
                  sabBorder,
                  SUBPHONEPERPHONE,
                  pwa->m_fMaxEnergy,
                  (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
                  1.0 ))
                  continue;
               
               pjpt->dwCount++;
            }

            // mid-phone
            dwConnectIndex = min((DWORD)ppa->bPhone, pJC->dwPhonemes-1);
            DWORD dwMid = (ppa->dwTimeStart + ppa->dwTimeEnd) / 2;
            if (dwMid < pwa->m_pWave->m_dwSRSamples) {
               // create hack phoneme starting just after the end
               for (k = 0; k < SUBPHONEPERPHONE; k++)
                  sabBorder[k] = (ppa->fIsVoiced ? psabVoiced : psabUnvoiced)[dwMid];

               pjpt = &pJC->paJCPHONETRAINConnectMid[dwConnectIndex];
               if (!pjpt->pPhoneme) {
                  pjpt->pPhoneme = new CPhoneme;
                  if (!pjpt->pPhoneme)
                     continue;   // error
               }

               if (!pjpt->pPhoneme->Train (
                  sabBorder,
                  SUBPHONEPERPHONE,
                  pwa->m_fMaxEnergy,
                  (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
                  1.0 ))
                  continue;
               
               pjpt->dwCount++;
            }
         }

         // train context
         if (dwPass == 5) {
            DWORD adwGroupCon[2][3];
            DWORD dwRight, dwLeft;
            for (dwRight = 0; dwRight < 2; dwRight++) {
               DWORD dwPhone = dwRight ? (DWORD)ppa->bPhoneRight : (DWORD)ppa->bPhoneLeft;
               if (dwPhone < dwJCPhoneNum) {
                  adwGroupCon[dwRight][0] = dwPhone;  // stressed
                  //adwGroup[dwRight][1] = (pjpi[dwPhone].dwNoStress != (DWORD)-1) ? pjpi[dwPhone].dwNoStress : dwPhone;  // unstressed
                  adwGroupCon[dwRight][1] = pjpi[dwPhone].dwGroup;  // since 0 is always silence phone
                  adwGroupCon[dwRight][2] = pjpi[dwPhone].dwMegaGroup;
               }
               else {
                  // silence
                  adwGroupCon[dwRight][0] = pJC->dwPhonemes - 1;  // stressed
                  //adwGroup[dwRight][1] = pJC->dwPhonemes - 1;  // unstressed
                  adwGroupCon[dwRight][1] = 0;  // since 0 is always silence phone
                  adwGroupCon[dwRight][2] = LexPhoneGroupToMega(adwGroupCon[dwRight][1]);
               }

               adwGroupCon[dwRight][0] += pJC->dwContextIndexStressedStart;
               //adwGroup[dwRight][1] += pJC->dwContextIndexUnstressedStart;
               adwGroupCon[dwRight][1] += pJC->dwContextIndexGroupStart;
               adwGroupCon[dwRight][2] += pJC->dwContextIndexMegaStart;
            } // dwRight
            for (dwLeft = 0; dwLeft < 3; dwLeft++) for (dwRight = 0; dwRight < 3; dwRight++) {
               pjpt = &pJC->paJCPHONETRAINContext[
                  (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
                  adwGroupCon[0][dwLeft] * pJC->dwContextIndexNum +
                  adwGroupCon[1][dwRight]
                  ];
               if (!pjpt->pPhoneme) {
                  pjpt->pPhoneme = new CPhoneme;
                  if (!pjpt->pPhoneme)
                     continue;   // error
               }

               DWORD dwStart, dwLength;
               DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

               if (!pjpt->pPhoneme->Train (
                  (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                  dwLength,
                  pwa->m_fMaxEnergy,
                  (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
                  1.0 ))
                  continue;
               pjpt->dwCount++;
            } // dwLeft, dwRight
         } // dwPass

         // dwPass == 6
         // if this is within the realms of Func word then train that
         if ((dwPass == 6) && ppa->pWord && (ppa->pWord->dwFuncWordGroup < NUMFUNCWORDGROUP)) {
            pjpt = &pJC->paJCPHONETRAINFunc[dwPhoneGroupIndex];
            if (!pjpt->pPhoneme) {
               pjpt->pPhoneme = new CPhoneme;
               if (!pjpt->pPhoneme)
                  continue;   // error
            }

            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            if (!pjpt->pPhoneme->Train (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               pwa->m_fMaxEnergy,
               (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
               1.0 ))
               continue;
            
            pjpt->dwCount++;
         }
      } // j

      // release the SR features so don't use too much memory
      pwa->m_pWave->ReleaseSRFeatures();
   } // i


   // make sure all the phonemes are trained

   // loop through all the phonemes and make sure safe for multithreded
   for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
      if (pJC->paJCPHONETRAINPitch[i].pPhoneme)
         pJC->paJCPHONETRAINPitch[i].pPhoneme->PrepForMultiThreaded ();

   for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
      if (pJC->paJCPHONETRAINDuration[i].pPhoneme)
         pJC->paJCPHONETRAINDuration[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
      if (pJC->paJCPHONETRAINEnergy[i].pPhoneme)
         pJC->paJCPHONETRAINEnergy[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < 4 * pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
      if (pJC->paJCPHONETRAINWordPos[i].pPhoneme)
         pJC->paJCPHONETRAINWordPos[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < pJC->dwPhonemes * pJC->dwPhonemes; i++)
      if (pJC->paJCPHONETRAINConnect[i].pPhoneme)
         pJC->paJCPHONETRAINConnect[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < pJC->dwPhonemes; i++)
      if (pJC->paJCPHONETRAINConnectMid[i].pPhoneme)
         pJC->paJCPHONETRAINConnectMid[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < pJC->dwPhonemes * pJC->dwContextIndexNum * pJC->dwContextIndexNum; i++)
      if (pJC->paJCPHONETRAINContext[i].pPhoneme)
         pJC->paJCPHONETRAINContext[i].pPhoneme->PrepForMultiThreaded ();;

   for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++)
      if (pJC->paJCPHONETRAINFunc[i].pPhoneme)
         pJC->paJCPHONETRAINFunc[i].pPhoneme->PrepForMultiThreaded ();

   return TRUE;

}



/*************************************************************************************
SRFEATURESimulatePCMPitch - Simulates PCM pitch shift with SRFEATURE

inputs
   PSRFEATURE     pSRFOrig - Orignal, not modified
   PSRFEATURE     pSRFNew - New, modified
   fp             fOctaves - Number of octaves to rainse new over orig
returns
   none
*/
void SRFEATURESimulatePCMPitch (PSRFEATURE pSRFOrig, PSRFEATURE pSRFNew, fp fOctaves)
{
   memcpy (pSRFNew, pSRFOrig, sizeof(*pSRFOrig));

   int iDelta = (int)(fOctaves * (fp)SRPOINTSPEROCTAVE);
   int iDbDelta = (int)(6.0 /* 6 dB per octave */ * fOctaves);
      // faster then using SRDATAPOINTS/2 as used elsewhere

   DWORD i;
   DWORD dwVoiced;
   for (dwVoiced = 0; dwVoiced < 2; dwVoiced++) {
      char *pacOrig = dwVoiced ? &pSRFOrig->acVoiceEnergy[0] : &pSRFOrig->acNoiseEnergy[0];
      char *pacNew = dwVoiced ? &pSRFNew->acVoiceEnergy[0] : &pSRFNew->acNoiseEnergy[0];

      for (i = 0; i < SRDATAPOINTS; i++) {
         int iOrig = (int)i - iDelta;
         int iDb;
         iOrig = max(iOrig, 0);
         if (iOrig < SRDATAPOINTS)
            iDb = pacOrig[iOrig];
         else
            iDb = SRABSOLUTESILENCE;

         iDb += iDbDelta;

         iDb = max(iDb, SRABSOLUTESILENCE);
         iDb = min(iDb, SRMAXLOUDNESS);

         pacNew[i] = (char)iDb;
      } // i
   } // dwVoiced
}


/*************************************************************************************
SRFEATURESimulatePCMPitch - Simulates PCM pitch shift over an entire wave.

inputs
   PSRFEATURE        paSRF - Array of features
   DWORD             dwNum - Number of entries in paSRF
   PCMem             pMem - Memory to fill in with shifted wave
   fp             fOctaves - Number of octaves to rainse new over orig
returns
   BOOL - TRUE if success
*/
BOOL SRFEATURESimluatePCMPitch (PSRFEATURE paSRF, DWORD dwNum, PCMem pMem, fp fOctaves)
{
   if (!pMem->Required (dwNum * sizeof(SRFEATURE)))
      return FALSE;

   PSRFEATURE paSRFDest = (PSRFEATURE) pMem->p;

   DWORD i;
   for (i = 0; i < dwNum; i++, paSRF++, paSRFDest++)
      SRFEATURESimulatePCMPitch (paSRF, paSRFDest, fOctaves);

   return TRUE;
}


/*************************************************************************************
CTTSWork::AnalysisJoinCostsSub - Sub-training of indiviual wave.

inputs
   DWORD             dwPass - Pass to use.
                        0 = pitch comparison
                        1 = energy
                        2 = duration
                        3 = position of unit within word
                        4 = arbitrary border connection
                        5 = context
   PTTSANAL          pAnal - Fill this with analysis information
   PTTSJOINCOSTS     pJC - Join costs info
   PCMTTS            pTTS - TTS engine
   DWORD             dwWave - Wave index
   DWORD             dwThread - 0..MAXRAYTHREAD-1

*/
#define JOINCOSTCOMPARENOTWIDE            FALSE       // set to FALSE for wide comparison, TRUE for narrow comparison


void CTTSWork::AnalysisJoinCostsSub (DWORD dwPass, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS, DWORD dwWave, DWORD dwThread)
{
   SRANALBLOCK sabBorder[SUBPHONEPERPHONE];
   CSRAnal SRAnalVoiced, SRAnalUnvoiced, SRAnalVoicedHigher, SRAnalVoicedLower, SRAnalUnvoicedHigher, SRAnalUnvoicedLower;
   CSRAnal SRAnalVoicedPSOLAHigher, SRAnalVoicedPSOLALower, SRAnalUnvoicedPSOLAHigher, SRAnalUnvoicedPSOLALower, SRAnalVoicedPSOLASame, SRAnalUnvoicedPSOLASame;
   CMem memHalfOctaveHigher, memHalfOctaveLower, memSameOctave;
   fp fMaxEnergy, fMaxEnergyHigher, fMaxEnergyLower, fMaxEnergyPSOLAHigher, fMaxEnergyPSOLALower, fMaxEnergyPSOLASame;
   PSRANALBLOCK psabVoiced, psabUnvoiced, psabVoicedHigher, psabVoicedLower, psabUnvoicedHigher, psabUnvoicedLower;
   PSRANALBLOCK psabVoicedPSOLAHigher, psabVoicedPSOLALower, psabUnvoicedPSOLAHigher, psabUnvoicedPSOLALower, psabVoicedPSOLASame, psabUnvoicedPSOLASame;
   DWORD j, k;
   BYTE bSilence;
   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return;
   bSilence = pLex->PhonemeFindUnsort(pLex->PhonemeSilence());


   PCWaveAn pwa = *((PCWaveAn*)pAnal->plPCWaveAn->Get(dwWave));

   // cache the entire wave since will be accessing it call
   PSRFEATURE psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, FALSE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);

   //psab = SRAnal.Init (psrCache, pwa->m_pWave->m_dwSRSamples, &fMaxEnergy);
   psabUnvoiced = SRAnalUnvoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);

   // if doing pitch comparison then simulate raising/lowering by half octave
   if (dwPass == 0) {
      // PSOLA
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveHigher, 1);
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveLower, 0);
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memSameOctave, -1);
      psabUnvoicedPSOLAHigher = SRAnalUnvoicedPSOLAHigher.Init ((PSRFEATURE)memHalfOctaveHigher.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLAHigher);
      psabUnvoicedPSOLALower = SRAnalUnvoicedPSOLALower.Init ((PSRFEATURE)memHalfOctaveLower.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLALower);
      psabUnvoicedPSOLASame = SRAnalUnvoicedPSOLASame.Init ((PSRFEATURE)memSameOctave.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLASame);

      // pitch synchornous PCM
      SRFEATURESimluatePCMPitch (psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveHigher, HALFOCTAVEFORPCM);
      SRFEATURESimluatePCMPitch (psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveLower, -HALFOCTAVEFORPCM);
      psabUnvoicedHigher = SRAnalUnvoicedHigher.Init ((PSRFEATURE)memHalfOctaveHigher.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyHigher);
      psabUnvoicedLower = SRAnalUnvoicedLower.Init ((PSRFEATURE)memHalfOctaveLower.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyLower);
   }

   psrCache = CacheSRFeaturesWithAdjust (pwa->m_pWave, 0, pwa->m_pWave->m_dwSRSamples, TRUE,
      pwa->m_fAvgEnergyForVoiced, pTTS, dwThread);
   psabVoiced = SRAnalVoiced.Init (psrCache, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergy);

   // if doing pitch comparison then simulate raising/lowering by half octave
   if (dwPass == 0) {
      // PSOLA
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveHigher, 1);
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveLower, 0);
      JoinSimulatePSOLAInWave (pwa->m_pWave, psrCache, pwa->m_pWave->m_dwSRSamples, &memSameOctave, -1);
      psabVoicedPSOLAHigher = SRAnalVoicedPSOLAHigher.Init ((PSRFEATURE)memHalfOctaveHigher.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLAHigher);
      psabVoicedPSOLALower = SRAnalVoicedPSOLALower.Init ((PSRFEATURE)memHalfOctaveLower.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLALower);
      psabVoicedPSOLASame = SRAnalVoicedPSOLASame.Init ((PSRFEATURE)memSameOctave.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyPSOLASame);

      // pitch synchornous PCM
      SRFEATURESimluatePCMPitch (psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveHigher, HALFOCTAVEFORPCM);
      SRFEATURESimluatePCMPitch (psrCache, pwa->m_pWave->m_dwSRSamples, &memHalfOctaveLower, -HALFOCTAVEFORPCM);
      psabVoicedHigher = SRAnalVoicedHigher.Init ((PSRFEATURE)memHalfOctaveHigher.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyHigher);
      psabVoicedLower = SRAnalVoicedLower.Init ((PSRFEATURE)memHalfOctaveLower.p, pwa->m_pWave->m_dwSRSamples, FALSE, &fMaxEnergyLower);
   }

   PJCPHONETRAIN pjpt;
   PJCPHONEMEINFO pjpi = (PJCPHONEMEINFO) pJC->plJCPHONEMEINFO->Get(0);
   DWORD dwJCPhoneNum = pJC->plJCPHONEMEINFO->Num ();
   fp fScore, fScoreHigher, fScoreLower, fScorePSOLAHigher, fScorePSOLALower, fScorePSOLASame;
   JCLINEARFITPOINT jcdp;
   DWORD dwGroup;


   // in each wave loop through all the phonemes
   PPHONEAN ppa = (PPHONEAN) pwa->m_lPHONEAN.Get(0);
   CListFixed lJustCompared;
   lJustCompared.Init (sizeof(DWORD));
   for (j = 0; j < pwa->m_lPHONEAN.Num(); j++, ppa++) {
      if ((ppa->bPhone == bSilence) || (ppa->dwTimeEnd <= ppa->dwTimeStart))
         continue;   // ignore silnce

      dwGroup = pLex->PhonemeToGroup (ppa->bPhone);

      // left/right context
      DWORD dwGroupLeft = ((DWORD)ppa->bPhoneLeft < dwJCPhoneNum) ? pjpi[ppa->bPhoneLeft].dwGroup : 0 /* silence */;
      DWORD dwGroupRight = ((DWORD)ppa->bPhoneRight < dwJCPhoneNum) ? pjpi[ppa->bPhoneRight].dwGroup : 0 /* silence */;
      DWORD dwGroupIndex = dwGroupLeft * PIS_PHONEGROUPNUM + dwGroupRight;
      DWORD dwPhoneGroupIndex = (DWORD)ppa->bPhone * PHONEGROUPSQUARE + dwGroupIndex;


      // see how this compares with pitch
      pjpt = &pJC->paJCPHONETRAINPitch[dwPhoneGroupIndex];
      if ((dwPass == 0) && pjpt->pPhoneme) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         fScore = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // just PCM
         fScoreHigher = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoicedHigher : psabUnvoicedHigher) + dwStart,
            dwLength,
            fMaxEnergyHigher,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
         fScoreLower = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoicedLower : psabUnvoicedLower) + dwStart,
            dwLength,
            fMaxEnergyLower,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);

         // PSOLA
         fScorePSOLASame = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoicedPSOLASame : psabUnvoicedPSOLASame) + dwStart,
            dwLength,
            fMaxEnergyPSOLASame,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
         fScorePSOLAHigher = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoicedPSOLAHigher : psabUnvoicedPSOLAHigher) + dwStart,
            dwLength,
            fMaxEnergyPSOLAHigher,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
         fScorePSOLALower = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoicedPSOLALower : psabUnvoicedPSOLALower) + dwStart,
            dwLength,
            fMaxEnergyPSOLALower,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);

         // add this to the list
         EnterCriticalSection (&pJC->cs);

         // how much going higher/lower PCM-wise does
         pJC->JCPITCHPCMHigher.afScore[dwGroup] += (fScoreHigher - fScore);
         pJC->JCPITCHPCMLower.afScore[dwGroup] += (fScoreLower - fScore);
         pJC->JCPITCHPCMPSOLAHigher.afScore[dwGroup] += (fScorePSOLAHigher - fScorePSOLASame); // NOTE: Was fScore, but to/from SRFEATURE causes minor changes
         pJC->JCPITCHPCMPSOLALower.afScore[dwGroup] += (fScorePSOLALower - fScorePSOLASame); // NOTE: Was fScore
         pJC->JCPITCHPCMHigher.adwCount[dwGroup]++;
         pJC->JCPITCHPCMLower.adwCount[dwGroup]++;
         pJC->JCPITCHPCMPSOLAHigher.adwCount[dwGroup]++;
         pJC->JCPITCHPCMPSOLALower.adwCount[dwGroup]++;

         if (!pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT) {
            pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT = new CListFixed;
            if (pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT)
               pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT->Init (sizeof(JCLINEARFITPOINT));
         }
         jcdp.fX = log(max(ppa->fPitch,1.0) / pAnal->fAvgPitch) / log(2.0);
         jcdp.fY = fScore;
         if (pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT)
            pJC->paJCLINEARFITPitch[dwPhoneGroupIndex].plJCLINEARFITPOINT->Add (&jcdp);
         LeaveCriticalSection (&pJC->cs);
      }


      // see how this compares with energy
      pjpt = &pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex];
      if ((dwPass == 1) && pjpt->pPhoneme) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         fScore = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         EnterCriticalSection (&pJC->cs);
         if (!pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT) {
            pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT = new CListFixed;
            if (pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT)
               pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT->Init (sizeof(JCLINEARFITPOINT));
         }
         jcdp.fX = log(max(ppa->fEnergyAvg,CLOSE) / max(pJC->paJCPHONETRAINEnergy[dwPhoneGroupIndex].fValueSum, CLOSE)) / log(2.0);
         jcdp.fY = fScore;
         if (pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT)
            pJC->paJCLINEARFITEnergy[dwPhoneGroupIndex].plJCLINEARFITPOINT->Add (&jcdp);
         LeaveCriticalSection (&pJC->cs);
      }


      // see how this compares with Duration
      pjpt = &pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex];
      if ((dwPass == 2) && pjpt->pPhoneme) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         fScore = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         EnterCriticalSection (&pJC->cs);
         if (!pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT) {
            pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT = new CListFixed;
            if (pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT)
               pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT->Init (sizeof(JCLINEARFITPOINT));
         }
         jcdp.fX = log((fp)max(ppa->dwDuration,1) / max(pJC->paJCPHONETRAINDuration[dwPhoneGroupIndex].fValueSum, CLOSE)) / log(2.0);
         jcdp.fY = fScore;
         if (pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT)
            pJC->paJCLINEARFITDuration[dwPhoneGroupIndex].plJCLINEARFITPOINT->Add (&jcdp);
         LeaveCriticalSection (&pJC->cs);
      }

      // compare this against 4 different word positions for same unit
      if (dwPass == 3) for (k = 0; k < 4; k++) {
         pjpt = &pJC->paJCPHONETRAINWordPos[k * pJC->dwPhonemes * PHONEGROUPSQUARE + dwPhoneGroupIndex];
         if (!pjpt->pPhoneme)
            continue;   // nothing

         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         fScore = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         DWORD dwXOR = k ^ (DWORD) (ppa->bWordPos & 0x03);
         EnterCriticalSection (&pJC->cs);
         pJC->paJCWORDPOSWordPos[dwPhoneGroupIndex].afScore[dwXOR] += fScore;
         pJC->paJCWORDPOSWordPos[dwPhoneGroupIndex].adwCount[dwXOR]++;
         LeaveCriticalSection (&pJC->cs);
      } // k
      // end dwPass == 3

      

      // compare against arbitrary connection
      DWORD dwConnectIndex = min((DWORD)ppa->bPhone, pJC->dwPhonemes-1) * pJC->dwPhonemes +
         min((DWORD)ppa->bPhoneRight, pJC->dwPhonemes-1);
      pjpt = &pJC->paJCPHONETRAINConnect[dwConnectIndex];
      if ((dwPass == 4) && pjpt->pPhoneme) {
         // create hack phoneme starting just after the end
         for (k = 0; k < SUBPHONEPERPHONE; k++)
            sabBorder[k] = (ppa->fIsVoiced ? psabVoiced : psabUnvoiced)[ppa->dwTimeEnd-1];

         fScore = pjpt->pPhoneme->Compare (
            sabBorder,
            SUBPHONEPERPHONE,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         EnterCriticalSection (&pJC->cs);
         pJC->paJCCONNECTConnect[dwConnectIndex].fScore += fScore;
         pJC->paJCCONNECTConnect[dwConnectIndex].dwCount++;
         LeaveCriticalSection (&pJC->cs);
      }
      // mid
      dwConnectIndex = min((DWORD)ppa->bPhone, pJC->dwPhonemes-1);
      pjpt = &pJC->paJCPHONETRAINConnectMid[dwConnectIndex];
      if ((dwPass == 4) && pjpt->pPhoneme) {
         // create hack phoneme starting just after the end
         for (k = 0; k < SUBPHONEPERPHONE; k++)
            sabBorder[k] = (ppa->fIsVoiced ? psabVoiced : psabUnvoiced)[(ppa->dwTimeStart+ppa->dwTimeEnd)/2];

         fScore = pjpt->pPhoneme->Compare (
            sabBorder,
            SUBPHONEPERPHONE,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         EnterCriticalSection (&pJC->cs);
         pJC->paJCCONNECTConnectMid[dwConnectIndex].fScore += fScore;
         pJC->paJCCONNECTConnectMid[dwConnectIndex].dwCount++;
         LeaveCriticalSection (&pJC->cs);
      }
      // end dwPass=4

      // train the context
      DWORD adwGroupCon[2][3];
      DWORD dwRight;
      PJCPHONEMEINFO pjpiCur = ((DWORD)ppa->bPhone < dwJCPhoneNum) ? &pjpi[ppa->bPhone] : NULL;
      for (dwRight = 0; dwRight < 2; dwRight++) {
         DWORD dwPhone = dwRight ? (DWORD)ppa->bPhoneRight : (DWORD)ppa->bPhoneLeft;
         if (dwPhone < dwJCPhoneNum) {
            adwGroupCon[dwRight][0] = dwPhone;  // stressed
            // adwGroup[dwRight][1] = (pjpi[dwPhone].dwNoStress != (DWORD)-1) ? pjpi[dwPhone].dwNoStress : dwPhone;  // unstressed
            adwGroupCon[dwRight][1] = pjpi[dwPhone].dwGroup;  // since 0 is always silence phone
            adwGroupCon[dwRight][2] = pjpi[dwPhone].dwMegaGroup;
         }
         else {
            // silence
            adwGroupCon[dwRight][0] = pJC->dwPhonemes - 1;  // stressed
            // adwGroup[dwRight][1] = pJC->dwPhonemes - 1;  // unstressed
            adwGroupCon[dwRight][1] = 0;  // since 0 is always silence phone
            adwGroupCon[dwRight][2] = LexPhoneGroupToMega(adwGroupCon[dwRight][1]);
         }

         adwGroupCon[dwRight][0] += pJC->dwContextIndexStressedStart;
         //adwGroup[dwRight][1] += pJC->dwContextIndexUnstressedStart;
         adwGroupCon[dwRight][1] += pJC->dwContextIndexGroupStart;
         adwGroupCon[dwRight][2] += pJC->dwContextIndexMegaStart;
      } // dwRight
      PJCCONTEXT pjcc = pJC->paJCCONTEXTContext + dwPhoneGroupIndex;

      if ((dwPass == 5) && pjpiCur) {
         // BUGFIX - Find out this SRScore compared to itself
         CPhoneme PhoneBase;
         double fScoreBase = 0.0;
         BOOL fCalcScoreBase = FALSE;

         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         if (PhoneBase.Train (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR)pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, TRUE,
            1.0 )) {

               fScoreBase = PhoneBase.Compare (
                  (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                  dwLength,
                  fMaxEnergy,
                  (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
                  FALSE /* slow */, FALSE /* all examplars */);

               fCalcScoreBase = TRUE;
         }

         double fScoreSameTriphone = 0.0;
         BOOL fCalcScoreSameTriphone = FALSE;
         DWORD dwSameTriphoneCount = 0;
         // compare stressed versions
         pjpt = &pJC->paJCPHONETRAINContext[
            (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
            adwGroupCon[0][0] * pJC->dwContextIndexNum +
            adwGroupCon[1][0]
            ];
         if (pjpt->pPhoneme) {
            DWORD dwStart, dwLength;
            DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

            fScoreSameTriphone = pjpt->pPhoneme->Compare (
               (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
               dwLength,
               fMaxEnergy,
               (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
               FALSE /* slow */, FALSE /* all examplars */);

                           // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

            fCalcScoreSameTriphone = TRUE;
            dwSameTriphoneCount= pjpt->dwCount;
         }

         _ASSERTE (fCalcScoreBase == fCalcScoreSameTriphone);

         for (dwRight = 0; dwRight < 2; dwRight++) {
            DWORD dwPhoneVary = dwRight ? (DWORD)ppa->bPhoneRight : (DWORD)ppa->bPhoneLeft;

            if (fCalcScoreBase || fCalcScoreSameTriphone) {
               EnterCriticalSection (&pJC->cs);
               // score base
               if (fCalcScoreBase) {
                  pjcc->afScoreCon[dwRight][0] += fScoreBase; // so know what this is
                  pjcc->adwCountCon[dwRight][0]++;
               }

               if (fCalcScoreSameTriphone) {
                  _ASSERTE (dwSameTriphoneCount);
                  pjcc->afScoreCon[dwRight][1] += (fScoreSameTriphone-fScoreBase) * (double) dwSameTriphoneCount;
                  pjcc->adwCountCon[dwRight][1] += dwSameTriphoneCount;
               }
               LeaveCriticalSection (&pJC->cs);
            }


            // compare unstressed versions, if this phoneme can be unstressed
            // there are stressed and unstressed versions of this phoneme, so find the ones that DON'T match
            // this and add

            DWORD dwPhoneMaster = dwPhoneVary;
            PJCPHONEMEINFO pjMaster = (dwPhoneMaster < dwJCPhoneNum) ? &pjpi[dwPhoneMaster] : NULL;
            if (pjMaster && (pjMaster->dwNoStress != (DWORD)-1)) {
               dwPhoneMaster = pjMaster->dwNoStress;
               pjMaster = &pjpi[pjMaster->dwNoStress];
            }
            lJustCompared.Clear();
            double fScoreJust = 0.0;
            DWORD dwCountJust = 0;
            if (pjMaster) for (k = 0; k <= pjMaster->dwWithStress; k++) { // intentionall <=
               DWORD dwPhoneNot = (k < pjMaster->dwWithStress) ? pjMaster->adwWithStress[k] : dwPhoneMaster;
               if (dwPhoneNot == dwPhoneVary)
                  continue;   // don't compare against self

               pjpt = &pJC->paJCPHONETRAINContext[
                  (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
                  (!dwRight ? (dwPhoneNot + pJC->dwContextIndexStressedStart) : adwGroupCon[0][0]) * pJC->dwContextIndexNum +
                  (dwRight ? (dwPhoneNot + pJC->dwContextIndexStressedStart) : adwGroupCon[1][0])
                  ];
               lJustCompared.Add (&dwPhoneNot);
               if (pjpt->pPhoneme) {
                  DWORD dwStart, dwLength;
                  DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

                  fScore = pjpt->pPhoneme->Compare (
                     (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                     dwLength,
                     fMaxEnergy,
                     (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
                     FALSE /* slow */, FALSE /* all examplars */);
                                 // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

                  fScoreJust += (fScore - fScoreSameTriphone) * (double)pjpt->dwCount;
                  dwCountJust += pjpt->dwCount;
               }
            } // k
            EnterCriticalSection (&pJC->cs);
            pjcc->afScoreCon[dwRight][2] += fScoreJust;
            pjcc->adwCountCon[dwRight][2] += dwCountJust;
            LeaveCriticalSection (&pJC->cs);

            // BUGFIX - Know it's not an unstressed verison
            fScoreJust = 0;
            dwCountJust = 0;

            // compare against phonemes in the same group, but not this one
            DWORD *padwJustCompared = (DWORD*)lJustCompared.Get(0);
            DWORD m;
            if (dwPhoneVary < dwJCPhoneNum) for (k = 0; k < dwJCPhoneNum; k++) {
               // must be in the same group
               if (pjpi[dwPhoneVary].dwGroup != pjpi[k].dwGroup)
                  continue;

               if (k == dwPhoneVary)
                  continue;   // don't compare against self

               // must be an unstressed
               // Don't do since removing one of the training groups if (pjpi[k].dwNoStress != (DWORD)-1)
               //   continue;

               for (m = 0; m < lJustCompared.Num(); m++)
                  if (padwJustCompared[m] == k)
                     break;
               if (m < lJustCompared.Num())
                  continue;   // already done

               // if get here, the in the group, but haven't compared against

               // compare against this
               pjpt = &pJC->paJCPHONETRAINContext[
                  (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
                  (!dwRight ? (k + pJC->dwContextIndexStressedStart) : adwGroupCon[0][0]) * pJC->dwContextIndexNum +
                  (dwRight ? (k + pJC->dwContextIndexStressedStart) : adwGroupCon[1][0])
                  ];
               if (pjpt->pPhoneme) {
                  DWORD dwStart, dwLength;
                  DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

                  fScore = pjpt->pPhoneme->Compare (
                     (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                     dwLength,
                     fMaxEnergy,
                     (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
                     FALSE /* slow */, FALSE /* all examplars */);
                                 // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

                  fScoreJust += (fScore - fScoreSameTriphone) * (double)pjpt->dwCount;
                  dwCountJust += pjpt->dwCount;
               }
            } // k
            EnterCriticalSection (&pJC->cs);
            pjcc->afScoreCon[dwRight][3] += fScoreJust;
            pjcc->adwCountCon[dwRight][3] += dwCountJust;

            LeaveCriticalSection (&pJC->cs);

            // BUGFIX - Know it's not in the same megagroup
            fScoreJust = 0;
            dwCountJust = 0;

            // loop over groups that are in the same megagroup, but not in this one
            DWORD dwGroupVary = (dwPhoneVary < dwJCPhoneNum) ? pjpi[dwPhoneVary].dwGroup : 0 /* silence group*/;
            DWORD dwMegaVary = LexPhoneGroupToMega (dwGroupVary);
            for (k = 0; k < PIS_PHONEGROUPNUM; k++) {
               // dont match against self
               if (k == dwGroupVary)
                  continue;

               // don't match against not in mega group
               if (LexPhoneGroupToMega (k) != dwMegaVary)
                  continue;

               // compare against this
               pjpt = &pJC->paJCPHONETRAINContext[
                  (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
                  (!dwRight ? (k + pJC->dwContextIndexGroupStart) : adwGroupCon[0][0]) * pJC->dwContextIndexNum +
                  (dwRight ? (k + pJC->dwContextIndexGroupStart) : adwGroupCon[1][0])
                  ];
               if (pjpt->pPhoneme) {
                  DWORD dwStart, dwLength;
                  DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

                  fScore = pjpt->pPhoneme->Compare (
                     (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                     dwLength,
                     fMaxEnergy,
                     (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
                     FALSE /* slow */, FALSE /* all examplars */);
                                 // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

                  fScoreJust += (fScore - fScoreSameTriphone) * (double)pjpt->dwCount;
                  dwCountJust += pjpt->dwCount;
               }
            } // k
            EnterCriticalSection (&pJC->cs);
            pjcc->afScoreCon[dwRight][4] += fScoreJust;
            pjcc->adwCountCon[dwRight][4] += dwCountJust;
            LeaveCriticalSection (&pJC->cs);

            // loop over all megagroups
            // BUGFIX - Know it's not in the same megagroup
            fScoreJust = 0;
            dwCountJust = 0;

            // loop over groups that are in the same megagroup, but not in this one
            for (k = 0; k < PIS_PHONEMEGAGROUPNUM; k++) {
               // dont match against self
               if (k == dwMegaVary)
                  continue;

               // compare against this
               pjpt = &pJC->paJCPHONETRAINContext[
                  (DWORD)ppa->bPhone * pJC->dwContextIndexNum * pJC->dwContextIndexNum +
                  (!dwRight ? (k + pJC->dwContextIndexMegaStart) : adwGroupCon[0][0]) * pJC->dwContextIndexNum +
                  (dwRight ? (k + pJC->dwContextIndexMegaStart) : adwGroupCon[1][0])
                  ];
               if (pjpt->pPhoneme) {
                  DWORD dwStart, dwLength;
                  DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

                  fScore = pjpt->pPhoneme->Compare (
                     (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
                     dwLength,
                     fMaxEnergy,
                     (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
                     FALSE /* slow */, FALSE /* all examplars */);
                                 // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled

                  fScoreJust += (fScore - fScoreSameTriphone) * (double)pjpt->dwCount;
                  dwCountJust += pjpt->dwCount;
               }
            } // k
            EnterCriticalSection (&pJC->cs);
            pjcc->afScoreCon[dwRight][5] += fScoreJust;
            pjcc->adwCountCon[dwRight][5] += dwCountJust;
            LeaveCriticalSection (&pJC->cs);

         } // dwRight
      } // dwPasss == 5

      pjpt = &pJC->paJCPHONETRAINFunc[dwPhoneGroupIndex];
      if ((dwPass == 6) && pjpt->pPhoneme) {
         DWORD dwStart, dwLength;
         DetermineStartEnd (ppa, (DWORD)-1, &dwStart, &dwLength);

         fScore = pjpt->pPhoneme->Compare (
            (ppa->fIsVoiced ? psabVoiced : psabUnvoiced) + dwStart,
            dwLength,
            fMaxEnergy,
            (PWSTR) pLex->PhonemeSilence(), (PWSTR)pLex->PhonemeSilence(), pLex, JOINCOSTCOMPARENOTWIDE, TRUE /* feature distortion */, TRUE,
            FALSE /* slow */, FALSE /* all examplars */);
                        // BUGFIX - Allow feature distortion since otherwise low frequencies count too much. But feature distrotion disabled
      
         // add this to the list
         EnterCriticalSection (&pJC->cs);
         if (!pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT) {
            pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT = new CListFixed;
            if (pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT)
               pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT->Init (sizeof(JCLINEARFITPOINT));
         }
         jcdp.fX = ppa->pWord ? ppa->pWord->dwFuncWordGroup : NUMFUNCWORDGROUP;
         jcdp.fY = fScore;
         if (pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT)
            pJC->paJCLINEARFITFunc[dwPhoneGroupIndex].plJCLINEARFITPOINT->Add (&jcdp);
         LeaveCriticalSection (&pJC->cs);
      }

   } // j

   // release the SR features so don't use too much memory
   pwa->m_pWave->ReleaseSRFeatures();

}


/*************************************************************************************
JoinCostsLinearFit - Given a list of linear fit points, does a linear fit

inputs
   PJCLINEARFITPOINT    paPoint - List of points
   DWORD                dwNum - Number
   int                  iKeep - If 0, keep all. If 1, only positive fX, -1 only negative fX.
   DWORD                *pdwCount - Filled with the count for the linear fit
returns
   fp - Slope. Assuming the the vertical offset is unimportant
*/
fp JoinCostsLinearFit (PJCLINEARFITPOINT paPoint, DWORD dwNum, int iKeep, DWORD *pdwCount)
{
   *pdwCount = 0;

   // find the average
   double fAvgX, fAvgY;
   DWORD i;
   fAvgX = fAvgY = 0;
   for (i = 0; i < dwNum; i++) {
      if (iKeep > 0) {
         if (paPoint[i].fX <= 0.0)
            continue;
      }
      else if (iKeep < 0) {
         if (paPoint[i].fX >= 0.0)
            continue;
      }

      // note this
      *pdwCount += 1;
      fAvgX += paPoint[i].fX;
      fAvgY += paPoint[i].fY;
   } // i

   // if not enough points then error
   if (*pdwCount <= 1) {
      // no detectable slope
      *pdwCount = 0;
      return 0;
   }

   // find center
   fAvgX /= (double) *pdwCount;
   fAvgY /= (double) *pdwCount;

   // do a linear fit
   double fSlope =0;
   double fStrength = 0;
   double fX, fTempSlope, fTempStrength;
   for (i = 0; i < dwNum; i++) {
      if (iKeep > 0) {
         if (paPoint[i].fX <= 0.0)
            continue;
      }
      else if (iKeep < 0) {
         if (paPoint[i].fX >= 0.0)
            continue;
      }

      fX = paPoint[i].fX - fAvgX;
      if (!fX)
         continue;   // right at center point so skip
      fTempSlope = (paPoint[i].fY - fAvgY) / fX;

      fTempStrength = fabs(fX);

      fSlope += fTempStrength * fTempSlope;
      fStrength += fTempStrength;
      // BUGFIX - Adding 1 to strength so always have at least something
   } // i

   if (fStrength)
      fSlope /= fStrength;

   return fSlope;
}



/*************************************************************************************
CTTSWork::JoinCostsContext - Given a context, fill in.

inputs
   DWORD             dwIndex - Index. pJC->m_dwPhonemes * PHONEGROUPSQUARE possible elements
   PCMLexicon        pLex - Lexicon
   char              *psz - Filled in
   DWORD             dwBytes - Number of bytes in psz
returns
   none
*/
void CTTSWork::JoinCostsContext (DWORD dwIndex, PCMLexicon pLex, char *psz, DWORD dwBytes)
{
   DWORD dwCenter = dwIndex / PHONEGROUPSQUARE;
   DWORD dwLeft = (dwIndex / PIS_PHONEGROUPNUM) % PIS_PHONEGROUPNUM;
   DWORD dwRight = dwIndex % PIS_PHONEGROUPNUM;

   strcpy (psz, "(");
   pLex->PhonemeGroupToPhoneString (dwLeft, psz + strlen(psz), dwBytes - (DWORD)strlen(psz));
   strcat (psz, ") - ");

   PLEXPHONE plp = pLex->PhonemeGetUnsort (dwCenter);
   if (plp)
      WideCharToMultiByte (CP_ACP, 0, plp->szPhoneLong, -1, psz + strlen(psz), dwBytes - (DWORD)strlen(psz), 0, 0);
   else
      strcat (psz, "<s>");   // silence

   strcat (psz, " - (");
   pLex->PhonemeGroupToPhoneString (dwRight, psz + strlen(psz), dwBytes - (DWORD)strlen(psz));
   strcat (psz, ")");
}

   

/*************************************************************************************
CTTSWork::JoinCostsWrite - Write out the join costs

inputs
   DWORD             dwPass - Pass to use.
                        0 = pitch comparison
                        1 = energy
                        2 = duration
                        3 = position of unit within word
                        4 = arbitrary border connection
                        5 = context
   PTTSANAL          pAnal - Fill this with analysis information
   PTTSJOINCOSTS     pJC - Join costs info
   PCMTTS            pTTS - TTS used to store pitch-range energy costs
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::JoinCostsWrite (DWORD dwPass, PTTSANAL pAnal, PTTSJOINCOSTS pJC, PCMTTS pTTS)
{
   FILE *pf = pJC->pfJoinCosts;

   // header
   char sza[1024];
   if (dwPass == 0) {
      WideCharToMultiByte (CP_ACP, 0, m_szFile, -1, sza, sizeof(sza), 0, 0);

      if (pf)
         fprintf (pf, "Join costs for \"%s\"\n", sza);
   }

   PCMLexicon pLex = Lexicon();

   PJCPHONEMEINFO pjcpi = (PJCPHONEMEINFO) pJC->plJCPHONEMEINFO->Get(0);
   DWORD dwNumJCPHONEMEINFO = pJC->plJCPHONEMEINFO->Num();

   char *pszSeparator = "\n\n=====================================================";

   // loop through all the pitch comparisons
   DWORD i, j;
   PJCLINEARFIT pjcd;
   PJCLINEARFITPOINT pjcdp;
   fp fSlopePos, fSlopeNeg;
   DWORD dwCountPos, dwCountNeg;

   double afLinearFitBack2[1][2], afLinearFitBack[PIS_PHONEMEGAGROUPNUM][2], afLinearFit[PIS_PHONEGROUPNUM][2];
   DWORD adwLinearFitBack2[1][2], adwLinearFitBack[PIS_PHONEMEGAGROUPNUM][2], adwLinearFit[PIS_PHONEGROUPNUM][2];

   CListFixed alJCLINEARFITPOINT[PIS_PHONEGROUPNUM];  // [fVoiced][fPlosive]
   //double afLinearFitSum[2][2][2];     // [fVoiced][fPlosive][1=pos,0=neg]
   //DWORD adwLinearFitCount[2][2][2];      // like afLinearFitSum
   DWORD dwPhone;
   DWORD dwGroup, dwMegaGroup;
   //memset (afLinearFitSum, 0, sizeof(afLinearFitSum));
   //memset (adwLinearFitCount, 0, sizeof(adwLinearFitCount));
   PCListFixed pl;
   for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++)
      alJCLINEARFITPOINT[dwGroup].Init (sizeof(JCLINEARFITPOINT));
   if (dwPass == 0) {
      // blank out
      memset (afLinearFitBack2, 0, sizeof(afLinearFitBack2));
      memset (afLinearFitBack, 0, sizeof(afLinearFitBack));
      memset (afLinearFit, 0, sizeof(afLinearFit));
      memset (adwLinearFitBack2, 0, sizeof(adwLinearFitBack2));
      memset (adwLinearFitBack, 0, sizeof(adwLinearFitBack));
      memset (adwLinearFit, 0, sizeof(adwLinearFit));
      adwLinearFitBack2[0][0] = adwLinearFitBack2[0][1] = PARENTCATEGORYWEIGHT;  // so wont get divide by zero

      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcd = pJC->paJCLINEARFITPitch + i;

         // if no entries then skip
         if (!pjcd->plJCLINEARFITPOINT)
            continue;

         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nF0 join costs for %s", sza);
         }

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per octave", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per octave", (double)fSlopeNeg);
         }

         // sum these
         dwPhone = i / PHONEGROUPSQUARE;
         if (dwPhone < dwNumJCPHONEMEINFO) {
            // dwVoiced = pjcpi[dwPhone].fVoiced ? 1 : 0;
            // dwPlosive = pjcpi[dwPhone].fPlosive ? 1 : 0;

            //afLinearFitSum[dwVoiced][dwPlosive][0] += (double)fSlopeNeg * (double)dwCountNeg;
            //adwLinearFitCount[dwVoiced][dwPlosive][0] += dwCountNeg;
            //afLinearFitSum[dwVoiced][dwPlosive][1] += (double)fSlopePos * (double)dwCountPos;
            //adwLinearFitCount[dwVoiced][dwPlosive][1] += dwCountPos;

            // append to mass of points
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++)
               alJCLINEARFITPOINT[pLex->PhonemeToGroup(dwPhone)].Add (pjcdp + j);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nOctave\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

      } // i

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         // write out context
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nF0 substitution costs for phone group %d", (int)dwGroup);
         }

         pl = &alJCLINEARFITPOINT[dwGroup];

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pl->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pl->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per octave", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per octave", (double)fSlopeNeg);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nOctave\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
            for (j = 0; j < pl->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

         // store away
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFitBack2[0][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack2[0][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack2[0][0] += dwCountPos;
         adwLinearFitBack2[0][1] += dwCountNeg;
         afLinearFitBack[dwMegaGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack[dwMegaGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack[dwMegaGroup][0] += dwCountPos;
         adwLinearFitBack[dwMegaGroup][1] += dwCountNeg;
         afLinearFit[dwGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFit[dwGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFit[dwGroup][0] += dwCountPos;
         adwLinearFit[dwGroup][1] += dwCountNeg;
      } // dwVoiced, dwPlosive

      // backoffs
      for (j = 0; j < 2; j++)
         afLinearFitBack2[0][j] /= (double)adwLinearFitBack2[0][j];  // know that won't be zero
      for (dwMegaGroup = 0; dwMegaGroup < PIS_PHONEMEGAGROUPNUM; dwMegaGroup++) for (j = 0; j < 2; j++) {
         afLinearFitBack[dwMegaGroup][j] += afLinearFitBack2[0][j] * PARENTCATEGORYWEIGHT;
         adwLinearFitBack[dwMegaGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFitBack[dwMegaGroup][j] /= (double)adwLinearFitBack[dwMegaGroup][j];  // know that wont be 0
      }
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) for (j = 0; j < 2; j++) {
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFit[dwGroup][j] += afLinearFitBack[dwMegaGroup][j] * PARENTCATEGORYWEIGHT;
         adwLinearFit[dwGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFit[dwGroup][j] /= (double)adwLinearFit[dwGroup][j];  // know that wont be 0
      }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for F0 substitutuion");
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         pLex->PhonemeGroupToPhoneString (dwGroup, sza, sizeof(sza));
         afLinearFit[dwGroup][0] = max(afLinearFit[dwGroup][0], CLOSE);  // BUGFIX - Don't allow to be negative
         afLinearFit[dwGroup][1] = min(afLinearFit[dwGroup][1], -CLOSE); // BUGFIX - Don't allow to be negative

         pJC->pTarget->afUnitScorePitch[dwGroup][0] = min(afLinearFit[dwGroup][0], MAXTARGETCOSTVALUE);
         pJC->pTarget->afUnitScorePitch[dwGroup][1] = min(-afLinearFit[dwGroup][1], MAXTARGETCOSTVALUE);

         if (pf)
            fprintf (pf, "\n\t%g, %g, // group %s", (double)afLinearFit[dwGroup][0], -(double)afLinearFit[dwGroup][1], sza);
      }



      DWORD dwPSOLA;
      for (dwPSOLA = 0; dwPSOLA < 2; dwPSOLA++) {

         // blank out
         memset (afLinearFitBack2, 0, sizeof(afLinearFitBack2));
         memset (afLinearFitBack, 0, sizeof(afLinearFitBack));
         memset (afLinearFit, 0, sizeof(afLinearFit));
         memset (adwLinearFitBack2, 0, sizeof(adwLinearFitBack2));
         memset (adwLinearFitBack, 0, sizeof(adwLinearFitBack));
         memset (adwLinearFit, 0, sizeof(adwLinearFit));
         adwLinearFitBack2[0][0] = adwLinearFitBack2[0][1] = PARENTCATEGORYWEIGHT;  // so wont get divide by zero

         PJCPITCHPCM pJCPITCHPCMHigher, pJCPITCHPCMLower;
         char *pszName;
         if (dwPSOLA) {
            pJCPITCHPCMHigher = &pJC->JCPITCHPCMPSOLAHigher;
            pJCPITCHPCMLower = &pJC->JCPITCHPCMPSOLALower;
            pszName = "PSOLA";
         }
         else {
            pJCPITCHPCMHigher = &pJC->JCPITCHPCMHigher;
            pJCPITCHPCMLower = &pJC->JCPITCHPCMLower;
            pszName = "Pitch-sync";
         }

         // also write higher/lower
         for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
            // write out context
            if (pf) {
               fprintf (pf, pszSeparator);
               fprintf (pf, "\nAggregate %s PCM incurred errer per HALF octave for group %d", pszName, (int)dwGroup);
            }

            pJCPITCHPCMHigher->afScore[dwGroup] = max(pJCPITCHPCMHigher->afScore[dwGroup], CLOSE);  // BUGFIX - Don't allow to be negative
            pJCPITCHPCMLower->afScore[dwGroup] = max(pJCPITCHPCMLower->afScore[dwGroup], CLOSE);  // BUGFIX - Don't allow to be negative

            double fHigher = pJCPITCHPCMHigher->afScore[dwGroup] /
               (double) max(1, pJCPITCHPCMHigher->adwCount[dwGroup]);
            double fLower = pJCPITCHPCMLower->afScore[dwGroup] /
               (double) max(1, pJCPITCHPCMLower->adwCount[dwGroup]);

            if (pf) {
               fprintf (pf, "\n%s Higher = %g per half octave", pszName, (double)fHigher);
               fprintf (pf, "\n%s Lower = %g per half octave", pszName, (double)fLower);
            }

            // store away
            dwMegaGroup = LexPhoneGroupToMega(dwGroup);
            afLinearFitBack2[0][0] += pJCPITCHPCMHigher->afScore[dwGroup];
            afLinearFitBack2[0][1] += pJCPITCHPCMLower->afScore[dwGroup];
            adwLinearFitBack2[0][0] += pJCPITCHPCMHigher->adwCount[dwGroup];
            adwLinearFitBack2[0][1] += pJCPITCHPCMLower->adwCount[dwGroup];
            afLinearFitBack[dwMegaGroup][0] += pJCPITCHPCMHigher->afScore[dwGroup];
            afLinearFitBack[dwMegaGroup][1] += pJCPITCHPCMLower->afScore[dwGroup];
            adwLinearFitBack[dwMegaGroup][0] += pJCPITCHPCMHigher->adwCount[dwGroup];
            adwLinearFitBack[dwMegaGroup][1] += pJCPITCHPCMLower->adwCount[dwGroup];
            afLinearFit[dwGroup][0] += pJCPITCHPCMHigher->afScore[dwGroup];
            afLinearFit[dwGroup][1] += pJCPITCHPCMLower->afScore[dwGroup];
            adwLinearFit[dwGroup][0] += pJCPITCHPCMHigher->adwCount[dwGroup];
            adwLinearFit[dwGroup][1] += pJCPITCHPCMLower->adwCount[dwGroup];
         } // dwVoiced, dwPlosive


         // backoffs
         for (j = 0; j < 2; j++)
            afLinearFitBack2[0][j] /= (double)adwLinearFitBack2[0][j];  // know that won't be zero
         for (dwMegaGroup = 0; dwMegaGroup < PIS_PHONEMEGAGROUPNUM; dwMegaGroup++) for (j = 0; j < 2; j++) {
            afLinearFitBack[dwMegaGroup][j] += afLinearFitBack2[0][j] * PARENTCATEGORYWEIGHT;
            adwLinearFitBack[dwMegaGroup][j] += PARENTCATEGORYWEIGHT;
            afLinearFitBack[dwMegaGroup][j] /= (double)adwLinearFitBack[dwMegaGroup][j];  // know that wont be 0
         }
         for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) for (j = 0; j < 2; j++) {
            dwMegaGroup = LexPhoneGroupToMega(dwGroup);
            afLinearFit[dwGroup][j] += afLinearFitBack[dwMegaGroup][j] * PARENTCATEGORYWEIGHT;
            adwLinearFit[dwGroup][j] += PARENTCATEGORYWEIGHT;
            afLinearFit[dwGroup][j] /= (double)adwLinearFit[dwGroup][j];  // know that wont be 0
         }

         // write c++ code
         if (pf)
            fprintf (pf, "\n\nC++ code for %s PCM F0 substitutuion", pszName);
         for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
            pLex->PhonemeGroupToPhoneString (dwGroup, sza, sizeof(sza));
            afLinearFit[dwGroup][0] = max(afLinearFit[dwGroup][0], CLOSE);  // BUGFIX - Don't allow to be negative
            afLinearFit[dwGroup][1] = max(afLinearFit[dwGroup][1], CLOSE); // BUGFIX - Don't allow to be negative

            if (dwPSOLA) {
               pJC->pTarget->afUnitScorePCMPitchPSOLA[dwGroup][0] = min(afLinearFit[dwGroup][0],MAXTARGETCOSTVALUE);
               pJC->pTarget->afUnitScorePCMPitchPSOLA[dwGroup][1] = min(afLinearFit[dwGroup][1],MAXTARGETCOSTVALUE);
            }
            else {
               pJC->pTarget->afUnitScorePCMPitch[dwGroup][0] = min(afLinearFit[dwGroup][0], MAXTARGETCOSTVALUE);
               pJC->pTarget->afUnitScorePCMPitch[dwGroup][1] = min(afLinearFit[dwGroup][1], MAXTARGETCOSTVALUE);
            }

            if (pf)
               fprintf (pf, "\n\t%g, %g, // group %s", (double)afLinearFit[dwGroup][0], (double)afLinearFit[dwGroup][1], sza);
         }
      } // dwPSOLA

      if (pf)
         fflush (pf);
   } // dwPass == 0

   // show linear fit for everything combined
   //fprintf (pf, pszSeparator);
   //fprintf (pf, "\nF0 join costs for unvoiced non-plosive = %g (neg) and %g (pos)",
   //   (double)afLinearFitSum[0][0][0] / (double)max(1,adwLinearFitCount[0][0][0]),
   //   (double)afLinearFitSum[0][0][1] / (double)max(1,adwLinearFitCount[0][0][1])
   //   );
   //fprintf (pf, "\nF0 join costs for unvoiced plosive = %g (neg) and %g (pos)",
   //   (double)afLinearFitSum[0][1][0] / (double)max(1,adwLinearFitCount[0][1][0]),
   //   (double)afLinearFitSum[0][1][1] / (double)max(1,adwLinearFitCount[0][1][1])
   //   );
   //fprintf (pf, "\nF0 join costs for voiced non-plosive = %g (neg) and %g (pos)",
   //   (double)afLinearFitSum[1][0][0] / (double)max(1,adwLinearFitCount[1][0][0]),
   //   (double)afLinearFitSum[1][0][1] / (double)max(1,adwLinearFitCount[1][0][1])
   //   );
   //fprintf (pf, "\nF0 join costs for voiced plosive = %g (neg) and %g (pos)",
   //   (double)afLinearFitSum[1][1][0] / (double)max(1,adwLinearFitCount[1][1][0]),
   //   (double)afLinearFitSum[1][1][1] / (double)max(1,adwLinearFitCount[1][1][1])
   //   );


   // energy

   // loop through all the pitch comparisons
   if (dwPass == 1) {
      // blank out
      memset (afLinearFitBack2, 0, sizeof(afLinearFitBack2));
      memset (afLinearFitBack, 0, sizeof(afLinearFitBack));
      memset (afLinearFit, 0, sizeof(afLinearFit));
      memset (adwLinearFitBack2, 0, sizeof(adwLinearFitBack2));
      memset (adwLinearFitBack, 0, sizeof(adwLinearFitBack));
      memset (adwLinearFit, 0, sizeof(adwLinearFit));
      adwLinearFitBack2[0][0] = adwLinearFitBack2[0][1] = PARENTCATEGORYWEIGHT;  // so wont get divide by zero

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++)
         alJCLINEARFITPOINT[dwGroup].Init (sizeof(JCLINEARFITPOINT));
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcd = pJC->paJCLINEARFITEnergy + i;

         // if no entries then skip
         if (!pjcd->plJCLINEARFITPOINT)
            continue;

         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nEnergy join costs for %s", sza);
         }

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per log-2 energy ratio", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per log-2 energy ratio", (double)fSlopeNeg);
         }

         // sum these
         dwPhone = i / PHONEGROUPSQUARE;
         if (dwPhone < dwNumJCPHONEMEINFO) {
            // dwVoiced = pjcpi[dwPhone].fVoiced ? 1 : 0;
            // dwPlosive = pjcpi[dwPhone].fPlosive ? 1 : 0;

            //afLinearFitSum[dwVoiced][dwPlosive][0] += (double)fSlopeNeg * (double)dwCountNeg;
            //adwLinearFitCount[dwVoiced][dwPlosive][0] += dwCountNeg;
            //afLinearFitSum[dwVoiced][dwPlosive][1] += (double)fSlopePos * (double)dwCountPos;
            //adwLinearFitCount[dwVoiced][dwPlosive][1] += dwCountPos;

            // append to mass of points
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++)
               alJCLINEARFITPOINT[pLex->PhonemeToGroup(dwPhone)].Add (pjcdp + j);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nLog2EnergyRatio\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

      } // i

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         // write out context
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nEnergy aggregate substitution costs for group %d", (int)dwGroup);
         }

         pl = &alJCLINEARFITPOINT[dwGroup];

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pl->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pl->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per log-2 energy ratio", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per log-2 energy ratio", (double)fSlopeNeg);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nLog2EnergyRatio\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
            for (j = 0; j < pl->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

         // store away
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFitBack2[0][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack2[0][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack2[0][0] += dwCountPos;
         adwLinearFitBack2[0][1] += dwCountNeg;
         afLinearFitBack[dwMegaGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack[dwMegaGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack[dwMegaGroup][0] += dwCountPos;
         adwLinearFitBack[dwMegaGroup][1] += dwCountNeg;
         afLinearFit[dwGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFit[dwGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFit[dwGroup][0] += dwCountPos;
         adwLinearFit[dwGroup][1] += dwCountNeg;

      } // dwVoiced, dwPlosive

      // backoffs
      for (j = 0; j < 2; j++)
         afLinearFitBack2[0][j] /= (double)adwLinearFitBack2[0][j];  // know that won't be zero
      for (dwMegaGroup = 0; dwMegaGroup < PIS_PHONEMEGAGROUPNUM; dwMegaGroup++) for (j = 0; j < 2; j++) {
         afLinearFitBack[dwMegaGroup][j] += afLinearFitBack2[0][j] * PARENTCATEGORYWEIGHT;
         adwLinearFitBack[dwMegaGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFitBack[dwMegaGroup][j] /= (double)adwLinearFitBack[dwMegaGroup][j];  // know that wont be 0
      }
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) for (j = 0; j < 2; j++) {
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFit[dwGroup][j] += afLinearFitBack[dwMegaGroup][j] * PARENTCATEGORYWEIGHT;
         adwLinearFit[dwGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFit[dwGroup][j] /= (double)adwLinearFit[dwGroup][j];  // know that wont be 0
      }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for energy substitutuion");
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         pLex->PhonemeGroupToPhoneString (dwGroup, sza, sizeof(sza));
         afLinearFit[dwGroup][0] = max(afLinearFit[dwGroup][0], CLOSE);  // BUGFIX - Don't allow to be negative
         afLinearFit[dwGroup][1] = min(afLinearFit[dwGroup][1], -CLOSE); // BUGFIX - Don't allow to be negative

         pJC->pTarget->afUnitScoreEnergy[dwGroup][0] = min(afLinearFit[dwGroup][0], MAXTARGETCOSTVALUE);
         pJC->pTarget->afUnitScoreEnergy[dwGroup][1] = min(-afLinearFit[dwGroup][1], MAXTARGETCOSTVALUE);

         if (pf)
            fprintf (pf, "\n\t%g, %g, // group %s", (double)afLinearFit[dwGroup][0], -(double)afLinearFit[dwGroup][1], sza);
      }


      if (pf)
         fflush (pf);
   } // dwPass == 1



   // duration

   if (dwPass == 2) {
      // blank out
      memset (afLinearFitBack2, 0, sizeof(afLinearFitBack2));
      memset (afLinearFitBack, 0, sizeof(afLinearFitBack));
      memset (afLinearFit, 0, sizeof(afLinearFit));
      memset (adwLinearFitBack2, 0, sizeof(adwLinearFitBack2));
      memset (adwLinearFitBack, 0, sizeof(adwLinearFitBack));
      memset (adwLinearFit, 0, sizeof(adwLinearFit));
      adwLinearFitBack2[0][0] = adwLinearFitBack2[0][1] = PARENTCATEGORYWEIGHT;  // so wont get divide by zero

      // loop through all the pitch comparisons
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++)
         alJCLINEARFITPOINT[dwGroup].Init (sizeof(JCLINEARFITPOINT));
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcd = pJC->paJCLINEARFITDuration + i;

         // if no entries then skip
         if (!pjcd->plJCLINEARFITPOINT)
            continue;

         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nDuration join costs for %s", sza);
         }

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per log-2 duration ratio", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per log-2 duration ratio", (double)fSlopeNeg);
         }

         // sum these
         dwPhone = i / PHONEGROUPSQUARE;
         if (dwPhone < dwNumJCPHONEMEINFO) {
            // dwVoiced = pjcpi[dwPhone].fVoiced ? 1 : 0;
            // wPlosive = pjcpi[dwPhone].fPlosive ? 1 : 0;

            //afLinearFitSum[dwVoiced][dwPlosive][0] += (double)fSlopeNeg * (double)dwCountNeg;
            //adwLinearFitCount[dwVoiced][dwPlosive][0] += dwCountNeg;
            //afLinearFitSum[dwVoiced][dwPlosive][1] += (double)fSlopePos * (double)dwCountPos;
            //adwLinearFitCount[dwVoiced][dwPlosive][1] += dwCountPos;

            // append to mass of points
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++)
               alJCLINEARFITPOINT[pLex->PhonemeToGroup(dwPhone)].Add (pjcdp + j);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nLog2DurationRatio\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

      } // i

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         // write out context
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nDuration aggregate substitution costs for group %d", (int)dwGroup);
         }

         pl = &alJCLINEARFITPOINT[dwGroup];

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pl->Num(), 1, &dwCountPos);
         fSlopeNeg = JoinCostsLinearFit (pjcdp, pl->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nPositive linear fit = %g per log-2 duration ratio", (double)fSlopePos);
            if (dwCountNeg)
               fprintf (pf, "\nNegative linear fit = %g per log-2 duration ratio", (double)fSlopeNeg);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nLog2DurationRatio\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
            for (j = 0; j < pl->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }


         // store away
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFitBack2[0][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack2[0][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack2[0][0] += dwCountPos;
         adwLinearFitBack2[0][1] += dwCountNeg;
         afLinearFitBack[dwMegaGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFitBack[dwMegaGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack[dwMegaGroup][0] += dwCountPos;
         adwLinearFitBack[dwMegaGroup][1] += dwCountNeg;
         afLinearFit[dwGroup][0] += fSlopePos * (double)dwCountPos;
         afLinearFit[dwGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFit[dwGroup][0] += dwCountPos;
         adwLinearFit[dwGroup][1] += dwCountNeg;
      } // dwVoiced, dwPlosive

      // backoffs
      for (j = 0; j < 2; j++)
         afLinearFitBack2[0][j] /= (double)adwLinearFitBack2[0][j];  // know that won't be zero
      for (dwMegaGroup = 0; dwMegaGroup < PIS_PHONEMEGAGROUPNUM; dwMegaGroup++) for (j = 0; j < 2; j++) {
         afLinearFitBack[dwMegaGroup][j] += afLinearFitBack2[0][j] * PARENTCATEGORYWEIGHT;
         adwLinearFitBack[dwMegaGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFitBack[dwMegaGroup][j] /= (double)adwLinearFitBack[dwMegaGroup][j];  // know that wont be 0
      }
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) for (j = 0; j < 2; j++) {
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFit[dwGroup][j] += afLinearFitBack[dwMegaGroup][j] * PARENTCATEGORYWEIGHT;
         adwLinearFit[dwGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFit[dwGroup][j] /= (double)adwLinearFit[dwGroup][j];  // know that wont be 0
      }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for duration substitutuion");
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         pLex->PhonemeGroupToPhoneString (dwGroup, sza, sizeof(sza));
         afLinearFit[dwGroup][0] = max(afLinearFit[dwGroup][0], CLOSE);  // BUGFIX - Don't allow to be negative
         afLinearFit[dwGroup][1] = min(afLinearFit[dwGroup][1], -CLOSE); // BUGFIX - Don't allow to be negative

         pJC->pTarget->afUnitScoreDuration[dwGroup][0] = min(afLinearFit[dwGroup][0], MAXTARGETCOSTVALUE);
         pJC->pTarget->afUnitScoreDuration[dwGroup][1] = min(-afLinearFit[dwGroup][1], MAXTARGETCOSTVALUE);

         if (pf)
            fprintf (pf, "\n\t%g, %g, // group %s", (double)afLinearFit[dwGroup][0], -(double)afLinearFit[dwGroup][1], sza);
      }


      if (pf)
         fflush (pf);
   } // dwPass == 2


   // word position
   if (dwPass == 3) {
      double afWordPosScore[4];     // score for 4 different word positions (opposite)
      DWORD adwWordPosCount[4];     // count
      memset (afWordPosScore, 0, sizeof(afWordPosScore));
      memset (adwWordPosCount, 0, sizeof(adwWordPosCount));
      PJCWORDPOS pjcwp;
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcwp = pJC->paJCWORDPOSWordPos + i;

         // make sure at least some entries
         for (j = 0; j < 4; j++)
            if (pjcwp->adwCount[j])
               break;
         if (j >= 4)
            continue;   // no entries, so skip


         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nMismatched position in word costs for %s", sza);
         }

         double fScoreNoMismatch = pjcwp->adwCount[0] ? ((double)pjcwp->afScore[0] / (double)pjcwp->adwCount[0]) : 0.0;
         fScoreNoMismatch = max(fScoreNoMismatch, 0.0);  // don't allow to be ngative
         pjcwp->afScore[1] = max(pjcwp->afScore[1], 0.0);   // don't allow to be negative
         pjcwp->afScore[2] = max(pjcwp->afScore[2], 0.0);   // don't allow to be negative
         pjcwp->afScore[3] = max(pjcwp->afScore[3], 0.0);   // don't allow to be negative

         if (pf) {
            if (pjcwp->adwCount[0])
               fprintf (pf, "\n\tNo mismatch = %g", fScoreNoMismatch);
            if (pjcwp->adwCount[1])
               fprintf (pf, "\n\tStart-of-word mismatch = %g", (double)pjcwp->afScore[1] / (double)pjcwp->adwCount[1]);
            if (pjcwp->adwCount[2])
               fprintf (pf, "\n\tEnd-of-word mismatch = %g", (double)pjcwp->afScore[2] / (double)pjcwp->adwCount[2]);
            if (pjcwp->adwCount[3])
               fprintf (pf, "\n\tStart and end-of-word mismatch = %g", (double)pjcwp->afScore[3] / (double)pjcwp->adwCount[3]);
         }

         // sum up
         // BUGFIX - Subtract fScoreNoMismatch
         for (j = 0; j < 4; j++) {
            afWordPosScore[j] += pjcwp->afScore[j] - (double) fScoreNoMismatch * pjcwp->adwCount[j];
            adwWordPosCount[j] += pjcwp->adwCount[j];
         }
      } // i

      // aggregate
      if (pf) {
         fprintf (pf, pszSeparator);
         fprintf (pf, "\nAggregate mismatched position in word");
         fprintf (pf, "\n\"No mismatch\" already subtracted from all");
      }
      for (j = 0; j < 4; j++) {
         afWordPosScore[j] = max(afWordPosScore[j], 0.0);   // don't allow to be negative

         if (adwWordPosCount[j])
            afWordPosScore[j] /= (double)adwWordPosCount[j];
      }

      if (pf) {
         if (adwWordPosCount[0])
            fprintf (pf, "\n\tNo mismatch = %g", (double)afWordPosScore[0]);
         if (adwWordPosCount[1])
            fprintf (pf, "\n\tStart-of-word mismatch = %g", (double)afWordPosScore[1]);
         if (adwWordPosCount[2])
            fprintf (pf, "\n\tEnd-of-word mismatch = %g", (double)afWordPosScore[2]);
         if (adwWordPosCount[3])
            fprintf (pf, "\n\tStart and end-of-word mismatch = %g", (double)afWordPosScore[3]);
      }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for word start/end mismatch");
      for (j = 0; j < 4; j++) {
         pJC->pTarget->afUnitScoreMismatchedWordPos[j] = min(afWordPosScore[j], MAXTARGETCOSTVALUE);

         if (pf)
            fprintf (pf, "\n\t%g, // mismatch %d", (double)afWordPosScore[j], (int)j);
      }

      if (pf)
         fflush (pf);
   } // dwPass == 3


   // boundary error between units
   if (dwPass == 4) {
      PJCCONNECT pjcc;
      double afConnectSum[PIS_PHONEGROUPNUM][PIS_PHONEGROUPNUM];    // [dwGroupLeft][dwGroupRight]
      DWORD adwConnectCount[PIS_PHONEGROUPNUM][PIS_PHONEGROUPNUM];  // like afConnectSum
      double afConnectSumBack[PIS_PHONEMEGAGROUPNUM][PIS_PHONEMEGAGROUPNUM];
      DWORD adwConnectCountBack[PIS_PHONEMEGAGROUPNUM][PIS_PHONEMEGAGROUPNUM];
      double afConnectSumBack2[1][1];
      DWORD adwConnectCountBack2[1][1];
      double afMidSum[PIS_PHONEGROUPNUM], afMidSumBack[PIS_PHONEMEGAGROUPNUM], afMidSumBack2[1];
      DWORD adwMidSum[PIS_PHONEGROUPNUM], adwMidSumBack[PIS_PHONEMEGAGROUPNUM], adwMidSumBack2[1];

      memset (afConnectSum, 0, sizeof(afConnectSum));
      memset (adwConnectCount, 0, sizeof(adwConnectCount));
      memset (afConnectSumBack, 0, sizeof(afConnectSumBack));
      memset (adwConnectCountBack, 0, sizeof(adwConnectCountBack));
      memset (afConnectSumBack2, 0, sizeof(afConnectSumBack2));
      memset (adwConnectCountBack2, 0, sizeof(adwConnectCountBack2));
      adwConnectCountBack2[0][0] = PARENTCATEGORYWEIGHT; // so always something

      memset (afMidSum, 0, sizeof(afMidSum));
      memset (afMidSumBack, 0, sizeof(afMidSumBack));
      memset (afMidSumBack2, 0, sizeof(afMidSumBack2));
      memset (adwMidSum, 0, sizeof(adwMidSum));
      memset (adwMidSumBack, 0, sizeof(adwMidSumBack));
      memset (adwMidSumBack2, 0, sizeof(adwMidSumBack2));
      adwMidSumBack2[0] = PARENTCATEGORYWEIGHT; // so always have something

      DWORD dwGroupLeft, dwGroupRight, dwMegaGroupLeft, dwMegaGroupRight;
      for (i = 0; i < pJC->dwPhonemes; i++) {
         pjcc = pJC->paJCCONNECTConnectMid + i;

         // make sure at least some entries
         if (!pjcc->dwCount)
            continue;

         dwGroupLeft = pLex->PhonemeToGroup (i);
         dwMegaGroupLeft = LexPhoneGroupToMega(dwGroupLeft);

         afMidSum[dwGroupLeft] += pjcc->fScore;
         afMidSumBack[dwMegaGroupLeft] += pjcc->fScore;
         afMidSumBack2[0] += pjcc->fScore;
         adwMidSum[dwGroupLeft] += pjcc->dwCount;
         adwMidSumBack[dwMegaGroupLeft] += pjcc->dwCount;
         adwMidSumBack2[0] += pjcc->dwCount;
      } // i

      for (i = 0; i < pJC->dwPhonemes * pJC->dwPhonemes; i++) {
         pjcc = pJC->paJCCONNECTConnect + i;

         pjcc->fScore = max(pjcc->fScore, CLOSE); // don't allow to be negative

         // make sure at least some entries
         if (!pjcc->dwCount)
            continue;

         DWORD dwPhoneLeft = i / pJC->dwPhonemes;
         DWORD dwPhoneRight = i % pJC->dwPhonemes;

         // write out context
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nNon-contiguous unit costs for %s - %s",
               (dwPhoneLeft < dwNumJCPHONEMEINFO) ? pjcpi[dwPhoneLeft].szaName : "<s>",
               (dwPhoneRight < dwNumJCPHONEMEINFO) ? pjcpi[dwPhoneRight].szaName : "<s>"
               );

            fprintf (pf, "\n\tCost = %g", (double)pjcc->fScore / (double)pjcc->dwCount);
         }


         // figure out voiced, etc.
         if (dwPhoneLeft >= dwNumJCPHONEMEINFO)
            continue;   // not valid
         dwGroupLeft = pLex->PhonemeToGroup (dwPhoneLeft);
         dwMegaGroupLeft = LexPhoneGroupToMega(dwGroupLeft);

         // and for right context
         if (dwPhoneRight >= dwNumJCPHONEMEINFO)
            continue;   // not valid
         dwGroupRight = pLex->PhonemeToGroup (dwPhoneRight);
         dwMegaGroupRight = LexPhoneGroupToMega(dwGroupRight);

         afConnectSum[dwGroupLeft][dwGroupRight] += pjcc->fScore;
         adwConnectCount[dwGroupLeft][dwGroupRight] += pjcc->dwCount;
         afConnectSumBack[dwMegaGroupLeft][dwMegaGroupRight] += pjcc->fScore;
         adwConnectCountBack[dwMegaGroupLeft][dwMegaGroupRight] += pjcc->dwCount;
         afConnectSumBack2[0][0] += pjcc->fScore;
         adwConnectCountBack2[0][0] += pjcc->dwCount;
      } // i

      // aggregate
      if (pf) {
         fprintf (pf, pszSeparator);
         fprintf (pf, "\nAggregate non-contiguous unit costs");
         fprintf (pf, "\n");
         for (dwGroupRight = 0; dwGroupRight < PIS_PHONEGROUPNUM; dwGroupRight++) {
            pLex->PhonemeGroupToPhoneString (dwGroupRight, sza, sizeof(sza));
            fprintf (pf, "\tRight-%s", sza);
         }
      }
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++) {
         if (pf) {
            pLex->PhonemeGroupToPhoneString (dwGroupLeft, sza, sizeof(sza));
            fprintf (pf, "\nLeft-%s", sza);
         }

         for (dwGroupRight = 0; dwGroupRight < PIS_PHONEGROUPNUM; dwGroupRight++) {
            afConnectSum[dwGroupLeft][dwGroupRight] = max(afConnectSum[dwGroupLeft][dwGroupRight], CLOSE); // don't allow to be negative

            if (pf) {
               if (adwConnectCount[dwGroupLeft][dwGroupRight])
                  fprintf (pf, "\t%g", (double)afConnectSum[dwGroupLeft][dwGroupRight] /
                     (double)adwConnectCount[dwGroupLeft][dwGroupRight]);
               else
                  fprintf (pf, "\tunknown");
            } // pf
         } // dwVoicedRight, dwPlosiveRight
      } // dwVoicedCenter, dwPlosiveCenter

      // backoff midphone
      afMidSumBack2[0] /= (double)adwMidSumBack2[0];
      for (dwMegaGroupLeft = 0; dwMegaGroupLeft < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLeft++) {
         afMidSumBack[dwMegaGroupLeft] += afMidSumBack2[0] * PARENTCATEGORYWEIGHT;
         adwMidSumBack[dwMegaGroupLeft] += PARENTCATEGORYWEIGHT;
         afMidSumBack[dwMegaGroupLeft] /= (double)adwMidSumBack[dwMegaGroupLeft];
      }
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++) {
         dwMegaGroupLeft = LexPhoneGroupToMega(dwGroupLeft);
         afMidSum[dwGroupLeft] += afMidSumBack[dwMegaGroupLeft] * PARENTCATEGORYWEIGHT;
         adwMidSum[dwGroupLeft] += PARENTCATEGORYWEIGHT;
         afMidSum[dwGroupLeft] /= (double)adwMidSum[dwGroupLeft];
      }

      // backoff diphone
      afConnectSumBack2[0][0] /= (double)adwConnectCountBack2[0][0];
      for (dwMegaGroupLeft = 0; dwMegaGroupLeft < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLeft++)
         for (dwMegaGroupRight = 0; dwMegaGroupRight < PIS_PHONEMEGAGROUPNUM; dwMegaGroupRight++) {
            afConnectSumBack[dwMegaGroupLeft][dwMegaGroupRight] += afConnectSumBack2[0][0] * PARENTCATEGORYWEIGHT;
            adwConnectCountBack[dwMegaGroupLeft][dwMegaGroupRight] += PARENTCATEGORYWEIGHT;
            afConnectSumBack[dwMegaGroupLeft][dwMegaGroupRight] /= (double)adwConnectCountBack[dwMegaGroupLeft][dwMegaGroupRight];
         }
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++)
         for (dwGroupRight = 0; dwGroupRight < PIS_PHONEGROUPNUM; dwGroupRight++) {
            dwMegaGroupLeft = LexPhoneGroupToMega(dwGroupLeft);
            dwMegaGroupRight = LexPhoneGroupToMega(dwGroupRight);

            afConnectSum[dwGroupLeft][dwGroupRight] += afConnectSumBack[dwMegaGroupLeft][dwMegaGroupRight]  * PARENTCATEGORYWEIGHT;
            adwConnectCount[dwGroupLeft][dwGroupRight] += PARENTCATEGORYWEIGHT;
            afConnectSum[dwGroupLeft][dwGroupRight] /= (double)adwConnectCount[dwGroupLeft][dwGroupRight];
         }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for mid-phone join-cost estimates");
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++) {
         afMidSum[dwGroupLeft] = max(afMidSum[dwGroupLeft], CLOSE); // don't allow to be negative

         pJC->pTarget->afUnitScoreNonContiguousMid[dwGroupLeft] = min(afMidSum[dwGroupLeft], MAXTARGETCOSTVALUE);

         pLex->PhonemeGroupToPhoneString (dwGroupLeft, sza, sizeof(sza));

         if (pf)
            fprintf (pf, "\n\t%g, // %s", (double)afMidSum[dwGroupLeft], sza);
      } // dwVoicedCenter, dwPlosiveCenter

      if (pf)
         fprintf (pf, "\n\nC++ code for diphone join-cost estimates");
      for (dwGroupLeft = 0; dwGroupLeft < PIS_PHONEGROUPNUM; dwGroupLeft++) {
         if (pf) {
            pLex->PhonemeGroupToPhoneString (dwGroupLeft, sza, sizeof(sza));
            fprintf (pf, "\n\t");
         }

         for (dwGroupRight = 0; dwGroupRight < PIS_PHONEGROUPNUM; dwGroupRight++) {
            afConnectSum[dwGroupLeft][dwGroupRight] = max(afConnectSum[dwGroupLeft][dwGroupRight], CLOSE);  // don't allow to be negative

            pJC->pTarget->afUnitScoreNonContiguous[dwGroupLeft][dwGroupRight] = min(afConnectSum[dwGroupLeft][dwGroupRight], MAXTARGETCOSTVALUE);

            if (pf)
               fprintf (pf, "%g, ", (double)afConnectSum[dwGroupLeft][dwGroupRight]);
         }

         if (pf)
            fprintf (pf, " // %s", sza);
      } // dwVoicedCenter, dwPlosiveCenter


      if (pf)
         fflush (pf);
   } // dwPass == 4

   // mismatched context
   if (dwPass == 5) {
      PJCCONTEXT pjcn;
      DWORD dwRight;
      char *pszRightLeft;
      double afContextScoreSum[PIS_PHONEGROUPNUM][PIS_PHONEMEGAGROUPNUM][2][6];  // [dwGroupCenter][dwGroupLR][0=left,1=right][0..5]
      DWORD adwContextScoreCount[PIS_PHONEGROUPNUM][PIS_PHONEMEGAGROUPNUM][2][6];   // same as afContextScoreSum
      double afContextScoreSumBack[PIS_PHONEMEGAGROUPNUM][PIS_PHONEMEGAGROUPNUM][2][6];
      DWORD adwContextScoreCountBack[PIS_PHONEMEGAGROUPNUM][PIS_PHONEMEGAGROUPNUM][2][6];
      double afContextScoreSumBack2[1][1][2][6];
      DWORD adwContextScoreCountBack2[1][1][2][6];

      memset (&afContextScoreSum, 0, sizeof(afContextScoreSum));
      memset (&adwContextScoreCount, 0, sizeof(adwContextScoreCount));
      memset (&afContextScoreSumBack, 0, sizeof(afContextScoreSumBack));
      memset (&adwContextScoreCountBack, 0, sizeof(adwContextScoreCountBack));
      memset (&afContextScoreSumBack2, 0, sizeof(afContextScoreSumBack2));
      memset (&adwContextScoreCountBack2, 0, sizeof(adwContextScoreCountBack2));
      for (i = 0; i < 2; i++) for (j = 0; j < 6; j++)
         adwContextScoreCountBack2[0][0][i][j] = PARENTCATEGORYWEIGHT;  // so always have something

      DWORD dwGroupCenter, dwGroupLeft, dwGroupRight;
      DWORD dwMegaGroupCenter, dwMegaGroupRight, dwMegaGroupLeft;
      DWORD dwGroupLR, dwMegaGroupLR;

      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcn = pJC->paJCCONTEXTContext + i;

         // make sure at least some entries
         for (j = 0; j < 6; j++)
            if (pjcn->adwCountCon[0][j] || pjcn->adwCountCon[1][j])
               break;
         if (j >= 6)
            continue;   // no entries, so skip


         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nMismatched LR context costs for %s", sza);
         }

         for (dwRight = 0; dwRight < 2; dwRight++) {

            // don't allow to be negative
            for (j = 0; j < 6; j++)
               pjcn->afScoreCon[dwRight][j] = max(pjcn->afScoreCon[dwRight][j], CLOSE);

            pszRightLeft = dwRight ? "right" : "left";

            if (pf) {
               if (pjcn->adwCountCon[dwRight][0])
                  fprintf (pf, "\n\tSame unit %s = %g", pszRightLeft, (double)pjcn->afScoreCon[dwRight][0] / (double)pjcn->adwCountCon[dwRight][0]);
               if (pjcn->adwCountCon[dwRight][1])
                  fprintf (pf, "\n\tNo mismatch in %s = %g (not scaled)", pszRightLeft, (double)pjcn->afScoreCon[dwRight][1] / (double)pjcn->adwCountCon[dwRight][1]);
               if (pjcn->adwCountCon[dwRight][2])
                  fprintf (pf, "\n\tStress mismatch in %s = %g", pszRightLeft, (double)pjcn->afScoreCon[dwRight][2] / (double)pjcn->adwCountCon[dwRight][2]);
               if (pjcn->adwCountCon[dwRight][3])
                  fprintf (pf, "\n\tPhone mismatch (but in group) in %s = %g", pszRightLeft, (double)pjcn->afScoreCon[dwRight][3] / (double)pjcn->adwCountCon[dwRight][3]);
               if (pjcn->adwCountCon[dwRight][4])
                  fprintf (pf, "\n\tGroup mismatch in %s = %g", pszRightLeft, (double)pjcn->afScoreCon[dwRight][4] / (double)pjcn->adwCountCon[dwRight][4]);
               if (pjcn->adwCountCon[dwRight][5])
                  fprintf (pf, "\n\tMega-group mismatch in %s = %g", pszRightLeft, (double)pjcn->afScoreCon[dwRight][5] / (double)pjcn->adwCountCon[dwRight][5]);
            }
         }

         dwPhone = i / PHONEGROUPSQUARE;
         dwGroupCenter = pLex->PhonemeToGroup (dwPhone);
         dwGroupLeft = (i / PIS_PHONEGROUPNUM) % PIS_PHONEGROUPNUM;
         dwGroupRight = i % PIS_PHONEGROUPNUM;
         dwMegaGroupCenter = LexPhoneGroupToMega (dwGroupCenter);
         dwMegaGroupRight = LexPhoneGroupToMega (dwGroupRight);
         dwMegaGroupLeft = LexPhoneGroupToMega (dwGroupLeft);

         // sum up
         // BUGFIX - Subtract no-mismatch from all
         for (dwRight = 0; dwRight < 2; dwRight++) {
            dwGroupLR = dwRight ? dwGroupRight : dwGroupLeft;
            dwMegaGroupLR = dwRight ? dwMegaGroupRight : dwMegaGroupLeft;

            for (j = 0; j < 6; j++) {
               afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] += pjcn->afScoreCon[dwRight][j];
               adwContextScoreCount[dwGroupCenter][dwMegaGroupLR][dwRight][j] += pjcn->adwCountCon[dwRight][j];
               afContextScoreSumBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] += pjcn->afScoreCon[dwRight][j];
               adwContextScoreCountBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] += pjcn->adwCountCon[dwRight][j];
               afContextScoreSumBack2[0][0][dwRight][j] += pjcn->afScoreCon[dwRight][j];
               adwContextScoreCountBack2[0][0][dwRight][j] += pjcn->adwCountCon[dwRight][j];
            }
         }
      } // i


      // aggregate
      if (pf) {
         fprintf (pf, pszSeparator);
         fprintf (pf, "\nAggregate mismatched LR costs");
         fprintf (pf, "\n\"No mismatch\" has alread been subtracted.");
         fprintf (pf, "\n\tSame unit\tMatch\tBad-stress\tBad-phone-in-group\tBad-group\tBad-megagroup");
      }
      for (dwGroupCenter = 0; dwGroupCenter < PIS_PHONEGROUPNUM; dwGroupCenter++)
         for (dwMegaGroupLR = 0; dwMegaGroupLR < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLR++)
            for (dwRight = 0; dwRight < 2; dwRight++) {
               if (pf) {
                  pLex->PhonemeGroupToPhoneString (dwGroupCenter, sza, sizeof(sza));
                  fprintf (pf, "\nCenter=%s, context megagroup=%d, %s", sza, (int) dwMegaGroupLR, dwRight ? "right" : "left");
               }

               for (j = 0; j < 6; j++) {
                  afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] = max(afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j], CLOSE); // don't allow to be negative

                  if (pf) {
                     if (adwContextScoreCount[dwGroupCenter][dwMegaGroupLR][dwRight][j])
                        fprintf (pf, "\t%g", afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] / (double)adwContextScoreCount[dwGroupCenter][dwMegaGroupLR][dwRight][j]);
                     else
                        fprintf (pf, "\tunknown");
                  }
               }
            } // dwRight

      // backoff
      for (dwRight = 0; dwRight < 2; dwRight++) for (j = 0; j < 6; j++) {
         afContextScoreSumBack2[0][0][dwRight][j] /= (double)adwContextScoreCountBack2[0][0][dwRight][j];

         // spread to megagroup
         for (dwMegaGroupCenter = 0; dwMegaGroupCenter < PIS_PHONEMEGAGROUPNUM; dwMegaGroupCenter++)
            for (dwMegaGroupLR = 0; dwMegaGroupLR < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLR++) {
               afContextScoreSumBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] +=
                  afContextScoreSumBack2[0][0][dwRight][j] * (double)PARENTCATEGORYWEIGHT;
               adwContextScoreCountBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] += PARENTCATEGORYWEIGHT;

               afContextScoreSumBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] /=
                  (double)adwContextScoreCountBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j];
            }

         // spread to group
         for (dwGroupCenter = 0; dwGroupCenter < PIS_PHONEGROUPNUM; dwGroupCenter++)
            for (dwMegaGroupLR = 0; dwMegaGroupLR < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLR++) {
               dwMegaGroupCenter = LexPhoneGroupToMega(dwGroupCenter);

               afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] +=
                  afContextScoreSumBack[dwMegaGroupCenter][dwMegaGroupLR][dwRight][j] * (double)PARENTCATEGORYWEIGHT;
               adwContextScoreCount[dwGroupCenter][dwMegaGroupLR][dwRight][j] += PARENTCATEGORYWEIGHT;

               afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] /=
                  (double)adwContextScoreCount[dwGroupCenter][dwMegaGroupLR][dwRight][j];
            }

      } // j

      // c++
      if (pf)
         fprintf (pf, "\n\nC++ code for mismatched context");
      for (dwGroupCenter = 0; dwGroupCenter < PIS_PHONEGROUPNUM; dwGroupCenter++)
         for (dwMegaGroupLR = 0; dwMegaGroupLR < PIS_PHONEMEGAGROUPNUM; dwMegaGroupLR++)
            for (dwRight = 0; dwRight < 2; dwRight++) {
               if (pf)
                  fprintf (pf, "\n\t");

               for (j = 0; j < 6; j++) {
                  afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j] = max(afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j], CLOSE); // don't allow to be negative

                  pJC->pTarget->afUnitScoreLRMismatch[dwGroupCenter][dwMegaGroupLR][dwRight][j] = min(afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j], MAXTARGETCOSTVALUE);

                  if (pf)
                     fprintf (pf, "%g, ", afContextScoreSum[dwGroupCenter][dwMegaGroupLR][dwRight][j]);
               }

               pLex->PhonemeGroupToPhoneString (dwGroupCenter, sza, sizeof(sza));
               if (pf)
                  fprintf (pf, " // Center context = %s, %s context mega group = %d", sza, dwRight ? "right" : "left", (int)dwMegaGroupLR);
            } // dwRight


      if (pf)
         fflush (pf);
   } // dwPass == 5

   // loop through all the Func comparisons
   if (dwPass == 6) {
      // blank out
      memset (afLinearFitBack2, 0, sizeof(afLinearFitBack2));
      memset (afLinearFitBack, 0, sizeof(afLinearFitBack));
      memset (afLinearFit, 0, sizeof(afLinearFit));
      memset (adwLinearFitBack2, 0, sizeof(adwLinearFitBack2));
      memset (adwLinearFitBack, 0, sizeof(adwLinearFitBack));
      memset (adwLinearFit, 0, sizeof(adwLinearFit));
      adwLinearFitBack2[0][0] = adwLinearFitBack2[0][1] = PARENTCATEGORYWEIGHT;  // so wont get divide by zero

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++)
         alJCLINEARFITPOINT[dwGroup].Init (sizeof(JCLINEARFITPOINT));
      for (i = 0; i < pJC->dwPhonemes * PHONEGROUPSQUARE; i++) {
         pjcd = pJC->paJCLINEARFITFunc + i;

         // if no entries then skip
         if (!pjcd->plJCLINEARFITPOINT)
            continue;

         // write out context
         JoinCostsContext (i, pLex, sza, sizeof(sza));
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nFunc word costs for %s", sza);
         }

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), 0, &dwCountPos);
         // fSlopeNeg = JoinCostsLinearFit (pjcdp, pjcd->plJCLINEARFITPOINT->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         //fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nLinear fit = %g per function word value", (double)fSlopePos);
            //if (dwCountNeg)
            //   fprintf (pf, "\nNegative linear fit = %g per function word value", (double)fSlopeNeg);
         }

         // sum these
         dwPhone = i / PHONEGROUPSQUARE;
         if (dwPhone < dwNumJCPHONEMEINFO) {
            // dwVoiced = pjcpi[dwPhone].fVoiced ? 1 : 0;
            // dwPlosive = pjcpi[dwPhone].fPlosive ? 1 : 0;

            //afLinearFitSum[dwVoiced][dwPlosive][0] += (double)fSlopeNeg * (double)dwCountNeg;
            //adwLinearFitCount[dwVoiced][dwPlosive][0] += dwCountNeg;
            //afLinearFitSum[dwVoiced][dwPlosive][1] += (double)fSlopePos * (double)dwCountPos;
            //adwLinearFitCount[dwVoiced][dwPlosive][1] += dwCountPos;

            // append to mass of points
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++)
               alJCLINEARFITPOINT[pLex->PhonemeToGroup(dwPhone)].Add (pjcdp + j);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nFuncWordValue\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pjcd->plJCLINEARFITPOINT->Get(0);
            for (j = 0; j < pjcd->plJCLINEARFITPOINT->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

      } // i

      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         // write out context
         if (pf) {
            fprintf (pf, pszSeparator);
            fprintf (pf, "\nFunction-word-value target costs for group %d", (int)dwGroup);
         }

         pl = &alJCLINEARFITPOINT[dwGroup];

         // linear fits
         pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
         fSlopePos = JoinCostsLinearFit (pjcdp, pl->Num(), 0, &dwCountPos);
         // fSlopeNeg = JoinCostsLinearFit (pjcdp, pl->Num(), -1, &dwCountNeg);
         fSlopePos = max(fSlopePos, CLOSE);  // BUGFIX - Don't allow to be negative
         // fSlopeNeg = min(fSlopeNeg, -CLOSE); // BUGFIX - Don't allow to be negative
         if (pf) {
            if (dwCountPos)
               fprintf (pf, "\nLinear fit = %g per function word value", (double)fSlopePos);
            // if (dwCountNeg)
            //   fprintf (pf, "\nNegative linear fit = %g per function word value", (double)fSlopeNeg);
         }

         // write out the points
         if (pf) {
            fprintf (pf, "\n\nFuncWordValue\tSRScore");
            pjcdp = (PJCLINEARFITPOINT) pl->Get(0);
            for (j = 0; j < pl->Num(); j++, pjcdp++)
               fprintf (pf, "\n%g\t%g", (double)pjcdp->fX, (double)pjcdp->fY);
         }

         // store away
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFitBack2[0][0] += fSlopePos * (double)dwCountPos;
         // afLinearFitBack2[0][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack2[0][0] += dwCountPos;
         // adwLinearFitBack2[0][1] += dwCountNeg;
         afLinearFitBack[dwMegaGroup][0] += fSlopePos * (double)dwCountPos;
         // afLinearFitBack[dwMegaGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFitBack[dwMegaGroup][0] += dwCountPos;
         // adwLinearFitBack[dwMegaGroup][1] += dwCountNeg;
         afLinearFit[dwGroup][0] += fSlopePos * (double)dwCountPos;
         // afLinearFit[dwGroup][1] += fSlopeNeg * (double)dwCountNeg;
         adwLinearFit[dwGroup][0] += dwCountPos;
         // adwLinearFit[dwGroup][1] += dwCountNeg;

      } // dwVoiced, dwPlosive

      // backoffs
      for (j = 0; j < 1; j++)
         afLinearFitBack2[0][j] /= (double)adwLinearFitBack2[0][j];  // know that won't be zero
      for (dwMegaGroup = 0; dwMegaGroup < PIS_PHONEMEGAGROUPNUM; dwMegaGroup++) for (j = 0; j < 1; j++) {
         afLinearFitBack[dwMegaGroup][j] += afLinearFitBack2[0][j] * PARENTCATEGORYWEIGHT;
         adwLinearFitBack[dwMegaGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFitBack[dwMegaGroup][j] /= (double)adwLinearFitBack[dwMegaGroup][j];  // know that wont be 0
      }
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) for (j = 0; j < 1; j++) {
         dwMegaGroup = LexPhoneGroupToMega(dwGroup);
         afLinearFit[dwGroup][j] += afLinearFitBack[dwMegaGroup][j] * PARENTCATEGORYWEIGHT;
         adwLinearFit[dwGroup][j] += PARENTCATEGORYWEIGHT;
         afLinearFit[dwGroup][j] /= (double)adwLinearFit[dwGroup][j];  // know that wont be 0
      }

      // write c++ code
      if (pf)
         fprintf (pf, "\n\nC++ code for function-word-value target");
      for (dwGroup = 0; dwGroup < PIS_PHONEGROUPNUM; dwGroup++) {
         pLex->PhonemeGroupToPhoneString (dwGroup, sza, sizeof(sza));
         afLinearFit[dwGroup][0] = max(afLinearFit[dwGroup][0], CLOSE);  // BUGFIX - Don't allow to be negative
         // afLinearFit[dwGroup][1] = min(afLinearFit[dwGroup][1], -CLOSE); // BUGFIX - Don't allow to be negative

         pJC->pTarget->afUnitScoreFunc[dwGroup] = min(afLinearFit[dwGroup][0], MAXTARGETCOSTVALUE);
         pJC->pTarget->afUnitScoreFunc[dwGroup] = max(pJC->pTarget->afUnitScoreFunc[dwGroup], -MAXTARGETCOSTVALUE);
         // pJC->pTarget->afUnitScoreFunc[dwGroup][1] = min(-afLinearFit[dwGroup][1], MAXTARGETCOSTVALUE);

         if (pf)
            fprintf (pf, "\n\t%g, // group %s", (double)afLinearFit[dwGroup][0], sza);
      }


      if (pf)
         fflush (pf);
   } // dwPass == 6


   if (pf)
      fflush (pf);

   return TRUE;

}


/*************************************************************************************
CTTSWork::JoinSimulatePSOLA - Simulates PSOLA on an individual SRFEATURE.

inputs
   PSRFEATURE        pSRF - Feature. Has valid data in, and is modified
   fp                fPitchOrig - Original pitch
   int               iRaise - 1 for higher, 0 for lower, -1 for no change
returns
   none
*/
void CTTSWork::JoinSimulatePSOLA (PSRFEATURE pSRF, fp fPitchOrig, int iRaise)
{
   float afVoiced[JOINPSOLAHARMONICS], afNoise[JOINPSOLAHARMONICS];
   float afVoicedNew[JOINPSOLAHARMONICS], afNoiseNew[JOINPSOLAHARMONICS];

   // convert to harmonics
   SRFEATUREToHarmonics (pSRF, JOINPSOLAHARMONICS, afVoiced, afNoise, fPitchOrig);

   // do effects
   fp fPitchNew = fPitchOrig;
   DWORD i, j;
   if ((iRaise >= 0) && (iRaise <= 1)) {
      fPitchNew = fPitchOrig * pow((fp)2.0, (fp)(iRaise ? 1 : -1) * (fp)HALFOCTAVEFORPCM);

      DWORD dwVoiced;
      for (dwVoiced = 0; dwVoiced < 2; dwVoiced++) {
         float *paf = dwVoiced ? afVoiced : afNoise;
         float *pafNew = dwVoiced ? afVoicedNew : afNoiseNew;

         memset (pafNew, 0, sizeof(afVoicedNew));
         for (i = 0; i < JOINPSOLAHARMONICS; i++) {
            // if zero, then continue to optimize
            if (!paf[i])
               continue;

            float *pafPSOLASpread = (float*)m_memJoinCalcPSOLASpread.p +
               ((DWORD) iRaise * JOINPSOLAHARMONICS * JOINPSOLAHARMONICS + i * JOINPSOLAHARMONICS);
            for (j = 0; j < JOINPSOLAHARMONICS; j++, pafPSOLASpread++)
               pafNew[j] += pafPSOLASpread[0] * paf[i];
         } // i
      } // dwVoiced
   }
   else {
      memcpy (afVoicedNew, afVoiced, sizeof(afVoiced));
      memcpy (afNoiseNew, afNoise, sizeof(afNoise));
   }
   
   // convert back
   SRFEATUREFromHarmonics (JOINPSOLAHARMONICS, afVoicedNew, afNoiseNew, fPitchNew, pSRF);
}

/*************************************************************************************
CTTSWork::JoinSimulatePSOLAInWave - Simulates PCM pitch shift over an entire wave.

inputs
   PCM3DWave         pWave - Wave, needed for pitch
   PSRFEATURE        paSRF - Array of features
   DWORD             dwNum - Number of entries in paSRF
   PCMem             pMem - Memory to fill in with shifted wave
   int               iRaise - 1 for higher, 0 for lower, -1 for no change
returns
   BOOL - TRUE if success
*/
BOOL CTTSWork::JoinSimulatePSOLAInWave (PCM3DWave pWave, PSRFEATURE paSRF, DWORD dwNum, PCMem pMem, int iRaise)
{
   if (!pMem->Required (dwNum * sizeof(SRFEATURE)))
      return FALSE;

   PSRFEATURE paSRFDest = (PSRFEATURE) pMem->p;

   DWORD i;
   for (i = 0; i < dwNum; i++, paSRF++, paSRFDest++) {
      memcpy (paSRFDest, paSRF, sizeof(*paSRF));
      JoinSimulatePSOLA (paSRFDest, pWave->PitchAtSample(PITCH_F0, i * pWave->m_dwSRSkip, 0), iRaise);
   }

   return TRUE;
}



/*************************************************************************************
CTTSWork::JoinCalcPSOLASpread - Allocates and fills in the memory needed to simulate
PSOLA harmonic spreads.

inputs
   PCMem                pMem - Allocated to the right size and filled in with an array
                              of float [2][JOINPSOLAHARMONICS][JOINPSOLAHARMONICS]. [2] is 0 for
                              lowered, 1 for raised. First [JOINPSOLAHARMONICS] is the origial
                              harmonic. Seconds [JOINPSOLAHARMONICS] is the new harmonic weight to
                              use (scaled by the original harmonic's level
   fp                   fPitchAdjust - Number of octaves to adjust the pitch up/down.
returns  
   BOOL - TRUE if success
*/
#define PSOLASPREADWINDOW     (JOINPSOLAHARMONICS*2)

BOOL CTTSWork::JoinCalcPSOLASpread (PCMem pMem, fp fPitchAdjust)
{
   CSinLUT SinLUT;
   CMem memScratch;

   DWORD dwNeed = 2 * JOINPSOLAHARMONICS * JOINPSOLAHARMONICS * sizeof(float);
   if (!pMem->Required (dwNeed))
      return FALSE;
   float *paf = (float*)pMem->p;
   memset (paf, 0 , sizeof(dwNeed));

   fp fHigher = pow((fp)2.0, fPitchAdjust);
   fp fLower = pow((fp)2.0, -fPitchAdjust);

   float afPCM[PSOLASPREADWINDOW];
   // loop
   DWORD dwHarmonic, i;
   fp fEnergyOrig;
   float *pafCur;
   for (dwHarmonic = 1; dwHarmonic < JOINPSOLAHARMONICS; dwHarmonic++) {
      // just a sine
      for (i = 0; i < PSOLASPREADWINDOW; i++)
         afPCM[i] = sin((fp)i * (fp)dwHarmonic * 2.0 * PI / PSOLASPREADWINDOW);
      FFTRecurseReal (afPCM - 1, PSOLASPREADWINDOW, 1, &SinLUT, &memScratch);
      fEnergyOrig = sqrt(afPCM[dwHarmonic*2+0] * afPCM[dwHarmonic*2+0] + afPCM[dwHarmonic*2+1] * afPCM[dwHarmonic*2+1]);

      // simulate psola higher/lower
      DWORD dwHigher;
      for (dwHigher = 0; dwHigher < 2; dwHigher++) {
         fp fPitchScale = dwHigher ? fHigher : fLower;

         for (i = 0; i < PSOLASPREADWINDOW; i++)
            afPCM[i] =
               HanningWindow ((fp)i/PSOLASPREADWINDOW/2.0 + 0.5) *
               sin((fp)i * (fp)dwHarmonic / fPitchScale * 2.0 * PI / PSOLASPREADWINDOW) +

               HanningWindow ((fp)i/PSOLASPREADWINDOW/2.0 + 0.0) *
               (-sin((fp)(PSOLASPREADWINDOW - i) * (fp)dwHarmonic / fPitchScale * 2.0 * PI / PSOLASPREADWINDOW))
               ;

         FFTRecurseReal (afPCM-1, PSOLASPREADWINDOW, 1, &SinLUT, &memScratch);

         // write out
         pafCur = paf + (dwHigher * JOINPSOLAHARMONICS * JOINPSOLAHARMONICS + dwHarmonic * JOINPSOLAHARMONICS);
         for (i = 0; i < JOINPSOLAHARMONICS; i++)
            pafCur[i] = sqrt(afPCM[i*2+0] * afPCM[i*2+0] + afPCM[i*2+1] * afPCM[i*2+1]) / fEnergyOrig;

      } // dwHigher

   } // dwHarmonic

   return TRUE;
}


// BUGBUG - When create sentences to record with 5 words in them, make sure
// that the words are real, and appear in the lexicon. SOme of the words generated dont
// appear to be in the lexicon
