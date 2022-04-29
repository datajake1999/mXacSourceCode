/* M3DWave.h - internal to m3dwave.exe */

#ifndef _M3DWAVE_H_
#define _M3DWAVE_H_



/***********************************************************************************
CWaveView */
extern char gszKeyTrainFile[];
extern char gszKeyLexFile[];
extern char gszKeyTTSFile[];


/***********************************************************************************
FFT */

#define SRNOISEFLOOR          -70      // when doing SR features, dont allow quieter than this
#define SRABSOLUTESILENCE     -110     // -110 db
#define SRMAXLOUDNESS         (-(SRABSOLUTESILENCE))      // maximum loudness

/* CSinLUT - Sin look-up table for FTT */
class CSinLUT {
public:
   ESCNEWDELETE;

   CSinLUT (void);
   BOOL Init (DWORD dwNum);

   __inline double SinFast (int iNumerator, DWORD dwDenominator)
      // NOTE: iNumberator can be negative
      // NOTE: dwDenominator is an integer fraction of m_dwNum, which is OK for FFTs
   {
      if (iNumerator < 0) {
         iNumerator = ((-iNumerator) % (int)dwDenominator); // work around c's negative modulo
         if (iNumerator)
            iNumerator = (int)dwDenominator - iNumerator;
         // else, leave as 0
      }
      else
         iNumerator = iNumerator % (int)dwDenominator;   // modulo
      iNumerator *= (int) (m_dwNum / dwDenominator);
      return m_pafLUT[iNumerator];
   }

   __inline double CosFast (int iNumerator, DWORD dwDenominator)
      // NOTE: same restrictions as SinFast
   {
      if (dwDenominator < 4) {
         // so dont get divide
         iNumerator *= (int)(m_dwNum / dwDenominator);
         dwDenominator = m_dwNum;
      }
      return SinFast (iNumerator + (int)dwDenominator/4, dwDenominator);
   }

private:
   DWORD          m_dwNum;       // number of points.
   double             *m_pafLUT;     // m_dwNumEntries
   CMem           m_memSin;      // memory to store the lookup in
};

typedef CSinLUT *PCSinLUT;

class CSinLUT;
typedef CSinLUT *PCSinLUT;
void FFTRecurseReal (float *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PCMem pMemScratch);
#ifdef _DEBUG
void FFTRecurseRealOld (float *paf, DWORD dwNum, int iSign, PCSinLUT pLUT, PCMem pMemScratch);
#endif
fp HanningWindow (fp fAlpha);
void CreateFFTWindow (DWORD dwType, float *pafWindow, DWORD dwWindowSize,
                      BOOL fAreaScale = TRUE);
int SineLUT (DWORD dwAngle);
int DbToAmplitude (char dB);
void SRDETAILEDPHASEToSRFEATURE (PSRDETAILEDPHASE pSDP, fp fPitch, PSRFEATURE pSRF);
void SRDETAILEDPHASEFromSRFEATURE (PSRFEATURE pSRF, fp fPitch, PSRDETAILEDPHASE pSDP);
void SRDETAILEDPHASEShiftAndBlend (PSRDETAILEDPHASE paSDP, DWORD dwNum, DWORD dwIndex,
                                   DWORD dwWidth, DWORD dwWidthTop, BOOL fShift, BOOL fBlend);
void CalcPscyhoacousticWeights (void);
void SRFEATUREToHarmonics (PSRFEATURE pSRF, DWORD dwHarmonics, float *pafVoiced, float *pafNoise, fp fPitch);
void SRFEATUREFromHarmonics (DWORD dwHarmonics, float *pafVoiced, float *pafNoise, fp fPitch, PSRFEATURE pSRF);
void SRFEATUREPhaseInterp (DWORD dwHarmonic, fp fAlpha, PSRFEATURE pSRFA, PSRFEATURE pSRFB, PSRFEATURE pSRFDest);

/******************************************************************************
AmplitudeToDb - Converts from an amplitude to dB

inputs
   fp       fAmp - Amplitude. 0x8000 will go to 0 dB.
returns
   char - Decibels
*/
__inline char AmplitudeToDb (double fAmp)
{
   fAmp = max(0.0001, fAmp);  // BUGFIX - Was .01
   fAmp = log10 (fAmp / (double)0x8000) * 20.0 + 0.5; // BUGFIX - added rounding
   fAmp = min(fAmp, 127);
   fAmp = max(fAmp, SRABSOLUTESILENCE);

   return (char)(int)floor(fAmp);   // BUGFIX - Added floor to be explicit
}

void SRFEATUREInterpolatePhase (PSRFEATURE paSRFeature, DWORD dwNum, fp *pafWeight, PSRFEATURE pFinal);
void SRFEATUREInterpolate (PSRFEATURE paSRFeature, DWORD dwNum, fp *pafWeight, PSRFEATURE pFinal);
void SRFEATUREInterpolate (PSRFEATURE pSRFeatureA, PSRFEATURE pSRFeatureB,
                           fp fWeightA, PSRFEATURE pFinal);
fp SRFEATUREEnergy (BOOL fPsycho, PSRFEATURE pSRFeature, BOOL fVoicedOnly = FALSE);
fp SRFEATUREEnergySmall (BOOL fPsycho, PSRFEATURESMALL pSRFeature, BOOL fVoicedOnly /*= FALSE*/, BOOL fIncludeDelta);
fp SRFEATURECompareSmall (BOOL fPsycho, PSRFEATURESMALL pSRFSpeech,
                     PSRFEATURESMALL pSRFModel);
fp SRFEATURECompare (BOOL fPsycho, PSRFEATURE pSRFSpeech, fp fSRFSpeech,
                     PSRFEATURE pSRFModel1, fp fSRFModel1,
                     PSRFEATURE pSRFModel2, fp fSRFModel2,
                     fp fWeightModel1);
fp SRFEATURECompareSequence (BOOL fPsycho, PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum, fp fMaxSpeechWindow,
                             PSRFEATURE pSRFModel1, fp fSRFModel1,
                             PSRFEATURE pSRFModel2, fp fSRFModel2,
                             PSRFEATURE pSRFModel3, fp fSRFModel3);
void SRFEATUREConvert (PSRFEATURESMALL pFrom, PSRFEATURE pTo, BOOL fIncludeDelta);
void SRFEATUREConvert (PSRFEATURE pFrom, PSRFEATURE pPrev, PSRFEATURESMALL pTo);
void SRFEATUREScale (PSRFEATURESMALL pSR, fp fScale);
void SRFEATUREScale (PSRFEATURE pSR, fp fScale);
fp SRFEATURECompareAbsolute (BOOL fPsycho, PSRFEATURE pSRFSpeech, PSRFEATURE pSRFModel);


/***********************************************************************************
Lexicon.cpp */
extern CRITICAL_SECTION gcsLexiconCache;   // critical section for lexiconf cache

PWSTR LexPhoneList (PCMLexicon pLex);


/***********************************************************************************
SpeechRecog.cpp */

// #define SRACCURACY_SRCOMPAREWEIGHT_40
// #define SRACCURACY_SRCOMPAREWEIGHT_60
// #define SRACCURACY_SRCOMPAREWEIGHT_80
#define SRACCURACY_SRCOMPAREWEIGHT_100
// #define SRACCURACY_SRCOMPAREWEIGHT_120
// #define SRACCURACY_SRCOMPAREWEIGHT_140
// #define SRACCURACY_SRCOMPAREWEIGHT_160

#ifdef SRACCURACY_SRCOMPAREWEIGHT_40
#define SRCOMPAREWEIGHT       40       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_60
#define SRCOMPAREWEIGHT       60       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_80
#define SRCOMPAREWEIGHT       80       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_100
#define SRCOMPAREWEIGHT       100       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_120
#define SRCOMPAREWEIGHT       120       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_140
#define SRCOMPAREWEIGHT       140       // after doing a compare, scale the energy by this much
#endif

#ifdef SRACCURACY_SRCOMPAREWEIGHT_160
#define SRCOMPAREWEIGHT       160       // after doing a compare, scale the energy by this much
#endif


// #define SRACCURACY_SUBPHONEPERPHONE_3
// #define SRACCURACY_SUBPHONEPERPHONE_4
// #define SRACCURACY_SUBPHONEPERPHONE_5
#define SRACCURACY_SUBPHONEPERPHONE_6
// #define SRACCURACY_SUBPHONEPERPHONE_7

#ifdef SRACCURACY_SUBPHONEPERPHONE_3
#define SUBPHONEPERPHONE      3        // number of sub-phonemes per phoneme
   // BUGFIX - Was 5, but trying to reduce size of models
#endif
#ifdef SRACCURACY_SUBPHONEPERPHONE_4
#define SUBPHONEPERPHONE      4        // number of sub-phonemes per phoneme
   // BUGFIX - Was 5, but trying to reduce size of models
#endif
#ifdef SRACCURACY_SUBPHONEPERPHONE_5
#define SUBPHONEPERPHONE      5        // number of sub-phonemes per phoneme
#endif
#ifdef SRACCURACY_SUBPHONEPERPHONE_6
#define SUBPHONEPERPHONE      6        // number of sub-phonemes per phoneme
#endif
#ifdef SRACCURACY_SUBPHONEPERPHONE_7
#define SUBPHONEPERPHONE      7        // number of sub-phonemes per phoneme
#endif



DWORD PhonemeLetterToPhonemes (PWSTR psz, DWORD dwChars, DWORD *padwPhoneIndex, DWORD dwNumLeft);
DWORD PhonemeDictionary (PWSTR psz, DWORD dwChars, DWORD *padwPhoneIndex, DWORD dwNumLeft);


//#define SRACCURACY_MAXPHONESAMPLESFIXED_32
//#define SRACCURACY_MAXPHONESAMPLESFIXED_64
// #define SRACCURACY_MAXPHONESAMPLESFIXED_128
#define SRACCURACY_MAXPHONESAMPLESFIXED_256
// #define SRACCURACY_MAXPHONESAMPLESFIXED_512
   // BUGBUG - better accuracy with SRACCURACY_MAXPHONESAMPLESFIXED_512 | SRACCURACY_TOPPERCENTUNIT_32 | SRACCURACY_SRSMALL_3, but
   // BUGBUG - May be better with SRACCURACY_TOPPERCENTUNIT_64
   // takes too much memory


#ifdef SRACCURACY_MAXPHONESAMPLESFIXED_32
#define MAXPHONESAMPLESFIXED       32     // maximum number of samples stored about a phone
#endif
#ifdef SRACCURACY_MAXPHONESAMPLESFIXED_64
#define MAXPHONESAMPLESFIXED       64     // maximum number of samples stored about a phone
   // BUGFIX - Was 1000, but dont think need quite to many
   // BUGFIX - Upped to 96 from 60
   // BUGFIX - Looking at the data, and noticing the slowness, 96 is way too many
   // BUGFIX - Update from 32 to 48 for a bit more stability
   // BUGFIX - Upped from 48 to 64 since added delta to srfeature
#endif
#ifdef SRACCURACY_MAXPHONESAMPLESFIXED_128
#define MAXPHONESAMPLESFIXED       128     // maximum number of samples stored about a phone
#endif
#ifdef SRACCURACY_MAXPHONESAMPLESFIXED_256
#define MAXPHONESAMPLESFIXED       256     // maximum number of samples stored about a phone
#endif
#ifdef SRACCURACY_MAXPHONESAMPLESFIXED_512
#define MAXPHONESAMPLESFIXED       512     // maximum number of samples stored about a phone
#endif

// CSubPhone - Stores a fraction (one-third or one-fifth) or a phoneme
#define PHONESAMPLENORMALIZED       10000 // normalized energy
#define MAXSPEECHWINDOWNORMALIZED   10000 // normalized max speech window

#define THEORETICALMAXENERGY        (10000.0 * (fp)SRDATAPOINTS / 84.0)
      // NOTE: Picking 15,000 mostly out of blue since seems to be about the lowest energy that get per normalized wave
      // BUGFIX - Changed once increased number of srdata points
      // BUGFIX - Move to 10000 base (instead of 15000) to minimumze chance to clipping

// PHONESAMPLE - Information stored about a phone sample
typedef struct {
   SRFEATURESMALL    sr;            // features, normalized to an energy of PHONESAMPLENORMALIZED
   fp                fEnergyAfterNormal;  // energy after noramlized, should be PHONESAMPLENORMALIZED, but never exactly right
   char              cDBAbovePhone; // number of DB above (or below) the standard volume for the phone
                                    // that was trained on
   char              cFill;         // filler to make word aligned
} PHONESAMPLE, *PPHONESAMPLE;

// SRANALSCORE - Store scores, along with original index
typedef struct {
   fp                fScore;        // score calculated by comparing sr features
   DWORD             dwIndex;       // original index
} SRANALSCORE, *PSRANALSCORE;

// SRANALCACHE - Cache information for pre-processed data
class CSubPhone;
typedef CSubPhone *PCSubPhone;
typedef struct {
   PCSubPhone        pSubPhone;     // sub-phoneme that cached the data
   SRANALSCORE       aScore[MAXPHONESAMPLESFIXED];  // score calculated by comparing SR features
                                    // normalized to PHONESAMPLENORMALIZED energy
   DWORD             dwScoreCount;  // number of valid entries in aScore
} SRANALCACHE, *PSRANALCACHE;

// CSRAnal - Object that quickly analyzes SRFEATUREs to make appropriate data structures
class CSRAnal {
public:
   ESCNEWDELETE;

   CSRAnal (void);
   ~CSRAnal (void);

   PSRANALBLOCK Init (PSRFEATURE paSRFeature, DWORD dwNum, BOOL fPrevIsValid, fp *pfMaxSpeechWindow);
   void ClearCachedSR (void);

   // can read, but don't modify
   DWORD          m_dwNum;       // current number there
   CMem           m_mem;         // memory that contains data

private:
   void Clear (void);

};
typedef CSRAnal *PCSRAnal;


class CSubPhone {
public:
   ESCNEWDELETE;

   CSubPhone (void);
   ~CSubPhone (void);
   void Clear (void);

   BOOL CloneTo (CSubPhone *pTo);
   CSubPhone *Clone (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   void HalfSize (BOOL fHalfSize);

   BOOL Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow, fp fPhoneEnergy,
      fp fWeight);
   fp Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow, fp fPhoneEnergy,
      BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fFast,
      BOOL fHalfExamples);
   DWORD FillWaveWithTraining (PSRFEATURE pSRF);

   void PrepForMultiThreaded (void);

private:
   BOOL TrainedMem (void);
   BOOL UntrainedMem (void);
   BOOL ConvertUntrainedToTrain (void);
   fp CompareSingle (PSRANALBLOCK pSR, fp fMaxSpeechWindow, fp fPhoneEnergy,
      BOOL fForUnitSelection, BOOL fAllowFeatureDistortion,
      BOOL fHalfExamples);
   fp CalcScore (DWORD dwSample, PSRANALBLOCK pSR, PSRANALCACHE pCache,
      fp fMaxSpeechWindow, fp fPhoneEnergy,
      fp fDbAbovePhoneCompare, fp fDbCompare, fp fDbThis);

   BOOL              m_fHalfSize;            // set to TRUE immediately after creation by calling HalfSize()
   CMem              m_memTrained;           // memory for storing trained
   CMem              m_memUntrained;         // memory for storing untrained
   PPHONESAMPLE      m_paTrained;            // pointer to an array of trained, MAXPHONESAMPLES
   PPHONESAMPLE      m_paUntrained;          // pointer to an array of untrained, MAXPHONESAMPLES
   //PHONESAMPLE       m_aTrained[MAXPHONESAMPLES];  // list of trained data
   //PHONESAMPLE       m_aUntrained[MAXPHONESAMPLES];   // list of untrained data
   DWORD             m_dwNumTrained;         // number of phone samples used for training so far.
                                             // if < MAXPHONESAMPLES, then only the first m_dwNumTrained
                                             // elements of m_aTrained are valid
   DWORD             m_dwNumUntrained;       // number of untrained phone samples in m_aUntrained

   DWORD             m_dwNumPhone;           // number of phoneme SRFEATUREs that have been trained here
   fp                m_fPhoneEnergy;         // average energy for the an average phoneme (in SRDATAPOINTSMALL energy) for all phonemes
                                             // this has been normalized assuming an average
                                             // fMaxSpeechWindow of MAXSPEECHWINDOWNORMALIZED
};
typedef CSubPhone *PCSubPhone;


// CPhoneInstance - Instance recording of a phoneme
class CPhoneInstance {
public:
   ESCNEWDELETE;

   CPhoneInstance (void);
   ~CPhoneInstance (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

#ifdef OLDSR
   fp Compare (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                             fp fMaxSpeechWindow);
   BOOL Train (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                            fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter);
#else
   BOOL Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter, fp fWeight);
   fp Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
      BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fFast,
      BOOL fHalfExamples);
   DWORD FillWaveWithTraining (PSRFEATURE pSRF);
#endif

   void PrepForMultiThreaded (void);

   CPhoneInstance *Clone (void);
   BOOL CloneTo (CPhoneInstance *pNew);
   void Clear (void);
   void FillWithSilence (fp fMaxEnergy);
   void HalfSize (BOOL fHalfSize);

   WCHAR             m_szBefore[16];         // phoneme before
   WCHAR             m_szAfter[16];          // phoneme after
   BOOL              m_fRoughSketch;         // if this is a rough sketch phone then only has one SUBPHONEPERPHONE

#ifdef OLDSR
   SRFEATURE         m_aSRFeature[3];        // sr features for the beginning, middle, and end
#else
   CSubPhone         m_aSubPhone[SUBPHONEPERPHONE];   // array of sub-phopnemes
#endif

   fp                m_fTrainCount;          // number of times have trained on this phone
   fp                m_fAverageDuration;     // average duration in samples

   // calculated
#ifdef OLDSR
   fp                m_afSRFEnergy[3];       // energy of the SR features
#endif

   //int               m_iScratch;          // used my CPhoneme when recognizing

private:
#ifdef OLDSR
   void CalcEnergy (void);
#endif
};
typedef CPhoneInstance *PCPhoneInstance;


// CPhoneme - Store information about a phoneme
class CPhoneme {
public:
   ESCNEWDELETE;

   CPhoneme (void);
   ~CPhoneme (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CPhoneme *Clone (void);
   void Clear (void);
   void HalfSize (BOOL fHalfSize);

#ifdef OLDSR
   fp Compare (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                        fp fMaxSpeechWindow,
                        BOOL fPrevPhoneSilence, BOOL fNextPhoneSilence);
   BOOL Train (PSRFEATURE paSRFSpeech, fp *pafSRFSpeech, DWORD dwNum,
                        fp fMaxSpeechWindow,
                        WCHAR *pszBefore, WCHAR *pszAfter, PCMLexicon pLex);
#else
   fp Compare (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                        PWSTR pszBefore, PWSTR pszAfter, PCMLexicon pLex,
                        BOOL fForUnitSelection, BOOL fAllowFeatureDistortion, BOOL fForceCI, BOOL fFast,
                        BOOL fHalfExamples);
   BOOL Train (PSRANALBLOCK paSR, DWORD dwNum, fp fMaxSpeechWindow,
                            WCHAR *pszBefore, WCHAR *pszAfter, PCMLexicon pLex,
                            BOOL fCIOnly, fp fWeight);
#endif // OLDSR
   void QueryNeedTraining (DWORD *pdwNumStart, DWORD *pdwNumMiddle, DWORD *pdwNumEnd,
      PCMLexicon pLex);

   PCPhoneInstance PhonemeInstanceGet (BOOL fPrevPhoneSilence, BOOL fNextPhoneSilence, BOOL fExact = FALSE);
   PCPhoneInstance PhonemeInstanceGet (PWSTR pszLeft, PWSTR pszRight, PCMLexicon pLex);
   PCPhoneInstance PhonemeInstanceGetRough (void);

   void PrepForMultiThreaded (void);

   WCHAR             m_szName[16];         // phoneme

private:
   BOOL ToName (PWSTR psz, PWSTR pszLeft, PWSTR pszRight, PCMLexicon pLex);
   BOOL ToName (PWSTR psz, BOOL fSilenceLeft, BOOL fSilenceRight);
   BOOL ToNameRough (PWSTR psz);

   // variables
   BOOL              m_fHalfSize;         // if TRUE then half-sized phonemes
   CHashString       m_hPCPhoneInstance;  // hash of strings to phoneme instances
   // CPhoneInstance    m_aPI[3];         // what phone sounds like...
                                       // [0] = at start of utterance (after silence)
                                       // [1] = middle, [2] = end (silence follows)
};
typedef CPhoneme *PCPhoneme;

DEFINE_GUID(GUID_VoiceFile, 
0xf464d433, 0xcd44, 0xaf82, 0xae, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);
DEFINE_GUID(GUID_SynthParam, 
0xa164d433, 0x6344, 0xaf82, 0xae, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);



// To solve a problem when comparing silence... If the signal is so quiet
// that the human ear wouldn't be able to compare independent of volume (such
// as silence) then set a limit to how much can scale up to compare
#define MINENERGYLEVEL           25       // assume every SRFEATURE bin has this level
#define MINENERGYCOMPARE         sqrt((fp)(SRDATAPOINTS * 2 * MINENERGYLEVEL * MINENERGYLEVEL))
#define MINENERGYCOMPARESMALL    sqrt((fp)(SRDATAPOINTSSMALL * 2 * MINENERGYLEVEL * MINENERGYLEVEL))



BOOL SentenceToPhonemeString (PWSTR pszText, PCMLexicon pLex, PCTextParse pTextParse,
                              PCListFixed plPhone, PCMMLNode2 *ppNode = NULL);
BOOL SentenceToPhonemeString (PCMMLNode2 pNode, PCMLexicon pLex, PCTextParse pTextParse,
                              PCListFixed plPhone);
BOOL WaveToPhonemeString (PCWSTR pszWave, PCMLexicon pLex,
                          PCListFixed plPhone, PCMMLNode2 *ppNode = NULL);
fp SRFEATUREDistortCompare (BOOL fForUnitSelection,
                            PSRFEATURESMALL pSRFSpeech, fp fSRFSpeech,
                     PSRFEATURESMALL pSRFModel, fp fSRFModel);


/***********************************************************************************
WaveOpen */

// CWaveFileInfo - Info about specific wave file in directory
class CWaveFileInfo {
public:
   ESCNEWDELETE;

   CWaveFileInfo (void);
   ~CWaveFileInfo (void);
   BOOL SetText (PWSTR pszFile, PWSTR pszText, PWSTR pszSpeaker);
   BOOL FillFromFile (PWSTR pszDir, PWSTR pszFile, BOOL fFast = FALSE);
   BOOL FillFromFindFile (PWIN32_FIND_DATA pFF);
   BOOL PlayFile (PWSTR pszDir);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CWaveFileInfo *Clone (void);

   // can be read, but dont change
   PWSTR          m_pszFile;        // file name (without directory)
   PWSTR          m_pszText;        // text spoken, might be NULL
   PWSTR          m_pszSpeaker;     // speaker, might be NULL

   // can be changed
   FILETIME       m_FileTime;       // file time
   __int64        m_iFileSize;      // file size
   fp             m_fDuration;      // duration in seconds
   DWORD          m_dwSamplesPerSec;// sampling rate
   DWORD          m_dwChannels;     // number of channels

private:
   CMem           m_memInfo;        // memory to store info
};
typedef CWaveFileInfo *PCWaveFileInfo;



// CWaveDirInfo - Information about all the wave files in a directory
class CWaveDirInfo {
public:
   ESCNEWDELETE;

   CWaveDirInfo(void);
   ~CWaveDirInfo (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   PCWaveDirInfo Clone (void);

   BOOL SyncWithDirectory (PCProgressSocket pProgress, BOOL fFast = FALSE);
   BOOL SyncFiles (PCProgressSocket pProgress, PCWSTR pszDir);
   BOOL FillListBox (PCEscPage pPage, PCWSTR pszControl,
      PCWSTR pszTextFilter, PCWSTR pszSpeakerFilter, PCWSTR pszDir);
   BOOL AddFilesUI (HWND hWnd, BOOL fRequireText);
   BOOL RemoveFile (PCWSTR pszFile);
   BOOL PlayFile (PCWSTR pszDir, PCWSTR pszFile);
   BOOL EditFile (PCWSTR pszDir, PCWSTR pszFile, HWND hWnd);
   BOOL InventFileName (PCWSTR pszMasterFile, PCWSTR pszStartName,
                                   PWSTR pszFile, DWORD dwChars, PWSTR pszSpeaker, DWORD dwSpeakerChars);
   void ClearFiles (void);

   // should set directory into this
   WCHAR             m_szDir[256];     // directory. Form "c:\hello\bye"... not final "\"

   // can read but don't change
   CListFixed        m_lPCWaveFileInfo;   // list of Pwavefileinfo, sorted alphabetically
};
typedef CWaveDirInfo *PCWaveDirInfo;



/***************************************************************************
CWaveToDo */

class DLLEXPORT CWaveToDo {
public:
   ESCNEWDELETE;

   CWaveToDo (void);
   ~CWaveToDo (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CWaveToDo *Clone (void);

   BOOL FillListBox (PCEscPage pPage, PCWSTR pszControl, PCWSTR pszTextFilter);
   BOOL RemoveToDo (PCWSTR pszText);
   BOOL AddToDo (PCWSTR pszText);
   void ClearToDo (void);

   CListVariable        m_lToDo;    // list of PWSTR for to-do
};
typedef CWaveToDo *PCWaveToDo;

/************************************************************************************
RecBatch */
//DWORD RecBatch (PCEscWindow pWindow, PCWSTR pszVoiceFile, BOOL fRequireVoiceFile,
//                BOOL fUIToReview, PCWaveToDo pWaveToDo, PCWaveDirInfo pWaveDirInfo,
//                PCWSTR pszMasterFile, PCWSTR pszStartName);


/************************************************************************************
CTTSWork */
DEFINE_GUID(GUID_TTSWork, 
0xa693d433, 0x1344, 0xc682, 0x14, 0x8a, 0x12, 0xb2, 0x8f, 0xa8, 0xfb, 0xb8);


#define NUMTRIPHONEGROUP         3     // three possible values for dwTriPhoneGroup

WORD PhoneToTriPhoneNumber (BYTE bLeft, BYTE bRight, PCMLexicon pLex, DWORD dwTriPhoneGroup);
BOOL LexiconExists (PWSTR pszLexicon, PWSTR pszSrc);


/*************************************************************************************
CMTTS */

// for determining unit scores
#define RANKDBSCALE                    4.0        // rank (in dB) is scaled by this much before being converted to a byte
#define RANKDBOFFSET                   64          // so that fp of 0.0 converted to 64 in binary, so can have negative DB rank
#define BORDERDBSCALE                  (0.5)        // scale units on the border by this much
//#define HYPDURATIONPENALTYPERDOUBLE    6.0        // every doubling of duration that off from ideal, add 6 db penalty
//#define HYPENERGYPENALTYPERDOUBLE      (HYPDURATIONPENALTYPERDOUBLE/2.0)       // every doubling of energy results in 3 dB penalty
            // BUGFIX - was 3, but hardcoded to be based on duration/2
//#define HYPPITCHPENALTYPEROCTAVE       (HYPDURATIONPENALTYPERDOUBLE*6.0)       // every doubling of pitch over ideal results 6 db penalty
            // BUGFIX - Reduced the pitch penalty since new EnergyPerPitch() should minimize effects
            // BUGFIX - Went from 6 to 24 since pitch error reduces quality of voice
            // BUGFIX - Went from 24 to 36 to encourage using closest pitch
            // BUGFIX - Change to 3x the penality for doubling/halving the length
            // BUGFIX - Changed from *3 to *6 since pitch change really messes up Blizzard2007 voice
#define HYPPITCHPENALTYPEROCTAVEFORGIVE 0.0    // pitch can be off by this much in either direction without being a problem
            // BUGFIX - Went from /3 to /6
            // BUGFIX - Changed from /6 to /12 to make more sensative
            // BUGFIX - Changed from (HYPPITCHPENALTYPEROCTAVE/12.0) to 0.0

//#define HYPDURATIONPENALTYPLOSIVE(x)   ((x)*2.0)     // plosives have double the durational penalty
//#define HYPDURATIONPENALTYUNVOICED(x)  ((x)/2.0)     // unvoiced, non-plosived have half the durational penalty

// BUGFIX - Try to pick units of exactly the right length
#define HYPDURATIONLONGERLESS(x)        0     // can be longer by 50% without penalty
//#define HYPDURATIONLONGERLESS(x)        ((x)/2.0)     // can be longer by 50% without penalty

#define HYPPITCHPENALTYPEROCTAVEBUILD(x)  ((x)/1.0)     // less error when building
   // BUGFIX - Was /2, but think should really be /1 to be accurate

// SENTENCESYLLABLE - Information about then sentence syllable
typedef struct {
   char           acPitchF0Extra[SSPITCHPOINTS];   // from left to right, pitch points for a syllable.
                                                   // 0 = no change, 100 = 1 octave higher, -100 = 1 octave lower
   char           cPitchF0FujiDelta;   // change in pitch from the previous pitch-based syllable
   char           cDurSkew;   // duration skew. This is:
                              // log ((Duration of right half of syllable) / (Duration of left half)) / log(2) * 100.
                              // Half way point is determined by finding middle of theoretical syllable (from triphonepros)
                              // and mapping that to the syllable as spoken.
                              // Thuse, 100 => 2nd half takes twice as much time as first half, -100 is opposite
   char           cAccent;    // how much accent, from -100 to 100, representing -1.0 to 1.0

   BYTE           bPitchF0Fuji;     // pitch scale, 100 = 1.0, linear. Might be 0 if punctuation
   BYTE           bVol;       // volume calse, 100 = 1.0, linear

   BYTE           bDurPhone;  // duration scale relative to what the phonemes imply, 100 = 1.0, linear
   BYTE           bDurSyl;    // duration scale relative to the average syllable length

   BYTE           bPauseProb;  // Low 4 bits are pause probability, from 0 (none) to 15 (100%)
                              // IF this sentence is for WORDS (changed by ToWords()) then the high 4 bits
                              // are bits 2-6 of the number of syllables in the word

   BYTE           bFill;      // filler. not used

   WORD           wWord;     // word number, from the list of words trained, or -1 if not on list
   WORD           wPhoneGroupBits;  // bit-field with PIS_FROMPHONEGROUP(x)-1 set for each phoneme, since silence not includedd
} SENTENCESYLLABLE, *PSENTENCESYLLABLE;


// BESTSENTSWEEP - Result of best sweep
typedef struct {
   fp                      fScoreAvg;    // score that matched with, Average score, lower is better
   DWORD                   dwSylMatch; // number or syllables that matched
   int                     iOffsetTest;   // offset into pSent where the match occurs
   fp                      fRank;      // ranking, but tent-filtered average over time
   CSentenceSyllable       *pSent;     // sentence that matched
   PCListFixed             plfScoreSyl; // list of fScoreSyl for this sentence. Only filled in if it's a best match

   // calculated for individual sentneces
   fp                      fScoreSyl;     // score for the specific syllable that on
   fp                      afScoreSylWithRand[MAXRAYTHREAD];   // random number added later

   // calculated later
   fp                      fRankScore; // combined rank and score
   DWORD                   dwCompareRegion;  // results from dwCompareRegion
} BESTSENTSWEEP, *PBESTSENTSWEEP;


// PROSODYTREND - Store sum of information about trend
// NOTE: Its important that all these elements be integers
typedef struct {
   int               iCount;        // total count. This MUST be first
   int               aiPitchF0Extra[SSPITCHPOINTS];   // acPitchF0Extra[] sum, already in log scale
   int               iPitchF0Fuji;        // bPitch sum, in log scale
   int               iPitchF0FujiDelta;   // how much pitch changes, log scale
   int               iVol;          // bVol sum, in log scale, 50 = 2x
   int               iAccent;       // cAccent sum
   int               iDurPhone;     // bDurPhone sum, in log scale, 50 = 2x
   int               iDurSyl;       // bDurSyl sum, in log scale, 50 = 2x
   int               iDurSkew;      // cDurSkew sum, already in log scale
} PROSODYTREND, *PPROSODYTREND;

// PROSODYTRENDS - storing all the prosody trends for a specific stress and part-of-speech
typedef struct {
   PROSODYTREND      aCommon[4];    // trend based on how common
   PROSODYTREND      aPhonemes[4];  // trend based on number of phonemes
   PROSODYTREND      aSyllable[8];  // trend based on the syllable number
   PROSODYTREND      aParse[16];    // trend based on parse level
} PROSODYTRENDS, *PPROSODYTRENDS;

// SENTENCEINDEX - For storing indexed sentences
typedef struct {
   PCSentenceSyllable   pSS;        // sentence that it's in
   int                  iStart;        // where the indexed units start (relative to start of sentence)
} SENTENCEINDEX, *PSENTENCEINDEX;

// SENTENCEMATCH - Places where the sentences matched
typedef struct {
   PCTTSProsody         pTTSProsody;   // prosody model
   PCSentenceSyllable   pSS;           // sentence that it's in
   int                  iStart;        // where the indexed units start (relative to start of sentence)
   DWORD                dwNum;         // number of syllables that were compared for a match
   PCSentenceSyllable   pSSRunnerUp;   // 2nd best sentence, so can average in
   int                  iStartRunnerUp;   // start of 2nd best sentence, so can average in
   fp                   fScore;        // score, for how alike sentence match is compared to other matches.
                                       // See CTTSProsody.ScoreMatches()
} SENTENCEMATCH, *PSENTENCEMATCH;

// SENTSYLPHRASE - Indicates where a phrase begins and ends in the sentence
typedef struct {
   WORD                 wStart;        // starting syllable
   WORD                 wEnd;          // ending syllable, exclusive
} SENTSYLPHRASE, *PSENTSYLPHRASE;

// SSENUMWORDS - For EnumWords() call
typedef struct {
   DWORD                dwStart;       // start of the word, in syllables
   DWORD                dwLength;      // number of syllables
} SSENUMWORDS, *PSSENUMWORDS;

void PitchLowPass (fp *pafVal, DWORD dwNum, DWORD dwHalfSize, fp fDefault);

/* CSentenceSyllable - Keeps track of all the syllables in a sentence
*/
class CSentenceSyllable {
public:
   ESCNEWDELETE;

   CSentenceSyllable (void);
   ~CSentenceSyllable (void);

   DWORD MemoryTouch (void);
   void Clear (void);
   CSentenceSyllable *Clone (void);
   BOOL CloneTo (CSentenceSyllable *pTo);
   BOOL Add (DWORD dwWord, BYTE bPOSStress, BYTE bSylIndex, BYTE bRuleDepth, PSENTSYLEMPH pEmph, BYTE bStress,
      DWORD dwPhonemes, DWORD dwWordRank, DWORD dwPauseProb,
      DWORD dwPhonemeGroupBits);
   BOOL Remove (DWORD dwStart, DWORD dwEnd);
   void ReplaceUnknownPOSWithNoun (void);
   DWORD PhoneGroupBitsCalc (PBYTE pabPhone, DWORD dwNum, PCMLexicon pLex);
   BOOL MMLToBinary (PCMem pMem);
   size_t MMLFromBinary (PBYTE pabMem, size_t dwLeft);
   void RemapWords (PCMLexicon pLexOrig, PCMLexicon pLexNew, BOOL fAddIfNotExist,
      PCListFixed plPCSentenceSyllable);
   BOOL IsStartOfWord (DWORD dwIndex);
   void EnumWords (PCListFixed plSSENUMWORDS, PCListFixed plRemap);
   CSentenceSyllable *ToWords (void);
   CSentenceSyllable *FromWords (CSentenceSyllable *pOrig);
   BOOL SetPARSERULEDEPTH (PPARSERULEDEPTH pPRD, DWORD dwNum);
   void PARSERULEDEPTHToSENTSYLPHRASE (PPARSERULEDEPTH pPRD, DWORD dwNum, DWORD dwStart, DWORD dwEnd, PCListFixed plSENTSYLPHRASE);
   BOOL Append (PCSentenceSyllable pssAppend, DWORD dwStartIndex = (DWORD)-1, DWORD dwEndIndex = (DWORD)-1);

   fp CompareSingleSyllable (PCOMPARESYLINFO pInfo, int iOffsetThis, PCSentenceSyllable pTest,
                                              int iOffsetTest, WORD wPeriod, BOOL fIgnorePOS, BOOL fIgnoreProsody,
                                              BOOL fWord);
   fp CSentenceSyllable::CompareSingleSyllableQuick (int iOffsetThis, PCSentenceSyllable pTest, int iOffsetTest);

   BOOL CompareSequence (PCOMPARESYLINFO pInfo, int iOffsetThis, CSentenceSyllable * pTest,
                                 int iOffsetTest, WORD wPeriod, BOOL fIgnoreProsody, BOOL fWord, PBESTSENTSWEEP pSweep);
   BOOL CompareSequenceQuick (int iOffsetThis, PCSentenceSyllable pTest,
                                 int iOffsetTest, DWORD dwNum);
   fp* CompareWindowGenerate (int iStart, int iEnd, BOOL fTriangle, PCMem pMem);
   fp CompareSequenceWindowed (PCOMPARESYLINFO pInfo, int iOffsetThis, PCSentenceSyllable pTest,
                                 int iOffsetTest, DWORD dwNum, WORD wPeriod, BOOL fIgnoreProsody, BOOL fWord, fp *pafWindow);
   void CompareSweepSequence (PCOMPARESYLINFO pInfo, int iOffsetThis, PCSentenceSyllable pTest,
                                      WORD wPeriod, BOOL fIgnoreProsody, PCListFixed plBESTSENTSWEEP, DWORD dwSlots,
                                      PCListFixed plBESTSENTSWEEPExclude);
   fp CompareRegion (PCOMPARESYLINFO pInfo, int iOffsetThis, CSentenceSyllable *pTest, int iOffsetTest, BOOL fWord, WORD wPeriod);
   BOOL AddToIndex (PCMLexicon pLexTTS, PCListFixed *ppaIndex);
#if 0 // old compare code
   DWORD Compare (DWORD dwOffsetThis, CSentenceSyllable *pTest, DWORD dwOffsetTest);
   void CompareSweep (DWORD dwOffsetThis, CSentenceSyllable *pTest,
                                      DWORD *pdwNum, DWORD dwMax, PBESTSENTSWEEP pBESTSENTSWEEP);
   void CompareSweep (DWORD dwOffsetThis, PCListFixed plPCSentenceSyllable,
                                      DWORD *pdwNum, DWORD dwMax, PBESTSENTSWEEP pBESTSENTSWEEP);
   void FindBestMatch (PCListFixed plPCSentenceSyllable);
#endif // 0

   // can read but dont modify
   DWORD             m_dwNum;       // number of syllables in the sentences
   BYTE              *m_pabPOSStress;     // part of speech of each of the syllables, in the low
                                    // nibble. The high 4 bits are set based on the syllable stress.
   BYTE              *m_pabSylIndex;  // low 3 bits are syllable number within the word
   BYTE              *m_pabRuleDepth;  // lower 4 bits stores rule depth info (two 2-bit depths).
                                    // Next 2 bits are number of phonemes in the syllable
                                    // Highest 2 bits are commonality of word
   CListFixed        m_lSENTSYLPHRASE; // list of SENTSYLPHRASE to indicate where the phrase boundaries are.
                                    // only kept if 2 or more words in the phrase
   PSENTENCESYLLABLE m_paSyl;       // array of m_dwNum syllable info

#ifdef _DEBUG
   DWORD             m_dwSentIndex; // original sentence index
#endif

private:
   CListFixed        m_lPOSStress;  // list of parts of speech
   CListFixed        m_lSylIndex;   // syllable index bytes
   CListFixed        m_lRuleDepth;  // lower 4 bits occupty the rule depth. Next two bits are # phonemes - 1. Upper 2 unused
   CListFixed        m_lSENTENCESYLLABLE; // list of sentence syllables
};

typedef CSentenceSyllable *PCSentenceSyllable;


/*************************************************************************************
CSentenceMatch - Stores a list of SENTENCEMATCH structures
*/

class CSentenceMatch {

public:
   ESCNEWDELETE;

   CSentenceMatch (void);
   ~CSentenceMatch (void);

   BOOL FindAndScoreMatches (PCMLexicon pLexTTS, PCOMPARESYLINFO pInfo, PCSentenceSyllable pSS, CTTSProsody **ppCTTSProsody, DWORD dwNum,
      int iTTSQuality, BOOL fWord, WORD wPeriod);

   // user-settable, used by FindAndScoreMatched()
   int               m_iStart;               // start syllable in m_pSS.
   int               m_iEnd;                 // end syllable in m_pSS
   BOOL              m_fTriangle;            // if TRUE then use triangle window comparison, FALSE then square
   BOOL              m_fIgnoreNotEnough;     // if TRUE, then succede even if not enough
   BOOL              m_fIgnoreProsody;       // if TRUE then ignore prosody (F0, duration, energy) in comparison. Defaults to FALSE
   BOOL              m_fSuccess;             // set to true by FindAndScoreMatches() if success filling in m_lSENTENCEMATCH
   BOOL              m_fCompareAgainstTarget;   // if TRUE, will compare the matches against the original target, and add that to the score
   CListFixed        m_lSENTENCEMATCH;       // list of sentence matches

   // settings
   int               m_iStartUse;            // actual syllable index into sentence that will use in the end
   int               m_iEndUse;              // actual end syllable

private:
};


typedef CSentenceMatch *PCSentenceMatch;




/*************************************************************************************
CTTSProsodyMatchHyp - Hypthesis for TTS prosody using CSentenceMatch.
*/
class CTTSProsodyMatchHyp {
public:
   ESCNEWDELETE;

   CTTSProsodyMatchHyp (void);
   ~CTTSProsodyMatchHyp (void);

   BOOL CloneTo (CTTSProsodyMatchHyp *pTo);
   CTTSProsodyMatchHyp *Clone (void);
   BOOL IncludeJoinCosts (PCOMPARESYLINFO pInfo, DWORD dwNextResynthIndex, PCListFixed plResynth,
                                            PCListFixed plJoin, DWORD dwMaxJoin, BOOL fWord, WORD wPeriod);
   BOOL Expand (PCOMPARESYLINFO pInfo, DWORD dwNextResynthIndex, PCListFixed plResynth,
                                  WORD wPeriod, PCListFixed plPCTTSProsodyMatchHyp);
   int Compare (CTTSProsodyMatchHyp *pWith, BOOL fIncludeScore);

   CSentenceSyllable    m_SS;       // sentence, as it stands
   DWORD                m_dwNextJoinIndex; // index into the list of PCSentenceMatch joins that is the next join to do
   fp                   m_fScore;   // score. Lower is better

   // stored away so can compare
   int                  m_iResynthNextSyl;   // next syllable that will resynthesize into
   int                  m_iJoinNextSyl;      // next syllable that will calculate join for

private:
};
typedef CTTSProsodyMatchHyp *PCTTSProsodyMatchHyp;



// TTSPWORDSYLHEADER - Header information for storing the typical syllable
// values for a specific word.
typedef struct {
   // DWORD             dwWordID;         // word number (or part-of-speech number, 4-bits)
   DWORD             dwSyllables;      // number of syllables
   DWORD             dwStressBitsMulti;   // (dwStressBitsMulti%pLex->Stresses()) => stress at syllable 0,
                                          // ((dwStressBitsMulti / pLex->Stresses())%pLex->Stresses()) => stress at syllable 1,
                                          // ((dwStressBitsMulti / (pLex->Stresses() * pLex->Stresses()) )%pLex->Stresses()) => stress at syllable 2,
                                          // etc.
                                          // if only one syllable then always 1
                                          // OLD: bit[0] => stressed at syllable 0, bit[1] => stressed at syllable 1, etc.
   DWORD             dwTrainCount;        // number of items this was trained on
   PCSentenceSyllable   pss;              // list of example versions of the word. Each word entry
                                          // includes single syllable to left, N syllables for the word, and single
                                          // syllable to the right
   // followed by an array of dwSyllables number of PROSODYTREND
} TTSPWORDSYLHEADER, *PTTSPWORDSYLHEADER;

// TTSPWORDSYLHEADERFILE - To be written to file
typedef struct {
   DWORD             dwSyllables;
   DWORD             dwStressBitsMulti;
   DWORD             dwTrainCount;        // number of items this was trained on
   DWORD             dwSSSize;            // size of sent syllable
} TTSPWORDSYLHEADERFILE, *PTTSPWORDSYLHEADERFILE;

#define TYPICALSYLINFO_STATEMENT       0  // used for a statement
#define TYPICALSYLINFO_QUESTION        1  // used for a question
#define TYPICALSYLINFO_EXCLAMATION     2  // used for an exclamation
#define TYPICALSYLINFO_NUM             3  // number of entries

/* CTTSProsody - Store prosody info in this*/
class DLLEXPORT CTTSProsody:  public CEscMultiThreaded {
public:
   ESCNEWDELETE;

   CTTSProsody (void);
   ~CTTSProsody (void);
   //void LexiconSet (PCMLexicon pLexTTS);
   //PCMLexicon LexiconGet (void);
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode, PWSTR pszSrcFile);
   BOOL CloneTo (CTTSProsody *pTo);
   CTTSProsody *Clone (void);
   void Clear (void);
   BOOL Save (PWSTR pszFile);
   BOOL Open (PWSTR pszFile);
   DWORD MemoryTouch (void);

   BOOL SentenceAdd (PCMLexicon pLexTTS, PCSentenceSyllable pss, PCMLexicon pLex);
   void FindBestMatch (fp fFujiRangeMax, fp fFujiPhraseRangeMax, fp fFujiPhraseRangeMin,
                           PCMLexicon pLexTTS, PCOMPARESYLINFO pInfo, PCSentenceSyllable pThis, PCMLexicon pLex,
                                 CTTSProsody **ppCTTSProsody, DWORD dwNum, int iTTSQuality, fp fAccuracy, DWORD dwRandomness,
                                 DWORD dwMultiPass);
   BOOL BeamSearchMatches (PCMLexicon pLexTTS, PCOMPARESYLINFO pInfo, PCSentenceSyllable pThis, PCMLexicon pLex,
                                 CTTSProsody **ppCTTSProsody, DWORD dwNum, int iTTSQuality, BOOL fWord, fp fAccuracy, DWORD dwRandomness,
                                 DWORD dwMultiPass);
   BOOL Merge (PCMLexicon pLexTTS, PCTTSProsody pOther);
   DWORD *PhonemePauseGet (DWORD dwPhoneLeft, DWORD dwPhoneRight, BOOL fCreateIfNotExist, PCMLexicon pLex);
   BOOL TypicalSylInfoTrain (DWORD dwSentenceType, PTYPICALSYLINFO pInfo, DWORD dwIndex, DWORD dwSentenceLength);
   void TypicalSylInfoGet (DWORD dwSentenceType, DWORD dwIndex, DWORD dwSentenceLength, PTYPICALSYLINFO pInfo,
                                     BOOL fAlsoSurround = TRUE);
   void TypicalSylInfoGet (DWORD dwSentenceType, DWORD dwIndex, DWORD dwSentenceLength,
                                     CTTSProsody **ppCTTSProsody, DWORD dwNum, PTYPICALSYLINFO pInfo,
                                     BOOL fAlsoSurround = TRUE);

   void ProsodyNGramInfoTrain (PTYPICALSYLINFO pInfo, PBYTE pabPOS, DWORD dwNum, DWORD dwIndex);
   void ProsodyNGramInfoGet (PBYTE pabPOS, DWORD dwNum, DWORD dwIndex, PTYPICALSYLINFO pInfo);
   void ProsodyNGramInfoGet (PBYTE pabPOS, DWORD dwNumPOS, DWORD dwIndex,
                                     CTTSProsody **ppCTTSProsody, DWORD dwNum, PTYPICALSYLINFO pInfo);

   PTTSPWORDSYLHEADER WordSylGet (PCMLexicon pLexTTS, PCWSTR pszWord, DWORD dwPOS, DWORD dwSyllables,
                         DWORD dwStressBitsMulti, BOOL fCreateIfNotExist);
   BOOL WordSylTrain (PCMLexicon pLexTTS, DWORD dwSyllables, PTYPICALSYLINFO pass, PCWSTR pszWord,
                                DWORD dwPOS, DWORD dwStressBitsMulti,
                                DWORD dwSyllablesLeft, PTYPICALSYLINFO passLeft, DWORD dwPOSLeft,
                                DWORD dwSyllablesRight, PTYPICALSYLINFO passRight, DWORD dwPOSRight
                                );
   void TYPICALSYLINFOAverage (DWORD dwSyllables, PTYPICALSYLINFO pass,
                                         PTYPICALSYLINFO passAvg);
   void TYPICALSYLINFOToSENTSYLEMPH (PTYPICALSYLINFO pTSI, PSENTSYLEMPH pEmph);
   //BOOL WordSylGetSENTENCESYLLABLE (PCWSTR pszWord, DWORD dwPOS, DWORD dwSyllables,
   //                                           DWORD dwStressBits, CTTSProsody **ppCTTSProsody,
   //                                           DWORD dwNum, PSENTENCESYLLABLE pass);
   BOOL WordSylGetTYPICALSYLINFO (PCMLexicon pLexTTS, PCWSTR pszWord, DWORD dwPOS, DWORD dwSyllables,
                                              DWORD dwStressBitsMulti, CTTSProsody **ppCTTSProsody,
                                              DWORD dwNum, PTYPICALSYLINFO pass);
   BOOL WordSylGetBestExample (PCOMPARESYLINFO pInfo, PCMLexicon pLexTTS, PCWSTR pszWord, DWORD dwPOS, DWORD dwSyllables,
                                              DWORD dwStressBitsMulti, CTTSProsody **ppCTTSProsody,
                                              DWORD dwNum,
                                              PSENTSYLEMPH pssLeft, PSENTSYLEMPH pssThis, PSENTSYLEMPH pssRight,
                                              PSENTSYLEMPH passResult);
   BOOL FindAllMatches (PCMLexicon pLexTTS, PCSentenceSyllable pSS, int iStart, int iEnd, BOOL fMustMatchPhrase, BOOL fWord,
                                  CTTSProsody **ppCTTSProsody, DWORD dwNum, PCListFixed plSENTENCEMATCH);
   BOOL ScoreMatches (PCOMPARESYLINFO pInfo, PCListFixed plSENTENCEMATCH, int iTTSQuality, WORD wPeriod,
      BOOL fIgnoreNotEnough, BOOL fIgnoreProsody, BOOL fWord, BOOL fTriangle,
      PCSentenceSyllable pSSCompare, int iStartCompare);
   BOOL GenerateMatches (PCMLexicon pLexTTS, PCOMPARESYLINFO pInfo, PCSentenceSyllable pSS, int iTTSQuality, BOOL fWord,
                                   CTTSProsody **ppCTTSProsody, DWORD dwNum, DWORD dwMultiPass, PCListFixed plResynth, PCListFixed plJoin);
   DWORD MaxJoinDistance (int iTTSQuality);
   void MaxMinMatches (int iTTSQuality, DWORD *pdwMax, DWORD *pdwMin);

   // from EscMultiThreaded
   virtual void EscMultiThreadedCallback (PVOID pParams, DWORD dwParamSize, DWORD dwThread);

private:
   void Randomize (void);
   void CompareSweepSequence (PCMLexicon pLexTTS, PCOMPARESYLINFO pInfo, PCSentenceSyllable pThis, int iOffsetThis, PCMLexicon pLex,
                                      PCListFixed plBESTSENTSWEEP, DWORD dwSlots,
                                      PCListFixed plBESTSENTSWEEPExclude);
   void SubdivideSentenceByPhrases (PCSentenceSyllable pSS, DWORD dwStartSyl, DWORD dwEndSyl, DWORD dwWindow,
                                              BOOL fCanIncludeAll, PCListFixed plPCSentenceMatch);
   void SubdivideAddAsSingle (PCSentenceSyllable pSS, DWORD dwStartSyl, DWORD dwEndSyl,
                              DWORD dwWindow, PCListFixed plPCSentenceMatch);
   void CalcNGramIfNecessary (PCMLexicon pLexTTS);
   DWORD *PauseNGram (PCMLexicon pLexTTS, BYTE bWord1, BYTE bWord2, BYTE bWord3, BYTE bWord4);
   fp PauseNGram (PCMLexicon pLexTTS, BYTE bWord1, BYTE bWord2, BYTE bWord3, BYTE bWord4,
                            CTTSProsody **ppCTTSProsody, DWORD dwNum);
   PPROSODYTRENDS ProsodyTrends (PCMLexicon pLexTTS, DWORD dwStress, DWORD dwPOS);
   BOOL ProsodyTrend (PCMLexicon pLexTTS,  DWORD dwStress, DWORD dwPOS, DWORD dwCommon, DWORD dwPhonemes,
                                 DWORD dwSyllable, DWORD dwParseLevel, PPROSODYTREND ptCommon,
                                 PPROSODYTREND ptPhonemes, PPROSODYTREND ptSyllable,
                                 PPROSODYTREND ptParseLevel, PPROSODYTREND ptAll);
   BOOL ProsodyTrend (PCMLexicon pLexTTS, DWORD dwStress, DWORD dwPOS, DWORD dwCommon, DWORD dwPhonemes,
                                 DWORD dwSyllable, DWORD dwParseLevel, PPROSODYTREND ptCommon,
                                 PPROSODYTREND ptPhonemes, PPROSODYTREND ptSyllable,
                                 PPROSODYTREND ptParseLevel, PPROSODYTREND ptAll,
                                 CTTSProsody **ppCTTSProsody, DWORD dwNum);
   BOOL ProsodyTrend (PCMLexicon pLexTTS, BYTE bPOSStressModel, BYTE bSylIndexModel, BYTE bRuleDepthModel,
                                   BYTE bPOSStressWant, BYTE bSylIndexWant, BYTE bRuleDepthWant,
                                PPROSODYTREND ptMods,
                                CTTSProsody **ppCTTSProsody, DWORD dwNum);
   PCListFixed BeamSearch (PCOMPARESYLINFO pInfo, DWORD dwThread, PCListFixed plSyl, fp *pfScore, fp fAccuracy, DWORD dwRandomness);
   fp BeamSearchSwitchSentencePenalty (PCOMPARESYLINFO pInfo, PCSentenceSyllable pssCur, int iOffsetCur,
                                                  PCSentenceSyllable pssTo, int iOffsetTo, WORD wPeriod);
   PCSentenceSyllable BeamSearchHypToSentenceSyllable (PCListFixed plBESTSENTSWEEP);
   void SentenceSyllablesCombineWithProsody (PCOMPARESYLINFO pInfo,
      fp fFujiRangeMax, fp fFujiPhraseRangeMax, fp fFujiPhraseRangeMin,
      PCMLexicon pLexTTS, PCSentenceSyllable pThis,
                                                       PCSentenceSyllable *papSS, DWORD dwNumSent,
                                                       PCMLexicon pLex,
                                 CTTSProsody **ppCTTSProsody, DWORD dwNum);

   void TypicalSylInfoGetInternal (DWORD dwSentenceType, DWORD dwBin, double fAlpha, PTYPICALSYLINFO pInfo);

   DWORD ProsodyNGramInfoCount (PCMLexicon pLexTTS);
   PPROSODYNGRAMINFO ProsodyNGramInfoGetInternal (PCMLexicon pLexTTS, PBYTE pabPOS, DWORD dwNum, DWORD dwIndex,
      BOOL fCreateIfNotExist);
   PTTSPWORDSYLHEADER WordSylGetInternal (PCWSTR psz, DWORD dwSyllables,
                         DWORD dwStressBitsMulti, BOOL fCreateIfNotExist);
   void CreateIndexIfNecessary (PCMLexicon pLexTTS, BOOL fWord);
   void CreateWordSentencesIfNeccssary (void);

   //PCMLexicon           m_pLexTTS;     // lexicon that TTS uses. Can get the list of phonemes from it

   PCMLexicon           m_pLexInProsody;         // lexicon of words mentioned in prosody
   CListFixed           m_lPCSentenceSyllable;  // list of sentence syllables
   CListFixed           m_pPCSSRandom; // randomized list of sentence syllables
   CMem                 m_memPhonemePause;   // store pauses between phonemes
                                       // list of dwNumPhones (left) x dwNumPhones(right) x 2 DWORDs
                                       // [0] = count total, [1] = number that result in pauses
                                       // m_dwCurPosn = total bytes.
   CMem                 m_amemTYPICALSYLINFO[TYPICALSYLINFO_NUM]; // array of SentenceLengthTotal() TYPICALSYLINFO structures
                                       // m_dwCurPosn = total bytes. [x] = TYPICALSYLINFO_XXX
   CMem                 m_memPROSODYNGRAMINFO;  // array of ProsodyNGramInfoCount() PROSODYNGRAMINFO structures for ngram
                                       // m_dwCurPosn = total bytes

   CListFixed           m_lPCHashStringWordSyl;  // list of hash's. Has [n] is for words with n+1 syllables. Each
                                       // has stores a list of TTSPWORDSYLHEADER (plus enough info for each syllable)
                                       // string is wordname (or "@" POS num) ":" stress bits

   CRITICAL_SECTION     m_csProsody;   // to keep multihreaded prosody fast

   // automatically calculated
   CListFixed           m_lPCSentenceSyllableWord; // list of sentence syllabes, but converted to words
   CMem                 m_memPauseNGram; // what the pause is given pos[-2] x pos[-1] x pos [1] x pos[2] x 2 DWORDs
                                       // [0] = count total, [1]= number that result in pauses
                                       // m_dwCurPosn = total bytes
   CMem                 m_memPROSODYTRENDS;  // array of pLex->Stresses() (unstressed & stress) x POS_MAJOR_NUMPLUSONE PROSODYTRENDS structures

   BOOL                 m_fSENTENCEINDEXValid;  // set to TRUE when m_alSENTENCEINDEX is valid
   BOOL                 m_fSENTENCEINDEXValidWord;  // set to TRUE when m_alSENTENCEINDEXWord is valid
   PCListFixed          m_aplSENTENCEINDEX[POS_MAJOR_NUMPLUSONE][MAXSTRESSES][POS_MAJOR_NUMPLUSONE][MAXSTRESSES]; // list of sentence indecies
                                       // so can quickly find sentences that begin/end with right units
   PCListFixed          m_aplSENTENCEINDEXWord[POS_MAJOR_NUMPLUSONE][MAXSTRESSES][POS_MAJOR_NUMPLUSONE][MAXSTRESSES]; // list of sentence indecies
};
typedef CTTSProsody *PCTTSProsody;


// CTTSPunctPros - Small class that stores punctuation prosody
#define PUNCTPROS    2        // prosody within 2 surrounding words
class DLLEXPORT CTTSPunctPros {
public:
   ESCNEWDELETE;

   CTTSPunctPros (void);
   ~CTTSPunctPros (void);

   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CTTSPunctPros *Clone (void);

   // varialbes
   CPoint      m_apBefore[PUNCTPROS]; // how prosody affects immediately before[0] and 2 words before[1], etc.
                                    // .p[0] = pitch, .p[1] = volume, .p[2] = duration
   CPoint      m_apAfter[PUNCTPROS];  // how affects after
   WCHAR       m_wPunct;              // punctuation character, if 0 then look at wFuncWord
   WCHAR       m_wFuncWord;           // function word index, 0 = most common func word, etc.
};



#define TPMML_TIMECOMPRESSA         0x01     // time compression, bit A
#define TPMML_FREQCOMPRESSA         0x02     // do frequency compression on the triphone
#define TPMML_TIMECOMPRESSB         0x04     // time compression, bit B
#define TPMML_FREQCOMPRESSB         0x08     // do frequency compression on the triphone
#define TPMML_PCMCOMPRESSA          0x10     // PCM compression bit A
#define TPMML_PCMCOMPRESSB          0x20     // PCM compression bit B
#define TPMML_TIMECOMPRESSMASK      (TPMML_TIMECOMPRESSA | TPMML_TIMECOMPRESSB)     // mask for time compression
#define TPMML_FREQCOMPRESSMASK      (TPMML_FREQCOMPRESSA | TPMML_FREQCOMPRESSB)     // mask for time compression
#define TPMML_PCMCOMPRESSMASK       (TPMML_PCMCOMPRESSA | TPMML_PCMCOMPRESSB)       // mask for PCM compression
#define TPMML_TIMECOMPRESS_0        0        // no time compression
#define TPMML_TIMECOMPRESS_1        TPMML_TIMECOMPRESSA     // minimal
#define TPMML_TIMECOMPRESS_2        TPMML_TIMECOMPRESSB     // 2nd best
#define TPMML_TIMECOMPRESS_3        (TPMML_TIMECOMPRESSA | TPMML_TIMECOMPRESSB)     // maximum time compress
#define TPMML_FREQCOMPRESS_0        0        // no freq compression
#define TPMML_FREQCOMPRESS_1        TPMML_FREQCOMPRESSA     // minimal
#define TPMML_FREQCOMPRESS_2        TPMML_FREQCOMPRESSB     // 2nd best
// #define TPMML_FREQCOMPRESS_3        (TPMML_FREQCOMPRESSA | TPMML_FREQCOMPRESSB)     // No compression, store original wave
#define TPMML_PCMCOMPRESS_0         0        // no PCM at all
#define TPMML_PCMCOMPRESS_1         TPMML_PCMCOMPRESSA   // ADPCM
#define TPMML_PCMCOMPRESS_2         TPMML_PCMCOMPRESSB     // Full PCM

#define NUMPHONECONTIGOUS           8        // how large for the structure

#define NUMJOINQUALITY              4  // number of different join-cost calculation qualities
#define QUICKJOINTESTQUALITYDELTA   1  // quick-join test quality is this many below

#define TPAPITCHPOINTS              5  // keep 5 pitch points for triphone audio for matches


// CMTTSTriPhoneAudio - Where audio for a unit is stored
class CMTTSTriPhoneAudio {
public:
   ESCNEWDELETE;

   CMTTSTriPhoneAudio (void);
   ~CMTTSTriPhoneAudio (void);

   CMTTSTriPhoneAudio *Clone (void);
   size_t MMLToBinary (PCMem pmem);
   BOOL MMLFromBinary (PVOID pMem, DWORD dwSize, PCMLexicon pLexicon);

   DWORD MemoryTouch (void);

//   BOOL Compress (void);
//   BOOL Decompress (void);

   PSRFEATURE SRFEATUREGetRange (PCMTTS pTTS, PCMem pMem, DWORD dwFeatureStart, DWORD dwFeatureEnd, DWORD *pdwNumFeat,
                                             BOOL *pfToLeft, float *pfPitch, PTTSFEATURECOMPEXTRA *ppaTFCA);
   //PSRFEATURE SRFEATUREGet (PCMTTS pTTS, PCMem pMem, DWORD dwSkip, DWORD dwDemiPhone, DWORD *pdwNumFeat,
   //   BOOL *pfToLeft, float *pfPitch, PTTSFEATURECOMPEXTRA *ppaTFCA);
   BOOL SRFEATURESet (PCMTTS pTTS, DWORD dwSentenceNum, DWORD dwFeatureStart, DWORD dwFeatureEnd,
      fp fMaxEnergyForWave, fp fMaxEnergyForWaveMod);
   void CalcInfo (PCMLexicon pLexicon);   // calculated the calculated variables

   void ConnectErrorCacheFree (void);
   BOOL ConnectErrorCacheSet (DWORD dwJoinQuality, PCMTTSTriPhoneAudio *papPTA, fp *pafValue, DWORD dwNum, DWORD dwSubPhone,
      DWORD dwMaxCache);
   BOOL ConnectErrorCacheGet (DWORD dwJoinQuality, PCMTTSTriPhoneAudio pPTA, DWORD dwSubPhone, fp *pfValue);

   WORD              m_wOrigWave;   // original wave number. Used for debugging
   WORD              m_wOrigPhone;  // phome index into the original wave
   BOOL              m_fReviewed;   // set to TRUE if triphone has been reviewed already

   BOOL              m_fCached;     // if TRUE, this object is part of an array allocated by new and shouldnt be deleted individually

   // saved
   DWORD             m_dwFeatureStart; // starting feature
   DWORD             m_dwFeatureEnd;   // ending feature
   int               m_iFeatureAdd;    // add this to the SRFEATUREs to generate the normalized energy
   DWORD             m_dwWord;      // word number this is specific to, or -1 if word independent
   DWORD             m_dwFuncWordGroup; // whether unit from a function word or not, 0..NUMFUNCWORDGROUP(inclusive)
   float             m_fOrigPitchF0Extra;  // original pitch of the unit
   float             m_fOrigPitchF0Fuji;  // original pitch of the unit
   float             m_fPitchDelta; // ratio of pitch to right / pitch to left
   float             m_fPitchBulge; // ratio of bulge in center. 2 = octave above, 1/2 = octave below
   float             m_afPitchF0Extra[TPAPITCHPOINTS];   // original pitch over phoneme
   float             m_afPitchF0Fuji[TPAPITCHPOINTS];    // original pitch over phoneme
   float             m_afPitchConfidence[TTSDEMIPHONES];   // how confident about the pitch, from 0.0 to 1.0
   float             m_fAccent;     // accent level, from -1 to 1
   float             m_fEnergyAvg;  // average enery for the unit, after some adjustment so that max energy in wave is PHONESAMPLENORMALIZED
   float             m_fCenterEnergy;  // energy for this one, as calculated by the calling fn
   float             m_fLeftEnergy; // left energy, for continuity. 0 if silence
   float             m_fLeftPitchF0Extra;  // left pitch, for continuity. 0 if silence
   float             m_fLeftPitchF0Fuji;  // left pitch, for continuity. 0 if silence
   DWORD             m_dwLeftDuration; // left duration, in SRFEATUREs.
   //WORD              m_wDuration;   // typical duration, in SRFEATURE units
   //short             m_iPitch;      // pitch increase/decraase. 0=no change, 1000=1 octave higher, -1000=1 octave lower, etc.
   //short             m_iPitchDelta; // chance in pitch over the phoneme, same scale as m_iPitch
   WORD              m_wWordSylPos;    // position within word, 0=middle, 1=start, 2=end. WORDPOS_XXX flags.
   BYTE              m_abPhoneContiguous[NUMPHONECONTIGOUS]; // array of 8 phonemes that immediately follow this one. Phone numbers
                                    // are +1 from unsort-phone number. phoneme 0 is blank (or end of list)
   BYTE              m_bPhoneLeft;  // phoneme to the left
   BYTE              m_bPhoneRight; // phoneme to the right
   BYTE              m_abRank[TTSDEMIPHONES];       // score, from 0..100,where 0 is the best, per demiphone
   // WORD              m_wFlags;      // TPMML_XXX
   MISMATCHINFO      m_aMMISpecific[TRIPHONESPECIFICMISMATCH];  // array of scores to used for probable mismatched phonemes
   DWORD             m_dwMismatchAccuracy;   // what units are used in m_aMMI. 0 for phone groups, 1 for unstressed phone, 2 for phonemes
//   DWORD             m_dwNumSRFEATURE; // number of SRFEATUREs in m_memSRFEATURE
//   PCMem             m_pmemSRFEATURE;   // memory containing all the SR features... normalized so max energy = 15000
//   PCMem             m_pmemTTSFEATURECOMPEXTRA; // extra info per SR feature (m_dwNumSRFEATURE)
   DWORD             m_dwTrimLeft;  // number of SRFEATURE units can trim from left if all alone, to improve connections
   DWORD             m_dwTrimRight;  // number of SRFEATURE units can trim from right if all alone, to improve connections
   // SRFEATURESMALL    m_aSRFeatBoundary[(TTSDEMIPHONES+1)*2];   // SR features on the boundary. [0] = SR feat immediately before phone begins,
                                    // [1] = 1st SR feat, [2] = last SR feat, [3] = SR feat immediately after phoneme ends
                                    // these are all normalized to PHONESAMPLENORMALIZED
   // float             m_afSRFeatBoundary[(TTSDEMIPHONES+1)*2];   // energy before normalization of each of the boundaries

   // calculated
//   BOOL              m_fEnergyIsValid; // set to TRUE if the energy m_fEnergyStart and m_fEnergyEnd are valid.
//   fp                m_fEnergyStart;   // energy of 1st feature. Call CalcEnergyIfNecesssary() before using
//   fp                m_fEnergyEnd;     // energy of last feature. Call CalcEnergyIfNecessary() before using
   WORD              m_awTriPhone[NUMTRIPHONEGROUP]; // different levels of backoff

   int               m_aiPitchF0Extra[TPAPITCHPOINTS];   // pitch over phoneme, 1000 units = 1 octave
   int               m_aiPitchF0Fuji[TPAPITCHPOINTS];    // pitch over phoneme, 1000 units = 1 octave

   // to speed up TPACONNECTIONERROR
   DWORD             m_dwUniqueID;     // unique ID assocaited with phoneme audio
   //WORD              m_wPhone;        // whatever phoneme this is
   //WORD              m_wTriPhoneIndex; // index

private:
//   void CalcEnergy (void);
//   void CalcEnergyIfNecessary (void);

//   CMem              m_memCompressed;  // compressed version of m_memSRFeature, NOT used if m_fKeepMemory
//   BOOL              m_fKeepMemory;    // if keep memory that passed in
//   PVOID             m_pKeepMemoryCompressed;   // memory where stored
//   size_t            m_dwKeepMemorySize;  // size of memory stored

   CListFixed        m_alTPACONNECTIONERROR[NUMJOINQUALITY][TTSDEMIPHONES]; // list of TPACONNECTIONERROR structures for cache
};

// CMTTSTriPhonePros - Where prosody information for a triphone is stored
class CMTTSTriPhonePros {
public:
   ESCNEWDELETE;

   CMTTSTriPhonePros (void);
   ~CMTTSTriPhonePros (void);

   CMTTSTriPhonePros *Clone (void);
   DWORD MMLToBinary (PCMem pmem);
   BOOL MMLFromBinary (PVOID pMem, DWORD dwSize, PCMLexicon pLexicon);

   DWORD MemoryTouch (void);

   void CalcInfo (PCMLexicon pLexicon);   // calculated the calculated variables

   BOOL              m_fCached;     // if TRUE, this object is part of an array allocated by new and shouldnt be deleted individually

   // saved
   WORD              m_wDuration;   // typical duration, in SRFEATURE units
   short             m_iPitchF0Extra;      // pitch increase/decraase. 0=no change, 1000=1 octave higher, -1000=1 octave lower, etc., above fuji
   short             m_iPitchF0Fuji;   // pitch increase/decrease, 0 = no change, 1000=1 octave higher, -1000=1 octave lower, etc.
   short             m_iPitchDelta; // chance in pitch over the phoneme, same scale as m_iPitch
   short             m_iPitchBulge; // bulge in pitch in center of phoneme, scame scale as m_iPitch
   WORD              m_wWordSylPos;    // position within word, 0=middle, 1=start, 2=end. WORDPOS_XXX
   BYTE              m_bPhoneLeft;  // phoneme to the left
   BYTE              m_bPhoneRight; // phoneme to the right
   float             m_fEnergyAvg;  // average energy for the triphone (all CMTTSTriPhoneAudio
                                    // have their volume normalized to this

   // calculated
   WORD              m_awTriPhone[NUMTRIPHONEGROUP]; // different levels of backoff
};



/* CTTSWaveSegment - For storing a segment ot TTS audio */
class DLLEXPORT CTTSWaveSegment {
public:
   ESCNEWDELETE;

   CTTSWaveSegment (void);
   ~CTTSWaveSegment (void);
   BOOL Compress (void);
   BOOL NeedToCompress (void);
   BOOL Decompress (void);
   BOOL MMLFromBinary (PVOID pMem, DWORD dwSize);
   size_t MMLToBinary (PCMem pmem);
   BOOL Init (WORD wFlags, PCM3DWave pWave, DWORD dwFeatureStart, DWORD dwFeatureEnd, PSRFEATURE pSRFToUse);
   DWORD MemoryTouch(void);
   BOOL Merge (CTTSWaveSegment *pWith);
   BOOL Exists (DWORD dwFeatureStart, DWORD dwFeatureEnd);
   PSRFEATURE GetSRFEATURE (CRITICAL_SECTION *pCS, DWORD dwFeatureStart, DWORD dwFeatureEnd,
                                          DWORD *pdwPrior = NULL, DWORD *pdwAfter = NULL, PTTSFEATURECOMPEXTRA *ppTFCE = NULL,
                                          short **ppaiPCM = NULL);
   PSRANALBLOCK GetSRANALBLOCK (CRITICAL_SECTION *pCS, DWORD dwFeatureStart, DWORD dwFeatureEnd,
                                          DWORD *pdwPrior, DWORD *pdwAfter, fp *pfMaxSpeechWindow,
                                          DWORD *pdwCompress, DWORD *pdwPriorComp, DWORD *pdwAfterComp, DWORD *pdwTotalComp);
   void FreeSRANALBLOCK (void);
   CTTSWaveSegment *Clone (void);

   // saved
   DWORD             m_dwFeatureStart;    // SRFeature that start of data. Don't change
   DWORD             m_dwFeatureEnd;      // SRFeature that's the end of the data (exlusive). Don't change.
   WORD              m_wFlags;            // TPMML_XXX
   DWORD             m_dwSamplesPerSecOrig;   // sampling rate of PCM audio
   DWORD             m_dwSRSkipOrig;          // number of samples per SRFEATURE

   // with upsamples PCM
   DWORD             m_dwSamplesPerSecUp; // after upsampling to remove aliasing
   DWORD             m_dwSRSkipUp;        // after upsampling to remove aliasing

private:
   CMem              m_memCompressed;  // compressed version of m_memSRFeature, NOT used if m_fKeepMemory
   PCMem             m_pmemSRFEATURE;   // memory containing all the SR features... normalized so max energy = 15000
   PCMem             m_pmemTTSFEATURECOMPEXTRA; // extra info per SR feature (m_dwNumSRFEATURE)
   PCMem             m_pmemPCM;        // memory for storing 16-bit linear PCM
   PCSRAnal          m_pSRAnal;           // SRFEATURESMALL version of all the data
   BOOL              m_fSRAnalCalc;       // set to TRUE after has been calculated
   PSRANALBLOCK      m_pSRANALBLOCK;      // analysis block result from pSRAnal
   fp                m_fMaxSpeechWindow;  // filled in by m_pSRAnal
   DWORD             m_dwSRFEATURECompress;  // amount of time-based compression in the srfeatures, keep every 1, 2, 3, or 4 features
   DWORD             m_dwSRFEATURESMALLCompress;   // time-based compression used for m_pSRAnal
   DWORD             m_dwNumSRANALBLOCK;  // number analysis blocks
};
typedef CTTSWaveSegment * PCTTSWaveSegment;


/* CTTSWave- For storing a list of CTTSWaveSegment objects */
class DLLEXPORT CTTSWave {
public:
   ESCNEWDELETE;

   CTTSWave (void);
   ~CTTSWave (void);
   BOOL Compress (void);
   BOOL NeedToCompress (void);
   void CalcSRANALBLOCK (CRITICAL_SECTION *pCS);
   PCTTSWaveSegment Find (DWORD dwFeatureStart, DWORD dwFeatureEnd);
   BOOL MMLFromBinary (PVOID pMem, DWORD dwSize);
   size_t MMLToBinary (PCMem pmem);
   BOOL Add (WORD wFlags, PCM3DWave pWave, DWORD dwFeatureStart, DWORD dwFeatureEnd, PSRFEATURE pSRFToUse);
   PSRFEATURE GetSRFEATURE (CRITICAL_SECTION *pCS, DWORD dwFeatureStart, DWORD dwFeatureEnd,
                                          DWORD *pdwPrior = NULL, DWORD *pdwAfter = NULL, PTTSFEATURECOMPEXTRA *ppTFCE = NULL,
                                          short **ppaiPCM = NULL);
   PSRANALBLOCK GetSRANALBLOCK (CRITICAL_SECTION *pCS, DWORD dwFeatureStart, DWORD dwFeatureEnd,
                                          DWORD *pdwPrior, DWORD *pdwAfter, fp *pfMaxSpeechWindow,
                                          DWORD *pdwCompress, DWORD *pdwPriorComp, DWORD *pdwAfterComp, DWORD *pdwTotalComp);
   void FreeSRANALBLOCK (void);
   CTTSWave *Clone (void);
   DWORD MemoryTouch (void);

   // saved to database
   DWORD             m_dwSentenceNum;     // sentence number. Change this to the appropriate number, so it's saved with MMLToBinary()

private:
   CListFixed        m_lPCTTSWaveSegment; // list of wave segments
};
typedef CTTSWave * PCTTSWave;


DEFINE_GUID(GUID_TTS, 
0x4593d433, 0x1362, 0xc686, 0x13, 0x8a, 0xa9, 0xf4, 0x8f, 0xa8, 0xfa, 0xb8);

DEFINE_GUID(GUID_TTSOLD, // older version, still use for derived tts
0x4593d433, 0x1362, 0xc682, 0x13, 0x8a, 0xa9, 0x24, 0x8f, 0xa8, 0xfb, 0xb8);

DEFINE_GUID(GUID_TTSProsody, 
0x6893d41c, 0x1362, 0xc682, 0x49, 0x8a, 0xa9, 0x8b, 0x8f, 0xa8, 0xfb, 0x21);

DWORD SentenceLengthToBin (DWORD dwLength);
DWORD SentenceLengthFromBin (DWORD dwBin);
DWORD SentenceLengthTotal (DWORD dwNum = (DWORD)-1);
#define SENTENCELENGTHTOBINNUM   6        // number of bins of sentence lengths

fp UnitScorePitch (PCMTTS pTTS, DWORD dwPhone, PCMLexicon pLex, BOOL fHigher, BOOL fFullPCM,
                   fp fPitchConfidence);
fp UnitScoreEnergy (PCMTTS pTTS, DWORD dwPhone, PCMLexicon pLex, BOOL fHigher);
fp UnitScoreDuration (PCMTTS pTTS, DWORD dwPhone, PCMLexicon pLex, BOOL fHigher, BOOL fFullPCM,
                      fp fPitchConfidence);
fp UnitScoreScoreLRMismatch (PCMTTS pTTS, DWORD dwPhoneCenter, DWORD dwPhoneLR, PCMLexicon pLex, BOOL fRightContext, DWORD dwMismatch);
fp UnitScoreJoinEstimate (PCMTTS pTTS, DWORD dwPhoneLeft, DWORD dwPhoneRight, BOOL fMidPhone, PCMLexicon pLex);
fp UnitScoreFunc (PCMTTS pTTS, DWORD dwPhone, PCMLexicon pLex, DWORD dwFuncWordGroupOrig, DWORD dwFuncWordGroupTarget);
fp UnitScoreCombine (fp fScorePitch, fp fScoreDuration, fp fScoreEnergy, fp fScoreFunc);
fp UnitScoreAccent (PCMTTS pTTS, DWORD dwPhone, PCMLexicon pLex, BOOL fHigher);


/*************************************************************************************
WaveCache.cpp */
extern CRITICAL_SECTION gcsWaveCache;   // critical section for wave cache


/********************************************************************************
ControlWaveView.cpp
*/
BOOL ControlWaveView (PCEscControl pControl, DWORD dwMessage, PVOID pParam);


// WAVEVIEWMARKER - indicate where markers are to go
typedef struct {
   DWORD          dwSample;      // sample
   COLORREF       cColor;        // color
} WAVEVIEWMARKER, *PWAVEVIEWMARKER;

#define  ESCM_WAVEVIEW         (ESCM_MESSAGE+234)
// Tells a waveview control what wave to use
typedef struct {
   PCM3DWave      pWave;   // new wave to use
} ESCMWAVEVIEW, *PESCMWAVEVIEW;

#define  ESCM_WAVEVIEWMARKERSSET    (ESCM_MESSAGE+235)
// Set the markers for a wave view object
typedef struct {
   DWORD          dwNum;   // number of markers
   PWAVEVIEWMARKER paMarker;   // pointer to dwNum markers
} ESCMWAVEVIEWMARKERSSET, *PESCMWAVEVIEWMARKERSSET;

#define  ESCN_WAVEVIEWMARKERCHANGED    (ESCM_NOTIFICATION+235)
// Set the markers for a wave view object
typedef struct {
   CEscControl    *pControl;  // control
   DWORD          dwNum;      // marker number
   DWORD          dwSample;   // new sample
} ESCNWAVEVIEWMARKERCHANGED, *PESCNWAVEVIEWMARKERCHANGED;


/********************************************************************************
ControlSRFEATUREView.cpp
*/
BOOL ControlSRFEATUREView (PCEscControl pControl, DWORD dwMessage, PVOID pParam);

#define  ESCM_SRFEATUREVIEW         (ESCM_MESSAGE+236)
// Sets the SRFeature info up
typedef struct {
   COLORREF       cColor;     // color to use
   SRFEATURE      srfWhite;   // white SRfeature
   SRFEATURE      srfBlack;   // black SRFeture
   BOOL           fShowWhite; // if true then display white
   BOOL           fShowBlack; // if true then display black
   fp             afOctaveBands[2][SROCTAVE];   // scaling of octave bands. Each is relative weight for size
                     // [0][x] = voiced, [1][x] = unvoiced
} ESCMSRFEATUREVIEW, *PESCMSRFEATUREVIEW;



/********************************************************************************
CM3DWave.cpp
*/
void PitchDetectCacheEnd (void);
void PitchDetectCacheInit (void);
DWORD ADPCMDecompress (PVOID pMemory, DWORD dwSize, PCMem pMemSamples);
BOOL ADPCMCompress (short *pasPCM, DWORD dwSamples, PCMem pMem);

#define PHASEBLURWIDTH_BASE               0.03
#define PHASEBLURWIDTH_TOP                (PHASEBLURWIDTH_BASE * 2.0)
   // BUGFIX - Was (PHASEBLURWIDTH_BASE/2.0), but because phase at high harmonics hard
   // to detect, make (PHASEBLURWIDTH_BASE*2.0) to average out and reduce phasing



/********************************************************************************
CPhaseModel.cpp
*/


#define PHASEMODELPITCH          7           // number of pitch bins to store
         // BUGFIX - Upped from 5 to 7
#define PHASEMODELPITCH_DELTA    0.10        // every pitch bin is 0.20 octaves higher/lower
   // BUGFIX - Was 0.25, but found didn't have many examples for highest/lowest frequencies
   // BUGFIX - When changed to 7 points, lowered to .1
#define PHASEMODELPITCH_MAXEXAMPLES 48       // maximum number of examples
   // BUGFIX - Upped from 32 to 48
#define PHASEMODELPITCH_MAXEXAMPLESTOP    (PHASEMODELPITCH_MAXEXAMPLES/16)  // keep top ones for wave


// PHASEMODELEXAMPLE - Example for the phase model
typedef struct {
   SRFEATURESMALL SRFS;                      // SR feature that this is associated with
#ifdef SRFEATUREINCLUDEPCM_SHORT
   short          asPCM[SRFEATUREPCM];       // points to store
#else
   char           acPCM[SRFEATUREPCM];       // points to store
#endif
} PHASEMODELEXAMPLE, *PPHASEMODELEXAMPLE;

// PHASEMODELEXAMPLECALC - Calculated information for phase model
typedef struct {
   fp             fEnergySRFS;               // energy in SRFS
   WAVESEGMENTFLOAT WSF;                     // wave segment, normalized to an energy of 1.0
} PHASEMODELEXAMPLECALC, *PPHASEMODELEXAMPLECALC;

// PHASEMODELHEADER - For binary MMLto
typedef struct {
   DWORD          dwTotal;                   // total size
   DWORD          adwSize[PHASEMODELPITCH];  // number of bytes for each of thse
   DWORD          adwTrainedTimes[PHASEMODELPITCH];            // value for this
} PHASEMODELHEADER, *PPHASEMODELHEADER;

// CPhaseModel - For storing the phase info for one phoneme
class CPhaseModel {
public:
   ESCNEWDELETE;

   CPhaseModel (void);
   ~CPhaseModel (void);

   BOOL CloneTo (CPhaseModel *pTo);
   CPhaseModel *Clone (void);

   size_t MMLToBinary (PCMem pMem);
   size_t MMLFromBinary (PBYTE pabMem, size_t dwLeft);

   DWORD Pitch (fp fPitch, fp fAvgPitch);
   fp PitchAsFp (fp fPitch, fp fAvgPitch);
   BOOL Train (DWORD dwPitch, PSRFEATURESMALL pSRFS, PSRFEATURE pSRF);
   void FillOut (void);
   PCListFixed CalcPHASEMODELEXAMPLE (DWORD dwPitch);

   BOOL MatchingExampleFindSRFEATURESMALL (PSRFEATURESMALL pSRFS, fp fEnergySRFS,
                                                     DWORD dwPitch, PCListFixed plPWAVESEGMENTFLOAT);
   BOOL MatchingExampleFindPCM (PWAVESEGMENTFLOAT pWSF, DWORD dwPitch, PCListFixed plPWAVESEGMENTFLOAT);
   BOOL MatchingExampleFind (PSRFEATURESMALL pSRFS, fp fEnergySRFS, fp fPitch,
                                       fp fPitchAvg,
                                       CPhaseModel *pPMB, fp fWeightB,
                                       PWAVESEGMENTFLOAT pwsf, BOOL fPreviousValid);

private:
   BOOL CommitTraining (DWORD dwPitch = (DWORD)-1);

   DWORD             m_adwTrainedTimes[PHASEMODELPITCH];       // number of times the model was trained
   CListFixed        m_alPHASEMODELEXAMPLE[PHASEMODELPITCH];   // trained/committed examples;
                        // won't have any more than PHASEMODELPITCH_MAXEXAMPLES entries
   CListFixed        m_alPHASEMODELEXAMPLEToTrain[PHASEMODELPITCH];  // list of exmaples to train
                        // won't have any more than PHASEMODELPITCH_MAXEXAMPLES entries
   CListFixed        m_alPHASEMODELEXAMPLECALC[PHASEMODELPITCH];  // one per m_lPHASEMODELEXAMPLE, if calculated
                        // won't have any more than PHASEMODELPITCH_MAXEXAMPLES entries
};
typedef CPhaseModel *PCPhaseModel;



/********************************************************************************
PitchSub.cpp
*/


// PITCHSUBSYLLABLE - Information about a syllable thats needed to calc the subpitch
typedef struct {
   DWORD             dwStart;       // start sample
   DWORD             dwEnd;         // end sample
   DWORD             dwMid;         // where the low (mid point) occurred
   fp                fPitchLowest;  // lowest pitch, log2() - log2(SRBASEPITCH)
   fp                fPitchHighest; // highest pitch, log2() - log2(SRBASEPITCH)
   BOOL              fSilence;      // TRUE if is silence
   BOOL              fVowel;        // TRUE if this is a vowel. FALSE it's a consonant
   BOOL              fVoiced;       // TRUE if is voiced
   DWORD             dwWordNum;     // word number

   // filled in
   fp                fPitchSub;     // calculated sub pitch, in log2() - log2(SRBASEPITCH),
                                    // or -1000 if not specified
} PITCHSUBSYLLABLE, *PPITCHSUBSYLLABLE;

BOOL PitchSubCalc (PCM3DWave pWave);
BOOL PitchSubCalc (PPITCHSUBSYLLABLE ppss, DWORD dwNum, DWORD dwSamplesPerSec);


#endif // _M3DWAVE_H_