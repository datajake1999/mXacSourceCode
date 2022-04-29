/***************************************************************************
SpeechRecog.cpp - C++ object for storing speech regontiion information away

begun 13/5/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#include <crtdbg.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"


// #define SRACCURACY_BENDFORMANTS_ON
#define SRACCURACY_BENDFORMANTS_OFF


// #define SRACCURACY_FUNCWORDWEIGHTPOW_050
// #define SRACCURACY_FUNCWORDWEIGHTPOW_066
#define SRACCURACY_FUNCWORDWEIGHTPOW_100
// #define SRACCURACY_FUNCWORDWEIGHTPOW_150
// #define SRACCURACY_FUNCWORDWEIGHTPOW_200


#ifdef SRACCURACY_FUNCWORDWEIGHTPOW_050
#define FUNCWORDWEIGHTPOW        0.5
#endif
#ifdef SRACCURACY_FUNCWORDWEIGHTPOW_066
#define FUNCWORDWEIGHTPOW        0.66
#endif
#ifdef SRACCURACY_FUNCWORDWEIGHTPOW_100
#define FUNCWORDWEIGHTPOW        1.0
#endif
#ifdef SRACCURACY_FUNCWORDWEIGHTPOW_150
#define FUNCWORDWEIGHTPOW        1.5
#endif
#ifdef SRACCURACY_FUNCWORDWEIGHTPOW_200
#define FUNCWORDWEIGHTPOW        2.0
#endif

// #define SRACCURACY_TRAINOPPOSITESTRESS_10
#define SRACCURACY_TRAINOPPOSITESTRESS_6
// #define SRACCURACY_TRAINOPPOSITESTRESS_4

#ifdef SRACCURACY_TRAINOPPOSITESTRESS_10
#define TRAINOPPOSITESTRESS    10     // phonemes which match stress exactly are over-trained 3x,
#endif
#ifdef SRACCURACY_TRAINOPPOSITESTRESS_6
#define TRAINOPPOSITESTRESS    6     // phonemes which match stress exactly are over-trained 3x,
#endif
#ifdef SRACCURACY_TRAINOPPOSITESTRESS_4
#define TRAINOPPOSITESTRESS    4     // phonemes which match stress exactly are over-trained 3x,
#endif




#define SRACCURACY_CIBACKOFFWEIGHT_10
// #define SRACCURACY_CIBACKOFFWEIGHT_20
// #define SRACCURACY_CIBACKOFFWEIGHT_40

#ifdef SRACCURACY_CIBACKOFFWEIGHT_10
#define CIBACKOFFWEIGHT          0.1   // backoff weight for context-independent phoneme
#endif
#ifdef SRACCURACY_CIBACKOFFWEIGHT_20
#define CIBACKOFFWEIGHT          0.2   // backoff weight for context-independent phoneme
#endif
#ifdef SRACCURACY_CIBACKOFFWEIGHT_40
#define CIBACKOFFWEIGHT          0.4   // backoff weight for context-independent phoneme
#endif


#define SRACCURACY_DBTHEORETICALPHONE_050
// #define SRACCURACY_DBTHEORETICALPHONE_100
// #define SRACCURACY_DBTHEORETICALPHONE_200
//#define SRACCURACY_DBTHEORETICALPHONE_400

#ifdef SRACCURACY_DBTHEORETICALPHONE_050
#define DBTHEORETICALPHONE       0.5   // how much to scale error if phone is louder/quieter than theoretical average
#endif
#ifdef SRACCURACY_DBTHEORETICALPHONE_100
#define DBTHEORETICALPHONE       1.0   // how much to scale error if phone is louder/quieter than theoretical average
#endif
#ifdef SRACCURACY_DBTHEORETICALPHONE_200
#define DBTHEORETICALPHONE       2.0   // how much to scale error if phone is louder/quieter than theoretical average
#endif
#ifdef SRACCURACY_DBTHEORETICALPHONE_400
#define DBTHEORETICALPHONE       4.0   // how much to scale error if phone is louder/quieter than theoretical average
#endif



// #define SRACCURACY_DBABOVEPHONESCALE_050
//#define SRACCURACY_DBABOVEPHONESCALE_100
#define SRACCURACY_DBABOVEPHONESCALE_200
// #define SRACCURACY_DBABOVEPHONESCALE_400

#ifdef SRACCURACY_DBABOVEPHONESCALE_050
#define DBABOVEPHONESCALE        0.5   // how much to increase error per db above phone
#endif
#ifdef SRACCURACY_DBABOVEPHONESCALE_100
#define DBABOVEPHONESCALE        1.0   // how much to increase error per db above phone
#endif
#ifdef SRACCURACY_DBABOVEPHONESCALE_200
#define DBABOVEPHONESCALE        2.0   // how much to increase error per db above phone
#endif
#ifdef SRACCURACY_DBABOVEPHONESCALE_400
#define DBABOVEPHONESCALE        4.0   // how much to increase error per db above phone
#endif


// #define SRACCURACY_MAXCOMPAREDIVIDE_2
// #define SRACCURACY_MAXCOMPAREDIVIDE_4
#define SRACCURACY_MAXCOMPAREDIVIDE_8

#ifdef SRACCURACY_MAXCOMPAREDIVIDE_2
#define MAXCOMPAREDIVIDE         2     // optimization for maximum that will compare
#endif
#ifdef SRACCURACY_MAXCOMPAREDIVIDE_4
#define MAXCOMPAREDIVIDE         4     // optimization for maximum that will compare
#endif
#ifdef SRACCURACY_MAXCOMPAREDIVIDE_8
#define MAXCOMPAREDIVIDE         8     // optimization for maximum that will compare
#endif


#define SRACCURACY_TOPPERCENTUNIT_64
   // BUGBUG - I'm not sure if 64 is the best thing since won't be too many examples
// #define SRACCURACY_TOPPERCENTUNIT_32
// #define SRACCURACY_TOPPERCENTUNIT_16
//#define SRACCURACY_TOPPERCENTUNIT_8
//#define SRACCURACY_TOPPERCENTUNIT_6
// #define SRACCURACY_TOPPERCENTUNIT_4
//#define SRACCURACY_TOPPERCENTUNIT_3
// #define SRACCURACY_TOPPERCENTUNIT_2
   // BUGBUG - better accuracy with SRACCURACY_MAXPHONESAMPLESFIXED_512 | SRACCURACY_TOPPERCENTUNIT_32 | SRACCURACY_SRSMALL_3, but
   // BUGBUG - May be better with SRACCURACY_TOPPERCENTUNIT_64
   // takes too much memory

#ifdef SRACCURACY_TOPPERCENTUNIT_64
#define TOPPERCENTUNIT 64         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_32
#define TOPPERCENTUNIT 32         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_16
#define TOPPERCENTUNIT 16         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_8
#define TOPPERCENTUNIT 8         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_6
#define TOPPERCENTUNIT 6         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_4
#define TOPPERCENTUNIT 4         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_3
#define TOPPERCENTUNIT 3         // unit compare is much smaller
#endif
#ifdef SRACCURACY_TOPPERCENTUNIT_2
#define TOPPERCENTUNIT 2         // unit compare is much smaller
#endif

#define TOPPERCENT   2          // keep top 1 out of 5 scores (20%)
            // BUGFIX - Was 5, but made 2, and then do ramp

#define MAXPHONESAMPLES             (m_fHalfSize ? (MAXPHONESAMPLESFIXED/2) : MAXPHONESAMPLESFIXED)
#define HALFMAXPHONESAMPLES         (MAXPHONESAMPLES/2)        // for a faster SR compare

#define NOMODS_UNDERWEIGHTFUNCTIONWORDS   // if defined, then underweight function words
   // NOTE: ALSO in CTTSWork.cpp



// HYPRESTRICTION - Restricts the hypothesis to the folloinwg
typedef struct {
   WORD           awPhoneStart[2];  // min[0] and max[1] times where the phone can start
   WORD           awPhoneEnd[2];    // min and max times where the phone can end
   BYTE           bPhone;           // phoneme. Silence phonemes not listed
   BYTE           bFill;            // filler
} HYPRESTRICTION, *PHYPRESTRICTION;

/* CSRPronTree - Keeps track of all the possible pronuncations for a phrase*/
typedef struct {
   DWORD          dwNextWordNode;         // node number (from text parse) that has the next word in it
   DWORD          dwThisWordNode;         // node number (from textparse) with this word
   //PWSTR          pszNextChar;            // pointer to the next character
   //PWSTR          pszThisChar;             // pointer to the current character
   DWORD          dwPhoneNum;             // current phoneme number
   PCListFixed    plPPRONTREENODE;       // list of possible next phonemes. Will be NULL if haven't
                                          // calculated yet. List will contain a NULL pointer if terminal node
   BOOL           fCantDelList;            // set to true if ccan delete the list
   BOOL           fNoReattach;            // if TRUE then can't reattach to this point
} PRONTREENODE, *PPRONTREENODE;

class CSRPronTree {
public:
   ESCNEWDELETE;

   CSRPronTree (void);
   ~CSRPronTree (void);

   BOOL Init (PCMMLNode2 pTextParse, PCTextParse pTP, PCMLexicon m_pLex);
   PPRONTREENODE RootGet (void);
   void CalcNextIfNecessary (PPRONTREENODE pNode, DWORD dwSilence, DWORD dwNextNode = -1);
   PPRONTREENODE LookForReattach (PPRONTREENODE pCompare, PPRONTREENODE pNode);

private:
   PCMMLNode2      m_pTextParse;           // main parent node of text parse
   PCTextParse    m_pTP;                  // actual text parser object
   PCMLexicon     m_pLex;                 // lexicon
   //PWSTR          m_pszOrigString;        // original string passed into init
   CListVariable  m_lPRONTREENODE;        // list of nodes
};
typedef CSRPronTree *PCSRPronTree;


typedef struct {
   WVPHONEME        wvf;        // phoneme
   DWORD            dwTextParse;  // index into text parse
   BYTE             bPhone;     // phoneme number
   BYTE             bFill;      // filler
   //PWSTR          pszString;  // current string text
   //DWORD          dwChars;    // number of characters in string
} SRPHONEME, *PSRPHONEME;

// HYPINFO - Information that's shared among hypothesis
typedef struct {
   PCM3DWave         pWave;             // wave that recognizing from
   DWORD             dwTimeAccuracy;    // how accurate this is as far as time search goes
   fp                fMaxEnergy;        // maximum energy
   PCVoiceFile       pVoiceFile;        // voice file that uysing
   PCSRPronTree      pPronTree;         // prounciation tree that traversing
   DWORD             dwSilence;         // silence phoneme index, to speed up since used lots
   PCPhoneInstance   pPhoneSilence;     // silence phoneme instance
   BOOL              fForUnitSelection;   // TRUE if used to select best units for TTS voice, FALSE for segmenttation
   BOOL              fAllowFeatureDistortion; // TRUE if allow feature distortion
   BOOL              fFast;             // if TRUE then do fast recognition this go-around, skipping every other sample
   BOOL              fHalfExamples;     // if TRUE then only use the first half of examples for faster SR
#ifdef OLDSR
   fp                *pafEnergy;        // energy for each of the waves
#else
   PSRANALBLOCK      pSRAB;             // analysis block for the wave
#endif OLDSR
} HYPINFO, *PHYPINFO;

// CSRHyp - SPeech recogntiion hypotheis
class CSRHyp {
public:
   ESCNEWDELETE;

   CSRHyp (void);
   ~CSRHyp (void);
   CSRHyp *Clone (void);
   BOOL HypothesizePhoneme (DWORD dwIndex, DWORD dwFrames, PWSTR pszBefore, PWSTR pszAfter,
      DWORD dwParseIndex);
   void SpawnHypToNextPhoneme (PCListFixed plPCSRHyp);
   BOOL AdvanceHyp (PCListFixed plPCSRHyp);
   BOOL AreEquivalent (CSRHyp *pCompare);


   // filled in by hypthesize phoneme
   DWORD             m_dwSamplesUsed;     // number of samples into the phoneme m_adwPhoneNExt[0]
   DWORD             m_dwSamplesNext;     // next sample to spawn a phoneme
   CListFixed        m_lSRPHONEME;        // list of phonemes chosen
   fp                m_fScorePerFrame;    // given this phoneme, update the score this amount per frame

   // filled in by SpawnHypBasedOnText
   DWORD             m_dwCurPhone;        // current phoneme
   PPRONTREENODE     m_pPronNext;         // next pronunciation that will deal with, can be NULL

   // updated in advanceHyp
   fp                m_fScore;            // current score  // BUGFIX - was a DWORD
   DWORD             m_dwCurTime;         // current time, since SR feature numbers

   DWORD             m_dwLastTimeTop;     // last time this was a top rated hypothesis

   PHYPRESTRICTION   m_pHypRestriction;   // if not NULL, points to a list of hypothesis
                                          // restrictions that can be used for a second pass
                                          // if NULL, there are no restrictions

   PHYPINFO          m_phi;               // information common to all hypothesis
};
typedef CSRHyp *PCSRHyp;






#define OVERTRAIN    6     // phonemes which match stress exactly are over-trained 3x,
                           // while cross-stress training is only done once
                           // BUGFIX - Was 3. Changed to 6



#ifdef _DEBUG
#define NUMENGLISHPHONE    41    // number of eblish phonemes
static double gafVisemeSound[NUMENGLISHPHONE][SROCTAVE][2]; // for each english phonemes
static DWORD gadwVisemeCount[NUMENGLISHPHONE];
#endif

static char gszKeyTrainFile[] = "TrainFile";


/*************************************************************************************
CPhoneInstance::Constructor and destructor */
CPhoneInstance::CPhoneInstance (void)
{
   Clear();
}

CPhoneInstance::~CPhoneInstance (void)
{
   // do nothing for now
}

/*************************************************************************************
CPhoneInstance::Clear - Clears out information
*/
void CPhoneInstance::Clear (void)
{
   m_szBefore[0] = m_szAfter[0] = 0;
   m_fRoughSketch = FALSE;

#ifdef OLDSR
   memset (m_aSRFeature, 0, sizeof(m_aSRFeature));
#else
   DWORD i;
   for (i = 0; i < SUBPHONEPERPHONE; i++)
      m_aSubPhone[i].Clear();
#endif

   m_fTrainCount = 0;
   m_fAverageDuration = 0;
   // dont bother with... CalcEnergy();
}

/************************************************************************************
CPhoneInstance::HalfSize - If you call this immediately after creating, the number of
examplar samples will be halved, saving memory (at the expense of quality)

inputs
   BOOL        fHalfSize - What to set to. TRUE to use 32 points, FALSE (defaults) to 64
*/
void CPhoneInstance::HalfSize (BOOL fHalfSize)
{
   DWORD i;
   for (i = 0; i < SUBPHONEPERPHONE; i++)
      m_aSubPhone[i].HalfSize(fHalfSize);
}


/*************************************************************************************
CPhoneInstance::FillWithSilence - Fills a phoneme instance with silence

inputs
   fp          fMaxEnergy - Maximum energy for the wave. If 0 then will default to MAXSPEECHWINDOWNORMALIZED
*/
void CPhoneInstance::FillWithSilence (fp fMaxEnergy)
{
   if (!fMaxEnergy)
      fMaxEnergy = MAXSPEECHWINDOWNORMALIZED;

   Clear();
   m_fTrainCount = 1;
   m_fAverageDuration = 0;

#ifdef OLDSR
   DWORD i,j;
   for (j = 0; j < 3; j++) for (i = 0; i < SRDATAPOINTS; i++) {
      m_aSRFeature[j].acVoiceEnergy[i] = -120;
      m_aSRFeature[j].acNoiseEnergy[i] = SRNOISEFLOOR;
   } //i,j

   CalcEnergy();
#else
   SRANALBLOCK srab;
   memset (&srab, 0, sizeof(srab));
   DWORD i;
   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      srab.sr.acVoiceEnergyMain[i] = srab.sr.acNoiseEnergyMain[i] = SRNOISEFLOOR;

#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
      srab.sr.acVoiceEnergyDelta[i] = srab.sr.acNoiseEnergyDelta[i] = SRABSOLUTESILENCE;
#endif
   }
   srab.fEnergy = SRFEATUREEnergySmall (TRUE, &srab.sr, FALSE, TRUE);
   SRFEATUREScale (&srab.sr, PHONESAMPLENORMALIZED / srab.fEnergy);
   srab.fEnergyAfterNormal = SRFEATUREEnergySmall (TRUE, &srab.sr, FALSE, TRUE);

   // clear and train all the contents
   for (i = 0; i < SUBPHONEPERPHONE; i++) {
      m_aSubPhone[i].Clear();
      m_aSubPhone[i].Train (&srab, 1, fMaxEnergy, srab.fEnergy, 1.0);
   } // i
#endif
}

#ifdef OLDSR
/*************************************************************************************
CPhoneInstance::CalcEnergy - Call this to calculate the energy after a SRFEATURE
has been changed.
*/
void CPhoneInstance::CalcEnergy (void)
{
   DWORD i;
   for (i = 0; i < 3; i++)
      m_afSRFEnergy[i] = SRFEATUREEnergy (&m_aSRFeature[i]);
}
#endif // 0

/*************************************************************************************
CPhoneInstance::MMLTo - Standard MMLTo
*/
static PWSTR gpszPhoneInstance = L"PhoneInstance";
static PWSTR gpszBefore = L"Before";
static PWSTR gpszAfter = L"After";
static PWSTR gpszTrainCount = L"TrainCount";
static PWSTR gpszAverageDuration = L"AvDur";
static PWSTR gpszPhoneSlice = L"PhoneSlice";
static PWSTR gpszAverage = L"Average";
static PWSTR gpszVoice = L"Voice";
static PWSTR gpszNoise = L"Noise";
static PWSTR gpszSubPhone = L"SubPhone";
static PWSTR gpszRoughSketch = L"RoughSketch";

PCMMLNode2 CPhoneInstance::MMLTo (void)
{
   PCMMLNode2 pNew = new CMMLNode2;
   if (!pNew)
      return NULL;
   pNew->NameSet (gpszPhoneInstance);

   if (m_szBefore[0])
      MMLValueSet (pNew, gpszBefore, m_szBefore);
   if (m_szAfter[0])
      MMLValueSet (pNew, gpszAfter, m_szAfter);
   

   if (m_fRoughSketch)
      MMLValueSet (pNew, gpszRoughSketch, (int)m_fRoughSketch);
   MMLValueSet (pNew, gpszTrainCount, (int)ceil(m_fTrainCount));
   MMLValueSet (pNew, gpszAverageDuration, m_fAverageDuration);

   // write out slices
   DWORD i;
   PCMMLNode2 pSub;
#ifdef OLDSR
   for (i = 0; i < 3; i++) {
      pSub = pNew->ContentAddNewNode();
      if (!pSub)
         continue;
      pSub->NameSet (gpszPhoneSlice);

      MMLValueSet (pSub, gpszVoice, (PBYTE)&m_aSRFeature[i].acVoiceEnergy[0], sizeof(m_aSRFeature[i].acVoiceEnergy));
      MMLValueSet (pSub, gpszNoise, (PBYTE)&m_aSRFeature[i].acNoiseEnergy[0], sizeof(m_aSRFeature[i].acNoiseEnergy));
   }
#else
   for (i = 0; i < SUBPHONEPERPHONE; i++) {
      pSub = m_aSubPhone[i].MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszSubPhone);
         pNew->ContentAdd (pSub);
      }
   } // i
#endif

   return pNew;
}



/*************************************************************************************
CPhoneSlice::MMLFrom - Stanard MMLFrom
*/
BOOL CPhoneInstance::MMLFrom (PCMMLNode2 pNode)
{
   // clear out existing slice
   DWORD i;
   m_szBefore[0] = m_szAfter[0] = 0;

   // names
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszBefore);
   if (psz)
      wcscpy (m_szBefore, psz);
   psz = MMLValueGet (pNode, gpszAfter);
   if (psz)
      wcscpy (m_szAfter, psz);

   m_fRoughSketch = (BOOL)MMLValueGetInt (pNode, gpszRoughSketch, FALSE);
   m_fTrainCount = (fp) MMLValueGetInt (pNode, gpszTrainCount, (int)0);
   m_fAverageDuration = MMLValueGetDouble (pNode, gpszAverageDuration, 0);

   // elements
   PCMMLNode2 pSub;
#ifdef OLDSR
   memset (m_aSRFeature, 0, sizeof(m_aSRFeature));
#endif // 0
   DWORD dwSlice = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

#ifdef OLDSR
      if (!_wcsicmp(psz, gpszPhoneSlice) && (dwSlice < 3)) {
         // load in
         MMLValueGetBinary (pSub, gpszVoice, (PBYTE)&m_aSRFeature[dwSlice].acVoiceEnergy[0], sizeof(m_aSRFeature[dwSlice].acVoiceEnergy));
         MMLValueGetBinary (pSub, gpszNoise, (PBYTE)&m_aSRFeature[dwSlice].acNoiseEnergy[0], sizeof(m_aSRFeature[dwSlice].acNoiseEnergy));
         // increment
         dwSlice++;
      }
#else
      if (!_wcsicmp(psz, gpszSubPhone) && (dwSlice < SUBPHONEPERPHONE)) {
         if (!m_aSubPhone[dwSlice].MMLFrom (pSub))
               return FALSE;
         // increment
         dwSlice++;
      }
#endif
   }

#ifdef OLDSR
   if (dwSlice != 3)
      return FALSE;
   CalcEnergy();
#else
   if (dwSlice != SUBPHONEPERPHONE)
      return FALSE;
#endif

   return TRUE;
}

/*************************************************************************************
CPhoneInstance::CloneTo - Clones to the instance

inputs
   PCPhoneInstance         pNew - Clone to this one
returns
   BOOL - TRUE if success
*/
BOOL CPhoneInstance::CloneTo (CPhoneInstance *pNew)
{
   wcscpy (pNew->m_szBefore, m_szBefore);
   wcscpy (pNew->m_szAfter, m_szAfter);

   pNew->m_fTrainCount = m_fTrainCount;
   pNew->m_fAverageDuration = m_fAverageDuration;
   pNew->m_fRoughSketch = m_fRoughSketch;

#ifdef OLDSR
   memcpy (pNew->m_aSRFeature, m_aSRFeature, sizeof(m_aSRFeature));
   memcpy (pNew->m_afSRFEnergy, m_afSRFEnergy, sizeof(m_afSRFEnergy));
#else
   DWORD i;
   for (i = 0; i < SUBPHONEPERPHONE; i++)
      m_aSubPhone[i].CloneTo (&pNew->m_aSubPhone[i]);
#endif

   return TRUE;
}

/*************************************************************************************
CPhoneInstance::Clone - Stanrdar clone
*/
CPhoneInstance *CPhoneInstance::Clone (void)
{
   PCPhoneInstance pNew = new CPhoneInstance;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



/************************************************************************************
CPhoneInstance::PrepForMultiThreaded - Prepare this for multithreaded use so doesn't
crash if several threads.
*/
void CPhoneInstance::PrepForMultiThreaded (void)
{
   DWORD i;
   for (i = 0; i < SUBPHONEPERPHONE; i++)
      m_aSubPhone[i].PrepForMultiThreaded ();
}

/*************************************************************************************
CPhoneInstance::Compare - Compares the phoneme against the analyzed data.

NOTE: Assumes that is loaded with valid phoneme instance information.

inputs
   PSRFEATURE     paSRFSpeech - Pointer to a sequence of speech SRFEATURE to compare to
   fp             *pafSRFSpeech - Pointer to energy values for each element in paSRFSpeech
   DWORD          dwNum - Number of elements in paSRFSpeech and pafSRFSpeech
   fp             fMaxSpeechWindow - The maximum energy of the speech in the
                        period about 5 seconds before and after the speech to be
                        analyzed. This ensures the volume of the phoneme with
                        resepect to the overall volume of the speech is taken
                        into account
   BOOL                 fForUnitSelection - Set to TRUE if this is being used for unitselect, in which
                        case a much smaller TOPPERCENT is used so that units aren't selected based
                        on their mediocraty, but their basic "sounding like" the phoneme in question.
                        Use FALSE for actual phoneme boundary detection.
   BOOL                 fAllowFeatureDistortion - If TRUE then allow distortion. If FALSE then don't allow any volume change.
                        Use FALSE for picking units for TTS. Use TRUE for SR alignment
                        BUGFIX - Put this in since seems like wrong (bass-y) units selected for TTS voice
   BOOL                 fFast - If TRUE, and dwNum >= 12, then this skips every other phone,
                        potentially doubling SR speed.
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
returns
   fp - Score. Higher values indicate more error.
*/
#ifdef OLDSR

fp CPhoneInstance::Compare (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                             fp fMaxSpeechWindow)
{
   return SRFEATURECompareSequence (paSRFSpeech, pafSRFSpeech, dwNum, fMaxSpeechWindow,
                             &m_aSRFeature[0], m_afSRFEnergy[0],
                             &m_aSRFeature[1], m_afSRFEnergy[1],
                             &m_aSRFeature[2], m_afSRFEnergy[2]);
}

#else

fp CPhoneInstance::Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                            BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fFast,
                            BOOL fHalfExamples)
{
   // determine the average energy for the phoneme
   DWORD i;
   fp fAverage = 0;
   for (i = 0; i < dwNum; i++)
      fAverage += paSR[i].fEnergy;
   fAverage /= (fp)dwNum;

   if (fFast && (dwNum < SUBPHONEPERPHONE*3))
      fFast = FALSE; // too few points

   // BUGFIX - if the phoneme starts with a "*" then it's a rough-sketch
   // phone and only has one subphone
   DWORD dwNumSlots = m_fRoughSketch ? 1 : SUBPHONEPERPHONE;

   // loop through all the versions and train
   fp fSum = 0;
   for (i = 0; i < dwNumSlots; i++) {
      DWORD dwStart = i * dwNum / dwNumSlots;
      DWORD dwEnd = (i+1) * dwNum / dwNumSlots;

      // compare
      fSum += m_aSubPhone[i].Compare (paSR + dwStart, dwEnd - dwStart,
         fMaxSpeechWindow, fAverage, fForUnitSelection, fAllowFeatureDistortion, fFast,
         fHalfExamples) * (fp)(dwEnd - dwStart);
   } // i, dwNumSlots

   fSum /= (fp) dwNum;
   return fSum;
}

#endif // OLDSR

/*************************************************************************************
CPhoneInstance::Train - Trains this phoneme instance on the given section of time.

inputs
   PSRANALBLOCK   paSR - Array of dwNum SRANALBLOCK. paSR->plCache is ignored
//   PSRFEATURE     paSRFSpeech - Pointer to a sequence of speech SRFEATURE to train to
//   fp             *pafSRFSpeech - Pointer to energy values for each element in paSRFSpeech
   DWORD          dwNum - Number of elements in paSRFSpeech and pafSRFSpeech
   fp             fMaxSpeechWindow - The maximum energy of the speech in the
                        period about 5 seconds before and after the speech to be
                        analyzed. This ensures the volume of the phoneme with
                        resepect to the overall volume of the speech is taken
                        into account

   char           pszBefore - Phoneme before. Note... this will replace the old version
   char           pszAfter - Phoneme after. NOte... this will replace the old version
   fp             fWeight - Weight to train the phoneme. 1.0 = normal, but lower numbers will put
                              less emphasis on
returns
   BOOL - TRUE if success
*/
#ifdef OLDSR
BOOL CPhoneInstance::Train (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                            fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter)
#else
BOOL CPhoneInstance::Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter, fp fWeight)
#endif
{
   // must have at least a certain width
   if (!dwNum || !fMaxSpeechWindow)
      return FALSE;
   if (fWeight <= 0.0)
      return TRUE;   // ignore

   fMaxSpeechWindow = max(fMaxSpeechWindow, MINENERGYCOMPARE);
   fp fWeightNew = (fp)fWeight / (fp)(m_fTrainCount+fWeight);

#ifdef OLDSR
   // figure out the amount of scaling that need to do so that convert from
   // fMaxSpeechWindow to the training level
   fp fScale = (fp)0x10000 / fMaxSpeechWindow;

   // determine energy for beginning, middle, and end
   SRFEATURE aSRTrain[3];
   DWORD i, j;
   CMem memScratch;
   if (!memScratch.Required (dwNum * sizeof(fp)))
      return FALSE;
   fp *pafWeight = (fp*)memScratch.p;
   DWORD dwLeft = dwNum / 3;
   DWORD dwRight = dwNum - dwLeft;
   DWORD dwLeftUse = max(dwLeft, 1);
   DWORD dwRightUse = min(dwRight, dwNum-1);
   for (i = 0; i < 3; i++) {
      // compute the scores
      fp fSum = 0;
      for (j = 0; j < dwNum; j++) {
         switch (i) {
         case 0:  // left
            // BUGFIX - Not doing traiing this way because blurs the phoneme, plus
            // when recognize, blur it one step further, and in the end, it way too
            // blurry
            //pafWeight[j] = (fp)dwNum/2.0 - (fp)j;
            //pafWeight[j] = max(pafWeight[j], 0);
            pafWeight[j] = (j < dwLeftUse) ? 1 : 0;
            break;
         default:
         case 1: // middle
            //pafWeight[j] = (fp)dwNum/2.0 - fabs((fp)dwNum/2.0 - (fp)j);
            //pafWeight[j] = max(pafWeight[j], 0);
            pafWeight[j] = ((j >= dwLeft) && (j < dwRight)) ? 1 : 0;
            break;
         case 2: // right
            //pafWeight[j] = (fp)j - (fp)dwNum/2.0;
            //pafWeight[j] = max(pafWeight[j], 0);
            pafWeight[j] = (j >= dwRightUse) ? 1 : 0;
            break;
         }
         fSum += pafWeight[j];
      } // j

      // scale the weights
      fSum = 1.0 / fSum * fScale;
      for (j = 0; j < dwNum; j++)
         pafWeight[j] *= fSum;

      // average
      SRFEATUREInterpolate (paSRFSpeech, dwNum, pafWeight, &aSRTrain[i]);
   } // i

   // now, interpolate with exisiting model, figuring out how many times have
   // trained before
   SRFEATURE SRTemp;
   for (i = 0; i < 3; i++) {
      SRFEATUREInterpolate (&aSRTrain[i], &m_aSRFeature[i], fWeightNew, &SRTemp);
      m_aSRFeature[i] = SRTemp;
   }
#else
   // determine the average energy for the phoneme
   DWORD i;
   fp fAverage = 0;
   for (i = 0; i < dwNum; i++)
      fAverage += paSR[i].fEnergy;
   fAverage /= (fp)dwNum;

   // BUGFIX - if the phoneme starts with a "*" then it's a rough-sketch
   // phone and only has one subphone
   DWORD dwNumSlots = m_fRoughSketch ? 1 : SUBPHONEPERPHONE;

   // loop through all the versions and train
   for (i = 0; i < dwNumSlots; i++) {
      DWORD dwStart = i * dwNum / dwNumSlots;
      DWORD dwEnd = (i+1) * dwNum / dwNumSlots;

      // train this
      m_aSubPhone[i].Train (paSR + dwStart, dwEnd - dwStart, fMaxSpeechWindow, fAverage, fWeight);
   } // i, dwNumSlots
#endif // OLDSR

   // update the average duration and found
   m_fAverageDuration = fWeightNew * (fp)dwNum + (1.0 - fWeightNew) * m_fAverageDuration;
   m_fTrainCount += fWeight;

   // remember the name
   wcscpy (m_szBefore, pszBefore);
   wcscpy (m_szAfter, pszAfter);

#ifdef OLDSR
   CalcEnergy();
#endif

   return TRUE;
}


#ifndef OLDSR
/************************************************************************************
CPhoneInstance::FillWaveWithTraining - Fills the SRFEATUREs with training information.

inputs
   PSRFEATURE        pSRF - To be filled with training information. This buffer is
                     large enough. Could be NULL, in which case just checking
                     to see how much space is necessary.
returns
   DWORD - Number of SRFEATURE samples that need.
*/
DWORD CPhoneInstance::FillWaveWithTraining (PSRFEATURE pSRF)
{
   DWORD i;
   DWORD dwCount = 0;
   for (i = 0; i < SUBPHONEPERPHONE; i++)
      dwCount += m_aSubPhone[i].FillWaveWithTraining (pSRF ? (pSRF + dwCount) : NULL);

   return dwCount;
}
#endif // OLDSR



/*************************************************************************************
CPhoneme::Contructor and destructor
*/
CPhoneme::CPhoneme (void)
{
   m_szName[0] = 0;
   m_fHalfSize = FALSE;
   m_hPCPhoneInstance.Init (sizeof(PCPhoneInstance));
}

CPhoneme::~CPhoneme (void)
{
   Clear ();
}


/*************************************************************************************
CPhoneme::Clear - Free up existing phoneme instances
*/
void CPhoneme::Clear (void)
{
   DWORD i;
   for (i = 0; i < m_hPCPhoneInstance.Num(); i++) {
      PCPhoneInstance ppi = *((PCPhoneInstance*)m_hPCPhoneInstance.Get(i));
      delete ppi;
   } // i
   m_hPCPhoneInstance.Clear();
}



/************************************************************************************
CPhoneme::HalfSize - If you call this immediately after creating, the number of
examplar samples will be halved, saving memory (at the expense of quality)

inputs
   BOOL        fHalfSize - What to set to. TRUE to use 32 points, FALSE (defaults) to 64
*/
void CPhoneme::HalfSize (BOOL fHalfSize)
{
   m_fHalfSize = fHalfSize;

   DWORD i;
   for (i = 0; i < m_hPCPhoneInstance.Num(); i++) {
      PCPhoneInstance ppi = *((PCPhoneInstance*)m_hPCPhoneInstance.Get(i));
      ppi->HalfSize (fHalfSize);
   } // i
}


/************************************************************************************
CPhoneme::PrepForMultiThreaded - Prepare this for multithreaded use so doesn't
crash if several threads.
*/
void CPhoneme::PrepForMultiThreaded (void)
{
   DWORD i;
   for (i = 0; i < m_hPCPhoneInstance.Num(); i++) {
      PCPhoneInstance ppi = *((PCPhoneInstance*)m_hPCPhoneInstance.Get(i));
      ppi->PrepForMultiThreaded();
   } // i
}

/*************************************************************************************
CPhoneme::ToName - Converts stirngs of the surrounding phonemes to the phoneme
isntance's name.

inputs
   PWSTR       psz - 32 chars in size. Filled in with name
   PWSTR       pszLeft - Left phoneme.
   PWSTR       pszRight - Right phoneme
   PCMLexicon   pLex - Lexicon used to identify left/right phonemes
returns
   BOOL - TRUE if success
*/
BOOL CPhoneme::ToName (PWSTR psz, PWSTR pszLeft, PWSTR pszRight, PCMLexicon pLex)
{
   DWORD dwIndex = pLex->PhonemeFindUnsort (pszLeft);
   if (dwIndex == (DWORD)-1)
      return FALSE;
   PLEXPHONE plp = pLex->PhonemeGetUnsort(dwIndex);
   if (!plp)
      return FALSE;
   PLEXENGLISHPHONE ple = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
   if (!ple)
      return FALSE;
   DWORD dwLeft = LexPhoneGroupToMega(MLexiconEnglishGroupEvenSmaller(PIS_FROMPHONEGROUP(ple->dwShape)));

   // right
   dwIndex = pLex->PhonemeFindUnsort (pszRight);
   if (dwIndex == (DWORD)-1)
      return FALSE;
   plp = pLex->PhonemeGetUnsort(dwIndex);
   if (!plp)
      return FALSE;
   ple = MLexiconEnglishPhoneGet(plp->bEnglishPhone);
   if (!ple)
      return FALSE;
   DWORD dwRight = LexPhoneGroupToMega(MLexiconEnglishGroupEvenSmaller(PIS_FROMPHONEGROUP(ple->dwShape)));

   // make a string
   swprintf (psz, L"%d-%d", (int)dwLeft, (int)dwRight);
   return TRUE;
}



/*************************************************************************************
CPhoneme::ToName - Converts flags of the surrounding phonemes to the phoneme
isntance's name.

inputs
   PWSTR       psz - 32 chars in size. Filled in with name
   BOOL        fSilenceLeft - TRUE if left is silence
   BOOL        fSilenceRight - TRUE if right is silence
returns
   BOOL - TRUE if success
*/
BOOL CPhoneme::ToName (PWSTR psz, BOOL fSilenceLeft, BOOL fSilenceRight)
{
   psz[0] = fSilenceLeft ? L's' : L'n';
   psz[1] = L'-';
   psz[2] = fSilenceRight ? L's' : L'n';
   psz[3] = 0;
   return TRUE;
}



/*************************************************************************************
CPhoneme::ToNameRough - Fills in the same of the rough phoneme.

inputs
   PWSTR       psz - 32 chars in size. Filled in with name
returns
   BOOL - TRUE if success
*/
BOOL CPhoneme::ToNameRough (PWSTR psz)
{
   psz[0] = L'*';
   psz[1] = 0;
   return TRUE;
}



/*************************************************************************************
CPhone::MMLTo - Standard MMLTo
*/
static PWSTR gpszPhoneme = L"Phoneme";
static PWSTR gpszName = L"Name";
static PWSTR gpszNameHash = L"NameHash";
PCMMLNode2 CPhoneme::MMLTo (void)
{
   PCMMLNode2 pNew = new CMMLNode2;
   if (!pNew)
      return NULL;
   pNew->NameSet (gpszPhoneme);

   if (m_szName[0])
      MMLValueSet (pNew, gpszName, m_szName);

   DWORD i;
   for (i = 0; i < m_hPCPhoneInstance.Num(); i++) {
      PCPhoneInstance ppi = *((PCPhoneInstance*)m_hPCPhoneInstance.Get(i));
      PWSTR psz = m_hPCPhoneInstance.GetString (i);

      PCMMLNode2 pSub = ppi->MMLTo();
      if (!pSub)
         continue;
      MMLValueSet (pSub, gpszNameHash, psz);
      pNew->ContentAdd (pSub);
   }

   return pNew;
}






/*************************************************************************************
CPhoneme::MMLFrom - Stanard MMLFrom
*/
BOOL CPhoneme::MMLFrom (PCMMLNode2 pNode)
{
   DWORD i;
   Clear();
   m_szName[0] = 0;

   // names
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      wcscpy (m_szName, psz);

   // elements
   PCMMLNode2 pSub;
   DWORD dwVersion = 0;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszPhoneInstance)) {
         PCPhoneInstance ppi = new CPhoneInstance;
         if (!ppi)
            continue;
         ppi->MMLFrom (pSub);
         if (m_fHalfSize)
            ppi->HalfSize (m_fHalfSize);

         PWSTR pszName = MMLValueGet (pSub, gpszNameHash);
         if (!pszName) {
            delete ppi;
            continue;   // ignore since no proper name
         }

         m_hPCPhoneInstance.Add (pszName, &ppi);
      }
   }

   return TRUE;
}

/*************************************************************************************
CPhoneme::Clone - Stanrdar clone
*/
CPhoneme *CPhoneme::Clone (void)
{
   PCPhoneme pNew = new CPhoneme;
   if (!pNew)
      return NULL;
   wcscpy (pNew->m_szName, m_szName);

   pNew->m_fHalfSize = m_fHalfSize;

   // clone
   pNew->Clear();
   m_hPCPhoneInstance.CloneTo (&pNew->m_hPCPhoneInstance);
   DWORD i;
   for (i = 0; i < pNew->m_hPCPhoneInstance.Num(); i++) {
      PCPhoneInstance *pppi = (PCPhoneInstance*)pNew->m_hPCPhoneInstance.Get(i);
      *pppi = (*pppi)->Clone();
   } // i

   return pNew;
}

/*************************************************************************************
CPhoneme::PhonemeInstanceGet - Gets a phoneme instance based on whether what occurred
before and after were silence.

inputs
   BOOL           fPrevPhoneSilence - TRUE if the previous phone is silence, FALSE if voiced phoneme
   BOOL           fNextPhoneSilence - TRUE if next phone is silence
   BOOL           fExact - If TRUE then fail if can't find exact match
returns
   PCPhoneInstance - Instance, or NULL if none
*/
PCPhoneInstance CPhoneme::PhonemeInstanceGet (BOOL fPrevPhoneSilence, BOOL fNextPhoneSilence, BOOL fExact)
{
   WCHAR szTemp[32];
   DWORD i;
   // loop through the primary choice, then back off
   DWORD dwNum = fExact ? 1 : 4;
   for (i = 0; i < dwNum; i++) {
      BOOL bLeft = fPrevPhoneSilence;
      BOOL bRight = fNextPhoneSilence;
      if (i & 0x01)
         bLeft = !bLeft;
      if (i & 0x02)
         bRight = !bRight;

      if (!ToName (szTemp, bLeft, bRight))
         continue;

      PCPhoneInstance *pppi = (PCPhoneInstance*)m_hPCPhoneInstance.Find (szTemp);
      if (!pppi)
         continue;
      PCPhoneInstance ppi = *pppi;
      if (!ppi->m_fTrainCount)
         continue;

      return ppi;
   } // i

   return NULL;
}


/*************************************************************************************
CPhoneme::PhonemeInstanceGet - Gets a phoneme instance based on whether what occurred
before and after were silence.

inputs
   PWSTR       pszLeft - Left
   PWSTR       pszRight - Right
   PCMLexicon  pLex - Lexicon
returns
   PCPhoneInstance - Instance, or NULL if none
*/
PCPhoneInstance CPhoneme::PhonemeInstanceGet (PWSTR pszLeft, PWSTR pszRight, PCMLexicon pLex)
{
   WCHAR szTemp[32];

   if (!ToName (szTemp, pszLeft, pszRight, pLex))
      goto silence;

   PCPhoneInstance *pppi = (PCPhoneInstance*)m_hPCPhoneInstance.Find (szTemp);
   if (!pppi)
      goto silence;
   PCPhoneInstance ppi = *pppi;
   if (!ppi->m_fTrainCount)
      goto silence;

   return ppi;

silence:
   // backoff to larger model
   PCWSTR pszSilence = pLex->PhonemeSilence ();
   return PhonemeInstanceGet (!_wcsicmp(pszLeft,pszSilence), !_wcsicmp(pszRight,pszSilence));
}



/*************************************************************************************
CPhoneme::PhonemeInstanceGetRough - Gets the rough version of the phoneme

inputs
   none
returns
   PCPhoneInstance - Instance, or NULL if none
*/
PCPhoneInstance CPhoneme::PhonemeInstanceGetRough (void)
{
   WCHAR szTemp[32];

   if (!ToNameRough (szTemp))
      return NULL;

   PCPhoneInstance *pppi = (PCPhoneInstance*)m_hPCPhoneInstance.Find (szTemp);
   if (!pppi)
      return NULL;
   PCPhoneInstance ppi = *pppi;
   if (!ppi->m_fTrainCount)
      return NULL;

   return ppi;
}


/*************************************************************************************
CPhoneme::QueryNeedTraining - Fills in the flags indicating if training is needed
for the phoneme

inputs
   DWORD          *pdwNumStart - Number of training instances for with phoneme at start
   DWORD          *pdwNumMiddle - Number with in middle
   DWORD          *pdwNumEnd - Number with at end
   PCMLexicon     pLex - Lexicon to use
*/
void CPhoneme::QueryNeedTraining (DWORD *pdwNumStart, DWORD *pdwNumMiddle, DWORD *pdwNumEnd,
                                  PCMLexicon pLex)
{
   *pdwNumStart = *pdwNumMiddle = *pdwNumEnd = 0;

   PCWSTR pszSilence = pLex->PhonemeSilence();

   // loop through all the instances
   DWORD i;
   BOOL fSilStart, fSilEnd;
   WCHAR szTemp[32];
   for (i = 0; i < 3; i++) {  // note: intentionally not doing phoneme by itself
      fSilStart = (i & 0x01) ? TRUE : FALSE;
      fSilEnd = (i & 0x02) ? TRUE : FALSE;
      if (!ToName (szTemp, fSilStart, fSilEnd))
         continue;
      if (!m_hPCPhoneInstance.Find (szTemp))
         continue;

      if (fSilStart)
         (*pdwNumStart) += 1;
      else if (fSilEnd)
         (*pdwNumEnd) += 1;
      else
         (*pdwNumMiddle) += 1;
   } // i
   
}

/*************************************************************************************
CPhoneme::Train - Trains this phoneme instance on the given section of time.

inputs
   PSRFEATURE     paSRFSpeech - Pointer to a sequence of speech SRFEATURE to train to
   fp             *pafSRFSpeech - Pointer to energy values for each element in paSRFSpeech
   DWORD          dwNum - Number of elements in paSRFSpeech and pafSRFSpeech
   fp             fMaxSpeechWindow - The maximum energy of the speech in the
                        period about 5 seconds before and after the speech to be
                        analyzed. This ensures the volume of the phoneme with
                        resepect to the overall volume of the speech is taken
                        into account
   PWSTR          pszBefore - Phoneme before
   PWSTR          pszAfter - Phoneme after
   PCMLexicon     pLex - lexicon
   BOOL           fCIOnly - If TRUE, then train only the context-independent phonemes
   fp             fWeight - How much to weight this in training. 1.0 = default. Lower values
                        weight training data less
returns
   BOOL - TRUE if success. FALSE if error
*/
#ifdef OLDSR
BOOL CPhoneme::Train (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                        fp fMaxSpeechWindow,
                        WCHAR *pszBefore, WCHAR *pszAfter, PCMLexicon pLex)
#else
BOOL CPhoneme::Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter, PCMLexicon pLex,
                            BOOL fCIOnly, fp fWeight)
#endif // OLDSR
{
   // two passes
   DWORD i;
   WCHAR szTemp[64];
   PCWSTR pszSilence = pLex->PhonemeSilence();
   BOOL fReturn = TRUE;
   for (i = 0; i < (DWORD)(fCIOnly ? 2 : 3); i++) {
      BOOL fRet;
      if (i == 0)
         // training rough-sketch
         fRet = ToNameRough (szTemp);
      else if (i == 1)
         // training CI phone
         fRet = ToName (szTemp, !_wcsicmp(pszSilence, pszBefore), !_wcsicmp(pszSilence, pszAfter));
      else // if (i == 2)
         fRet = ToName (szTemp, pszBefore, pszAfter, pLex);
      if (!fRet)
         continue;   // error

      PCPhoneInstance *pppi = (PCPhoneInstance*)m_hPCPhoneInstance.Find (szTemp);
      PCPhoneInstance ppi;
      if (pppi)
         ppi = *pppi;
      else {
         // new one
         ppi = new CPhoneInstance;
         if (!ppi)
            continue;
         if (m_fHalfSize)
            ppi->HalfSize (m_fHalfSize);


         if (!i)
            ppi->m_fRoughSketch = TRUE;

         wcscpy (ppi->m_szBefore, pszBefore);
         wcscpy (ppi->m_szAfter, pszAfter);
         m_hPCPhoneInstance.Add (szTemp, &ppi);
      }
      // train it
#ifdef OLDSR
      fReturn &= ppi->Train (paSRFSpeech, pafSRFSpeech, dwNum, fMaxSpeechWindow,
         pszBefore, pszAfter);
#else
      fReturn &= ppi->Train (paSR, dwNum, fMaxSpeechWindow, pszBefore, pszAfter, fWeight);
#endif // OLDSR
   } // i

   return fReturn;
}

/*************************************************************************************
CPhoneme::Compare - Sweeps over the whole time range calculating the scores for all
the sub-phonemes.

inputs
   PSRFEATURE     paSRFSpeech - Pointer to a sequence of speech SRFEATURE to compare to
   fp             *pafSRFSpeech - Pointer to energy values for each element in paSRFSpeech
   DWORD          dwNum - Number of elements in paSRFSpeech and pafSRFSpeech
   fp             fMaxSpeechWindow - The maximum energy of the speech in the
                        period about 5 seconds before and after the speech to be
                        analyzed. This ensures the volume of the phoneme with
                        resepect to the overall volume of the speech is taken
                        into account
   PWSTR          pszBefore - Phoneme before
   PWSTR          pszAfter - Pheoneme after
   PCMLexicon     pLex - Lexicon
   BOOL                 fForUnitSelection - Set to TRUE if this is being used for unitselect, in which
                        case a much smaller TOPPERCENT is used so that units aren't selected based
                        on their mediocraty, but their basic "sounding like" the phoneme in question.
                        Use FALSE for actual phoneme boundary detection.
   BOOL                 fAllowFeatureDistortion - If TRUE then allow distortion. If FALSE then don't allow any volume change.
                        Use FALSE for picking units for TTS. Use TRUE for SR alignment
                        BUGFIX - Put this in since seems like wrong (bass-y) units selected for TTS voice
   BOOL           fForceCI - If TRUE then only ever compares context-independent models. If FALSE (default)
                        then first tries for context dependent.
   BOOL           fFast - If TRUE, and if there are enough samples, this will do fast recognition,
                        skipping every other SR feature.
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
returns
   fp - Score. Higher values indicate more error.
*/
#ifdef OLDSR
fp CPhoneme::Compare (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                        fp fMaxSpeechWindow,
                        BOOL fPrevPhoneSilence, BOOL fNextPhoneSilence)
#else
fp CPhoneme::Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                        PWSTR pszBefore, PWSTR pszAfter, PCMLexicon pLex,
                        BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fForceCI, BOOL fFast,
                        BOOL fHalfExamples)
#endif // OLDSR
{

// #ifdef _WIN64 - to see if repro crash quickly
//   return 1;
// #endif

   PCPhoneInstance ppi;
   if (fForceCI) {
      PCWSTR pszSilence = pLex->PhonemeSilence ();
      ppi = PhonemeInstanceGet (!_wcsicmp(pszBefore,pszSilence), !_wcsicmp(pszAfter,pszSilence));
   }
   else
      ppi = PhonemeInstanceGet (pszBefore, pszAfter, pLex);
   if (!ppi)
      return 1000000;   // so wont accept
   PCPhoneInstance ppiRough = PhonemeInstanceGetRough ();

#ifdef OLDSR
   return ppi->Compare (paSRFSpeech, pafSRFSpeech, dwNum, fMaxSpeechWindow);
#else
   fp fScore = ppi->Compare (paSR, dwNum, fMaxSpeechWindow, fForUnitSelection, fAllowFeatureDistortion, fFast,
      fHalfExamples);

   // include the rough phoneme so that triphones and bins can never get too far
   // away from basics
   if (ppiRough)
      fScore = fScore * (1.0 - CIBACKOFFWEIGHT) + ppiRough->Compare (paSR, dwNum, fMaxSpeechWindow, fForUnitSelection, fAllowFeatureDistortion, fFast,
         fHalfExamples) * CIBACKOFFWEIGHT;

   return fScore;
#endif OLDSR
}


/*************************************************************************************
CVoiceFile::Constructor and destructor
*/
CVoiceFile::CVoiceFile (void)
{
   m_hPCPhoneme.Init (sizeof(PCPhoneme));
   m_pTextParse = new CTextParse;
   m_szLexicon[0] = 0;
   m_szDefTraining[0] = 0;
   m_szFile[0] = 0;
   m_fForTTS = FALSE;
   m_fCDPhone = FALSE;
   m_pWaveDir = new CWaveDirInfo;
   m_pWaveToDo = new CWaveToDo;
   m_pLex = NULL;
   m_dwTooShortWarnings = 0;
   if (m_pTextParse)
      m_pTextParse->Init (MAKELANGID(LANG_ENGLISH,0), NULL);
}

CVoiceFile::~CVoiceFile (void)
{
   if (m_pTextParse)
      delete m_pTextParse;

   if (m_pWaveDir)
      delete m_pWaveDir;
   m_pWaveDir = NULL;
   if (m_pWaveToDo)
      delete m_pWaveToDo;
   m_pWaveToDo = NULL;

   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   m_pLex = NULL;

   DWORD i;
   for (i = 0; i < m_hPCPhoneme.Num(); i++) {
      PCPhoneme pp = *((PCPhoneme*)m_hPCPhoneme.Get(i));
      delete pp;
   } // i
}


/*************************************************************************************
CVoiceFile::MMLTo - Standard MMLTo
*/
static PWSTR gpszVoiceFile = L"VoiceFile";
static PWSTR gpszLexicon = L"Lexicon";
static PWSTR gpszDefTraining = L"DefTraining";
static PWSTR gpszForTTS = L"ForTTS";
static PWSTR gpszCDPhone = L"CDPhone";
static PWSTR gpszWaveDir = L"WaveDir";
static PWSTR gpszWaveToDo = L"WaveToDo";

PCMMLNode2 CVoiceFile::MMLTo (void)
{
   PCMMLNode2 pNew = new CMMLNode2;
   if (!pNew)
      return NULL;
   pNew->NameSet (gpszVoiceFile);

   if (m_szLexicon[0])
      MMLValueSet (pNew, gpszLexicon, m_szLexicon);
   if (m_szDefTraining[0])
      MMLValueSet (pNew, gpszDefTraining, m_szDefTraining);

   MMLValueSet (pNew, gpszForTTS, (int)m_fForTTS);
   MMLValueSet (pNew, gpszCDPhone, (int)m_fCDPhone);

   PCMMLNode2 pSub;
   pSub = m_pWaveDir->MMLTo();
   pSub->NameSet (gpszWaveDir);
   pNew->ContentAdd (pSub);

   pSub = m_pWaveToDo->MMLTo();
   pSub->NameSet (gpszWaveToDo);
   pNew->ContentAdd (pSub);

   DWORD i;
   for (i = 0; i < m_hPCPhoneme.Num(); i++) {
      PCPhoneme pp = *((PCPhoneme*)m_hPCPhoneme.Get(i));
      PCMMLNode2 pSub = pp->MMLTo();
      if (!pSub)
         continue;
      pNew->ContentAdd (pSub);
   }

   return pNew;
}

/*************************************************************************************
CVoiceFile::MMLFrom - Stanard MMLFrom

inputs
   PCMMLNode2         pNode - Node to read from
   PWSTR             pszSrcFile - If the main lexicon doesnt exist, then the root
                           directory is taken from this and used
*/
BOOL CVoiceFile::MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile)
{
   DWORD i;
   for (i = 0; i < m_hPCPhoneme.Num(); i++) {
      PCPhoneme pp = *((PCPhoneme*)m_hPCPhoneme.Get(i));
      delete pp;
   }
   m_hPCPhoneme.Clear();

   // clear training
   m_pWaveDir->ClearFiles();
   m_pWaveToDo->ClearToDo();

   PWSTR psz;

   m_szDefTraining[0] = 0;
   psz = MMLValueGet (pNode, gpszDefTraining);
   if (psz)
      wcscpy (m_szDefTraining, psz);

   m_fForTTS = (BOOL)MMLValueGetInt (pNode, gpszForTTS, FALSE);
   m_fCDPhone = (BOOL)MMLValueGetInt (pNode, gpszCDPhone, FALSE);

   m_szLexicon[0] = 0;
   psz = MMLValueGet (pNode, gpszLexicon);
   if (psz)
      wcscpy (m_szLexicon, psz);
   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   m_pLex = NULL;
   if (!LexiconExists (m_szLexicon, pszSrcFile))
      return FALSE;

   // clear the text parse
   if (m_pTextParse) {
      delete m_pTextParse;
      m_pTextParse = NULL;
   }


   // elements
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszPhoneme)) {
         PCPhoneme pSlice = new CPhoneme;
         if (!pSlice)
            continue;
         if (!pSlice->MMLFrom (pSub)) {
            delete pSlice;
            continue;
         }
         m_hPCPhoneme.Add (pSlice->m_szName, &pSlice);
      }
      else if (!_wcsicmp(psz, gpszWaveDir)) {
         m_pWaveDir->MMLFrom (pSub);
      }
      else if (!_wcsicmp(psz, gpszWaveToDo)) {
         m_pWaveToDo->MMLFrom (pSub);
      }
   }

   LexiconRequired ();

   // create the text parse based on the language
   m_pTextParse = new CTextParse;
   if (!m_pTextParse)
      return FALSE;
   if (!m_pTextParse->Init (m_pLex ? m_pLex->LangIDGet() : MAKELANGID(LANG_ENGLISH,0),
      m_pLex))
      return FALSE;

   return TRUE;
}

/*************************************************************************************
CVoiceFile::Clone - Stanrdar clone
*/
CVoiceFile *CVoiceFile::Clone (void)
{
   PCVoiceFile pNew = new CVoiceFile;
   if (!pNew)
      return NULL;

   // free old collection
   DWORD i;
   for (i = 0; i < pNew->m_hPCPhoneme.Num(); i++) {
      PCPhoneme pp = *((PCPhoneme*)m_hPCPhoneme.Get(i));
      delete pp;
   }
   pNew->m_hPCPhoneme.Clear();

   // fil lup
   m_hPCPhoneme.CloneTo (&pNew->m_hPCPhoneme);
   for (i = 0; i < pNew->m_hPCPhoneme.Num(); i++) {
      PCPhoneme *pp = (PCPhoneme*)pNew->m_hPCPhoneme.Get(i);
      *pp = (*pp)->Clone();
   } // i

   if (pNew->m_pTextParse)
      delete pNew->m_pTextParse;
   pNew->m_pTextParse = m_pTextParse->Clone();

   wcscpy (pNew->m_szFile, m_szFile);
   wcscpy (pNew->m_szDefTraining, m_szDefTraining);
   pNew->m_fForTTS = m_fForTTS;
   pNew->m_fCDPhone = m_fCDPhone;

   if (pNew->m_pWaveDir)
      delete pNew->m_pWaveDir;
   pNew->m_pWaveDir = m_pWaveDir->Clone();

   if (pNew->m_pWaveToDo)
      delete pNew->m_pWaveToDo;
   pNew->m_pWaveToDo = m_pWaveToDo->Clone();

   wcscpy (pNew->m_szLexicon, m_szLexicon);
   if (pNew->m_pLex)
      MLexiconCacheClose (pNew->m_pLex);
   pNew->m_pLex = NULL;

   return pNew;
}


/************************************************************************************
CVoiceFile::PrepForMultiThreaded - Prepare this for multithreaded use so doesn't
crash if several threads.
*/
void CVoiceFile::PrepForMultiThreaded (void)
{
   DWORD i;
   for (i = 0; i < m_hPCPhoneme.Num(); i++) {
      PCPhoneme *pp = (PCPhoneme*)m_hPCPhoneme.Get(i);
      (*pp)->PrepForMultiThreaded();
   } // i
}

/*************************************************************************************
CVoiceFile::LexiconRequired - Loads the lexicon if it isn't already loaded.
Returns TRUE if success, FALSE if error
*/
BOOL CVoiceFile::LexiconRequired (void)
{
   if (m_pLex)
      return TRUE;

   m_pLex = MLexiconCacheOpen(m_szLexicon, FALSE);
   return (m_pLex ? TRUE : FALSE);
}

/*************************************************************************************
CVoiceFile::LexiconSet - Sets a new lexicon to use

inputs
   PWSTR          pszLexicon - Lexicon
returns
   BOOL - TRUE if was able to open, FALSE if not
*/
BOOL CVoiceFile::LexiconSet (PWSTR pszLexicon)
{
   if (m_pLex)
      MLexiconCacheClose (m_pLex);
   wcscpy (m_szLexicon, pszLexicon);

   BOOL fRet = LexiconRequired ();
   m_pTextParse->Init (m_pLex ? m_pLex->LangIDGet() : MAKELANGID(LANG_ENGLISH,0), m_pLex);


   return fRet;
}

/*************************************************************************************
CVoiceFile::LexiconGet - Returns a pointer to the lexicon string. DO NOT modify it.
*/
PWSTR CVoiceFile::LexiconGet (void)
{
   return m_szLexicon;
}

/*************************************************************************************
CVoiceFile::Lexicon - Returns a pointer to the lexicon to use for SR. NULL if cant open
*/
PCMLexicon CVoiceFile::Lexicon (void)
{
   LexiconRequired ();
   return m_pLex;
}



/*************************************************************************************
CVoiceFile::Save - Saves the file

inputs
   PWSTR          szFile - Fle to save as, or use NULL to rely upon m_szFile
*/
BOOL CVoiceFile::Save (WCHAR *szFile)
{
   if (!szFile)
      szFile = m_szFile;

   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;
   BOOL fRet;
   fRet = MMLFileSave (szFile, &GUID_VoiceFile, pNode);
   delete pNode;

   if (fRet && (szFile != m_szFile))
      wcscpy (m_szFile, szFile);

   return fRet;
}


/*************************************************************************************
CVoiceFile::Open - Opens the file

inputs
   WCHAR              *szFile - FIle. This file name is rememberd in to m_szFile.
                        If NULL, then open default voice file, VoiceFileDefaultGet()
   PWSTR             pszSrcFile - If the main lexicon doesnt exist, then the root
                           directory is taken from this and used
*/
BOOL CVoiceFile::Open (WCHAR *szFile)
{
   WCHAR szDefault[256];
   if (!szFile) {
      VoiceFileDefaultGet (szDefault, sizeof(szDefault)/sizeof(WCHAR));
      if (!szDefault[0])
         return FALSE;
      szFile = szDefault;
   }

   PCMMLNode2 pNode = MMLFileOpen (szFile, &GUID_VoiceFile);
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


/*************************************************************************************
CVoiceFile::PhonemeGet - Returns a pointer to the phoneme object.

inputs
   char           *pszName - Phoneme name
   BOOL           fCreateIfNotExist - If TRUE and the phoneme doens't exist then
                  it will be created. If FALSE and the phoneme doesnt exist then
                  NULL is returned
*/
PCPhoneme CVoiceFile::PhonemeGet (WCHAR *pszName, BOOL fCreateIfNotExist)
{
#if 0 // no longer use
   if (fMatchUnstressed) {
      // convert from this phoneme to the unstressed version
      PLEXPHONE plp2, plp = m_pLex->PhonemeGetUnsort (m_pLex->PhonemeFindUnsort(pszName));
      if (!plp)
         return NULL;
      if (plp->bStress && (plp2 = m_pLex->PhonemeGetUnsort(plp->wPhoneOtherStress)))
         pszName = plp2->szPhone;   // user other stress
   }
#endif // 0


   // find it
   PCPhoneme *pps = (PCPhoneme*) m_hPCPhoneme.Find (pszName);
   if (pps)
      return *pps;

   // doesnt exist
   if (!fCreateIfNotExist)
      return NULL;

   // create
   PCPhoneme pPhone;
   pPhone = new CPhoneme;
   if (!pPhone)
      return NULL;
   wcscpy (pPhone->m_szName, pszName);
   m_hPCPhoneme.Add (pPhone->m_szName, &pPhone);

   return pPhone;
}

/*************************************************************************************
CVoiceFile::PhonemeRemove - Removes a phoneme based on its name

inputs
   char           *pszName - Phoneme name
*/
BOOL CVoiceFile::PhonemeRemove (WCHAR *pszName)
{
   // find it
   DWORD dwIndex = m_hPCPhoneme.FindIndex (pszName);
   if (dwIndex == (DWORD)-1)
      return FALSE;
   PCPhoneme pp = *((PCPhoneme*) m_hPCPhoneme.Get(dwIndex));
   delete pp;
   m_hPCPhoneme.Remove (dwIndex);

   return TRUE;
}


/*************************************************************************************
CVoiceFile::PhonemeNum - Returns the number of phonemes
*/
DWORD CVoiceFile::PhonemeNum (void)
{
   return m_hPCPhoneme.Num();
}

/*************************************************************************************
CVoiceFile::PhonemeGet - Gets a phoneme based on the index

inputs
   DWORD       dwINdex - from 0 to PhonemeNum()-1
returns
   pCPhoneme  - phoneme
*/
PCPhoneme CVoiceFile::PhonemeGet (DWORD dwIndex)
{
   PCPhoneme *pps = (PCPhoneme*) m_hPCPhoneme.Get(dwIndex);
   return pps ? pps[0] : NULL;
}



/*************************************************************************************
CVoiceFile::TrainSingleWave - Does training based on the wave.

inputs
   PCM3DWave         pWave - Wave to use. Must have phoneme segments marked
   HWND              hWnd - Window to show progress on, and to ask questions from
   PCProgressSocket  pProgress - Progress bar to use. NULL if pull up one by default.
   BOOL              fLessPickey - If used then training will be less picky about phonme timings
   BOOL              fSimple - If TRUE then simple UI is used
   PCHashString      phFunctionWords - Hash of function words. The hash points to floating
                     point value (fp) with the weight (0.0 to 1.0) of how much the word's phonemes
                     should affect training. If the word isn't found, 1.0 is used
   BOOL              fFillFunctionWords - Normally this is FALSE. However, as an internal-use
                     method, if this is true, phFunctionWords should be initialized to sizeof(fp).
                     As new words are found in the wave, the count of the value is increased.
                     This won't actually do any training yet, since a second pass should take
                     the massaged phFunctionWords to be used as weights.
returns
   BOOL - TRUE if success
*/
BOOL CVoiceFile::TrainSingleWave (PCM3DWave pWave, HWND hWnd, PCProgress pProgress, BOOL fLessPickey, BOOL fSimple,
                                  PCHashString phFunctionWords, BOOL fFillFunctionWords)
{
   // make file WCHAR
   WCHAR szFile[256];
   MultiByteToWideChar (CP_ACP, 0, pWave->m_szFile, -1, szFile, sizeof(szFile)/sizeof(WCHAR));

   // should be mono and 22khz+
   if (pWave->m_dwChannels >= 2) {
      EscMessageBox (hWnd, ASPString(),
         L"Training must be on monaural (not stereo) audio.",
         szFile,
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   if (pWave->m_dwSamplesPerSec < 16000) {
      EscMessageBox (hWnd, ASPString(),
         L"You must have at least a 16 kHz recording to train.",
         szFile,
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   if (!LexiconRequired ()) {
      EscMessageBox (hWnd, ASPString(),
         L"The lexicon file can't be loaded.",
         L"Speech recognition needs the lexicon file to work.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   // get rid of silence phonemes
   pWave->PhonemeGetRidOfShortSilence();

   // do sr training data
   pWave->CalcSRFeaturesIfNeeded (WAVECALC_SEGMENT, hWnd, pProgress);

   // if filling the function words then special code
   DWORD i;
   if (fFillFunctionWords) {
      DWORD dwNumWave = pWave->m_lWVWORD.Num();
      for (i = 0; i < dwNumWave; i++) {
         PWVWORD pWord = (PWVWORD)pWave->m_lWVWORD.Get(i);
         PWSTR psz = (PWSTR)(pWord+1);
         if (!psz[0])
            continue;   // just silence

         // else, a word
         fp *pfWeight = (fp*)phFunctionWords->Find(psz);
         if (pfWeight)
            *pfWeight = *pfWeight + 1; // increment count of words
         else {
            fp fWeight = 1.0;
            phFunctionWords->Add (psz, &fWeight);
         }
      } // i
      return TRUE;
   }


   // calculate the energy for all of the phones
   DWORD dwValidPhones, dwNum;

#ifdef OLDSR
   CMem memEnergy;
   if (!memEnergy.Required (pWave->m_dwSRSamples * sizeof(fp)))
      return FALSE;
   fp *pafEnergy = (fp*)memEnergy.p;
   fp fMaxEnergy = 0;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      pafEnergy[i] = SRFEATUREEnergy(&pWave->m_paSRFeature[i]);
      fMaxEnergy = max (fMaxEnergy, pafEnergy[i]);
         // note - could potentially look at the energy for the last N seconds to test
         // this, but might as well do the entire wave
   }
#endif // OLDSR

   // look through phonemes for suspicious stuff
   PCWSTR pszSilence = m_pLex->PhonemeSilence ();
   PWVPHONEME pp;
   dwValidPhones = 0;
   dwNum = pWave->m_lWVPHONEME.Num();
   pp = (PWVPHONEME)pWave->m_lWVPHONEME.Get(0);
   WCHAR szTemp[512];
   for (i = 0; i+1 < dwNum; i++) {
      DWORD dwLength;
      dwLength = pp[i+1].dwSample - pp[i].dwSample;

      WCHAR szName[16];
      memset (szName, 0, sizeof(szName));
      memcpy (szName, pp[i].awcNameLong, sizeof(pp[i].awcNameLong));

      // if it's silence then skip
      if (!_wcsicmp(szName, pszSilence)) {
         if (!fSimple && !fLessPickey && (i && (dwLength < pWave->m_dwSamplesPerSec / 25))) {
            swprintf (szTemp, L"In the wave file, %s, you have a very short silence between words at %g sec.",
               szFile,
               (double) pp[i].dwSample / (double)pWave->m_dwSamplesPerSec);
            if (IDCANCEL == EscMessageBox (hWnd, ASPString(),
               szTemp,
               L"You have a very short silence (less than 1/25th second) between words. "
               L"Short silences should be treated as within one of the phonemes.",
               MB_ICONEXCLAMATION | MB_OKCANCEL))
                  return FALSE;
            return TRUE;   // so continues
         }
         continue;
      }

      // length too long?
      if (!fSimple && !fLessPickey && (dwLength+10 < pWave->m_dwSamplesPerSec / 50) && (m_dwTooShortWarnings < 10)) { // BUGFIX bit of rounoff err
         swprintf (szTemp, L"In the wave file, %s, one of the phonemes is too short at %g sec.",
            szFile,
            (double) pp[i].dwSample / (double)pWave->m_dwSamplesPerSec);

         // BUGGIX - Don't provide too many too-short wanrings
         m_dwTooShortWarnings++;

         if (IDCANCEL == EscMessageBox (hWnd, ASPString(),
            szTemp,
            L"Phonemes should be longer than 1/50th of a second (.02 sec).",
            MB_ICONEXCLAMATION | MB_OKCANCEL))
               return FALSE;
         return TRUE;   // so continues
      }
      if (!fSimple && !fLessPickey && (dwLength > pWave->m_dwSamplesPerSec /*/ 2*/)) {
               // BUGFIX - For chinese, allow phones to be a second long
         swprintf (szTemp, L"In the wave file, %s, one of the phonemes is too long at %g sec.",
            szFile,
            (double) pp[i].dwSample / (double)pWave->m_dwSamplesPerSec);
         if (IDCANCEL == EscMessageBox (hWnd, ASPString(),
            szTemp,
            L"Phonemes should be shorter than 1/2 of a second (.2 sec).",
            MB_ICONEXCLAMATION | MB_OKCANCEL))
               return FALSE;
         return TRUE;   // so continues
      }

      // make sure it's a valid phoneme
      DWORD dwPhone = m_pLex->PhonemeFindUnsort (szName);
      if (dwPhone == -1) {
         swprintf (szTemp, L"The phonemes, %s, is not used by this lexicon.",
            szName);
         if (IDCANCEL == EscMessageBox (hWnd, ASPString(),
            szTemp,
            szFile,
            MB_ICONEXCLAMATION | MB_OKCANCEL))
               return FALSE;
         return TRUE;   // so continues
      }

      dwValidPhones++;
   }
   if (!dwValidPhones) {
      swprintf (szTemp, L"In the wave file, %s, there aren't any phonemes marked out.",
         szFile);
      if (!fSimple) {
         if (IDCANCEL == EscMessageBox (hWnd, ASPString(),
            szTemp,
            L"You need to mark out where the phonemes begin and end.",
            MB_ICONEXCLAMATION | MB_OKCANCEL))
               return FALSE;
      }
      return TRUE;   // so continues
   }

#ifndef OLDSR
   CSRAnal SRAnal;
   fp fMaxEnergy;
   PSRANALBLOCK psab = SRAnal.Init (pWave->m_paSRFeature, pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif

   DWORD dwNumWord = pWave->m_lWVWORD.Num();

   // training
   DWORD j;
   for (i = 0; i+1 < dwNum; i++) {
      WCHAR szBefore[16], szAfter[16], szName[16];
      memset (szBefore, 0 ,sizeof(szBefore));
      memset (szAfter, 0, sizeof(szAfter));
      memset (szName, 0, sizeof(szName));
      if (i)
         memcpy (szBefore, pp[i-1].awcNameLong, sizeof(pp[i-1].awcNameLong));
      else
         wcscpy (szBefore, pszSilence);
      memcpy (szAfter, pp[i+1].awcNameLong, sizeof(pp[i+1].awcNameLong));
      memcpy (szName, pp[i].awcNameLong, sizeof(pp[i].awcNameLong));
      if (!_wcsicmp(szName, pszSilence))
         continue;   // if silence then skip

#if 0 // since now use stress
      // now, go through and get rid of stressed phonemes so have a smaller training set
      PLEXPHONE plp, plp2;
      plp = m_pLex->PhonemeGetUnsort (m_pLex->PhonemeFindUnsort (szName));
      if (plp && plp->bStress && (plp2 = m_pLex->PhonemeGetUnsort(plp->wPhoneOtherStress)))
         wcscpy (szName, plp2->szPhone);
      plp = m_pLex->PhonemeGetUnsort (m_pLex->PhonemeFindUnsort (szBefore));
      if (plp && plp->bStress && (plp2 = m_pLex->PhonemeGetUnsort(plp->wPhoneOtherStress)))
         wcscpy (szBefore, plp2->szPhone);
      plp = m_pLex->PhonemeGetUnsort (m_pLex->PhonemeFindUnsort (szAfter));
      if (plp && plp->bStress && (plp2 = m_pLex->PhonemeGetUnsort(plp->wPhoneOtherStress)))
         wcscpy (szAfter, plp2->szPhone);
#endif // 0

      // start and end
      DWORD dwStart, dwEnd;
      dwStart = pp[i].dwSample / pWave->m_dwSRSkip + 1;
      dwEnd = pp[i+1].dwSample / pWave->m_dwSRSkip + 1;
      dwEnd = min(dwEnd, (int)pWave->m_dwSRSamples);
      if (dwStart >= dwEnd)
         continue;   // nothing to train


#ifdef _DEBUG
      // figure out what the visemes sound like
      DWORD k;
      DWORD dwViseme = pp[i].dwEnglishPhone;
      if (!dwViseme)
         dwViseme = MLexiconEnglishPhoneFind(szName);
      if (dwViseme < NUMENGLISHPHONE) {
         for (j = dwStart; j < dwEnd; j++) {
            for (k = 0; k < SRDATAPOINTS; k++) {
               gafVisemeSound[dwViseme][k / SRPOINTSPEROCTAVE][0] += (double) DbToAmplitude (pWave->m_paSRFeature[j].acVoiceEnergy[k]);
               gafVisemeSound[dwViseme][k / SRPOINTSPEROCTAVE][1] += (double) DbToAmplitude (pWave->m_paSRFeature[j].acNoiseEnergy[k]);
            } // k

            // keep count
            gadwVisemeCount[dwViseme] += SRPOINTSPEROCTAVE;
         } // j
      } // ple
#endif

      // determine the word it's in
      PWSTR pszWord = NULL;
      for (j = 0; j+1 < dwNumWord; j++) {
         PWVWORD pWord = (PWVWORD) pWave->m_lWVWORD.Get(j);
         PWVWORD pWord2 = (PWVWORD) pWave->m_lWVWORD.Get(j+1);
         if ((pp[i].dwSample >= pWord->dwSample) && (pp[i].dwSample < pWord2->dwSample)) {
            pszWord = (PWSTR)(pWord+1);
            break;
         }
      } // j
      fp *pfWeight;
      if (pszWord)
         pfWeight = (fp*)phFunctionWords->Find (pszWord);
      else
         pfWeight = NULL;
      fp fWeight = pfWeight ? *pfWeight : 1.0;

      // find the phoneme
      PCPhoneme pPhone;
      pPhone = PhonemeGet (szName, TRUE);
      // BUGFIX - Do overtraining here so that when cross-train stressed and unstressed
      // the right training is more heavily weighted

#ifdef OLDSR
      pPhone->Train (pWave->m_paSRFeature + dwStart, pafEnergy + dwStart,
         dwEnd - dwStart, fMaxEnergy, szBefore, szAfter, m_pLex);
         // NOTE: Not doing overtraining properly
#else
      pPhone->Train (psab + dwStart, dwEnd - dwStart, fMaxEnergy, szBefore, szAfter, m_pLex, !m_fCDPhone,
         fWeight);
#endif

      // find different versions of this phoneme with different stress levels
      PLEXPHONE plp, plp2;
      DWORD dwOrig = m_pLex->PhonemeFindUnsort (szName);
      plp = m_pLex->PhonemeGetUnsort (dwOrig);
      DWORD dwPrimaryStress = (plp && plp->bStress) ? plp->wPhoneOtherStress : dwOrig;
      if (dwPrimaryStress >= m_pLex->PhonemeNum())
         dwPrimaryStress = (DWORD)-1;
      DWORD dwOtherStressCount = 0;
      for (j = 0; ; j++) {
         // if original phoneme dont bother
         if (dwOrig == j)
            continue;

         plp2 = m_pLex->PhonemeGetUnsort (j);
         if (!plp2)
            break;
         DWORD dwThisStress = (plp2->bStress) ? plp2->wPhoneOtherStress : j;
         if (dwThisStress >= m_pLex->PhonemeNum())
            continue;   // bad stress

         // if not match then done
         if (dwThisStress != dwPrimaryStress)
            continue;

         // else, note
         dwOtherStressCount++;
      } // k

      for (j = 0; ; j++) {
         // if original phoneme dont bother
         if (dwOrig == j)
            continue;

         plp2 = m_pLex->PhonemeGetUnsort (j);
         if (!plp2)
            break;
         DWORD dwThisStress = (plp2->bStress) ? plp2->wPhoneOtherStress : j;
         if (dwThisStress >= m_pLex->PhonemeNum())
            continue;   // bad stress

         // if not match then done
         if (dwThisStress != dwPrimaryStress)
            continue;

         // else, match so do cross-stress training
         pPhone = PhonemeGet (plp2->szPhoneLong, TRUE);
         pPhone->Train (psab + dwStart, dwEnd - dwStart, fMaxEnergy, szBefore, szAfter, m_pLex, !m_fCDPhone,
            fWeight / (fp)TRAINOPPOSITESTRESS / (fp)dwOtherStressCount);
            // BUGFIX - Moved OVERTRAIN to here
            // BUGFIX - If have more stresses, then alternate training for unstressed less score
      } // i

   }

   // done
   return TRUE;

}
// BUGBUG - might need caller to TrainSingleWave to merge in CD with CI.
// Not doing right now, but might be an optimization later on



/*************************************************************************************
CVoiceFile::Recognize - Recognizes the speech in the wave.

inputs
   PWSTR             pszText - Text to recognize. Sounds like form,
   PCM3DWave         pWave - Modify the phonemes here
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
   PCProgressSocket  pProgress - Progress bar
returns
   BOOL - TRUE if success
*/
BOOL CVoiceFile::Recognize (PWSTR pszText, PCM3DWave pWave, BOOL fHalfExamples, PCProgressSocket pProgress)
{
   return HypRecognize (pszText, pWave, fHalfExamples, pProgress);
}

/*************************************************************************************
CVoiceFile::FillWaveWithTraining - Fills the wave file with the training data.

inputs
   PCM3DWave         pWave - Modified in place
returns
   BOOL - TRUE if success
*/
BOOL CVoiceFile::FillWaveWithTraining (PCM3DWave pWave)
{
   if (!LexiconRequired ())
      return FALSE;

   // how much for SR skip
   pWave->BlankSRFeatures();

   // loop through all the phonemes and figure out how much space will need
   WCHAR szName[16];
   PCPhoneme pPhone;
   memset (szName, 0, sizeof(szName));
   DWORD dwNeed;
#define SILENCEBETWEENPHONE      10
#define SILENCEBETWEENINSTANCE   2
   DWORD i, j;
   dwNeed = 0;
   for (i = 0; i < m_pLex->PhonemeNum(); i++) {
      PLEXPHONE plp = m_pLex->PhonemeGet(i);

      // find it
      wcscpy (szName, plp->szPhoneLong);

      pPhone = PhonemeGet (szName);
      if (!pPhone)
         continue;

      for (j = 0; j < 3; j++) {  // NOTE: Intentionally use 3
         PCPhoneInstance ppi = pPhone->PhonemeInstanceGet ((j&0x01) ? TRUE : FALSE, (j&0x02) ? TRUE : FALSE, TRUE);
         if (!ppi || !ppi->m_fTrainCount)
            continue;

         dwNeed += SILENCEBETWEENINSTANCE;
#ifdef OLDSR
         dwNeed += max((DWORD)ppi->m_fAverageDuration, 1);
#else
         dwNeed += ppi->FillWaveWithTraining (NULL);
#endif
      } // j
      dwNeed += SILENCEBETWEENPHONE;   // separate phonemes
   }

   pWave->New (pWave->m_dwSamplesPerSec, pWave->m_dwChannels, dwNeed * pWave->m_dwSRSkip);

   // clear out audio
   memset (pWave->m_psWave, 0, pWave->m_dwSamples * pWave->m_dwChannels * sizeof(short));

   // blank feautres
   pWave->BlankSRFeatures();
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      for (j = 0; j < SRDATAPOINTS; j++)
         pWave->m_paSRFeature[i].acVoiceEnergy[j] = pWave->m_paSRFeature[i].acNoiseEnergy[j] = -100;  // so very quiet
   }


   WVPHONEME wv;
   PCWSTR pszSilence = m_pLex->PhonemeSilence();

   // fill it in
   DWORD dwCur; //, k;
   dwCur = 0;
   for (i = 0; i < m_pLex->PhonemeNum(); i++) {
      PLEXPHONE plp = m_pLex->PhonemeGet(i);

      // find it
      wcscpy (szName, plp->szPhoneLong);

      pPhone = PhonemeGet (szName);
      if (!pPhone)
         continue;

      for (j = 0; j < 3; j++) {  // NOTE: Intentionally use 3
         PCPhoneInstance ppi = pPhone->PhonemeInstanceGet ((j&0x01) ? TRUE : FALSE, (j&0x02) ? TRUE : FALSE, TRUE);
         if (!ppi || !ppi->m_fTrainCount)
            continue;

         dwCur += SILENCEBETWEENINSTANCE;

         // add in the phoneme
         memset (wv.awcNameLong, 0, sizeof(wv.awcNameLong));
         memcpy (wv.awcNameLong, plp->szPhoneLong, min(wcslen(plp->szPhoneLong)*sizeof(WCHAR),sizeof(wv.awcNameLong)));
         wv.dwSample = dwCur * pWave->m_dwSRSkip;
         wv.dwEnglishPhone = plp->bEnglishPhone;
         pWave->m_lWVPHONEME.Add (&wv);

#ifdef OLDSR
         // length of phoneme
         DWORD dwDuration = max((DWORD)pPhone->m_aPI[j].m_fAverageDuration, 1);
         PSRFEATURE paSRFeature = &pPhone->m_aPI[j].m_aSRFeature[0];

         for (k = 0; k < dwDuration; k++, dwCur++) {
            // determine the interpolation weights
            fp afWeight[3];
            SRFEATURE SRTemp;
            if (dwDuration == 1) {
               afWeight[0] = afWeight[2] = 0;
               afWeight[1] = 1;
            }
            else {
               afWeight[0] = 1.0 - (fp)k / ((fp)(dwDuration-1)/2.0);
               afWeight[2] = -afWeight[0];
               afWeight[0] = max(afWeight[0], 0);
               afWeight[2] = max(afWeight[2], 0);
               afWeight[1] = 1.0 - afWeight[0] - afWeight[2];
               afWeight[1] = max(afWeight[1], 0);
            }

            // interpolate
            memset (&SRTemp, 0, sizeof(SRTemp));
            SRFEATUREInterpolate (paSRFeature, 3, afWeight, &SRTemp);

            pWave->m_paSRFeature[dwCur] = SRTemp;
         } // k
#else
         DWORD dwAdded = ppi->FillWaveWithTraining (pWave->m_paSRFeature + dwCur);
         dwCur += dwAdded;
#endif // OLDSR

         // add in silence
         memcpy (wv.awcNameLong, pszSilence, sizeof(wv.awcNameLong));
         wv.dwSample = dwCur * pWave->m_dwSRSkip;
         pWave->m_lWVPHONEME.Add (&wv);

      } // j
      dwCur += SILENCEBETWEENPHONE;   // separate phonemes
   }


   return TRUE;
}




/************************************************************************************
CSRPronTree::Constructor and destructor*/
CSRPronTree::CSRPronTree (void)
{
   m_pTextParse = NULL;
   m_pTP = NULL;
   m_pLex = NULL;
}

CSRPronTree::~CSRPronTree (void)
{
   DWORD i;
   for (i = 0; i < m_lPRONTREENODE.Num(); i++) {
      PPRONTREENODE ptn = (PPRONTREENODE) m_lPRONTREENODE.Get(i);
      if (ptn->plPPRONTREENODE && !ptn->fCantDelList)
         delete ptn->plPPRONTREENODE;
   }
}

/************************************************************************************
CSRPronTree::LookForReattach - This searches through the list to see if
a hypothesis can reattach back into the list. It's used to speed up the system
and reduce memory.

inputs
   PPRONTREENODE pCompare - Compare it to this node. NOte: Will make sure dont
         reattach to itslef
   PPRONTREENODE pNode - Node to start at, or NULL if root
returns
   PPRONTREENODE - Reattach point, or NULL if cant find
*/
PPRONTREENODE CSRPronTree::LookForReattach (PPRONTREENODE pCompare, PPRONTREENODE pNode)
{
   DWORD dwNode;
   if (!m_lPRONTREENODE.Num())
      return NULL;   // nothing to compare against
   for (dwNode = m_lPRONTREENODE.Num()-1; dwNode < m_lPRONTREENODE.Num(); dwNode--) {
      PPRONTREENODE pNode = (PPRONTREENODE) m_lPRONTREENODE.Get(dwNode);

      // if no sub-list then fail
      if (!pNode->plPPRONTREENODE || (pNode == pCompare))
         continue;


      // see if have match
      if ((pNode->dwNextWordNode == pCompare->dwNextWordNode)  && !pNode->fNoReattach)
         return pNode;
   } // i

   // else cant find
   return NULL;

#if 0    // old code - gets stuck ocasionally, and slow
   if (!pNode)
      pNode = RootGet();

   // if no sub-list then fail
   if (!pNode->plPPRONTREENODE || (pNode == pCompare))
      return NULL;


   // see if have match
   if ((pNode->pszNextChar == pCompare->pszNextChar)  && !pNode->fNoReattach)
      return pNode;

   // else, look in children
   DWORD i;
   PPRONTREENODE *ppt = ((PPRONTREENODE*) pNode->plPPRONTREENODE->Get(0));
   PPRONTREENODE pFind;
   for (i = 0; i < pNode->plPPRONTREENODE->Num(); i++) {
      if (!ppt[i])
         continue;
      
      pFind = LookForReattach (pCompare, ppt[i]);
      if (pFind)
         return pFind;
   } // i

   // else cant find
   return NULL;
#endif
}


/************************************************************************************
CSRPronTree::Init - Initializes the pronunciation tree to use the given text
string. The string must remain valid while the object is around because it doesn't
make a copy for itself.

inputs
   PCMMLNode2         pTextParse - Output from CTextParse object
   PCTextParse       pTP - Actual parser
   PCMLexicon        pLex - Lexicon to use
*/
BOOL CSRPronTree::Init (PCMMLNode2 pTextParse, PCTextParse pTP, PCMLexicon pLex)
{
   if (m_pTextParse)
      return FALSE;
   m_pTextParse = pTextParse;
   m_pTP = pTP;
   m_pLex = pLex;
   return TRUE;
}

/************************************************************************************
CSRPronTree::RootGet - Returns the root for the pronunication tree node.
*/
PPRONTREENODE CSRPronTree::RootGet (void)
{
   // if there's no root then add one
   if (!m_lPRONTREENODE.Num()) {
      PRONTREENODE ptn;
      memset (&ptn, 0, sizeof(ptn));
      ptn.dwPhoneNum = m_pLex->PhonemeFindUnsort(m_pLex->PhonemeSilence());
      ptn.dwNextWordNode = 0;
      ptn.dwThisWordNode = -1; // do this on purpse
      m_lPRONTREENODE.Add (&ptn, sizeof(ptn));
   }

   return (PPRONTREENODE) m_lPRONTREENODE.Get(0);
}

/************************************************************************************
CSRPronTree::CalcNextIfNecessary - If no alternative prounciations have been
calculated for the future then this expands the list one leve.

inputs
   PPRONTREENODE           pNode - Node to expand
   DWORD                   dwSilence - Index for silence phoneme
   DWORD                   dwNextNode - Normally this is -1. However, this function
                           recursively calls itself. On subsequent calls will pass
                           dwNextNode as non-NULL.
returns
   none
*/
void CSRPronTree::CalcNextIfNecessary (PPRONTREENODE pNode, DWORD dwSilence, DWORD dwNextNode)
{
   if (!pNode)
      return;  // no node to test against

   if (dwNextNode == -1) {
      if (pNode->plPPRONTREENODE)
         return;

      // see if can reattach
      PPRONTREENODE pAttach;
      pAttach = LookForReattach (pNode, NULL);
      if (pAttach) {
         pNode->plPPRONTREENODE = pAttach->plPPRONTREENODE;
         pNode->fCantDelList = TRUE;
         return;
      }

      // else, need to expand
      pNode->plPPRONTREENODE = new CListFixed;
      if (!pNode->plPPRONTREENODE)
         return;  // error
      pNode->plPPRONTREENODE->Init (sizeof(PPRONTREENODE));

      dwNextNode = pNode->dwNextWordNode;
   }

   // get the node
   PCMMLNode2 pParse;
   PWSTR pszParse;
   BOOL fWord, fJustCameToEnd;
   fWord = FALSE;
   fJustCameToEnd = FALSE;
   while (TRUE) {
      pParse = NULL;
      pszParse = NULL;
      m_pTextParse->ContentEnum (dwNextNode, &pszParse, &pParse);
      if (pszParse && !pParse) {
         // this is just text (shouldn't be), so continue
         dwNextNode++;
         continue;
      }
      if (!pParse) {
         // just came to an end
         dwNextNode = -1;
         pszParse = NULL;
         fJustCameToEnd = TRUE;
         break;
      }

      // get the name
      pszParse = pParse->NameGet();
      if (!pszParse) {
         // shouldnt happen, next node
         dwNextNode++;
         continue;
      }

      if (!_wcsicmp(pszParse, m_pTP->Word())) {
         pszParse = pParse->AttribGetString (m_pTP->Text());
         if (!pszParse || !pszParse[0]) {
            // shouldnt happen
            dwNextNode++;
            continue;
         }
         fWord = TRUE;
         break;
      }
      else if (!_wcsicmp(pszParse, m_pTP->Punctuation())) {
         pszParse = pParse->AttribGetString (m_pTP->Text());
         if (!pszParse || !pszParse[0]) {
            // shouldnt happen
            dwNextNode++;
            continue;
         }
         fWord = FALSE;
         break;
      }
      else {
         // dont know
         dwNextNode++;
         continue;
      }
   }

   PRONTREENODE ptn;
   memset (&ptn, 0, sizeof(ptn));
   ptn.dwThisWordNode = -1;

   // if just came to an end
   if (fJustCameToEnd) {
      ptn.dwPhoneNum = dwSilence;
      ptn.dwNextWordNode = -1;

      // add this
      m_lPRONTREENODE.Add (&ptn, sizeof(ptn));
      PPRONTREENODE pptn = (PPRONTREENODE) m_lPRONTREENODE.Get(m_lPRONTREENODE.Num()-1);
      if (pptn)
         pNode->plPPRONTREENODE->Add (&pptn);
      return;
   }

   // expand...
   if (dwNextNode == -1) {
      PPRONTREENODE pptn = NULL;
      pNode->plPPRONTREENODE->Add (&pptn);
      return;  // nothing left
   }

   // see if have a word
   DWORD i;
   DWORD adwIndex[256], dwFind;
   BOOL fSilenceBeforeWord = FALSE;
   dwFind = 0;
   if (fWord) {
      CListVariable lForm, lDontRecurse;

      // check for an entire word
      dwFind = 0;
      lDontRecurse.Clear();
      if (m_pLex->WordPronunciation (pszParse, &lForm, TRUE, NULL, &lDontRecurse) && lForm.Num()) {
         // loop over all the forms and add pronunciations
         DWORD i,j;
         for (i = 0; i < lForm.Num(); i++) {
            BYTE *pbForm = (BYTE*)lForm.Get(i)+1;
            DWORD dwLen = (DWORD)strlen((char*)pbForm);
            if (dwLen+1 + dwFind > sizeof(adwIndex)/sizeof(DWORD))
               continue; // too large

            for (j = 0; j < dwLen; j++, dwFind++) {
               adwIndex[dwFind] = (DWORD)pbForm[j] - 1;

               if (j+1 < dwLen)
                  adwIndex[dwFind] |= 0x80000000;
            }
         }

         fSilenceBeforeWord = TRUE;
         goto foundword;
      }
   }
   else {
      // it's a puncuation, so force a silence
      dwFind = 1;
      adwIndex[0] = dwSilence;
   }

foundword:
   // if didn't find anything then hypothesize either a silence or a skip
   if (!dwFind) {
      dwFind = 2;
      adwIndex[0] = dwSilence;
      adwIndex[1] = 1000;  // sign for skip
   }

   // and on these
   DWORD j, k;
   BOOL fLongPhone;
   for (i = 0; i < dwFind; i = j) {
      // see if need to loop for end
      if (adwIndex[i] & 0x80000000) {
         fLongPhone = TRUE;
         for (j = i+1; j < dwFind; j++)
            if (!(adwIndex[j] & 0x80000000))
               break;
         j++;  // so that include last one (which doesnt have high bit)
         j = min(j, dwFind);

         // wipe out high bit
         for (k = i; k < j; k++)
            adwIndex[k] &= ~(0x80000000);
      }
      else {
         fLongPhone = FALSE;
         j = i+1;
      }
      DWORD dwPhoneStart = i;
      DWORD dwPhoneEnd = j;

      // see if the phone contains skip...
      BOOL fContainSkip;
      fContainSkip = FALSE;
      for (k = dwPhoneStart; k < dwPhoneEnd; k++)
         if (adwIndex[k] == 1000)
            fContainSkip = TRUE;

      // if it contains a skip then just recurse
      if (fContainSkip) {
         CalcNextIfNecessary (pNode, dwSilence, dwNextNode+1);
         continue;
      }

      // if this is silence, and already in silence... then don't bother adding to the current one if already
      // silence
      if ((dwPhoneEnd == dwPhoneStart+1) && (adwIndex[dwPhoneStart] == dwSilence) && (pNode->dwPhoneNum == dwSilence)) {
         CalcNextIfNecessary (pNode, dwSilence, dwNextNode+1);
         continue;
      }

      // new phone sequence info
      PCListFixed plAddTo = pNode->plPPRONTREENODE;
      PCListFixed plSecond = NULL;
      for (k = dwPhoneStart; k < dwPhoneEnd; k++) {
         // create a new node for this pronunciations
         memset (&ptn, 0, sizeof(ptn));
         ptn.dwPhoneNum = adwIndex[k];
         ptn.dwNextWordNode = dwNextNode + 1;
         ptn.dwThisWordNode = dwNextNode;
         ptn.plPPRONTREENODE = NULL;
         if (k+1 < dwPhoneEnd)
            ptn.fNoReattach = TRUE; // if this is a string of phonemes then cant reattach in the middle

         // if there's another element after this one then add a new entry
         if (k+1 < dwPhoneEnd) {
            ptn.plPPRONTREENODE = new CListFixed;
            ptn.plPPRONTREENODE->Init (sizeof(PPRONTREENODE));
         }

         // add to the list
         m_lPRONTREENODE.Add (&ptn, sizeof(ptn));
         PPRONTREENODE pptn = (PPRONTREENODE) m_lPRONTREENODE.Get(m_lPRONTREENODE.Num()-1);

         // remember the one we just added in the list of added
         plAddTo->Add (&pptn);

         // finally, the next time we add, add to this
         plAddTo = ptn.plPPRONTREENODE;

         // remember the second phoneme for attaching to alternate silnce
         if (k == dwPhoneStart)
            plSecond = plAddTo;
      } // k, over phonemes in the node

      // if allow silence before the beginning of the word then allow an
      // alternate choice of silence then first phone
      if (fSilenceBeforeWord) {
         // reset plAddTo
         plAddTo = pNode->plPPRONTREENODE;

         // create a new node for silnce
         memset (&ptn, 0, sizeof(ptn));
         ptn.dwPhoneNum = dwSilence;
         ptn.dwNextWordNode = dwNextNode + 1;
         ptn.dwThisWordNode = -1;   // so silence isnt associated with the word
         ptn.fNoReattach = TRUE; // if this is a string of phonemes then cant reattach in the middle
         ptn.plPPRONTREENODE = new CListFixed;
         if (!ptn.plPPRONTREENODE)
            continue;
         ptn.plPPRONTREENODE->Init (sizeof(PPRONTREENODE));
         m_lPRONTREENODE.Add (&ptn, sizeof(ptn));
         PPRONTREENODE pptn = (PPRONTREENODE) m_lPRONTREENODE.Get(m_lPRONTREENODE.Num()-1);
         plAddTo->Add (&pptn);
         plAddTo = ptn.plPPRONTREENODE;

         // create a new node for the first phoneme... and have it relink back
         // into the word
         ptn.dwPhoneNum = adwIndex[dwPhoneStart];
         ptn.dwThisWordNode = dwNextNode;
         if (dwPhoneStart+1 < dwPhoneEnd)
            ptn.fNoReattach = TRUE; // if this is a string of phonemes then cant reattach in the middle
         else
            ptn.fNoReattach = FALSE;
         ptn.plPPRONTREENODE = plSecond;
         ptn.fCantDelList = (ptn.plPPRONTREENODE ? TRUE : FALSE); // if more then one phoneme then
               // cant delete list because just looped back into previous phones
         m_lPRONTREENODE.Add (&ptn, sizeof(ptn));
         pptn = (PPRONTREENODE) m_lPRONTREENODE.Get(m_lPRONTREENODE.Num()-1);
         plAddTo->Add (&pptn);
      } // fSilenceBeforeWord

   } // i - over alternate pronunciations

   // done
}



/*************************************************************************************
CSRHyp::Constructor and destructor
*/
CSRHyp::CSRHyp (void)
{
   m_dwCurPhone = 0;
   m_pPronNext = NULL;
   m_dwSamplesUsed = m_dwSamplesNext = 0;
   m_fScorePerFrame = 0;
   m_fScore = 0;
   m_dwCurTime = 0;
   m_dwLastTimeTop = 0;
   m_pHypRestriction = NULL;
   m_lSRPHONEME.Init (sizeof(SRPHONEME));

   m_phi = NULL;
}

CSRHyp::~CSRHyp (void)
{
   // do nothing for now
}

/*************************************************************************************
CSRHyp::Clone - Clones all the information
*/
CSRHyp *CSRHyp::Clone (void)
{
   PCSRHyp pNew = new CSRHyp;
   if (!pNew)
      return NULL;

   pNew->m_dwSamplesUsed = m_dwSamplesUsed;
   pNew->m_dwSamplesNext = m_dwSamplesNext;
   pNew->m_fScorePerFrame = m_fScorePerFrame;
   pNew->m_fScore = m_fScore;
   pNew->m_dwCurTime = m_dwCurTime;
   pNew->m_dwLastTimeTop = m_dwLastTimeTop;
   pNew->m_pHypRestriction = m_pHypRestriction;
   pNew->m_lSRPHONEME.Init (sizeof(SRPHONEME), m_lSRPHONEME.Get(0), m_lSRPHONEME.Num());
   pNew->m_dwCurPhone = m_dwCurPhone;
   pNew->m_pPronNext = m_pPronNext;
   pNew->m_phi = m_phi;

   return pNew;
}

/*************************************************************************************
CSRHyp::HypotheizePhoneme - Sets up the variables so that the given phoneme is hypothesized.

inputs
   DWORD       dwIndex - Phoneme index. Unsorted phoneme number
   DWORD       dwFrames - Number of frames to hypothesize the phoneme for... this
                  will be ignored if it's silence
   PWSTR       pszBefore - Phoneme before
   PWSTR       pszAfter - Phoneme after
   DWORD       dwParseIndex - Index into the text parse so know what it's associated with for phoneme
returns
   BOOL - TRUE if success
*/
BOOL CSRHyp::HypothesizePhoneme (DWORD dwIndex, DWORD dwFrames,
                                 PWSTR pszBefore, PWSTR pszAfter,
                                 DWORD dwParseIndex)
{
   // find the phoneme
   PLEXPHONE plp = m_phi->pVoiceFile->Lexicon()->PhonemeGetUnsort(dwIndex);
   if (!plp)
      return FALSE;  // error

   // min and max
   if (dwIndex == m_phi->dwSilence) {
      // it's silence, special case
      m_dwSamplesNext = 10000;
      m_fScorePerFrame = 0;
   }
   else {
      if (dwFrames + m_dwCurTime > m_phi->pWave->m_dwSRSamples)
         return FALSE;

      PCPhoneme pPhone = m_phi->pVoiceFile->PhonemeGet(plp->szPhoneLong);
      if (!pPhone)
         return FALSE;
      m_dwSamplesNext = dwFrames;
#ifdef OLDSR
      m_fScorePerFrame = pPhone->Compare (m_phi->pWave->m_paSRFeature + m_dwCurTime,
         m_phi->pafEnergy + m_dwCurTime, m_dwSamplesNext, m_phi->fMaxEnergy,
         fPrevPhoneSilence, fNextPhoneSilence);
#else
      m_fScorePerFrame = pPhone->Compare (m_phi->pSRAB + m_dwCurTime, m_dwSamplesNext, m_phi->fMaxEnergy,
         pszBefore, pszAfter, m_phi->pVoiceFile->Lexicon(), m_phi->fForUnitSelection, m_phi->fAllowFeatureDistortion, FALSE,
         m_phi->fFast, m_phi->fHalfExamples);
#endif
   }
   m_dwSamplesUsed = 0;

   // add phoneme to hypothesis
   SRPHONEME wp;
   memset (&wp, 0, sizeof(wp));
   memcpy (wp.wvf.awcNameLong, plp->szPhoneLong, min(sizeof(plp->szPhoneLong)*sizeof(WCHAR), sizeof(wp.wvf.awcNameLong)));
   wp.wvf.dwSample = m_dwCurTime * m_phi->pWave->m_dwSRSkip;
   wp.wvf.dwEnglishPhone = plp->bEnglishPhone;
   wp.dwTextParse = dwParseIndex;
   wp.bPhone = (BYTE)dwIndex;
   PSRPHONEME pp;
   DWORD dwNum;
   pp = (PSRPHONEME)m_lSRPHONEME.Get(0);
   dwNum = m_lSRPHONEME.Num();
   //if (!dwNum || _wcsnicmp(pp[dwNum-1].wvf.awcName, wp.wvf.awcName, sizeof(wp.wvf.awcName)/sizeof(WCHAR)) ||
   //   (pp[dwNum-1].dwTextParse != wp.dwTextParse))
   if (!dwNum || (pp[dwNum-1].bPhone != wp.bPhone) || (pp[dwNum-1].dwTextParse != wp.dwTextParse) ||
      (wp.bPhone != (BYTE) m_phi->dwSilence))
         // BUGFIX - Take out this if and add the phoneme anyway since if don't
         // causes works like bookcase = b oo k k ae s, to be misrecognized.
         // This then causes problems with the second pass
      m_lSRPHONEME.Add (&wp);
   // else, dont add since would just duplicate the same phoneme that's there
   // BUGFIX - If the phonemes are the same, but in different words, then add anyway

   // done
   return TRUE;
}


/*************************************************************************************
CSRHyp::SpawnHypToNextPhoneme - Spawns one or more hypothesis based on the next
phoneme (in m_pPronNext).

This:
   1) Sees if the text-to-phoneme conversion has any suggestions (for 1..N characters).
      If it does then these are spawned to the new list.
   2) If not, advance a character, hypothesize a silence, and advance that.
   3) If no futher characters then dont spawn anything

   NOTE: If a text string can be skipped without a phoneme, will get into some recursion
   adding possible hypothesis.

inputs
   PCListFixed       plPCSRHyp - pointer to a list of PCSRHyp pointers. New hypthesis
                              are spawned onto this.
*/
void CSRHyp::SpawnHypToNextPhoneme (PCListFixed plPCSRHyp)
{
   if (!m_pPronNext)
      return;  // nothing left

   // make sure is expanded
   m_phi->pPronTree->CalcNextIfNecessary (m_pPronNext, m_phi->dwSilence);
   if (!m_pPronNext->plPPRONTREENODE)
      return;  // nothing

   // given this next phoneme, determine what phoneme it is...
   PCPhoneme pPhone;
   PCMLexicon pLex = m_phi->pVoiceFile->Lexicon();
   if (m_pPronNext->dwPhoneNum == m_phi->dwSilence)
      pPhone = NULL;
   else {
      PLEXPHONE plp = pLex->PhonemeGetUnsort(m_pPronNext->dwPhoneNum);
      if (!plp)
         return;
      pPhone = m_phi->pVoiceFile->PhonemeGet (plp->szPhoneLong);
      // BUGFIX - Was pPhone = m_phi->pVoiceFile->PhonemeGet (m_pPronNext->dwPhoneNum);
      if (!pPhone)
         return;  // cant find
   }

   // spawn one instance per possible phonemes...
   DWORD dwBranch;
   for (dwBranch = 0; dwBranch < m_pPronNext->plPPRONTREENODE->Num(); dwBranch++) {
      PPRONTREENODE ppt = *((PPRONTREENODE*)m_pPronNext->plPPRONTREENODE->Get(dwBranch));
      // NOTE: ppt may be NULL

      // what are the durations
      DWORD dwDurMin, dwDurMax, dwDurAvg;
      //BOOL fPrevSilence, fNextSilence;
      //fPrevSilence = (m_dwCurPhone == m_phi->dwSilence);
      //fNextSilence = (!ppt || (ppt->dwPhoneNum == m_phi->dwSilence));
      PWSTR pszBefore = pLex->PhonemeGetUnsort(m_dwCurPhone)->szPhoneLong;
      PWSTR pszAfter = pLex->PhonemeGetUnsort(ppt ? ppt->dwPhoneNum : m_phi->dwSilence)->szPhoneLong;
      if (pPhone) {
         PCPhoneInstance ppi = pPhone->PhonemeInstanceGet (pszBefore, pszAfter, pLex);
         if (!ppi)
            continue;   // cant find phoneme training data

         // BUGFIX - Duration was ignore SRSAMPLESPERSEC
         fp fAverageDuration = ppi->m_fAverageDuration * (fp) m_phi->pWave->m_dwSRSAMPLESPERSEC / (fp)SRSAMPLESPERSEC;

         // allow duration to go from half to double
         // NOTE: Not penalizing for long/short phonemes
         dwDurMin = (DWORD)(fAverageDuration / 2.0); // BUGFIX - was 2, made /3 since sometimes phones very short
         dwDurMax = (DWORD)(fAverageDuration * 3.0); // BUGFIX - Was 2, but not long enough
            // BUGFIX - Reduce duration variablility to speed up hypothesis and hopefully
            // make more accurate, was /3 and *4, now /2 and * 3
         dwDurAvg = (DWORD)fAverageDuration;
         dwDurMin = max(2, dwDurMin);  // BUGFIX - Was a max of 1
         dwDurMax = max(2, dwDurMax);  // BUGFIX - Was a max of 1
         dwDurMax += (m_phi->dwTimeAccuracy-1); // BUGFIX - So dont have round-off errors with time accuracy
         dwDurAvg = max(2, dwDurAvg);  // do this so later divides dont get low
      }
      else
         dwDurMin = dwDurMax = dwDurAvg = 0;

      // if we have restrictions, and this isn't silence, then see if this phoneme
      // is to be restricted away
      if (m_pHypRestriction && pPhone && (m_pPronNext->dwPhoneNum != m_phi->dwSilence)) {
         // toss out hypothesis that have already passed
         while ((WORD)m_dwCurTime > m_pHypRestriction->awPhoneStart[1])
            m_pHypRestriction++; // dont need to worry about going off edge sincel last one has alye of 60000

         // see if this matches any phonemes
         int iTimeMin = -1, iTimeMax = -1;
         PHYPRESTRICTION phs;
         for (phs = m_pHypRestriction; (WORD)m_dwCurTime >= phs->awPhoneStart[0]; phs++) {
            // must be the phone number we're looking for
            if (phs->bPhone != m_pPronNext->dwPhoneNum)
               continue;
            if ((WORD)m_dwCurTime > phs->awPhoneStart[1])
               continue;   // phone doesnt fit in this time

            // else, found a phone that would match, remember time min
            if ((iTimeMin < 0) || (iTimeMin > (int)phs->awPhoneEnd[0]))
               iTimeMin = (int)phs->awPhoneEnd[0];
            if ((iTimeMax < 0) || (iTimeMax < (int)phs->awPhoneEnd[1]))
               iTimeMax = (int)phs->awPhoneEnd[1];
         } // over hyps

         // if there aren't any acceptable times then skip this
         if ((iTimeMin >= 0) && (iTimeMax >= 0)) {
            // convert the min and max to durations, and have them affect minmum and maximum duration
            iTimeMin -= (int)m_dwCurTime;
            iTimeMax -= (int)m_dwCurTime;
            if (iTimeMin > (int)dwDurMin)
               dwDurMin = (DWORD)iTimeMin;
            if (iTimeMax < (int)dwDurMax)
               dwDurMax = (DWORD) iTimeMax;
         }
         else
            continue; // this phoneme duration shouldnt be hypothesized
      }

      // create a hypothesis...
      DWORD dwDur;
      //DWORD dwLastMin = -1;
      //fp fMin = 0;
      for (dwDur = dwDurMin; dwDur <= dwDurMax; dwDur += m_phi->dwTimeAccuracy) {
         // BUGFIX - Increased dwCur by one, but too slow, so increase by 2
         // create clone
         PCSRHyp pNew;
         pNew = Clone();
         if (!pNew)
            continue;   // error
         pNew->m_dwCurPhone = m_pPronNext->dwPhoneNum;

         pNew->m_pPronNext = ppt;
         if (!pNew->HypothesizePhoneme (pNew->m_dwCurPhone, dwDur, pszBefore, pszAfter,
            m_pPronNext->dwThisWordNode)) {
            delete pNew;
            break;   // if happens might as well break
         }

         plPCSRHyp->Add (&pNew);

#if 0 // take out this optimization hack because might be causing problems with some units
      // Removing this optimization improved SR accuracy for difficult words, but slowed down
         // keep track of the last minimum
         if ((dwLastMin == -1) || (pNew->m_fScorePerFrame < fMin)) {
            dwLastMin = dwDur;
            fMin = pNew->m_fScorePerFrame;
         }

         // if we are more than 50% of the average phone length, and we
         // havent gotten a min for awhile then the chances are a longer
         // phone will only make for greater error, so skip out
         if ((dwLastMin != -1) && (dwDur > dwDurAvg*3/2) && (dwDur - dwLastMin > dwDurAvg/2))
            break;   // give up since assume will only get slower after this
#endif // 0
      } // dwDur
   } // dwBranch
   
   // done
}



/*************************************************************************************
CSRHyp::AdvanceHyp - Advances the hypothesis one time slot.

This:
   1) Looks at the current phoneme in the wave's scores and adds that to the score.
   2) Increases the time and the amount of phoneme used
   3) If the time >= mintime for the phoneme, spawns a phoneme continuing on to the next
      phoneme in the list.
   4) If time <= maxtime, spawns a copy of this

inputs
   PCListFixed       plPCSRHyp - pointer to a list of PCSRHyp pointers. New hypthesis
                              are spawned onto this.
returns
   BOOL - TRUE if caller should keep this hypothesis alive. FALSE if should delete it
         when returns
*/
#define SHORTESTSILENCE    (m_phi->pWave->m_dwSRSAMPLESPERSEC/25)    // shorted duration for silence

BOOL CSRHyp::AdvanceHyp (PCListFixed plPCSRHyp)
{
   BOOL fSilence = (m_phi->dwSilence == m_dwCurPhone);

   // BUGFIX - Expand the future hypthesis frequently just so that pruning works
   // faster because can't prune if don't know what future hyp is
   m_phi->pPronTree->CalcNextIfNecessary (m_pPronNext, m_phi->dwSilence);

   // what is the new score change
   fp fScoreInc;
   if (fSilence) {
      // special case... compare against main silence...
#ifdef OLDSR
      fScoreInc = m_phi->pPhoneSilence->Compare (m_phi->pWave->m_paSRFeature + m_dwCurTime,
         m_phi->pafEnergy + m_dwCurTime, 1, m_phi->fMaxEnergy);
#else
      fScoreInc = m_phi->pPhoneSilence->Compare (m_phi->pSRAB + m_dwCurTime,
         1, m_phi->fMaxEnergy, m_phi->fForUnitSelection, m_phi->fAllowFeatureDistortion, m_phi->fFast,
         m_phi->fHalfExamples);
#endif
   }
   else
      fScoreInc = m_fScorePerFrame;

   m_fScore += fScoreInc;

   // advance the time
   m_dwCurTime++;
   m_dwSamplesUsed++;   // keep track of samples used in this phoneme

   // spwan off this? if it has gotten to the end (or if it's silence) then consider
   // spawining another one
   BOOL fMoreLife = TRUE;
   if (fSilence) {
      // BUGFIX - only spawn another hypothesis every other sample
      // BUGFIX - Silence must be at least 1/25th second of silence
      // BUGFIX - If this is the first silence (can tell because m_dwCurTime==m_dwSamplesUsed
      // then allow to be very short
      // BUGFIX - Silence must be at least 1/15th of a second, so don't put
      // as many silence gaps between words, especially plosives
      // BUGFIX - Moved back to 1/25th of second because put in feature to remember
      // if silence between words for tts
      if (((m_dwSamplesUsed > SHORTESTSILENCE) || (m_dwCurTime == m_dwSamplesUsed)) && ((m_dwSamplesUsed % m_phi->dwTimeAccuracy) == 0))
         SpawnHypToNextPhoneme (plPCSRHyp);
   }
   else if (m_dwSamplesUsed >= m_dwSamplesNext) {
      // dealing with phoneme
      SpawnHypToNextPhoneme (plPCSRHyp);

      // if this isn't silence, then spawned to the next phoneme as the last
      // act, so we will die
      fMoreLife = FALSE;
   } // spawn

   return fMoreLife;
}



/*************************************************************************************
CSRHyp::AreEquivalent - See if any of the paths are essentially the same. This checks
that the current state is the same (except for score) and that the phonemes generated
is the same. If so, returns TRUE so that one of them can be eliminated.

inputs
   PCSRHyp        pCompare - Compare with this
returns
   BOOL - TRUE if they're the same
*/
BOOL CSRHyp::AreEquivalent (PCSRHyp pCompare)
{
   DWORD dwNextParse, dwNextParseCompare;
   dwNextParse = m_pPronNext ? m_pPronNext->dwNextWordNode : -1;
   dwNextParseCompare = pCompare->m_pPronNext ? pCompare->m_pPronNext->dwNextWordNode : -1;

   if ((dwNextParse != dwNextParseCompare) ||
      (m_dwCurTime != pCompare->m_dwCurTime) )
      return FALSE;

   // BUGFIX - PronNext can be equivalent too
   if ((m_pPronNext && !pCompare->m_pPronNext) || (!m_pPronNext && pCompare->m_pPronNext))
      return FALSE;
   if (m_pPronNext != pCompare->m_pPronNext) {  // know that they're both pointers
      if (m_pPronNext->dwPhoneNum != pCompare->m_pPronNext->dwPhoneNum)
         return FALSE;
      if (!m_pPronNext->plPPRONTREENODE || !pCompare->m_pPronNext->plPPRONTREENODE)
         return FALSE;  // not calculated far ahead so have to assume different
      if (m_pPronNext->plPPRONTREENODE != pCompare->m_pPronNext->plPPRONTREENODE)
         return FALSE;  // different lists
   }

   if (m_dwCurPhone != pCompare->m_dwCurPhone)
      return FALSE;


#if 0
   // BUGFIX - Take out: if last phones different then doesnt matter because will ultimately
   // be using the best result anyway, and there's no way that the worse of the two
   // results can ever get better because it's on the same recognition path from here
   // on, therefore eleiminate

   // BUGFIX - If the last entry for the phonemes are the same, and all of the other
   // conditions in this thest are met, then it doesn't really matter what came before
   // because while the paths diverged earlier, they have reconverged. As a result,
   // might as will take the highest score since the one behind will never catch up
   if (!m_lSRPHONEME.Num() || !pCompare->m_lSRPHONEME.Num())
      return FALSE;  // must have at least have a phoneme before can compare
   PSRPHONEME p1, p2;
   p1 = (PSRPHONEME)m_lSRPHONEME.Get(m_lSRPHONEME.Num()-1);
   p2 = (PSRPHONEME)pCompare->m_lSRPHONEME.Get(pCompare->m_lSRPHONEME.Num()-1);
   if (memcmp(p1, p2, sizeof(SRPHONEME)))
      return FALSE;
#endif 0

#if 0 // this is too restrictive
   if (m_lSRPHONEME.Num() != pCompare->m_lSRPHONEME.Num())
      return FALSE;
   if (memcmp(m_lSRPHONEME.Get(0), pCompare->m_lSRPHONEME.Get(0), m_lSRPHONEME.Num()*sizeof(SRPHONEME)))
      return FALSE;
#endif // 0

   // BUGFIX - If both are silence
   // they're essentially the same since they can both go on forever
   if (m_dwCurPhone == m_phi->dwSilence) {// if gets here automatically know that same phone
      // BUGFIX - Because do a hackish test for silence that prevents phoneme from
      // doing comparison, (m_dwSamplesUsed > SHORTESTSILENCE), must put
      // that in here
      if ((m_dwSamplesUsed <= SHORTESTSILENCE) || (pCompare->m_dwSamplesUsed <= SHORTESTSILENCE))
         return FALSE;
      return TRUE;
   }
   else if ((pCompare->m_dwSamplesUsed != m_dwSamplesUsed) ||
       (pCompare->m_dwSamplesNext != m_dwSamplesNext))
      return FALSE;

   return TRUE;
}




/*************************************************************************************
CVoiceFile::HypAdvanceAll - Given a list of PCSRHyp, this advances any of the hypothesis
whoe m_dwCurTime is less than the given time. Those that return FALSE for advance are
deleted from the list.

inputs
   DWORD             dwTime - Time to advance to
   PCListFixed       plPCSRHyp - pointer to a list of PCSRHyp pointers. Initially contains
                                    a list of all hypothesis. They are advanced (and the list
                                    is changed) until all are up to dwTime
*/
void CVoiceFile::HypAdvanceAll (DWORD dwTime, PCListFixed plPCSRHyp)
{
   while (TRUE) {
      DWORD i;
      BOOL fAdvanced = FALSE;
      for (i = 0; i < plPCSRHyp->Num(); i++) {
         PCSRHyp pHyp = *((PCSRHyp*) plPCSRHyp->Get(i));

         if (pHyp->m_dwCurTime >= dwTime)
            continue;

         // else, need to advance this
         fAdvanced++;   // so go back and check
         BOOL fKeep;
         fKeep = pHyp->AdvanceHyp (plPCSRHyp);
         // NOTE: assuming that advance only ADDS to the end of the list

         if (!fKeep) {
            plPCSRHyp->Remove (i);
            delete pHyp;
            i--;  // so dont skip one
         }
      } // i

      if (!fAdvanced)
         break;
   } // while
}


/*************************************************************************************
CVoiceFile::HypRemoveDuplicates - Removes duplicaes from the list of hypothesis. If a duplicate
is removed it will be the lowest one.

inputs
   PCListFixed       plPCSRHyp - pointer to a list of PCSRHyp pointers. Initially contains
                                    a list of all hypothesis. They are advanced (and the list
                                    is changed) until all are up to dwTime
*/
static int _cdecl PCSRHypSort (const void *elem1, const void *elem2)
{
   PCSRHyp pdw1, pdw2;
   pdw1 = *((PCSRHyp*) elem1);
   pdw2 = *((PCSRHyp*) elem2);

   if (pdw1->m_fScore > pdw2->m_fScore)
      return 1;
   else if (pdw1->m_fScore < pdw2->m_fScore)
      return -1;
   else
      return 0;
   //return (int)pdw1->m_dwScore - (int)pdw2->m_dwScore;
}

void CVoiceFile::HypRemoveDuplicates (PCListFixed plPCSRHyp)
{
   DWORD i, j;
#ifdef _DEBUG
   DWORD dwOrig = plPCSRHyp->Num();
#endif
   qsort (plPCSRHyp->Get(0), plPCSRHyp->Num(), sizeof(PCSRHyp), PCSRHypSort);
   for (i = 0; i < plPCSRHyp->Num(); i++) {
      PCSRHyp pHyp = *((PCSRHyp*) plPCSRHyp->Get(i));

      // look futher in the list
      for (j = i+1; j <plPCSRHyp->Num(); j++) {
         PCSRHyp pHyp2 = *((PCSRHyp*) plPCSRHyp->Get(j));

         if (!pHyp->AreEquivalent (pHyp2))
            continue;

         // else, the same, so take the lowest score
         if (pHyp->m_fScore > pHyp2->m_fScore) {
            delete pHyp;
            plPCSRHyp->Remove (i);
            i--;  // so when reloop dont skip
            break;   // out of j loop
         }
         else {
            delete pHyp2;
            plPCSRHyp->Remove (j);
            j--;  // so wont skip
            continue;   // in j loop
         }
      } // j
   } // i

#if 0 // def _DEBUG
   char szTemp[64];
   sprintf (szTemp, "HypRemoveDuplicates: %d removed of %d\r\n", (int)dwOrig - (int)plPCSRHyp->Num(), (int)dwOrig);
   OutputDebugString (szTemp);
#endif
}

/*************************************************************************************
CVoiceFile::HypPrune - This prunes out any hypothesis:
   1) whose scores lower than N points below the best
   2)If there are more than 1000 hypothesis, it prunes out the lowest ones.
   3) If they have been not been top hypothesis for a long time

inputs
   PCListFixed       plPCSRHyp - pointer to a list of PCSRHyp pointers. Initially contains
                                    a list of all hypothesis. They are advanced (and the list
                                    is changed) until all are up to dwTime
   PCM3DWave         pWave - Where m_dwSRSAMPLESPERSEC is gotten from
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate.
                           Causes to use half the hypothesis
*/
void CVoiceFile::HypPrune (PCListFixed plPCSRHyp, PCM3DWave pWave, BOOL fHalfExamples)
{
#define MAXHYP       (fHalfExamples ? 500 : 1000)   // BUGFIX - make 1000
   // BUGFIX - Worked best at 500, works ok at 250 but more errors, 1000 has same results as 500
#define HYPONTOP     1        // BUGFIX - Should just be one. Thought out algorithm
//#define HYPONTOP     (MAXHYP/5) // BUGFIX - Make /5 instead of /10
   // BUGFIX - Was 20

   // sort the list
   PCSRHyp *pph = (PCSRHyp*) plPCSRHyp->Get(0);
   DWORD dwNum = plPCSRHyp->Num();
   if (!dwNum)
      return;  // none left
   qsort (pph, dwNum, sizeof(PCSRHyp), PCSRHypSort);

   // find the point where the worst score is more than N points worse than the best score
   fp fBest;
   DWORD i, dwDelete;
   fBest = pph[0]->m_fScore;
   fBest += (fp)pWave->m_dwSRSAMPLESPERSEC/8.0 /* 1/2 second */ * ((SRCOMPAREWEIGHT)/2) *
      (fHalfExamples ? 1.0 : 2.0);   // 1 second of really bad results
      // BUGFIX - Changed to 1/2 second, from 1/4 second
      // BUGFIX - Changed to 1/8 sec, from 1/2, because was way too high
   for (i = 1; i < dwNum; i++)
      if (pph[i]->m_fScore >= fBest)
         break;
   dwDelete = i;  // delete from this point

   // set a flag for any hypothesis in the top so that later on can kill hypothesis
   // that havent been on top for awhile
   for (i = 0; i < min(HYPONTOP, dwNum); i++)
      pph[i]->m_dwLastTimeTop = pph[i]->m_dwCurTime;

   // remove anything more than 1000 hyps
   dwDelete = min(dwDelete, (DWORD)MAXHYP); // no more than this many hyps around
   if (dwNum > dwDelete) {
      for (i = dwDelete; i < dwNum; i++)
         delete pph[i];
      for (i = dwNum-1; i >= max(dwDelete,1); i--)
         plPCSRHyp->Remove (i);
   }

   pph = (PCSRHyp*) plPCSRHyp->Get(0);
   dwNum = plPCSRHyp->Num();


    // get rid - since dont think need when have good pruning due to matches
   // if there are any hypthesis that havent been on the top for a long time
   // then get rid of them
   // BUGFIX - Put back in using 1 second - hopefully works
#define OLDHYP    (pWave->m_dwSRSAMPLESPERSEC*(fHalfExamples ? 1 : 2))      // 1 seconds
      // BUGFIX - Changed from 1 second to 2 seconds
   for (i = dwNum-1; i < dwNum; i--) {
      if (pph[i]->m_dwLastTimeTop + OLDHYP >= pph[i]->m_dwCurTime)
         continue;   // not old enough yet

      // else remove
      delete pph[i];
      plPCSRHyp->Remove (i);
      pph = (PCSRHyp*) plPCSRHyp->Get(0);
      dwNum = plPCSRHyp->Num();
   }
}

/*************************************************************************************
CVoiceFile::HypRecognize - Use the scores for all the phonemes, along with the text
string for the recognition, and figures out what was spoken.

inputs
   PWSTR             pszText - Text string that's spken
   PCM3DWave         pWave - Wave file that has already had m_paiSRScore filled in
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
   PCProgressSocket  pProgress - Progress bar
returns
   BOOL - TRUE if success
*/

#define SRPASSES                 3        // number of SR passes, BUGFIX - back down to 2 passes
   // BUGFIX - Changed to 3 passes because problems with ritas voice, and I think this
   // ends up producing slightly better results. Should drop entire words at end with 3 passes
//#define ACCURACYPERPASS          2        // how much the accuracy goes down for each pass
// BUGFIX - Accuracy per pass based on wave's samples per sec
#define ACCURACYPERPASS          (pWave->m_dwSRSAMPLESPERSEC/50)        // how much the accuracy goes down for each pass
// #define SECONDPASSWIGGLE         (ACCURACYPERPASS*2)   // how much 2nd pass can wiggle
#define WIGGLESCALE              1           // how much larger wiggle is than general dwTimeAccuracy
   // BUGFIX - Added wigglescale. Old equivalent was wigglescale of 1
// #define FIRSTPASSTIMEACCURACY    4        // recognize over 1/4 phonemes on first pass

BOOL CVoiceFile::HypRecognize (PWSTR pszText, PCM3DWave pWave, BOOL fHalfExamples, PCProgressSocket pProgress)
{
   // clear the sub-fundamental pitch since depends on SR
   pWave->PitchSubClear ();

   if (!LexiconRequired())
      return FALSE;

   // calculate the energy for all the samples
   if (!pWave->m_dwSRSamples)
      return FALSE;
   DWORD i;
#ifdef OLDSR
   CMem memEnergy;
   if (!memEnergy.Required (pWave->m_dwSRSamples * sizeof(fp)))
      return FALSE;

   fp *pafEnergy = (fp*)memEnergy.p;
   fp fMaxEnergy = 0;
   for (i = 0; i < pWave->m_dwSRSamples; i++) {
      pafEnergy[i] = SRFEATUREEnergy(&pWave->m_paSRFeature[i]);
      fMaxEnergy = max (fMaxEnergy, pafEnergy[i]);
         // note - could potentially look at the energy for the last N seconds to test
         // this, but might as well do the entire wave
   }
#else
   CSRAnal SRAnal;
   fp fMaxEnergy;
   PSRANALBLOCK pSRAB = SRAnal.Init (pWave->m_paSRFeature, pWave->m_dwSRSamples, FALSE, &fMaxEnergy);
#endif



#if 0 // to test
   PWSTR apszPhone[] = {L"n", L"eh1"};
   DWORD dwPhone, dwBin;
   fp fRet;
   WCHAR szTemp[64];
   for (dwPhone = 0; dwPhone < sizeof(apszPhone)/sizeof(PWSTR); dwPhone++) {
      PCPhoneme pPhone = PhonemeGet (apszPhone[dwPhone]);
      if (!pPhone)
         continue;

      PCPhoneInstance pPhoneInst = pPhone->PhonemeInstanceGet (FALSE, FALSE);
      pPhoneInst = pPhone->PhonemeInstanceGetRough ();
      if (!pPhoneInst)
         continue;

      OutputDebugStringW (L"\r\n\r\n");
      OutputDebugStringW (apszPhone[dwPhone]);

      for (i = 0; i < pWave->m_dwSRSamples; i++) {
         swprintf (szTemp, L"\r\n\t%d: ", i);
         OutputDebugStringW (szTemp);
         for (dwBin = 0; dwBin < (DWORD) (pPhoneInst->m_fRoughSketch ? 1 : SUBPHONEPERPHONE); dwBin++) {
            fRet = pPhoneInst->m_aSubPhone[dwBin].Compare (pSRAB + i, 1,
               fMaxEnergy, pSRAB[i].fEnergy, FALSE);

            if (dwBin)
               OutputDebugStringW (L", ");
            swprintf (szTemp, L"%.3g", (double)fRet);
            OutputDebugStringW (szTemp);
         } // dwBin
      } // i
   }
#endif // to test


   // parse the text
   PCMMLNode2 pParse;
   pParse = m_pTextParse->ParseFromText (pszText, FALSE, FALSE);
   if (!pParse)
      return FALSE;

#ifdef _DEBUG
   CMem memParse;
   char szHuge[1024];
   m_pTextParse->ParseToText (pParse, &memParse);
   WideCharToMultiByte (CP_ACP, 0, (PWSTR)memParse.p, -1, szHuge, sizeof(szHuge), 0, 0);
   strcat (szHuge, "\r\n");
   OutputDebugString (szHuge);
#endif

   // make up silence
   CPhoneInstance Silence;
#ifdef OLDSR
   Silence.FillWithSilence();
#else
   Silence.FillWithSilence(fMaxEnergy);
#endif

#ifdef _DEBUG
   DWORD dwStartTime = GetTickCount();
#endif

   DWORD dwPass;
   CListFixed lHypRestriction;
   lHypRestriction.Init (sizeof(HYPRESTRICTION));
   DWORD dwSilence;
   PCSRHyp pLastGoodHyp = NULL;

   DWORD dwPasses = SRPASSES;

   for (dwPass = 0; dwPass < dwPasses; dwPass++) {
      DWORD dwTimeAccuracy = ACCURACYPERPASS * (dwPasses-dwPass-1) + 1;

      if (pProgress)
         pProgress->Push (sqrt((fp)dwPass / (fp)dwPasses), sqrt((fp)(dwPass+1) / (fp)dwPasses));

      // creat the pronunciation tree
      PCSRPronTree pPronTree = new CSRPronTree;
      if (!pPronTree || !pPronTree->Init (pParse, m_pTextParse, m_pLex)) {
         if (pPronTree)
            delete pPronTree;
         delete pParse;
         if (pProgress)
            pProgress->Pop();
         return FALSE;
      }

      // start out with a hypothesis list containing one hypothesis for silence
      PCSRHyp phStart = new CSRHyp;
      if (!phStart) {
         delete pPronTree;
         delete pParse;
         if (pProgress)
            pProgress->Pop();
         return FALSE;
      }

      HYPINFO hi;
      memset (&hi, 0, sizeof(hi));
      hi.dwSilence = m_pLex->PhonemeFindUnsort(m_pLex->PhonemeSilence());
      hi.dwTimeAccuracy = dwTimeAccuracy; // early passes are innacurate
         // was: (dwPass ? 1 : FIRSTPASSTIMEACCURACY);  // 1st pass in inaccurate
      // OPTIMIZATION - Introduces a very small bit of error. Basically, skips every other sample
      // in the first pass when hypothesizing longer phonemes
      hi.fFast = (dwPass+1 < dwPasses) && (dwTimeAccuracy > 2);
         // BUGFIX - Was just !dwPass, but changed to (dwPass+1 < dwPasses)
         // should be slightly faster, and ok, especially with 200 fps
      hi.fForUnitSelection = FALSE; // since doing for full speech recognition fForUnitSelection;
      hi.fAllowFeatureDistortion = TRUE;  // allow feature distortion for recognition
      hi.fHalfExamples = fHalfExamples;
      hi.fMaxEnergy = fMaxEnergy;
      hi.pVoiceFile = this;
      hi.pPronTree = pPronTree;
      hi.pPhoneSilence = &Silence;
      hi.pWave = pWave;
   #ifdef OLDSR
      hi.pafEnergy = pafEnergy;
   #else
      hi.pSRAB = pSRAB;
   #endif


      CListFixed lHyp;
      phStart->m_phi = &hi;
      phStart->m_pHypRestriction = (dwPass ? (PHYPRESTRICTION)lHypRestriction.Get(0) : NULL);
      dwSilence = phStart->m_dwCurPhone = hi.dwSilence;
      phStart->m_dwCurTime = 0;
      phStart->m_dwLastTimeTop = 0;
      phStart->m_fScore = 0;
      phStart->m_pPronNext = hi.pPronTree->RootGet();
      // phStart->HypothesizePhoneme (phStart->m_adwPhoneNext[0]);
      //phStart-> m_dwSamplesUsed filled in by hypothesize phoneme

      lHyp.Init (sizeof(PCSRHyp));
      phStart->SpawnHypToNextPhoneme (&lHyp);
      delete phStart;

      // repeat through all the time slices
      for (i = 1; i < pWave->m_dwSRSamples; i++) {
         if (((i%10) == 0) && pProgress)
            pProgress->Update ((fp)i / (fp) pWave->m_dwSRSamples);

         // advance all the hypothesis
         HypAdvanceAll (i, &lHyp);

#if 0 // def _DEBUG
         if (TRUE /*dwPass*/) {
            DWORD i;
            PCSRHyp *pph;
            pph = (PCSRHyp*)lHyp.Get(0);
            // display top N hypotehsis and see what's the diff
            WCHAR szTemp[128];
            char szaTemp[128];
            DWORD j;
            BOOL fEquiv;
            for (i = 0; i < min(10, lHyp.Num()); i++) {
               sprintf (szaTemp, "%g score:", (double)pph[i]->m_fScore);
               OutputDebugString (szaTemp);

               PSRPHONEME pp = (PSRPHONEME) pph[i]->m_lSRPHONEME.Get(0);
               DWORD dwNum = pph[i]->m_lSRPHONEME.Num();
               for (j = 0; j < dwNum; j++, pp++) {
                  memcpy (szTemp, pp->wvf.awcName, sizeof(pp->wvf.awcName));
                  szTemp[sizeof(pp->wvf.awcName)/sizeof(WCHAR)] = 0;
                  swprintf (szTemp + wcslen(szTemp), L" %.3gs, ", (double)pp->wvf.dwSample / (double)pWave->m_dwSamplesPerSec);
                  WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
                  OutputDebugString (szaTemp);
               }

               // see if they're equipvalent
               if (i)
                  fEquiv = pph[i]->AreEquivalent (pph[i-1]);

               OutputDebugString ("\r\n");
            }
            OutputDebugString ("\r\n\r\n");
         }
#endif // _DEBUG

         // BUGFIX - Remove duplicates first
         // remove duplicates
         HypRemoveDuplicates (&lHyp);

         // prune the tree down
         HypPrune (&lHyp, pWave, fHalfExamples);

#if 0 // def _DEBUG
         if (TRUE /*dwPass*/) {
            DWORD i;
            PCSRHyp *pph;
            pph = (PCSRHyp*)lHyp.Get(0);
            // display top N hypotehsis and see what's the diff
            WCHAR szTemp[128];
            char szaTemp[128];
            DWORD j;
            BOOL fEquiv;
            for (i = 0; i < min(10, lHyp.Num()); i++) {
               sprintf (szaTemp, "%g score:", (double)pph[i]->m_fScore);
               OutputDebugString (szaTemp);

               PSRPHONEME pp = (PSRPHONEME) pph[i]->m_lSRPHONEME.Get(0);
               DWORD dwNum = pph[i]->m_lSRPHONEME.Num();
               for (j = 0; j < dwNum; j++, pp++) {
                  memcpy (szTemp, pp->wvf.awcName, sizeof(pp->wvf.awcName));
                  szTemp[sizeof(pp->wvf.awcName)/sizeof(WCHAR)] = 0;
                  swprintf (szTemp + wcslen(szTemp), L" %.3gs, ", (double)pp->wvf.dwSample / (double)pWave->m_dwSamplesPerSec);
                  WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
                  OutputDebugString (szaTemp);
               }

               // see if they're equipvalent
               if (i)
                  fEquiv = pph[i]->AreEquivalent (pph[i-1]);

               OutputDebugString ("\r\n");
            }
            OutputDebugString ("\r\n\r\n");
         }
#endif // 0 or _DEBUG

      }

      // when get to here should have at least one hypothesis, if not then error
      if (!lHyp.Num()) {
         delete pPronTree;
         delete pParse;
         if (pProgress)
            pProgress->Pop();
         return FALSE;
      }

      // find the first (and highest) hypothesis that actually completes the path
      PCSRHyp *pph;
      pph = (PCSRHyp*)lHyp.Get(0);

      for (i = 0; i < lHyp.Num(); i++) {
         if (!pph[i]->m_pPronNext)
            break;

         // BUGFIX - If get to last except for silence at the end (from punct) then accept
         if ((pph[i]->m_pPronNext->dwNextWordNode == (DWORD)-1) && (pph[i]->m_pPronNext->dwPhoneNum == hi.dwSilence))
            break;
      }
      if (i >= lHyp.Num())
         i = 0;   // default to 0

      PCSRHyp pStart;
      // BUGFIX - if this hypothesis is just silence, then use the last good hyp
      // Put in because some blizzard2008 sentences not coming through on 3rd pass.
      // Not sure why but suspect decided better alignment of silence that precluded
      // the fixed hyp. Roger_10303 as an example
      PSRPHONEME psp;
      if (pLastGoodHyp && (pph[i]->m_lSRPHONEME.Num() == 1)) {
         psp = (PSRPHONEME)pph[i]->m_lSRPHONEME.Get(0);
         if ((DWORD)psp->bPhone == dwSilence) {
            delete pph[i];
            pph[i] = pLastGoodHyp->Clone();
         }
      }
      pStart = pph[i];

      // rember this hyp
      if (pLastGoodHyp)
         delete pLastGoodHyp;
      pLastGoodHyp = pStart->Clone();

      psp = (PSRPHONEME)pStart->m_lSRPHONEME.Get(0);
      DWORD dwNum = pStart->m_lSRPHONEME.Num();
      if (dwPass >= dwPasses-1) {
         // fill in the wave file with the final results

         // create a list of phonemes and a list of words
         pWave->m_lWVPHONEME.Clear();
         pWave->m_lWVWORD.Clear();
         BYTE abHuge[512];
         PWVWORD pNewWord = (PWVWORD)&abHuge[0];
         PWSTR pNewString = (PWSTR)(pNewWord+1);
         for (i = 0; i < dwNum; i++, psp++) {
            // add the phoneme
            pWave->m_lWVPHONEME.Add (&psp->wvf);

            // if this is the same as the previous string dont add the word
            if (i && (psp->dwTextParse == psp[-1].dwTextParse))
               continue;

            // add the word...
            DWORD dwChars;
            PCMMLNode2 pSub;
            PWSTR pszSub;
            pSub = NULL;
            pszSub = NULL;
            pParse->ContentEnum (psp->dwTextParse, &pszSub, &pSub);
            if (pSub)
               pszSub = pSub->AttribGetString (m_pTextParse->Text());
            else
               pszSub = NULL;
            pNewWord->dwSample = psp->wvf.dwSample;
            dwChars = pszSub ? (DWORD)wcslen(pszSub) : 0;
            dwChars = min(dwChars, 128);  // so not too long
            if (dwChars)
               memcpy (pNewString, pszSub, dwChars * sizeof(WCHAR));
            pNewString[dwChars] = 0;   // null terminate

            // add
            pWave->m_lWVWORD.Add (pNewWord, sizeof(WVWORD) + sizeof(WCHAR)*(wcslen(pNewString)+1));
         }

         pWave->PhonemeRemoveDup(TRUE); // shouldnt happen, but just in case
         pWave->WordRemoveDup();
      }
      else {
         lHypRestriction.Clear();   // so start out with blank hypothesis
         // fill in the hypothesis restrictions
         HYPRESTRICTION hr;
         memset (&hr, 0, sizeof(hr));
         for (i = 0; i < dwNum; i++, psp++) {
            if ((DWORD) psp->bPhone == dwSilence)
               continue;   // dont restrict silence

            hr.bPhone = psp->bPhone;

            // start
            WORD wWiggle = (WORD) dwTimeAccuracy * WIGGLESCALE; // BUGFIX - was SECONDPASSWIGGLE;
            WORD wStart = (WORD) (psp->wvf.dwSample / pWave->m_dwSRSkip);
            hr.awPhoneStart[0] = (wStart > wWiggle) ? (wStart - wWiggle) : 0;
            hr.awPhoneStart[1] = wStart + wWiggle;

            // end
            WORD wEnd = (i+1<dwNum) ? (psp[1].wvf.dwSample / pWave->m_dwSRSkip) : pWave->m_dwSRSamples;
            hr.awPhoneEnd[0] = (wEnd > wWiggle) ? (wEnd - wWiggle) : 0;
            hr.awPhoneEnd[1] = wEnd + wWiggle;

            lHypRestriction.Add (&hr);
         } // i

         // one final one
         hr.bPhone = 0;
         hr.awPhoneStart[0] = hr.awPhoneStart[1] = 0xf000;
         hr.awPhoneEnd[0] = hr.awPhoneEnd[1] = 0xf000;
         lHypRestriction.Add (&hr);
      } // dwPass

   #ifdef _DEBUG
      // display top N hypotehsis and see what's the diff
      WCHAR szTemp[128];
      char szaTemp[128];
      DWORD j;
      BOOL fEquiv;
      for (i = 0; i < min(6, lHyp.Num()); i++) {
         sprintf (szaTemp, "%g score:", (double)pph[i]->m_fScore);
         OutputDebugString (szaTemp);

         PSRPHONEME pp = (PSRPHONEME) pph[i]->m_lSRPHONEME.Get(0);
         DWORD dwNum = pph[i]->m_lSRPHONEME.Num();
         for (j = 0; j < dwNum; j++, pp++) {
            memcpy (szTemp, pp->wvf.awcNameLong, sizeof(pp->wvf.awcNameLong));
            szTemp[sizeof(pp->wvf.awcNameLong)/sizeof(WCHAR)] = 0;
            swprintf (szTemp + wcslen(szTemp), L" %.3gs, ", (double)pp->wvf.dwSample / (double)pWave->m_dwSamplesPerSec);
            WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
            OutputDebugString (szaTemp);
         }

         // see if they're equipvalent
         if (i) {
            fEquiv = pph[i]->AreEquivalent (pph[i-1]);
            if (fEquiv)
               OutputDebugStringW (L"\r\n\tEquivalent to the one above");
         }

         OutputDebugString ("\r\n");
      }
      OutputDebugString ("\r\n\r\n");
   #endif

      // free all the hypothesis

      for (i = 0; i < lHyp.Num(); i++)
         delete pph[i];
      delete pPronTree;

      if (pProgress)
         pProgress->Pop();
   }  // dwPass

   if (pLastGoodHyp)
      delete pLastGoodHyp;

   #ifdef _DEBUG
      char szaTemp[64];
      DWORD dwNewTime = GetTickCount();
      sprintf (szaTemp, "\r\nRecognize time, pass %d: %d\r\n", (int)dwPass, (int)(dwNewTime - dwStartTime));
      dwStartTime = dwNewTime;
      OutputDebugString (szaTemp);
   #endif

   delete pParse;

   // done
   return TRUE;
}



/*************************************************************************************
VoiceFileDefaultGet - Get the default SR file.

inputs
   PWSTR          pszFile - Filled with the file name
   DWORD          dwChars - Number of characters in pws
returns
   none
*/
void VoiceFileDefaultGet (PWSTR pszFile, DWORD dwChars)
{
   // at least fill with null
   pszFile[0] = 0;

   HKEY  hKey = NULL;
   DWORD dwDisp;
   char szTemp[512];
   szTemp[0] = 0;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);
   if (hKey) {
      DWORD dw, dwType;
      LONG lRet;
      dw = sizeof(szTemp);
      lRet = RegQueryValueEx (hKey, gszKeyTrainFile, NULL, &dwType, (LPBYTE) szTemp, &dw);
      RegCloseKey (hKey);

      if (lRet != ERROR_SUCCESS)
         szTemp[0] = 0;
   }
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pszFile, dwChars);

   if (!pszFile[0])
      wcscpy (pszFile, L"c:\\program files\\mXac\\3D Outside the Box\\EnglishMale.mtf");
}


/*************************************************************************************
VoiceFileDefaultGet - Get the default SR file and write it in a control

inputs
   PCEscPage         pPage - Page
   PWSTR             pszControl - Control name
returns
   none
*/
void VoiceFileDefaultGet (PCEscPage pPage, PWSTR pszControl)
{
   PCEscControl pControl = pPage->ControlFind (pszControl);
   if (!pControl)
      return;

   WCHAR szFile[256];
   VoiceFileDefaultGet (szFile, sizeof(szFile) / sizeof(WCHAR));
   pControl->AttribSet (Text(), szFile[0] ? szFile : L"(No TTS file)");
}

/*************************************************************************************
VoiceFileDefaultSet - Set the default SR file.

inputs
   PWSTR          pszFile - File name
returns
   none
*/
void VoiceFileDefaultSet (PWSTR pszFile)
{
   // save to registry
   HKEY  hKey = NULL;
   DWORD dwDisp;
   RegCreateKeyEx (HKEY_CURRENT_USER, RegBase(), 0, 0, REG_OPTION_NON_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &hKey, &dwDisp);

   if (hKey) {
      char szTemp[512];
      WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
      RegSetValueEx (hKey, gszKeyTrainFile, 0, REG_SZ, (BYTE*) szTemp,
         (DWORD)strlen(szTemp)+1);

      RegCloseKey (hKey);
   }
}



/*************************************************************************************
VoiceFileDefaultUI - UI to select a new SR voice.

inputs
   HWND           hWnd - To display the window off of
returns
   BOOL - TRUE if changed
*/
BOOL VoiceFileDefaultUI (HWND hWnd)
{
   WCHAR szFile[256];
   VoiceFileDefaultGet (szFile, sizeof(szFile)/sizeof(WCHAR));

   if (!VoiceFileOpenDialog (hWnd, szFile, sizeof(szFile)/sizeof(WCHAR), FALSE))
      return FALSE;

   // save the file
   VoiceFileDefaultSet (szFile);
   return TRUE;
}


/*************************************************************************************
VoiceFileOpenDialog - Dialog box for opening a CVoiceFile

inputs
   HWND           hWnd - To display dialog off of
   PWSTR          pszFile - Pointer to a file name. Must be filled with initial file
   DWORD          dwChars - Number of characters in the file
   BOOL           fSave - If TRUE then saving instead of openeing. if fSave then
                     pszFile contains an initial file name, or empty string
returns
   BOOL - TRUE if pszFile filled in, FALSE if nothing opened
*/
BOOL VoiceFileOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
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
   ofn.lpstrFilter = "Speech recognition training data (*.mtf)\0*.mtf\0\0\0";
   ofn.lpstrFile = szTemp;
   ofn.nMaxFile = sizeof(szTemp);
   ofn.lpstrTitle = fSave ? "Save the speech recognition trainign data" :
      "Open speech recognition training data";
   ofn.Flags = fSave ? (OFN_PATHMUSTEXIST | OFN_HIDEREADONLY) :
      (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
   ofn.lpstrDefExt = "mtf";
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
SRMainPage
*/
static BOOL SRMainPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCVoiceFile pVF = (PCVoiceFile) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         pControl = pPage->ControlFind (L"fortts");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pVF->m_fForTTS);
         pControl = pPage->ControlFind (L"cdphone");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pVF->m_fCDPhone);

         // update the file list
         pPage->Message (ESCM_USER+82);
         pPage->Message (ESCM_USER+84);
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
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"textfilter") || !_wcsicmp(psz, L"speakerfilter")) {
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

         if (!_wcsicmp(psz, L"fortts")) {
            pVF->m_fForTTS = p->pControl->AttribGetBOOL(Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"cdphone")) {
            pVF->m_fCDPhone = p->pControl->AttribGetBOOL(Checked());
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
         else if (!_wcsicmp(psz, L"recordsel")) {
            if (!pszSelToDo) {
               pPage->MBWarning (L"You must select a sentence before you can record it.");
               return TRUE;
            }

            DWORD dwNum;
            CWaveToDo wtd;
            wtd.AddToDo (pszSelToDo);
            dwNum = RecBatch (WAVECALC_SEGMENT, pPage->m_pWindow, pVF->m_szDefTraining, FALSE, FALSE, TRUE,
               &wtd, pVF->m_pWaveDir, pVF->m_szFile, L"SpeechRec", NULL);

            if (dwNum)
               pVF->m_pWaveToDo->RemoveToDo (pszSelToDo);

            // rescan
            pPage->Message (ESCM_USER+82);
            pPage->Message (ESCM_USER+84);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"recordall")) {
            DWORD dwNum;
            dwNum = RecBatch (WAVECALC_SEGMENT, pPage->m_pWindow, pVF->m_szDefTraining, FALSE, FALSE, TRUE,
               pVF->m_pWaveToDo, pVF->m_pWaveDir, pVF->m_szFile, L"SpeechRec", NULL);

            // rescan
            pPage->Message (ESCM_USER+82);
            pPage->Message (ESCM_USER+84);
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
         else if (!_wcsicmp(psz, L"add")) {
            if (pVF->m_pWaveDir->AddFilesUI (pPage->m_pWindow->m_hWnd, FALSE)) {
               pVF->TrainClear();
               pPage->Message (ESCM_USER+82);
            }
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"train")) {
            pVF->TrainRecordings (pPage->m_pWindow->m_hWnd, GetKeyState (VK_CONTROL) < 0, FALSE);
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
            pVF->TrainClear();
            pPage->Message (ESCM_USER+83);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"clearall")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to clear out the training list?"))
               return TRUE;

            pVF->m_pWaveDir->ClearFiles ();
            pVF->TrainClear();
            pPage->Message (ESCM_USER+83);
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
            wcscpy (szTemp, pVF->m_szDefTraining);

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
            wcscpy (pVF->m_szDefTraining, szTemp);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Speech recognition training main page";
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
            p->pszSubString = pVF->m_szDefTraining[0] ? pVF->m_szDefTraining : L"NO TRAINING (you should choose one)";
            return TRUE;
         }
      }
      break;



   };


   return DefPage (pPage, dwMessage, pParam);
}

/*************************************************************************************
CVoiceFile::DialogMain - Brings up the main voice file editing dialog

inputs
   PCEscWindow          pWindow - window to use
returns
   BOOL - TRUE if pressed back, FALSE if close
*/
BOOL CVoiceFile::DialogMain (PCEscWindow pWindow)
{
   PWSTR pszRet;

redo:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLSRMAIN, SRMainPage, this);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, Back()))
      return TRUE;
   else if (!_wcsicmp(pszRet, RedoSamePage()))
      goto redo;

   return FALSE;
}


/*************************************************************************************
CVoiceFile::TrainClear - Clears out current training
*/
void CVoiceFile::TrainClear (void)
{
   DWORD i;
   for (i = 0; i < m_hPCPhoneme.Num(); i++) {
      PCPhoneme pp = *((PCPhoneme*)m_hPCPhoneme.Get(i));
      delete pp;
   } // i
   m_hPCPhoneme.Clear();
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
FuncWordGroup - Determines the group that the function word is in.

inputs
   DWORD          dwIndex - Index into the sorted function list, 0 = most common word (such as the),
                  etc.
returns
   DWORD - Function word group number, 0..NUMFUNCWORDGROUP. If == NUMFUNCWORDGROUP then
            means that not a function word.
*/
DWORD FuncWordGroup (DWORD dwIndex)
{
   // add this to the function words list
   DWORD dwVal = dwIndex >> 4;   // so 16 in base function group
   DWORD dwBin = 0;
   for (; dwVal; dwVal /= 2, dwBin++);
   dwBin = min(dwBin, NUMFUNCWORDGROUP);
   return dwBin;
}



/*************************************************************************************
FuncWordWeight - Returns a weighting for the word.

inputs
   DWORD          dwGroup - Group from FuncWordGroup()
returns
   fp - 0.001 to 1.0. Low values are for very common function words. 1.0 is for
      non-function words
*/
fp FuncWordWeight (DWORD dwGroup)
{
#ifdef NOMODS_UNDERWEIGHTFUNCTIONWORDS
   return pow ((fp)dwGroup / (fp)NUMFUNCWORDGROUP * 0.9 + 0.1, FUNCWORDWEIGHTPOW);   // BUGFIX - so gentler slope

   // else not
#endif // 0

   return 1.0;
}


/*************************************************************************************
CVoiceFile::TrainRecordings - Train based upon the recordings

inputs
   HWND              hWnd - Window to show progress on, and to ask questions from
   BOOL              fLessPickey - If TRUE the timing errors in the sentence will be ignored
   BOOL              fSimple - If TRUE use for the simple UI
returns
   BOOL - TRUE if success
*/


BOOL CVoiceFile::TrainRecordings (HWND hWnd, BOOL fLessPickey, BOOL fSimple)
{
   // clear out
   TrainClear();

   // set random so that reproducible
   srand (1000);

   // if no lexicon then error
   if (!Lexicon()) {
      EscMessageBox (hWnd, ASPString(),
         L"You must choose a lexicon to use.",
         L"Training cannot occur without a lexicon.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   // if no waves then error
   if (!m_pWaveDir->m_lPCWaveFileInfo.Num()) {
      EscMessageBox (hWnd, ASPString(),
         L"You haven't added any files into the list of recordings.",
         L"You must have recordings to train from.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

#ifdef _DEBUG
   // clear out values for summing together energy
   memset (gadwVisemeCount, 0, sizeof(gadwVisemeCount));
   memset (gafVisemeSound, 0, sizeof(gafVisemeSound));
#endif

   DWORD dwAdded;
   m_dwTooShortWarnings = 0;
   {
      // progress bar
      CProgress Progress;
      Progress.Start (hWnd, 
         fSimple ? "Analyzing (Stage 2 of 4)..." : "Analyzing recordings...", TRUE);

      // loop
      DWORD i;
      DWORD dwNum = m_pWaveDir->m_lPCWaveFileInfo.Num();
      PCWaveFileInfo *ppwi = (PCWaveFileInfo*) m_pWaveDir->m_lPCWaveFileInfo.Get(0);
      CM3DWave Wave;
      char szTemp[256];
      BOOL fRet;
      CHashString hWordWeight;
      hWordWeight.Init (sizeof(fp));
      
      // two passes
      DWORD dwPass;
      for (dwPass = 0; dwPass < 2; dwPass++) {
         Progress.Push ((fp)dwPass / 2.0, (fp)(dwPass+1) / 2.0);

         for (i = 0; i < dwNum; i++) {
            PCWaveFileInfo pwi = ppwi[i];

            // load it in
            WideCharToMultiByte (CP_ACP, 0, pwi->m_pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
            if (!Wave.Open (NULL, szTemp)) {
               WCHAR szw[512];
               swprintf (szw, L"The recording, %s, couldn't be opened.", pwi->m_pszFile);
               EscMessageBox (hWnd, ASPString(),
                  szw,
                  L"Analysis will stop.",
                  MB_ICONEXCLAMATION | MB_OK);
               return FALSE;
            }

            // remember if have phoneme info
            BOOL fSRInfo = (Wave.m_dwSRSamples ? TRUE : FALSE);

            // progress
            Progress.Push ((fp)i / (fp)dwNum, (fp)(i+1) / (fp)dwNum);
            Progress.Update (0);
            fRet = TrainSingleWave (&Wave, hWnd, &Progress, fLessPickey, fSimple, &hWordWeight, !dwPass);
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
               // BUGFIX - Use common function for function word weight
               f = FuncWordWeight (FuncWordGroup(i));
               //f = (fp)i / 100;
               //f = f * f;  //so very common words have very low score
               //f = f * 0.9 + 0.1;  // so even most common words have some weight
               *(papfp[i]) = f; // min(1.0, f);
            } // i
         } // if !dwPass
      } // dwPass
   }

#ifdef _DEBUG
   // write out this info
   char szaTemp[64];
   OutputDebugString ("\r\n\r\nViseme sounds = {");
   DWORD i, j;

   for (i = 0; i < NUMENGLISHPHONE; i++) {
      OutputDebugString ("\r\n\t");
      double *paf = &gafVisemeSound[i][0][0];

      // fill in silence
      if (!i) for (j = 0; j < sizeof(gafVisemeSound[j]) / sizeof(gafVisemeSound[0][0][0]); j++) {
         paf[j] = DbToAmplitude(SRNOISEFLOOR);
         gadwVisemeCount[i] = 1;
      }
      for (j = 0; j < sizeof(gafVisemeSound[j]) / sizeof(gafVisemeSound[0][0][0]); j++) {
         sprintf (szaTemp, "%g, ", gadwVisemeCount[i] ? (paf[j] / (double) gadwVisemeCount[i]) : (double)0);
         OutputDebugString (szaTemp);
      } // j
   } // i
   OutputDebugString ("\r\n}\r\n");
#endif

   // make sure all the phonemes have been recorded...
   dwAdded = TrainVerifyAllTrained (&gMemTemp);

   if (dwAdded) {
      if (!fSimple)
         EscMessageBox (hWnd, ASPString(),
            L"Training failed because the following phonemes weren't in the recordings.",
            (PWSTR)gMemTemp.p,
            MB_ICONEXCLAMATION | MB_OK);
      TrainClear();
      return FALSE;
   }

   // else done
   if (!fSimple)
      EscMessageBox (hWnd, ASPString(),
         L"Training has successfully finished.",
         NULL,
         MB_ICONINFORMATION | MB_OK);
   return TRUE;
}

/*************************************************************************************
CVoiceFile::TrainVerifyAllTrained - This verifies that all the phonemes have been
 trained. If any aren't they're added to the list in pMem.

inputs
   PCMem          pMem - Filled with a string containing the untrained phonemes.
                     This can be null
returns
   DWORD - Number of untrained phonemes. 0 if all trained
*/
DWORD CVoiceFile::TrainVerifyAllTrained (PCMem pMem)
{
   DWORD dwAdded, i;

   if (pMem)
      MemZero (pMem);

   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return 1000;

   PLEXPHONE plp;
   PCPhoneme pPhone;
   DWORD dwStart, dwMid, dwEnd;
   dwAdded = 0;
   for (i = 0; i < pLex->PhonemeNum(); i++) {
      plp = pLex->PhonemeGetUnsort (i);
      if (!plp)
         continue;
#if 0 // since now use stress
      if (plp->bStress && plp->wPhoneOtherStress)
         continue;   // ignore phones that have master
#endif // 0

      // see if exists
      pPhone = PhonemeGet (plp->szPhoneLong, FALSE);
      if (pPhone) {
         pPhone->QueryNeedTraining (&dwStart, &dwMid, &dwEnd, pLex);
         if (dwStart + dwMid + dwEnd)
            continue;   // doesnt need training
      }

      // else note that need
      if (pMem) {
         MemCat (pMem, plp->szPhoneLong);
         MemCat (pMem, L" - ");
         MemCat (pMem, plp->szSampleWord);
         MemCat (pMem, L"\r\n");
      }

      dwAdded++;
   }

   return dwAdded;
}


/*************************************************************************************
SentenceToPhonemeString - Takes a PCMMLNode2 from CTextParse and produces a phoneme string for it.

inputs
   PCMMLNode2      pNode - From CTextParse's ParseText()
   PCMLexicon     pLex - Lexicon to use
   PCTextParse    pTextParse - Text parser to use. Must also be using pLex
   PCListFixed    plPhone - Initialized to sizeof(DWORD) and filled with UN-SORTED phoneme numbers.
                     The high BYTE contains 0 if mid-word, 1 if start of word, 2 if end of word. WORDPOS_XXX flags.
                     NOTE: WORDPOS_SYLSTART and WORDPOS_SYLEND are NOT added at this time
returns
   BOOL - TRUE if success, FALSE if failed to load wave
*/
BOOL SentenceToPhonemeString (PCMMLNode2 pNode, PCMLexicon pLex, PCTextParse pTextParse,
                              PCListFixed plPhone)
{
   plPhone->Init (sizeof(DWORD));

   DWORD dwNumPhone = min(pLex->PhonemeNum(), 255);

   // loop through the elements
   DWORD i;
   PWSTR psz;
   PCMMLNode2 pSub;
   BYTE abPron[256];
   size_t dwSize;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // does it have a prounciation
      psz = pSub->AttribGetString (pTextParse->Pronunciation());
      if (!psz)
         continue;

      // else convert to binary
      dwSize = MMLBinaryFromString (psz, abPron, sizeof(abPron));
      if (!dwSize || (dwSize > sizeof(abPron)))
         continue;   // either nothing there or too large

      // add
      DWORD j, dwPhone;
      for (j = 0; j < dwSize; j++) {
         dwPhone = abPron[j];

         // see if it has an unstressed version
         PLEXPHONE plp = pLex->PhonemeGetUnsort (dwPhone);
         if (!plp)
            dwPhone = 255;  // if cant get this just make 255  // BUGFIX - was -1
#if 0 // since now use stress
         else {
            if (fIgnoreStress && plp->bStress && (plp->wPhoneOtherStress < dwNumPhone))
               dwPhone = plp->wPhoneOtherStress;
         }
#endif // 0

         if (j == 0)
            dwPhone |= ((WORDPOS_WORDSTART | WORDPOS_SYLSTART) << 24);   // note that start of word
         if (j+1 == dwSize)
            dwPhone |= ((WORDPOS_WORDEND | WORDPOS_SYLEND) << 24);   // note that end of word
         // NOTE: Not getting the syllable boundaries and setting WORDPOS_SYLSTART and WORDPOS_SYLEND separately

         plPhone->Add (&dwPhone);
      } // j
   } // i


   return (plPhone->Num() ? TRUE : FALSE);
}




/*************************************************************************************
SentenceToPhonemeString - Takes a PWSTR and produces a phoneme string for it.

inputs
   PWSTR          pszText - Text to parse
   PCMLexicon     pLex - Lexicon to use
   PCTextParse    pTextParse - Text parser to use. Must also be using pLex
   PCListFixed    plPhone - Initialized to sizeof(DWORD) and filled with UN-SORTED phoneme numbers
                     The high BYTE contains 0 if mid-word, 1 if start of word, 2 if end of word.
                     WORDPOS_XXX flags.
                     NOTE: WORDPOS_SYLSTART and WORDPOS_SYLEND are NOT added at this time
   PCMMLNode2      *ppNode - If not NULL, this is filled in the the text's MMLNode (from CTextParse).
                  This must be deleted.
returns
   BOOL - TRUE if success, FALSE if failed to load wave
*/
BOOL SentenceToPhonemeString (PWSTR pszText, PCMLexicon pLex, PCTextParse pTextParse,
                              PCListFixed plPhone, PCMMLNode2 *ppNode)
{
   if (ppNode)
      *ppNode = NULL;

   PCMMLNode2 pNode = pTextParse->ParseFromText (pszText, TRUE, FALSE);
   if (!pNode)
      return FALSE;

   BOOL fRet;
   fRet = SentenceToPhonemeString (pNode, pLex, pTextParse, plPhone);

   if (fRet && ppNode)
      *ppNode = pNode;
   else if (pNode)
      delete pNode;

   return fRet;
}

/*************************************************************************************
WaveToPhonemeString - Reads in a wave file and fills in a phoneme string of the
phonemes appearing in the wave.

inputs
   PCWSTR         pszWave - Wave file
   PCMLexicon     pLex - Lexicon to use
   PCListFixed    plPhone - Initialized to sizeof(DWORD) and filled with UN-SORTED phoneme numbers
                     The high BYTE contains 0 if mid-word, 1 if start of word, 2 if end of word
   PCMMLNode2      *ppNode - If not NULL, this is filled in with a node with subnodes
                  containing "Word" and "Punct" elements, like in CTextParse
returns
   BOOL - TRUE if success, FALSE if failed to load wave
*/
BOOL WaveToPhonemeString (PCWSTR pszWave, PCMLexicon pLex,
                          PCListFixed plPhone, PCMMLNode2 *ppNode)
{
   if (ppNode)
      *ppNode = NULL;

   plPhone->Init (sizeof(DWORD));
   PCWSTR pszSilence = pLex->PhonemeSilence();
   DWORD dwSilence = pLex->PhonemeFindUnsort (pszSilence);
   DWORD dwNumPhone = min(pLex->PhonemeNum(), 255);

   // open
   PCM3DWave pWave = new CM3DWave;
   if (!pWave)
      return FALSE;

   // convert to ANSI
   char szFile[256];
   WideCharToMultiByte (CP_ACP, 0, pszWave, -1, szFile, sizeof(szFile), 0, 0);
   if (!pWave->Open (NULL, szFile, FALSE)) {
      delete pWave;
      return FALSE;
   }

   // look for phonemes
   DWORD dwNum = pWave->m_lWVPHONEME.Num();
   PWVPHONEME pwp = (PWVPHONEME) pWave->m_lWVPHONEME.Get(0);
   if (!dwNum) {
      delete pWave;
      return FALSE;
   }
   
   // conver the phoneme strings to numbers
   DWORD j, k;
   DWORD dwPhone;
   for (j = 0; j < dwNum; j++) {
      WCHAR szPhone[16];
      memset (szPhone, 0, sizeof(szPhone));
      memcpy (szPhone, pwp[j].awcNameLong, sizeof(pwp[j].awcNameLong));

      dwPhone = pLex->PhonemeFindUnsort (szPhone);

      // see if it has an unstressed version
      PLEXPHONE plp = pLex->PhonemeGetUnsort (dwPhone);
      if (!plp)
         dwPhone = 255;  // if cant get this just make -1   // BUGFIX - Was setting to -1
#if 0 // since now use stress
      else if (plp) {
         if (fIgnoreStress && plp->bStress && (plp->wPhoneOtherStress < dwNumPhone))
            dwPhone = plp->wPhoneOtherStress;
      }
#endif // 0

      // BUGFIX - See if matches the start of a word
      DWORD dwPhonemeStart, dwPhonemeEnd;
      dwPhonemeStart = pwp[j].dwSample;
      dwPhonemeEnd = ((j+1) < dwNum) ? pwp[j+1].dwSample : pWave->m_dwSamples;
      for (k = 0; k < pWave->m_lWVWORD.Num(); k++) {
         PWVWORD pwv = (PWVWORD)pWave->m_lWVWORD.Get(k);
         PWVWORD pwv2 = (PWVWORD)pWave->m_lWVWORD.Get(k+1);
         DWORD dwWordStart = pwv->dwSample;
         DWORD dwWordEnd = pwv2 ? pwv2->dwSample : pWave->m_dwSamples;


         if (dwWordEnd <= dwPhonemeStart)
            continue;   // out of range
         else if (dwWordStart > dwPhonemeEnd)
            break;   // gone too far

         if (dwWordStart == dwPhonemeStart)
            dwPhone |= ((WORDPOS_WORDSTART | WORDPOS_SYLSTART) << 24);   // not that at start of word
         if (dwWordEnd == dwPhonemeEnd)
            dwPhone |= ((WORDPOS_WORDEND | WORDPOS_SYLEND) << 24);   // note that at end of word
         // NOTE: not exactring WORDPOS_SYLSTART and WORDPOS_SYLEND from
         // syllable boundaries because I don't think that omportant for this case
         break;
      }

      plPhone->Add (&dwPhone);
   } // j

   // create the text
   if (ppNode) {
      CTextParse TextParse;
      PCMMLNode2 pNode = *ppNode = new CMMLNode2;
      if (!pNode)
         goto done;
      
      // add children
      DWORD i;
      for (i = 0; i < pWave->m_lWVWORD.Num(); i++) {
         PWVWORD pw = (PWVWORD)pWave->m_lWVWORD.Get(i);
         PWSTR psz = (PWSTR)(pw+1);
         if (!psz[0])
            continue;   // blank

         PCMMLNode2 pSub = pNode->ContentAddNewNode ();
         if (!pSub)
            continue;
         pSub->AttribSetString (TextParse.Text(), psz);
         pSub->NameSet ((!psz[1] && iswpunct(psz[0])) ? TextParse.Punctuation() : TextParse.Word());
      } // i
   } // if want node

done:
   delete pWave;

   return TRUE;
}


/*************************************************************************************
RecommendFromPhones - Internal func. This scans through a list of phonemes in plPhone and figures
and increments the counters of pabPhone[][]. If the phoneme (before incrementing)
is less than the threshold then the return flag will be TRUE, indicating that the
word should be added.

inputs
   PCListFixed       plPhone - List of phones. Array of DWORDs
   DWORD             dwSilence - So know which pheoneme is silence
   DWORD             dwNumPhone - Number of phonemes
   DWORD             dwThresh - If the number of phones (for any phone) is LESS than this before add
                     then will return TRUE.
   DWORD             padwPhoneCount[256][3] - Pointer to an array to be filled in
returns
   BOOL - TRUE if an important phoneme is added, FALSE if not
*/
static BOOL RecommendFromPhones (PCListFixed plPhone, DWORD dwSilence, DWORD dwNumPhone,
                                 DWORD dwThresh,
                                 DWORD padwPhoneCount[256][3])
{
   BOOL fRet = FALSE;

   // loop through phonemes
   DWORD j;
   DWORD *padwPhone = (DWORD*)plPhone->Get(0);
   DWORD dwNum = plPhone->Num();
   for (j = 0; j < dwNum; j++) {
      padwPhone[j] &= 0xffff; // BUGFIX - ignore high byte, since dont care if at begin/end of woird

      // if silence phoneme ignore
      if ((padwPhone[j] == dwSilence) || (padwPhone[j] > dwNumPhone))
         continue;

      // figure out if silence to left and right...
      DWORD dwPhoneLeft, dwPhoneRight, dwForm;
      BOOL fSilenceLeft, fSilenceRight;
      dwPhoneLeft = (j ? padwPhone[j-1] : dwSilence);
      dwPhoneRight = (((j+1) < dwNum) ? padwPhone[j+1] : dwSilence);
      fSilenceLeft = (dwPhoneLeft == dwSilence) || (dwPhoneLeft >= dwNumPhone);
      fSilenceRight = (dwPhoneRight == dwSilence) || (dwPhoneRight >= dwNumPhone);
      if (fSilenceLeft)
         dwForm = 1; // silence on left
      else if (fSilenceRight)
         dwForm = 2; // silence on right
      else
         dwForm = 0; // sound to left and right

      // should set?
      if (padwPhoneCount[padwPhone[j]][dwForm] < dwThresh)
         fRet = TRUE;

      // increase the count
      padwPhoneCount[padwPhone[j]][dwForm]++;
   } // j

   return fRet;
}

/*************************************************************************************
CVoiceFile::Recommend - This recommends a set of words to speak based upon the
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
DWORD CVoiceFile::Recommend (HWND hWnd)
{
   // create a counter for the number of phonemes of each type
   DWORD padwPhoneCount[256][3];   // [0..255 = phone num], [0=phone in mid word, 1=phone at start, 2=phone at end]
   BOOL  afCareAbout[256];     // which pheonems care about
   memset (padwPhoneCount, 0, sizeof(padwPhoneCount));
   memset (afCareAbout, 0, sizeof(afCareAbout));

   PCMLexicon pLex = Lexicon();
   if (!pLex)
      return 0;  // no lexicon

   // open file
   CListFixed lWords;
   CBTree TreeWords;
   PWSTR *ppsz;
   DWORD i, j;
   lWords.Init (sizeof(PWSTR));
   if (hWnd) {
      PCMMLNode2 pNode = pLex->TextScan (NULL, hWnd, &TreeWords);

      for (i = 0; i < TreeWords.Num(); i++) {
         PWSTR psz = TreeWords.Enum(i);
         if (!psz)
            continue;
         lWords.Add (&psz);
      }
      if (pNode)
         delete pNode;

      // randomsize
      srand(GetTickCount());
      ppsz = (PWSTR*)lWords.Get(0);
      DWORD dwNum = lWords.Num();
      for (i = 0; i < dwNum; i++) {
         j = rand() % dwNum;
         PWSTR pszTemp = ppsz[i];
         ppsz[i] = ppsz[j];
         ppsz[j] = pszTemp;
      } // i
   } // if can scan in text file

   // figure out which phonemes care about
   for (i = 0; i < pLex->PhonemeNum(); i++) {
      PLEXPHONE plp = pLex->PhonemeGetUnsort (i);
      if (!plp)
         continue;
#if 0 // since now use stress
      if (!plp->bStress)
#endif // 0
         afCareAbout[i] = TRUE;
   } // i

   // silnece phoneme
   PCWSTR pszSilence = pLex->PhonemeSilence();
   DWORD dwSilence = pLex->PhonemeFindUnsort (pszSilence);
   DWORD dwNumPhone = min(pLex->PhonemeNum(), 255);
   DWORD dwThresh = (m_fForTTS ? 3 : 1) * OVERTRAIN;

   // look through wave files and use them to fill in phone count
   CListFixed lPhone;
   for (i = 0; i < m_pWaveDir->m_lPCWaveFileInfo.Num(); i++) {
      PCWaveFileInfo pfi = *((PCWaveFileInfo*)m_pWaveDir->m_lPCWaveFileInfo.Get(i));
      if (!WaveToPhonemeString (pfi->m_pszFile, pLex, /*TRUE,*/ &lPhone))
         continue;

      // add recommendations
      RecommendFromPhones (&lPhone, dwSilence, dwNumPhone, dwThresh, padwPhoneCount);
   } // i

   // look through existing recommended sentences and use them to fill in phone count
   for (i = 0; i < m_pWaveToDo->m_lToDo.Num(); i++) {
      PWSTR psz = (PWSTR)m_pWaveToDo->m_lToDo.Get(i);
      if (!psz)
         continue;

      if (!SentenceToPhonemeString(psz, pLex, m_pTextParse, &lPhone))
         continue;

      // add recommendations
      RecommendFromPhones (&lPhone, dwSilence, dwNumPhone, dwThresh, padwPhoneCount);
   } // i

   // see if care about any phones
   for (i = 0; i < 256; i++)
      if (afCareAbout[i] && ((padwPhoneCount[i][0] < dwThresh) ||
         (padwPhoneCount[i][1] < dwThresh) || (padwPhoneCount[i][2] < dwThresh)))
         break;
   if (i >= 256)
      return 3;

   // create a list of words that might recommend
   if (!lWords.Num())
      pLex->EnumEntireLexicon (&lWords);
   ppsz = (PWSTR*)lWords.Get(0);

   // loop over all words
   CListVariable lForm, lDontRecurse;
   WCHAR szSentence[256];
   szSentence[0] = 0;
   DWORD dwWords = 0;
   for (i = 0; i < lWords.Num(); i++) {
      // if all pronunciations satisified break
      for (j = 0; j < 256; j++)
         if (afCareAbout[j] && ((padwPhoneCount[j][0] < dwThresh) ||
            (padwPhoneCount[j][1] < dwThresh) || (padwPhoneCount[j][2] < dwThresh)))
            break;
      if (j >= 256)
         break;


      PWSTR psz = ppsz[i];
      lForm.Clear();
      lDontRecurse.Clear();
      if (!pLex->WordPronunciation (psz, &lForm, FALSE, NULL, &lDontRecurse)) // no LTS here
         continue;
      if (!lForm.Num())
         continue;

      // convert to phoneme string
      lPhone.Init(sizeof(DWORD));
      DWORD dwPhone;
      PBYTE pb = (PBYTE)lForm.Get(0) + 1;
      for (j = 0; pb[j]; j++) {
         dwPhone = pb[j] - 1;

         // see if it has an unstressed version
         PLEXPHONE plp = pLex->PhonemeGetUnsort (dwPhone);
         if (!plp)
            dwPhone = 255;  // if cant get this just make -1, // BUGFIX - Changed to 255
#if 0 // since now use stress
         if (plp) {
            if (plp->bStress && (plp->wPhoneOtherStress < dwNumPhone))
               dwPhone = plp->wPhoneOtherStress;
         }
#endif // 0

         lPhone.Add (&dwPhone);
      }

      // see if care
      if (!RecommendFromPhones (&lPhone, dwSilence, dwNumPhone, dwThresh, padwPhoneCount))
         continue;
      
      // add to sentence
      if (wcslen(szSentence)+wcslen(psz)+3 > sizeof(szSentence)/sizeof(WCHAR))
         continue;   // not enough space
      if (szSentence[0])
         wcscat (szSentence, L", ");
      wcscat (szSentence, psz);
      dwWords++;
      
      // add and clear
      if (dwWords >= 5) {
         m_pWaveToDo->AddToDo (szSentence);
         szSentence[0] = 0;
         dwWords = 0;
      }
   } // i

   // if any words in final then add
   if (szSentence[0])
      m_pWaveToDo->AddToDo (szSentence);

   // see how many holes
   DWORD dwTotalPhones, dwNotEnough, dwCouldBeMore;
   dwTotalPhones = dwNotEnough = dwCouldBeMore = 0;
   for (j = 0; j < 256; j++) {
      if (!afCareAbout[j])
         continue;

      dwTotalPhones++;

      // must have enough full phonemes
      if (padwPhoneCount[j][0] < dwThresh) {
         dwNotEnough++;
         continue;
      }

      DWORD dwGood = 0;
      dwGood += ((padwPhoneCount[j][1] >= dwThresh) ? 1 : 0);
      dwGood += ((padwPhoneCount[j][2] >= dwThresh) ? 1 : 0);
      if (!dwGood)
         dwCouldBeMore += 2;  // bugfix - was dwNotEnough=TRUE, but some unrealistic
      else if (dwGood == 1)
         dwCouldBeMore++;
   }

   // if there arent enough return 1
   if (dwNotEnough)
      return 1;   // definitely not enough
   else if (dwCouldBeMore >= dwTotalPhones/10)
      return 2;   // recommend more
   else
      return 3;   // ok
}



/************************************************************************************
CSubPhone::Constructor and destructor
*/
CSubPhone::CSubPhone (void)
{
   m_fHalfSize = FALSE;
   Clear();
}

CSubPhone::~CSubPhone (void)
{
   // nothing for now
}


/************************************************************************************
CSubPhone::Clear - Clears out subphone
*/
void CSubPhone::Clear (void)
{
   m_paTrained = m_paUntrained = NULL;
   m_dwNumTrained = m_dwNumUntrained = 0;
   m_dwNumPhone = 0;
   m_fPhoneEnergy = 0;
}

/************************************************************************************
CSubPhone::CloneTo - Standard API
*/
BOOL CSubPhone::CloneTo (CSubPhone *pTo)
{
   if (m_paTrained) {
      pTo->TrainedMem();
      memcpy (pTo->m_paTrained, m_paTrained, sizeof(PHONESAMPLE) * MAXPHONESAMPLES);
   }
   if (m_paUntrained) {
      pTo->UntrainedMem();
      memcpy (pTo->m_paUntrained, m_paUntrained, sizeof(PHONESAMPLE) * MAXPHONESAMPLES);
   }
   pTo->m_dwNumTrained = m_dwNumTrained;
   pTo->m_dwNumUntrained = m_dwNumUntrained;
   pTo->m_dwNumPhone = m_dwNumPhone;
   pTo->m_fPhoneEnergy = m_fPhoneEnergy;
   pTo->m_fHalfSize = m_fHalfSize;

   return TRUE;
}


/************************************************************************************
CSubPhone::Clone - Standard API
*/
CSubPhone *CSubPhone::Clone (void)
{
   PCSubPhone pNew = new CSubPhone;
   if (!pNew)
      return NULL;

   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}

static PWSTR gpszTrained = L"Trained1";   // BUGFIX - Upped string number since changed training format
static PWSTR gpszNumTrained = L"NumTrained";
static PWSTR gpszNumPhone = L"NumPhone";
static PWSTR gpszPhoneEnergy = L"PhoneEnergy";

/************************************************************************************
CSubPhone::MMLTo - Standard API
*/
PCMMLNode2 CSubPhone::MMLTo (void)
{
   // convert any untrained data to trained
   ConvertUntrainedToTrain();

   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszSubPhone);

   if (m_paTrained)
      MMLValueSet (pNode, gpszTrained, (PBYTE)m_paTrained, sizeof(PHONESAMPLE) * MAXPHONESAMPLES);
   MMLValueSet (pNode, gpszNumTrained, (int) m_dwNumTrained);
   MMLValueSet (pNode, gpszNumPhone, (int) m_dwNumPhone);
   MMLValueSet (pNode, gpszPhoneEnergy, m_fPhoneEnergy);

   return pNode;
}

/************************************************************************************
CSubPhone::MMLFrom - Standard API
*/
BOOL CSubPhone::MMLFrom (PCMMLNode2 pNode)
{
   m_dwNumUntrained = 0;   // to clear out

   if (!TrainedMem())
      return FALSE;

   MMLValueGetBinary (pNode, gpszTrained, (PBYTE)m_paTrained, sizeof(PHONESAMPLE)*MAXPHONESAMPLES);
   m_dwNumTrained = (DWORD) MMLValueGetInt (pNode, gpszNumTrained, (int) 0);
   m_dwNumPhone = (DWORD) MMLValueGetInt (pNode, gpszNumPhone, (int) 0);
   m_fPhoneEnergy = MMLValueGetDouble (pNode, gpszPhoneEnergy, 0);

   return TRUE;
}


/************************************************************************************
CSubPhone::HalfSize - If you call this immediately after creating, the number of
examplar samples will be halved, saving memory (at the expense of quality)

inputs
   BOOL        fHalfSize - What to set to. TRUE to use 32 points, FALSE (defaults) to 64
*/
void CSubPhone::HalfSize (BOOL fHalfSize)
{
   m_fHalfSize = fHalfSize;
}


/************************************************************************************
CSubPhone::PrepForMultiThreaded - Prepare this for multithreaded use so doesn't
crash if several threads.
*/
void CSubPhone::PrepForMultiThreaded (void)
{
   ConvertUntrainedToTrain ();
}


/************************************************************************************
CSubPhone::ConvertUntrainedToTrain - This converts any untrained phoneme data
into training, basically by selecting random elements and adding them to the
training database.

returns
   BOOL - TRUE if success
*/
BOOL CSubPhone::ConvertUntrainedToTrain (void)
{
   // if there's nothing untrained then done
   if (!m_dwNumUntrained)
      return TRUE;

   // make sure have trained and untrained
   if (!TrainedMem() || !UntrainedMem())
      return FALSE;

   // might be able to copy some over
   if (m_dwNumTrained < (DWORD) MAXPHONESAMPLES) {
      // if we havent completely filled up the training set then just copy over
      // data
      DWORD dwCopy = min (MAXPHONESAMPLES - m_dwNumTrained, m_dwNumUntrained);
      memcpy (m_paTrained + m_dwNumTrained, m_paUntrained + 0, sizeof(PHONESAMPLE)*dwCopy);
      memmove (m_paUntrained + 0, m_paUntrained + dwCopy, sizeof(PHONESAMPLE)*(m_dwNumUntrained - dwCopy));
      m_dwNumTrained += dwCopy;
      m_dwNumUntrained -= dwCopy;
   } // copy over

   // test again, if there's nothing untrained then done
   if (!m_dwNumUntrained)
      return TRUE;

   // figure out how many samples need to take from untrained to trained
   fp fCopyOver = (fp)(m_dwNumUntrained * m_dwNumUntrained) / (fp)(m_dwNumTrained + m_dwNumUntrained);
   DWORD dwCopyOver = (DWORD)fCopyOver;
   fCopyOver -= dwCopyOver;
   if (MyRand (0, 1) < fCopyOver)
      dwCopyOver++;
   dwCopyOver = min(dwCopyOver, m_dwNumUntrained);

   // copy them over
   DWORD i, j, dwFrom, dwTo;
   DWORD adwUsed[MAXPHONESAMPLESFIXED];
   for (i = 0; i < dwCopyOver; i++) {
      while (TRUE) {
         dwFrom = (DWORD)rand() % m_dwNumUntrained;

         for (j = 0; j < i; j++)
            if (dwFrom == adwUsed[j])
               break;
         if (j < i)  // if this happens then already added this one
            continue;

         // remeber this
         adwUsed[i] = dwFrom;
         break;
      } // to make sure unique
      dwTo = (DWORD)rand() % MAXPHONESAMPLES;

      m_paTrained[dwTo] = m_paUntrained[dwFrom];
   } // i

   // clear out number of untrained
   m_dwNumUntrained = 0;

   return TRUE;
}



/************************************************************************************
CSubPhone::Train - Trains the sub-phone based on the given data.

inputs
   PSRANALBLOCK      paSR - Analysis information, dwNumEntries
                        paSR->plCache is not used.
   DWORD             dwNum - Number of entries to train on
   fp                fMaxSpeechWindow - Maximum speech window value for the area.
   fp                fPhoneEnergy - Average energy across the phoneme, from SRFEATUREEnergyCalc()
   fp                fWeight - Weight to put on this. 1.0 = normal weight. Can
                        use lower values and will have less of an effect on model
returns
   BOOL - TRUE if success
*/
BOOL CSubPhone::Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow, fp fPhoneEnergy,
                       fp fWeight)
{
   // BUGFIX - If nothing then done. otherwise get divide-by-zero
   if (!dwNum)
      return TRUE;

   // BUGFIX - Deal with weight
   while (fWeight > 1.0) {
      // send in whole copies
      if (!Train (paSR, dwNum, fMaxSpeechWindow, fPhoneEnergy, 1.0))
         return FALSE;
      fWeight -= 1.0;
   }

   // do fractional amount
   if (fWeight != 1.0) {
      // if no wieght left then done
      if (fWeight <= 0.0)
         return TRUE;

      // do a jittered comb and pull out values to use
      CListFixed lSR;
      lSR.Init (sizeof(SRANALBLOCK));
      fp fDelta = 1.0 / fWeight;
      fp fCur, fRand;
      DWORD dwRand;
      for (fCur = 0; fCur < (fp)dwNum; fCur += fDelta) {
         fRand = floor(randf(fCur, fCur+fDelta));
         dwRand = (DWORD)fRand;
         if (dwRand < dwNum)
            lSR.Add (paSR + dwRand);
      } // fCur

      if (!lSR.Num())
         return TRUE;   // nothing added

      return Train ((PSRANALBLOCK)lSR.Get(0), lSR.Num(), fMaxSpeechWindow, fPhoneEnergy, 1.0);
   }

   // loop
   DWORD i;
   PPHONESAMPLE ps;
   fp fScaled;
   if (!TrainedMem() || !UntrainedMem())
      return FALSE;
   for (i = 0; i < dwNum; i++, paSR++) {
      // if the training buffer is full then train
      if (m_dwNumUntrained >= (DWORD) MAXPHONESAMPLES)
         ConvertUntrainedToTrain ();

      // fill in the info
      ps = &m_paUntrained[m_dwNumUntrained];
      m_dwNumUntrained++;

      // copy over the already-normalized features
      memcpy (&ps->sr, &paSR->sr, sizeof(paSR->sr));

      // determine the energy
      fScaled = paSR->fEnergy / fPhoneEnergy;
      ps->cDBAbovePhone = AmplitudeToDb (fScaled * (fp)0x8000);
      ps->fEnergyAfterNormal = SRFEATUREEnergySmall (TRUE, &ps->sr, FALSE, TRUE);
   } // i

   // train the wole phoneme volume, so know average volume for phoneme,
   // given a normalized maxspeechwindow
   fScaled = fPhoneEnergy / fMaxSpeechWindow * MAXSPEECHWINDOWNORMALIZED;
   // BUGFIX - Instead of increasing m_dwNumPhone by 1, increase by dwNum
   m_fPhoneEnergy = (fp) (((double)m_fPhoneEnergy * (double)m_dwNumPhone + (double)fScaled * (double)dwNum) /
      (double)(m_dwNumPhone+dwNum));
   m_dwNumPhone += dwNum;

   return TRUE;
}


/************************************************************************************
CSubPhone::CalcScore - Calculates the score (in dB) for the phoneme comparison.

inputs
   DWORD                dwSample - Sample index, from 0..MAXPHONESAMPLES-1
   PSRANALBLOCK         pSR - Single analysis block
   PSRANALCACHE         pCache - Cache information, already calculated
   fp                   fMaxSpeechWindow - Maximum speech window for the area
   fp                   fPhoneEnergy - Average energy of the entire phoneme, from SRENERGYCalc() averaged
   fp                   fDbAbovePhoneCompare - Precalc to AmplitudeToDb (pSR->fEnergy / fPhoneEnergy * (fp)0x8000);
   fp                   fDbCompare - Precalc to AmplitudeToDb (fPhoneEnergy / fMaxSpeechWindow * (fp)0x8000);
   fp                   fDbThis - Precalc to AmplitudeToDb (m_fPhoneEnergy / MAXSPEECHWINDOWNORMALIZED * (fp)0x8000);
returns
   fp - Error, in dB
*/
__inline fp CSubPhone::CalcScore (DWORD dwSample, PSRANALBLOCK pSR, PSRANALCACHE pCache,
                                  fp fMaxSpeechWindow, fp fPhoneEnergy,
                                  fp fDbAbovePhoneCompare, fp fDbCompare, fp fDbThis)
{
   fp fErr = pCache->aScore[dwSample].fScore; // default error, from 0..40 dB error

   // BUGFIX - take into account decibles over phone. If is louder/quieter than expected,
   // then error
   // Precalced: fp fDbAbovePhoneCompare = AmplitudeToDb (pSR->fEnergy / fPhoneEnergy * (fp)0x8000);
   fp fDbAvobePhoneThis = (fp)pCache->pSubPhone->m_paTrained[pCache->aScore[dwSample].dwIndex].cDBAbovePhone;
   fErr += fabs(fDbAvobePhoneThis - fDbAbovePhoneCompare) * DBABOVEPHONESCALE;

   // also calculate decibels of phone and theoretical average
   // figure out how much louder or quieter this phoneme is than expected
   //fLouder /= m_fPhoneEnergy;
   //fLouder = AmplitudeToDb (fLouder * (fp)0x8000);
   // precalced: fp fDbCompare = AmplitudeToDb (fPhoneEnergy / fMaxSpeechWindow * (fp)0x8000);
   // precalced: fp fDbThis = AmplitudeToDb (m_fPhoneEnergy / MAXSPEECHWINDOWNORMALIZED * (fp)0x8000);
   fErr += fabs(fDbCompare - fDbThis /*fLouder*/) * DBTHEORETICALPHONE;  // penalty for phone being louder/quieter than expected
      // BUGFIX - Penalty for a louder/quiter phoneme not as severe as louder/quieter
      // sections within phoneme
      // BUGFIX - Was multiplying by 0.5 but decided to take out

   // maximum, so that silence is always 0 difference
   fDbCompare += fDbAbovePhoneCompare;
   fDbThis += fDbAvobePhoneThis;
   fp fMaxDb = max(fDbCompare, fDbThis);
   fMaxDb = 1 + fMaxDb/(-(fp)SRNOISEFLOOR);   // so that at -60 decibles max, fCompare won't count for anything
   fMaxDb = max(fMaxDb, 0);
   fMaxDb = min(fMaxDb, 1);

   fErr *= fMaxDb;

   // figure out how mouch louder or quieter this sound is than the phoneme in general
   //fLouder = pSR->fEnergy / fPhoneEnergy;
   //fLouder = AmplitudeToDb (fLouder * (fp)0x8000);
   //fErr += fabs(fLouder);  // penalty for being louder/queiter than expected, within context of phone

   return fErr;
}

/************************************************************************************
SRFEATUREDistortCompare - Distorts a SRFEATURE by stretching/shrinking on pitch, and
increasing/decreasing volume, to minimize the difference from SRFEATURECompare.
The smallest SRFETURECOMPARE value is returned.


NOTE: This uses the energy to make the comparison energy/amplitude independent.

NOTE: This assumes that both speech and model DON'T have 0 energy level

inputs
   BOOL              fForUnitSelection - Set to TRUE if this is for unit selection (deciding
                     overall score for the unit). This will cause some
                     bending of the SRFeatures, and a slight variation in amplitude.
                     If FALSE, use for recogniting boundaries. Doesn't allow bending of SRFeatures
                     because too slow, but more variation in amplitude
   PSRFEATURESMALL   pSRFSpeech - SR feature from the speech
   fp                fSRFSpeech - Energy of pSRFSpeech, from SRFEATUREEnergy()
   PSRFEATURESMALL   pSRFModel - SR feature #1 from the model
   fp                fSRFModel - Energy of this feature
returns
   fp - 0 if entirely the same, 1 if entirely different
*/
fp SRFEATUREDistortCompare (BOOL fForUnitSelection,
                            PSRFEATURESMALL pSRFSpeech, fp fSRFSpeech,
                     PSRFEATURESMALL pSRFModel, fp fSRFModel)
{
#ifdef SRACCURACY_BENDFORMANTS_ON

   // if this is for unit selection then find a bend for the model
   // BUGFIX - Important to allow slight bending so that will choose units with slight variation
   SRFEATURESMALL SRFBent;
   DWORD i;
   fp fMaxScale;
   PSRFEATURESMALL pSRFModelUse = pSRFModel;
   if (fForUnitSelection) {
      fp afOctaveBend[SROCTAVE], afOctaveScale[SROCTAVE];
      DetermineBlend (FALSE, (PSRFEATURE) pSRFModel, (PSRFEATURE) pSRFSpeech, 1.0, 1.0,
                            FALSE, afOctaveBend, afOctaveScale);

      // since have scale later on, wipe to 1.0
      for (i = 0; i < SROCTAVE; i++)
         afOctaveScale[i] = 1.0;

      // bend
      SRFEATUREBendAndScale (FALSE, (PSRFEATURE) pSRFModel, NULL, afOctaveBend, afOctaveScale, (PSRFEATURE) &SRFBent, NULL);

      pSRFModelUse = &SRFBent;

      // maximum that can scale up/down is 3db
      fMaxScale = sqrt(2.0);
   }
   else
      fMaxScale = 2.0;  // can scale more for SR

   // convert to energy
   fp afVoicedOrigMain[SRDATAPOINTSSMALL], afNoiseOrigMain[SRDATAPOINTSSMALL];
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
   fp afVoicedOrigDelta[SRDATAPOINTSSMALL], afNoiseOrigDelta[SRDATAPOINTSSMALL];
#endif
   for (i = 0; i < SRDATAPOINTSSMALL; i++) {
      afVoicedOrigMain[i] = (fp)DbToAmplitude(pSRFModelUse->acVoiceEnergyMain[i]);
      afNoiseOrigMain[i] = (fp)DbToAmplitude(pSRFModelUse->acNoiseEnergyMain[i]);

#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
      afVoicedOrigDelta[i] = (fp)DbToAmplitude(pSRFModelUse->acVoiceEnergyDelta[i]);
      afNoiseOrigDelta[i] = (fp)DbToAmplitude(pSRFModelUse->acNoiseEnergyDelta[i]);
#endif
   } // i

#if 0 //def _DEBUG
   char szTemp[64];
   OutputDebugString ("\r\nSR:");
#endif

   // loop over the stretches
#define NUMSTRETCHES    1        // BUGFIX - was 5, but one seems more accurate
#define STRETCHMAX      1.3
#define SMALLOCTAVE     (SRDATAPOINTSSMALL / SROCTAVE)
   DWORD dwStretch;
   fp afVoicedStretchMain[SRDATAPOINTSSMALL], afNoiseStretchMain[SRDATAPOINTSSMALL];
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
   fp afVoicedStretchDelta[SRDATAPOINTSSMALL], afNoiseStretchDelta[SRDATAPOINTSSMALL];
#endif
   fp afEnergyOrig[SROCTAVE+1], afEnergyStretch[SROCTAVE+1], afScale[SROCTAVE+1];
   SRFEATURESMALL sNew;
   fp f, fBest = 1;
   for (dwStretch = 0; dwStretch < 1 /*(DWORD) (fAllowFeatureDistortion ? NUMSTRETCHES : 1)*/; dwStretch++) {
      // NOTE: Won't use this stretch anymore. Using the DetermineBlend() function instead
#if (NUMSTRETCHES >= 2)
       fp fStretch = pow (STRETCHMAX, (fp)((int)dwStretch - (int)((NUMSTRETCHES-1)/2)) / (fp)((NUMSTRETCHES-1)/2));

      // stretch
      for (i = 0; i < SRDATAPOINTSSMALL; i++) {
         f = (fp)i * fStretch;
         DWORD dwLeft = (DWORD)f;
         f -= (fp)f; // so get fractional
         DWORD dwRight = dwLeft+1;
         dwLeft = min(dwLeft, SRDATAPOINTSSMALL-1); // so dont go over
         dwRight = min(dwRight, SRDATAPOINTSSMALL-1);  // so dont go over

         afVoicedStretch[i] = (1.0-f) * afVoicedOrig[dwLeft] + f * afVoicedOrig[dwRight]; note - not modified to include delta
         afNoiseStretch[i] = (1.0-f) * afNoiseOrig[dwLeft] + f * afNoiseOrig[dwRight];
      } // i
#else // NUMSTRETCHED
      memcpy (afVoicedStretchMain, afVoicedOrigMain, sizeof(afVoicedStretchMain));
      memcpy (afNoiseStretchMain, afNoiseOrigMain, sizeof(afNoiseStretchMain));

#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
      memcpy (afVoicedStretchDelta, afVoicedOrigDelta, sizeof(afVoicedStretchDelta));
      memcpy (afNoiseStretchDelta, afNoiseOrigDelta, sizeof(afNoiseStretchDelta));
#endif

#endif

      if (fMaxScale) {
         // determine the energy at each octave
         memset (afEnergyOrig, 0, sizeof(afEnergyOrig));
         memset (afEnergyStretch, 0, sizeof(afEnergyStretch));
         for (i = 0; i < SRDATAPOINTSSMALL; i++) {
            // calc square of energies
            fp fEnergyOrig = afVoicedOrigMain[i] * afVoicedOrigMain[i] + afNoiseOrigMain[i] * afNoiseOrigMain[i]
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
               + afVoicedOrigDelta[i] * afVoicedOrigDelta[i] + afNoiseOrigDelta[i] * afNoiseOrigDelta[i]
#endif
               ;
            fp fEnergyStretch = afVoicedStretchMain[i] * afVoicedStretchMain[i] + afNoiseStretchMain[i] * afNoiseStretchMain[i]
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
               + afVoicedStretchDelta[i] * afVoicedStretchDelta[i] + afNoiseStretchDelta[i] * afNoiseStretchDelta[i]
#endif
               ;

            // determine the octave, and add
            DWORD dwOctave = i / SMALLOCTAVE;
            f = (fp)i / (fp)SMALLOCTAVE - (fp)dwOctave;  // so have fractional
            afEnergyOrig[dwOctave] += (1.0-f) * fEnergyOrig;
            afEnergyStretch[dwOctave] += (1.0-f) * fEnergyStretch;
            afEnergyOrig[dwOctave+1] += f * fEnergyOrig;
            afEnergyStretch[dwOctave+1] += f * fEnergyStretch;
         } // i

         // figure out the scale
         for (i = 0; i < SROCTAVE+1; i++) {
            afEnergyOrig[i] = sqrt(afEnergyOrig[i]);
            afEnergyStretch[i] = sqrt(afEnergyStretch[i]);
            afEnergyOrig[i] = max(afEnergyOrig[i], CLOSE);  // so dont have 0
            afEnergyStretch[i] = max(afEnergyStretch[i], CLOSE);  // so dont have 0

            afScale[i] = afEnergyOrig[i] / afEnergyStretch[i];
            if (afScale[i] > fMaxScale)
               afScale[i] = fMaxScale;
            else if (afScale[i] < 1.0 / fMaxScale)
               afScale[i] = 1.0 / fMaxScale;
         } // i
      }
      else {
         // no scale
         for (i = 0; i < SROCTAVE+1; i++)
            afScale[i] = 1.0;
      }

      // scale the stretched out version, AND calculate the energy, as per SRFEATUREEnergy
      fp fEnergy = 0;
      for (i = 0; i < SRDATAPOINTSSMALL; i++) {
         // determine the octave, and add
         DWORD dwOctave = i / SMALLOCTAVE;
         f = (fp)i / (fp)SMALLOCTAVE - (fp)dwOctave;  // so have fractional

         // how much to scale
         f = (1.0 - f) * afScale[dwOctave] + f * afScale[dwOctave+1];

         // scale
         afVoicedStretchMain[i] *= f;
         afNoiseStretchMain[i] *= f;
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
         afVoicedStretchDelta[i] *= f;
         afNoiseStretchDelta[i] *= f;
#endif

         fEnergy += afVoicedStretchMain[i] * afVoicedStretchMain[i] + afNoiseStretchMain[i] * afNoiseStretchMain[i]
#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
            + afVoicedStretchDelta[i] * afVoicedStretchDelta[i] + afNoiseStretchDelta[i] * afNoiseStretchDelta[i]
#endif
            ;
      } // i
      fEnergy = sqrt(fEnergy * SRSMALL);  // BUGFIX - Put in SRSMALL so would be on smae units as

      // determine the scale
      fp fScale;
      if (fEnergy)
         fScale = PHONESAMPLENORMALIZED / fEnergy;
      else
         fScale = 1;

      // scale and produce new SRFEATURE
      for (i = 0; i < SRDATAPOINTSSMALL; i++) {
         sNew.acVoiceEnergyMain[i] = AmplitudeToDb(fScale * afVoicedStretchMain[i]);
         sNew.acNoiseEnergyMain[i] = AmplitudeToDb(fScale * afNoiseStretchMain[i]);

#ifndef SRACCURACY_PSYCHOWEIGHTS_DELTA_000
         sNew.acVoiceEnergyDelta[i] = AmplitudeToDb(fScale * afVoicedStretchDelta[i]);
         sNew.acNoiseEnergyDelta[i] = AmplitudeToDb(fScale * afNoiseStretchDelta[i]);
#endif
      } // i

      // redermine the energy of this
      // fEnergy = SRFEATUREEnergySmall (TRUE, &sNew, FALSE,TRUE);

      // compare
      f = SRFEATURECompareSmall (TRUE, pSRFSpeech, fSRFSpeech, &sNew /*, fEnergy*/);
#if 0 //def _DEBUG
      sprintf (szTemp, "%g ", (double)f);
      OutputDebugString (szTemp);
#endif
      if (!dwStretch || (f < fBest))
         fBest = f;
   } // dwStretch

#if 0 //def _DEBUG
      sprintf (szTemp, "B = %g", (double)fBest);
      OutputDebugString (szTemp);
#endif

   return fBest;

#else // ifdef SRACCURACY_BENDFORMANTS_OFF
      return SRFEATURECompareSmall (TRUE, pSRFSpeech, pSRFModel);
#endif

}

/************************************************************************************
CSubPhone::CompareSingle - Compares a single SRANALBLOCK with this phoneme and
returns a score (in dB) for the error rate.

This does the following:

1) If the pSR->plCache does NOT include a cache for this sub-phone then created one...
   a) loop through all the entries in m_pTrained and compare them to pSR, taking difference
   b) Store that in the cache.
2) Given that a cache exists, loops through all the points and finds the lowest (10%)
   scores. These are averaged togehter and returned.

inputs
   PSRANALBLOCK         pSR - Single analysis block
   fp                   fMaxSpeechWindow - Maximum speech window for the area
   fp                   fPhoneEnergy - Average energy of the entire phoneme, from SRENERGYCalc() averaged
   BOOL                 fForUnitSelection - Set to TRUE if this is being used for unitselect, in which
                        case a much smaller TOPPERCENT is used so that units aren't selected based
                        on their mediocraty, but their basic "sounding like" the phoneme in question.
                        Use FALSE for actual phoneme boundary detection.
   BOOL                 fAllowFeatureDistortion - If TRUE then allow distortion. If FALSE then don't allow any volume change.
                        Use FALSE for picking units for TTS. Use TRUE for SR alignment
                        BUGFIX - Put this in since seems like wrong (bass-y) units selected for TTS voice
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
returns
   fp - Score in dB error
*/
#define TOPN         ((fHalfExamples ? HALFMAXPHONESAMPLES : MAXPHONESAMPLES) / TOPPERCENT)

static int _cdecl FPSort (const void *elem1, const void *elem2)
{
   fp pdw1, pdw2;
   pdw1 = *((fp*) elem1);
   pdw2 = *((fp*) elem2);

   if (pdw1 > pdw2)
      return 1;
   else if (pdw1 < pdw2)
      return -1;
   else
      return 0;
}

fp CSubPhone::CompareSingle (PSRANALBLOCK pSR, fp fMaxSpeechWindow, fp fPhoneEnergy,
                             BOOL fForUnitSelection, BOOL fAllowFeatureDistortion,
                             BOOL fHalfExamples)
{
   // precalc for CalcScore
   fp fDbAbovePhoneCompare = AmplitudeToDb (pSR->fEnergy / fPhoneEnergy * (fp)0x8000);
   fp fDbCompare = AmplitudeToDb (fPhoneEnergy / fMaxSpeechWindow * (fp)0x8000);
   fp fDbThis = AmplitudeToDb (m_fPhoneEnergy / MAXSPEECHWINDOWNORMALIZED * (fp)0x8000);


   PSRANALCACHE pc = (PSRANALCACHE) pSR->plCache->Get(0);
   DWORD dwNum = pSR->plCache->Num();
   DWORD dwMaxPhoneSamples = fHalfExamples ? HALFMAXPHONESAMPLES : MAXPHONESAMPLES;
   DWORD dwMaxCompare = min(m_dwNumTrained, dwMaxPhoneSamples );
   DWORD i;
   for (i = 0; i < dwNum; i++, pc++)
      if (pc->pSubPhone == this)
         break;

   // if didn't find cache then create
   if (i >= dwNum) {
      SRANALCACHE ac;
      ac.pSubPhone = this;
      for (i = 0; i < dwMaxCompare; i++) {
         ac.aScore[i].dwIndex = i;
         ac.aScore[i].fScore = SRFEATUREDistortCompare (fForUnitSelection, &pSR->sr, pSR->fEnergyAfterNormal,
                     &m_paTrained[i].sr, m_paTrained[i].fEnergyAfterNormal);
            // BUGFIX - Was just SRFEATURECompare, but add distortion
            // BUGFIX - Was passing in fAllowFeatureDisortion, but changed so that only
            // use flag for unit selection to determine distorion

         ac.aScore[i].fScore = max(ac.aScore[i].fScore, 0);
         ac.aScore[i].fScore *= SRCOMPAREWEIGHT; // 40; // BUGFIX - Provide a 40 "db" range
            // BUGFIX - Was 40. Upped to 80 since seem to improve quality
      }
      ac.dwScoreCount = dwMaxCompare;

      qsort (ac.aScore, dwMaxCompare, sizeof(SRANALSCORE), FPSort);

      pSR->plCache->Add (&ac);
      pc = (PSRANALCACHE) pSR->plCache->Get(dwNum);   // since will be the last once added
   } // i

   DWORD dwTopN = min(dwMaxCompare/TOPPERCENTUNIT+1, (DWORD)TOPN);   //+1 makes sure round up
   _ASSERTE ((MAXPHONESAMPLESFIXED / TOPPERCENTUNIT) >= 4);
      // assert makes sure have enough examples to be statistically relevent
      // BUGFIX - Changed from >=8 to >= 4

      // BUGFIX - dwTopN used to be dwMaxCompare/(fForUnitSelection ? TOPPERCENTUNIT : TOPPERCENT)+1,
      // useding TOPPERCENT of 2 since assumed that more units would produce a more accurate score
      // when determining how much the phoneme sounded like the original, which is DOES NOT
   dwTopN = min(dwTopN, dwMaxCompare); // BUGFIX - So dont try to read beyond end of range
#ifdef OLDSORT // bubble sort, designed for small #
   // figure out how many want in the top N
   fp afTop[TOPN];
   dwTopN = max(dwTopN, 2);   // try to have at least 2 to compare
   dwTopN = min(dwTopN, dwMaxCompare);
   if (!dwTopN)
      return 100000; // cant actually compare since there aren't any points

   // fill in the topN
   for (i = 0; i < dwTopN; i++)
      afTop[i] = CalcScore (i, pSR, pc, fPhoneEnergy);

   // sort it
   qsort (afTop, dwTopN, sizeof(fp), FPSort);

   // loop over the others and do a bubble sort...
   fp fCurMax = afTop[dwTopN-1];
   int j;
   for (i = dwTopN; i < dwMaxCompare; i++) {
      fp fScore = CalcScore (i, pSR, pc, fPhoneEnergy);

      if (fScore >= fCurMax)
         continue;   // too high

      // else, find slot... linear search.. should be quick enough
      // because no more than 100 entries, while a btree is 8 searches,
      // but lots more code
      for (j = (int)dwTopN-2; j >= 0; j--)
         if (fScore >= afTop[j])
            break;
      j++;  // so insert after

      // insert this entry
      memmove (afTop + (j+1), afTop + j, (dwTopN - (DWORD)j - 1) * sizeof(fp));
      afTop[j] = fScore;

      // update new max
      fCurMax = afTop[dwTopN-1];
   } // i

   // return the average of these scores
   fp fSum = 0;
   for (i = 0; i < dwTopN; i++)
      fSum += afTop[i];
   return fSum / (fp)dwTopN;
#else // new sort
   // BUGFIX - This new code does a ramp on the weight afforded to the best
   // and worse scores
   fp afTop[MAXPHONESAMPLESFIXED];

   // ERROR-INDUCING OPTIMIZATION - This optimizations creates a very small amount
   // of error that shouldnt be detectable, but it speed up SR by around 2x.
   // Since pc->ac is sorted so the lowest scores come first, and since
   // CalcScore() won't change pc->ac[] that much (most of the change affects
   // all equally), and since we have a ramp applied to weighting, only
   // calculating the scores and sorting them for the best half of the
   // dwMaxCompare samples
   dwMaxCompare = max(min(dwMaxPhoneSamples/MAXCOMPAREDIVIDE, dwMaxCompare), dwTopN);
   _ASSERTE (MAXCOMPAREDIVIDE <= TOPPERCENTUNIT);
   
   // BUGFIX - Make sure not to go beyond max scores
   if (dwMaxCompare > pc->dwScoreCount) {
      _ASSERTE (FALSE);
      dwMaxCompare = pc->dwScoreCount; // NOTE: Shouldnt happen, but just in case
   }

   // fill in the topN
   for (i = 0; i < dwMaxCompare; i++)
      afTop[i] = CalcScore (i, pSR, pc, fMaxSpeechWindow, fPhoneEnergy,
         fDbAbovePhoneCompare, fDbCompare, fDbThis);

   // sort it
   qsort (afTop, dwMaxCompare, sizeof(fp), FPSort);

   // sum the top
   fp fSum = 0;
   DWORD dwCount = 0;
   for (i = 0; i < dwTopN; i++) {
      DWORD dwWeight = dwTopN - i;
      dwCount += dwWeight;
      fSum += (fp)dwWeight * afTop[i];
   } // i
   return fSum / (fp)max(dwCount,1);   // BUGFIX - just in case !dwCount
#endif
}


/************************************************************************************
CSubPhone::Compare - Compares a subphoneme block to speech recognition data that's
in paSR.

inputs
   PSRANALBLOCK         pSR - Array of dwNum blocks
   DWORD                dwNum - Number of blocks
   fp                   fMaxSpeechWindow - Maximum speech window for the area
   fp                   fPhoneEnergy - Average energy of the entire phoneme, from SRENERGYCalc() averaged
   BOOL                 fForUnitSelection - Set to TRUE if this is being used for unitselect, in which
                        case a much smaller TOPPERCENT is used so that units aren't selected based
                        on their mediocraty, but their basic "sounding like" the phoneme in question.
                        Use FALSE for actual phoneme boundary detection.
   BOOL                 fAllowFeatureDistortion - If TRUE then allow distortion. If FALSE then don't allow any volume change.
                        Use FALSE for picking units for TTS. Use TRUE for SR alignment
                        BUGFIX - Put this in since seems like wrong (bass-y) units selected for TTS voice
   BOOL                 fFast - If TRUE then this skips every other sample
   BOOL                 fHalfExamples - If TRUE, then use only half the examples when doing SR compare.
                           This will make SR twice as fast, but slightly less accurate
returns
   fp - Score in dB error
*/
fp CSubPhone::Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow, fp fPhoneEnergy,
                       BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fFast,
                       BOOL fHalfExamples)
{
   if (!dwNum)
      return 0;   // since nothing to test

   // make sure trained
   ConvertUntrainedToTrain ();

   // loop through all the samples and determine the average error
   DWORD i;
   DWORD dwInc = fFast ? 2 : 1;
   DWORD dwTotal = 0;
   fp fErr = 0;
   for (i = 0; i < dwNum; i += dwInc) {
      fErr += CompareSingle (paSR + i, fMaxSpeechWindow, fPhoneEnergy, fForUnitSelection, fAllowFeatureDistortion,
         fHalfExamples);
      dwTotal++;
   }
   fErr /= (fp)dwTotal;

#if 0 // now handled in CalcScore()
   // figure out how much louder or quieter this phoneme is than expected
   fp fCompare = fPhoneEnergy / fMaxSpeechWindow; //  * MAXSPEECHWINDOWNORMALIZED / MAXSPEECHWINDOWNORMALIZED;
   //fLouder /= m_fPhoneEnergy;
   //fLouder = AmplitudeToDb (fLouder * (fp)0x8000);
   fp fDbCompare = AmplitudeToDb (fCompare * (fp)0x8000);
   fp fDbThis = AmplitudeToDb (m_fPhoneEnergy / MAXSPEECHWINDOWNORMALIZED * (fp)0x8000);
   fErr += fabs(fDbCompare - fDbThis /*fLouder*/);  // penalty for phone being louder/quieter than expected
      // BUGFIX - Penalty for a louder/quiter phoneme not as severe as louder/quieter
      // sections within phoneme
      // BUGFIX - Was multiplying by 0.5 but decided to take out

   // maximum, so that silence is always 0 difference
   fp fMaxDb = max(fDbCompare, fDbThis);
   fMaxDb = 1 + fMaxDb/(-(fp)SRNOISEFLOOR);   // so that at -60 decibles max, fCompare won't count for anything
   fMaxDb = max(fMaxDb, 0);
   fMaxDb = min(fMaxDb, 1);

   fErr *= fMaxDb;
#endif // 0

   return fErr;
}


/************************************************************************************
CSubPhone::FillWaveWithTraining - Fills the SRFEATUREs with training information.

inputs
   PSRFEATURE        pSRF - To be filled with training information. This buffer is
                     large enough. Could be NULL, in which case just checking
                     to see how much space is necessary.
returns
   DWORD - Number of SRFEATURE samples that need.
*/
DWORD CSubPhone::FillWaveWithTraining (PSRFEATURE pSRF)
{
   ConvertUntrainedToTrain ();

   DWORD i;
   SRFEATURESMALL sms;
   if (pSRF) for (i = 0; i < m_dwNumTrained; i++, pSRF++) {
      // determine how much to scale this
      fp fScale = DbToAmplitude(m_paTrained[i].cDBAbovePhone) / (fp)0x8000;
      fScale *= m_fPhoneEnergy;
      fScale /= PHONESAMPLENORMALIZED;

      sms = m_paTrained[i].sr;
      SRFEATUREScale (&sms, fScale);
      SRFEATUREConvert (&sms, pSRF, TRUE);
   }

   return m_dwNumTrained;
}


/************************************************************************************
CSubPhone::TrainedMem - Makes sure the memory is allocated for m_paTrained. If
it isn't, this allocated it. Returns FALSE if allocation fails
*/
BOOL CSubPhone::TrainedMem (void)
{
   if (m_paTrained)
      return TRUE;

   if (!m_memTrained.Required (MAXPHONESAMPLES * sizeof(PHONESAMPLE)))
      return FALSE;

   m_paTrained = (PPHONESAMPLE) m_memTrained.p;
   memset (m_paTrained, 0, MAXPHONESAMPLES * sizeof(PHONESAMPLE));   // make sure trained ones are zeroed
   return TRUE;
}

/************************************************************************************
CSubPhone::UntrainedMem - Makes sure the memory is allocated for m_paUntrained. If
it isn't, this allocated it. Returns FALSE if allocation fails
*/
BOOL CSubPhone::UntrainedMem (void)
{
   if (m_paUntrained)
      return TRUE;

   if (!m_memUntrained.Required (MAXPHONESAMPLES * sizeof(PHONESAMPLE)))
      return FALSE;

   m_paUntrained = (PPHONESAMPLE) m_memUntrained.p;
   // don't need to zero untrained
   return TRUE;
}



/************************************************************************************
CSRAnal::Constructor and destructor
*/
CSRAnal::CSRAnal (void)
{
   m_dwNum = 0;
}

CSRAnal::~CSRAnal (void)
{
   Clear();
}


/************************************************************************************
CSRAnal::ClearCachedSR - Clears cached SR information. Need to call in some cases
if reusing the sr cache with the same phoneme (but retrained)
*/
void CSRAnal::ClearCachedSR (void)
{
   DWORD i;
   PSRANALBLOCK psab = (PSRANALBLOCK) m_mem.p;
   for (i = 0; i < m_dwNum; i++)
      psab[i].plCache->Clear();
}

/************************************************************************************
CSRAnal::Clear - Frees up memory allocated
*/
void CSRAnal::Clear(void)
{
   DWORD i;
   PSRANALBLOCK psab = (PSRANALBLOCK) m_mem.p;
   for (i = 0; i < m_dwNum; i++)
      delete psab[i].plCache;
   m_dwNum = 0;
}


/************************************************************************************
CSRAnal::Init - This converts allthe SRFEATURE's in paSRFature to SRANALBLOCK
structures, that can then be used for training or speech recognition. The memory
is valid until this object is freed, or init is called again.

inputs
   PSRFEATURE     paSRFeature - Array of dwNum features
   DWORD          dwNum - Number of features
   BOOL           fPrevIsValid - Set to TRUE if paSRFeature[-1] is valid. Used for delta
   fp             *pfMaxSpeechWindow - Maximum speech window calculated, if not NULL
returns
   PSRANALBLOCk - Block, or NULL if error
*/
PSRANALBLOCK CSRAnal::Init (PSRFEATURE paSRFeature, DWORD dwNum, BOOL fPrevIsValid, fp *pfMaxSpeechWindow)
{
   Clear();

   DWORD dwNeed = dwNum *sizeof(SRANALBLOCK);
   if (!m_mem.Required (dwNeed))
      return NULL;
   PSRANALBLOCK psab = (PSRANALBLOCK) m_mem.p;

   DWORD i;
   m_dwNum = dwNum;
   fp fMax = 0;
   for (i = 0; i < dwNum; i++) {
      SRFEATUREConvert (paSRFeature + i, (i || fPrevIsValid) ? &paSRFeature[(int)i-1] : NULL, &psab[i].sr);
      psab[i].fEnergy = SRFEATUREEnergySmall (TRUE, &psab[i].sr, FALSE, TRUE);
      psab[i].plCache = new CListFixed; // assume works
      psab[i].plCache->Init (sizeof(SRANALCACHE));
      SRFEATUREScale (&psab[i].sr, PHONESAMPLENORMALIZED / psab[i].fEnergy);
      psab[i].fEnergyAfterNormal = SRFEATUREEnergySmall (TRUE, &psab[i].sr, FALSE, TRUE);

      // calculate the maximum speech window
      if (pfMaxSpeechWindow) {
         fp fCalc = SRFEATUREEnergy (TRUE, paSRFeature + i);
         fMax = max(fMax, fCalc);
      }

   } // i

   if (pfMaxSpeechWindow)
      *pfMaxSpeechWindow = fMax;

   return psab;
}
