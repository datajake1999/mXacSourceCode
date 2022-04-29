/************************************************************************
Animation.cpp - Code to bring up the create-an-AVI window and actually
handle the animation increments.

begun 1/2/03 by Mike Rozak
Copyright 2003 Mike Rozak
*/

#include <windows.h>
#include <mmreg.h>
#include <msacm.h>
#define  COMPILE_MULTIMON_STUBS     // so that works win win95
#include <multimon.h>
#include <math.h>
#include <commctrl.h>
#include <vfw.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"


#define TIMER_BETWEEN      898
#define TIMERBETWEENTIME   10       // 10 ms

#define IDC_PAUSE          1000
#define IDC_CANCEL         1001
#define IDC_STATICALL      1002
#define IDC_STATICTHIS     1003
#define IDC_STATICLEFT     1004
#define IDC_PROGRESSALL    1005
#define IDC_PROGRESSTHIS   1006

// CAnimWindow - animation window
class CAnimWindow : public CProgressSocket, CViewSocket {
public:
   CAnimWindow (DWORD dwRenderShard);
   ~CAnimWindow (void);
   BOOL Init (PANIMINFO pai);

   LRESULT WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
   LRESULT ImageWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

   // CProgressSocket
   virtual BOOL Push (float fMin, float fMax);
   virtual BOOL Pop (void);
   virtual int Update (float fProgress);
   virtual BOOL WantToCancel (void);
   virtual void CanRedraw (void);

   // CViewSocket
   virtual void WorldChanged (DWORD dwChanged, GUID *pgObject);
   virtual void WorldUndoChanged (BOOL fUndo, BOOL fRedo);
   virtual void WorldAboutToChange (GUID *pgObject);

private:
   void UpdateTotalDisplay (void);
   void UpdateThisDisplay (fp fProgress);
   void UpdateBitmap (PCImage pImage);
   void UpdateBitmap (PCFImage pImage);
   void SetRenderCamera (void);
   BOOL WriteFrame (void);
   void IntegrateImage (BOOL fFinal);
   void CalcAudio (void);

   DWORD          m_dwRenderShard;

   // original info
   ANIMINFO       m_AI; // animation info
   HBITMAP        *phBitFromAI;  // pointer to the hToBitmap from the original m_AI

   // calculated info
   DWORD          m_dwFrames; // number of frames
   fp             m_fCurFrame;   // current frame time that working on
   DWORD          m_dwCurFrame;  // current frame number that working on
   double         m_fTimeUsedForFrames;   // amount of times used for the frames so far

   // rendering
   CImage         m_ImageRenderTo;  // rendering to this image
   CImage         m_ImageDown;      // downsampled image
   CImage         m_ImageMotion;    // motion blue image
   CImage         m_ImageMotionScratch;   // scratch for blurring in ray tracing
   PCRenderTraditional m_pRender;     // rendering engine - if not using ray tracing
   CFImage        m_FImageRenderTo; // rendering to this image
   CFImage        m_FImageDown;     // downsampled image
   CFImage        m_FImageMotion;   // motion blue image
   CFImage        m_FImageMotionScratch;  // scratch for blurring in ray tracing
   PCRenderRay     m_pRay;            // ray tracer, if necessary
   HBITMAP        m_hBmpDisplay;    // bitmap to display. can be NULL.
   BOOL           m_fCantQuit;      // if TRUE then if press exit, cant actually exit now
   BOOL           m_fWantToQuit;    // if cant exit will set this to true
   BOOL           m_fPaused;        // set to TRUE if paused
   DWORD          m_dwLastTimeSum;  // GetTickCount() of last time that checked for summing
   DWORD          m_dwWorldChanged; // so keep track of what has changed
   BOOL           m_fUseRay;        // set to TRUE if using ray tracing
   BOOL           m_fFloatImage;    // if TRUE then use floating point image
   DWORD          m_dwStrobe;       // current strobe to draw

   // image post processing
   PCNPREffectsList m_pEffect;      // effect to use

   // audio
   CMem           m_memAudio;       // memory used for audio
   CMem           m_memWFEX;        // memory for WFEX
   CMem           m_memAudioDest;   // desination buffer for ACM
   PWAVEFORMATEX  m_pWFEX;          // wave format. if NULL then no audio
   short          *m_pasAudioBuf;   // audio from the object
   float          *m_pafAudioSum;   // sum up all the audio here.
   DWORD          m_dwAudioSamples; // number of samples in m_pasAudioBuf
   HACMSTREAM     m_hACMStream;     // ACM stream
   ACMSTREAMHEADER   m_ACMHeader;   // header
   CMem           m_memAudioSrc;    // for ACM buffer
   BOOL           m_fAudioStart;    // set to TRUE if this is the 1st buffer of the audio data
   HMMIO          m_hWriteWave;     // handle for writing the wave file
   DWORD          m_dwACMSrcLength; // src length passed into acm header
   DWORD          m_dwACMDstLength; // dest length passed into acm header
   DWORD          m_dwAudioAVICurBlock;   // current block writing

   // windows
   HWND           m_hWndAnim;       // window used for animating
   HWND           m_hWndButtonPause;   // pause button
   HWND           m_hWndButtonCancel;   // cancel button
   HWND           m_hWndProgressAll;   // progress on everything
   HWND           m_hWndProgressThis;   // process on this frame
   HWND           m_hWndStaticAll;   // % progress on all
   HWND           m_hWndStaticThis;   // % progress on this
   HWND           m_hWndStaticLeft;   // guestimated time left
   HWND           m_hWndStaticLeftInfo;   // information
   HWND           m_hWndViewport;      // display image as rendered
   BOOL           m_fTimerBetween;  // set to true if timer exists

   // for progress
   DWORD          m_dwStartTime;          // GetTickCount() when start last called, 0 if not started
   DWORD          m_dwLastUpdateTime;     // last time update was called
   fp             m_fProgress;            // progress, from 0..1
   CListFixed     m_lStack;               // stack containing the current min and max - to scale Update() call
   DWORD          m_dwLastRedraw;         // last redraw time
   MMCKINFO       m_mmckinfoMain;         // main chunck
   MMCKINFO       m_mmckinfoData;         // data chunk

   // file saving
   DWORD          m_dwFileFormat;         // 0 for AVi, 1 for bitmpa, 2 for jpeg
   DWORD          m_dwFilenameExt;        // index into m_AI.szFile to the extension ".bmp", ".jpg", etc.
   PAVIFILE       m_pAVIFile;             // AVI file
   PAVISTREAM     m_pAVIStreamVideo;      // stream for the video
   PAVISTREAM     m_pAVIStreamAudio;      // audio stream. only valid if m_AI.fIncludeAudio
   HIC            m_hIC;                  // compression handler
   CMem           m_memCompFmt;           // memory from ICCompressGetFormat
   DWORD          m_dwCompFmt;            // size of compfmt
   DWORD          m_dwCompBuff;           // size of a compressed frame buffer
   BOOL           m_fSentICCompressBegin; // if compression was sent
   BITMAPINFOHEADER m_bi;                 // header for compression from
   CMem           m_memCompBuff;          // memory from ICCompress. Allocaed to ICCompressGetSize()
   DWORD          m_dwQuality;            // quality value of compressor
   BOOL           m_fCrunch;              // set to TRUE if suppors VIDCF_CRUNCH
   CMem           m_amemFrameBits[2];     // memory allocated large enough to store frame bits (dwWidth * dwHeight * 4)
};
typedef CAnimWindow *PCAnimWindow;


void AnimShowAllWindows (DWORD dwRenderShard);
void AnimHideAllWindows (void);
LRESULT CALLBACK AnimSceneWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AnimSceneImageWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);



static PWSTR gpszNext = L"next";
//static BOOL gfAnimating = FALSE;
static PCAnimWindow gapAnim[MAXRENDERSHARDS] = {NULL, NULL, NULL, NULL};


/****************************************************************************
CAnimWindow::Constructor and destructor
*/
CAnimWindow::CAnimWindow (DWORD dwRenderShard)
{
   m_dwRenderShard = dwRenderShard;

   gapAnim[m_dwRenderShard] = this;
   memset (&m_AI, 0, sizeof(m_AI));
   m_pRender = new CRenderTraditional (m_dwRenderShard);
   m_pRay = new CRenderRay (m_dwRenderShard);

   m_fSentICCompressBegin = FALSE;
   m_dwFrames = m_dwCurFrame = 0;
   m_dwAudioAVICurBlock = 0;
   m_fCurFrame = 0;
   m_fTimeUsedForFrames = 0;
   m_hBmpDisplay = NULL;
   m_fCantQuit = m_fWantToQuit = m_fPaused = FALSE;
   m_dwLastTimeSum = m_dwWorldChanged = 0;
   m_dwFileFormat = 0;
   m_dwFilenameExt = 0;
   m_fUseRay = FALSE;
   m_fFloatImage = FALSE;
   m_dwLastRedraw = 0;

   m_hWndButtonPause = m_hWndButtonCancel = NULL;
   m_hWndProgressAll = m_hWndProgressThis = NULL;
   m_hWndStaticAll = m_hWndStaticThis = m_hWndStaticLeft = m_hWndStaticLeftInfo = NULL;
   m_hWndViewport = NULL;
   m_fTimerBetween = FALSE;

   m_dwStartTime = m_dwLastUpdateTime = 0;
   m_fProgress = 0;
   m_lStack.Init (sizeof(TEXTUREPOINT));

   m_pAVIFile = NULL;
   m_pAVIStreamVideo = NULL;
   m_pAVIStreamAudio = NULL;
   m_hIC = NULL;

   m_pWFEX = NULL;
   m_pasAudioBuf = NULL;
   m_pafAudioSum = NULL;
   m_hACMStream = NULL;
   m_hWriteWave = NULL;
   m_fAudioStart = TRUE;

   m_pEffect = NULL;

   // hide it all
   AnimHideAllWindows ();

}

CAnimWindow::~CAnimWindow (void)
{
   // how long it took
   if (m_dwFrames >= 2)
      KeySet ("Last animation time (ms)", (DWORD)(m_fTimeUsedForFrames * 1000.0));

   if (m_AI.pWorld)
      m_AI.pWorld->NotifySocketRemove (this);

   if (m_hWndAnim)
      DestroyWindow (m_hWndAnim);
   m_hWndAnim = NULL;

   // free up bitmap
   if (m_hBmpDisplay)
      DeleteObject (m_hBmpDisplay);
   m_hBmpDisplay = NULL;

   MMRESULT mm;
   if (m_hACMStream) {
      // convert
      m_ACMHeader.cbSrcLength = (DWORD)m_memAudioSrc.m_dwCurPosn;
      m_ACMHeader.cbDstLengthUsed = 0;
      mm = acmStreamConvert (m_hACMStream, &m_ACMHeader,
         (m_fAudioStart ? ACM_STREAMCONVERTF_START : 0) |
         ACM_STREAMCONVERTF_END);
      m_fAudioStart = FALSE;

      // write last chunk of data
      if (m_ACMHeader.cbDstLengthUsed) {
         if (m_hWriteWave)
            mmioWrite (m_hWriteWave, (char*) m_ACMHeader.pbDst, m_ACMHeader.cbDstLengthUsed);
         if (m_pAVIStreamAudio) {
            // save it out
            LONG lSamp, lByte;
            DWORD dwWritten = m_hACMStream ? (m_ACMHeader.cbDstLengthUsed / m_pWFEX->nBlockAlign) : m_dwAudioSamples;
            AVIStreamWrite (m_pAVIStreamAudio, m_dwAudioAVICurBlock, dwWritten,
               m_hACMStream ? (LPBYTE)m_ACMHeader.pbDst : (LPBYTE)m_pasAudioBuf,
               m_hACMStream ? m_ACMHeader.cbDstLengthUsed : m_dwAudioSamples * m_pWFEX->nChannels * sizeof(short),
               0, &lSamp, &lByte);
            m_dwAudioAVICurBlock += dwWritten;
         }
      }

      // unprepare the header
      m_ACMHeader.cbSrcLength = m_dwACMSrcLength;
      m_ACMHeader.cbDstLength = m_dwACMDstLength;
      mm = acmStreamUnprepareHeader (m_hACMStream, &m_ACMHeader, 0);

      mm = acmStreamClose (m_hACMStream, 0);
      m_hACMStream = NULL;
   }

   if (m_hWriteWave) {
      // will need to ascend data
      mmioAscend (m_hWriteWave, &m_mmckinfoData, 0);

      // ascent main riff
      mmioAscend (m_hWriteWave, &m_mmckinfoMain, 0);


      mmioClose (m_hWriteWave, 0);
      m_hWriteWave = NULL;
   }


   if (m_pAVIStreamAudio) {
      AVIStreamRelease (m_pAVIStreamAudio);
      m_pAVIStreamAudio = NULL;
   }

   if (m_pAVIStreamVideo) {
      AVIStreamRelease (m_pAVIStreamVideo);
      m_pAVIStreamVideo = NULL;
   }

   if (m_pAVIFile) {
      AVIFileRelease (m_pAVIFile);
      m_pAVIFile = NULL;
      AVIFileExit();
   }

   if (m_hIC) {
      if (m_fSentICCompressBegin)
         ICCompressEnd (m_hIC);
      ICClose (m_hIC);
      m_hIC = NULL;
   }

   // show everything again
   AnimShowAllWindows (m_dwRenderShard);

   // play chimes so user knows is done
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   EscChime (ESCCHIME_INFORMATION);
	MALLOCOPT_RESTORE;

   // restore fast refresh
   if (m_AI.pSceneSet)
      m_AI.pSceneSet->m_fFastRefresh = TRUE;

   gapAnim[m_dwRenderShard] = NULL;

   if (m_pEffect)
      delete m_pEffect;

   if (m_pRender) {
      delete m_pRender;
      m_pRender = NULL;
   }
   if (m_pRay) {
      delete m_pRay;
      m_pRay = NULL;
   }
}

/****************************************************************************8
CAnimWindow::Init - Initializes the object. Returns FALSE if won't work

inputs
   PANIMINFO         pai - animation info to use
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CAnimWindow::Init (PANIMINFO pai)
{
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_AI = *pai;
   phBitFromAI = &pai->hToBitmap;

   BOOL fRaySecondPass;
   fRaySecondPass = FALSE;
   m_fFloatImage = FALSE;
   if (m_AI.dwQuality >= 6) {
      m_fUseRay = m_fFloatImage = TRUE;

      if (m_AI.dwAnti >= 2) {
         fRaySecondPass = TRUE;
         m_AI.dwAnti--;
      }
   }
   else if (m_AI.dwQuality == 5)
      m_fFloatImage = TRUE;


   // BUGFIX - If using ray tracing then step down antialiasing one level, but use
   // the extra antialias feature

   m_hWndAnim = NULL;
   m_hWndButtonPause = m_hWndButtonCancel = NULL;
   m_hWndProgressAll = m_hWndProgressThis = NULL;
   m_hWndStaticAll = m_hWndStaticThis = m_hWndStaticLeft = m_hWndStaticLeftInfo = NULL;
   m_hWndViewport = NULL;
   m_hBmpDisplay = NULL;
   m_fTimerBetween = FALSE;
   m_fCantQuit = FALSE;
   m_fWantToQuit = FALSE;
   m_fPaused = FALSE;

   m_dwStartTime = 0;
   m_dwLastUpdateTime = 0;
   m_fProgress = 0;
   m_lStack.Init (sizeof(TEXTUREPOINT));

   // while in this mode dont bother doing refresh
   if (m_AI.pSceneSet)
      m_AI.pSceneSet->m_fFastRefresh = FALSE;


   // calcualte the number of frames
   fp fTime;
   m_AI.dwFPS = max(1,m_AI.dwFPS);
   fTime = max(0, m_AI.fTimeEnd - m_AI.fTimeStart);
   m_dwFrames = max(1, (DWORD)((double)fTime * (double)m_AI.dwFPS));
   m_fCurFrame = m_AI.fTimeStart;
   m_dwCurFrame = 0;
   m_dwAudioAVICurBlock = 0;
   m_fTimeUsedForFrames = 0;
   m_dwLastTimeSum = 0;
   m_dwWorldChanged = -1;  // assume everything changed at first

   // expsoure
   m_AI.dwExposureStrobe = max(1, m_AI.dwExposureStrobe);
   m_AI.fExposureTime = max(0, m_AI.fExposureTime);
   if (m_AI.fExposureTime < CLOSE) {
      // so close to 0 make it 0
      m_AI.dwExposureStrobe = 1;
      m_AI.fExposureTime = 0;
   }

   // create render info
   m_AI.dwPixelsX = max(m_AI.dwPixelsX,1);
   m_AI.dwPixelsY = max(m_AI.dwPixelsY,1);
   m_AI.dwAnti = max(m_AI.dwAnti,1);
   m_AI.pWorld->SelectionClear();   // just so wont draw selection

   // initialize the ray tracer
   if (m_fFloatImage) {
      m_FImageRenderTo.Init (m_AI.dwPixelsX * m_AI.dwAnti, m_AI.dwPixelsY * m_AI.dwAnti);
      m_FImageDown.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
      m_FImageMotion.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
      m_FImageMotionScratch.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
   }
   else {
      m_ImageRenderTo.Init (m_AI.dwPixelsX * m_AI.dwAnti, m_AI.dwPixelsY * m_AI.dwAnti);
      m_ImageDown.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
      m_ImageMotion.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
      m_ImageMotionScratch.Init (m_AI.dwPixelsX, m_AI.dwPixelsY);
   }

   if (m_fUseRay) {

      m_pRay->CWorldSet (m_AI.pWorld);
      m_pRay->CFImageSet (&m_FImageRenderTo);

      DWORD dwCamera;
      CPoint pCenter, pRot, pTrans;
      fp fScale;
      dwCamera = m_AI.pRender->CameraModelGet ();
      switch (dwCamera) {
      case CAMERAMODEL_FLAT:
         m_AI.pRender->CameraFlatGet (&pCenter, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale, &pTrans.p[0], &pTrans.p[1]);
         m_pRay->CameraFlat (&pCenter, pRot.p[2], pRot.p[0], pRot.p[1], fScale, pTrans.p[0], pTrans.p[1]);
         break;
      case CAMERAMODEL_PERSPWALKTHROUGH:
         m_AI.pRender->CameraPerspWalkthroughGet (&pTrans, &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         m_pRay->CameraPerspWalkthrough (&pTrans, pRot.p[2], pRot.p[0], pRot.p[1], fScale);
         break;
      case CAMERAMODEL_PERSPOBJECT:
         m_AI.pRender->CameraPerspObjectGet (&pTrans, &pCenter,  &pRot.p[2], &pRot.p[0], &pRot.p[1], &fScale);
         m_pRay->CameraPerspObject (&pTrans, &pCenter,  pRot.p[2], pRot.p[0], pRot.p[1], fScale);
         break;
      default:
         return FALSE;  // error
      }
      m_pRay->AmbientExtraSet (m_AI.pRender->AmbientExtraGet());
      m_pRay->BackgroundSet (m_AI.pRender->BackgroundGet());
      m_pRay->ExposureSet (m_AI.pRender->ExposureGet());
      m_pRay->RenderShowSet ( m_pRender->RenderShowGet() & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_ANIMCAMERA));

      m_pRay->QuickSetRay (m_AI.dwQuality - 6, m_AI.dwExposureStrobe, m_AI.dwAnti);
      m_pRay->m_fAntiEdge = fRaySecondPass;
   }
   else { // traditional renderer
      m_fUseRay = FALSE;
      
      if (m_AI.pRender)
         m_AI.pRender->CloneTo (m_pRender);
      m_pRender->CWorldSet (m_AI.pWorld);
      if (m_fFloatImage)
         m_pRender->CFImageSet (&m_FImageRenderTo);
      else
         m_pRender->CImageSet (&m_ImageRenderTo);

      ::QualityApply (m_AI.dwQuality, m_pRender);
      m_pRender->m_fFinalRender = TRUE;
      m_pRender->RenderShowSet ( m_pRender->RenderShowGet() & ~(RENDERSHOW_VIEWCAMERA | RENDERSHOW_ANIMCAMERA));
         // dont show the cameras

      // clear fog and outlines
      if (m_pRender->m_pEffectFog)
         delete m_pRender->m_pEffectFog;
      if (m_pRender->m_pEffectOutline)
         delete m_pRender->m_pEffectOutline;
      m_pRender->m_pEffectFog = NULL;
      m_pRender->m_pEffectOutline = NULL;
   }

   if (m_AI.fToBitmap) {
      m_dwFileFormat = 3;  // to bitmap
      m_AI.fIncludeAudio = FALSE;   // no audio for bitmap
   }
   else {
      // find the extension
      DWORD dwLen;
      dwLen = (DWORD)wcslen(m_AI.szFile);
      if (dwLen <= 4)
         return FALSE;  // too short
      m_dwFilenameExt = dwLen - 4;
      if (!_wcsicmp(m_AI.szFile + m_dwFilenameExt, L".avi"))
         m_dwFileFormat = 0;
      else if (!_wcsicmp(m_AI.szFile + m_dwFilenameExt, L".bmp"))
         m_dwFileFormat = 1;
      else if (!_wcsicmp(m_AI.szFile + m_dwFilenameExt, L".jpg"))
         m_dwFileFormat = 2;
      else
         return FALSE;  // error in file fomat
   }

   // set up the audio buffers
   m_pWFEX = NULL;
   m_pasAudioBuf = NULL;
   m_pafAudioSum = NULL;
   if (m_AI.fIncludeAudio && m_AI.pScene && m_AI.pmemWFEX && (m_AI.pmemWFEX->m_dwCurPosn >= sizeof(WAVEFORMATEX))) {
      if (!m_memWFEX.Required (m_AI.pmemWFEX->m_dwCurPosn))
         return FALSE;
      memcpy (m_memWFEX.p, m_AI.pmemWFEX->p, m_AI.pmemWFEX->m_dwCurPosn);
      m_pWFEX = (PWAVEFORMATEX) m_memWFEX.p;
   }
   if (m_pWFEX) {
      // what's the largest number of samples
      DWORD dwLargest = m_pWFEX->nSamplesPerSec / m_AI.dwFPS + 1;
      dwLargest += (dwLargest % 2); // so DWORD align
      if (!m_memAudio.Required (dwLargest * m_pWFEX->nChannels * (sizeof(short) + sizeof(float))))
         return FALSE;  // error
      m_pasAudioBuf = (short*) m_memAudio.p;
      m_pafAudioSum = (float*) (m_pasAudioBuf + dwLargest * m_pWFEX->nChannels);
      m_dwAudioSamples = dwLargest; // use this for now althroughwill overwrite later
   }


   // initialize the acm
   if (m_pWFEX && ( (m_pWFEX->wFormatTag != WAVE_FORMAT_PCM) || (m_pWFEX->wBitsPerSample != 16) )) {
      // needs to get converted
      WAVEFORMATEX   WFEXOld;
      memset (&WFEXOld, 0, sizeof(WFEXOld));
      WFEXOld.nChannels = m_pWFEX->nChannels;
      WFEXOld.nSamplesPerSec = m_pWFEX->nSamplesPerSec;
      WFEXOld.wFormatTag = WAVE_FORMAT_PCM;
      WFEXOld.wBitsPerSample = 16;
      WFEXOld.nBlockAlign = (WFEXOld.wBitsPerSample * WFEXOld.nChannels) / 8;
      WFEXOld.nAvgBytesPerSec = WFEXOld.nBlockAlign * WFEXOld.nSamplesPerSec;

      // open the converter
      m_hACMStream = NULL;
      MMRESULT mm;
      mm = acmStreamOpen (&m_hACMStream, NULL, &WFEXOld, m_pWFEX, NULL, NULL, 0,
         ACM_STREAMOPENF_NONREALTIME);
      if (!m_hACMStream)
         return FALSE;

      // figure out the destination stream size
      DWORD dwDestSize;
      DWORD dwBlock;
      dwBlock = m_dwAudioSamples*2;
      dwBlock = max (dwBlock, WFEXOld.nSamplesPerSec);
         // BUGFIX - At least one second since some compressors, like ADPCM, use
         // blocks longer than a frame length
      dwBlock *= WFEXOld.nBlockAlign; // BUGFIX - Allocate more so can have cache
      dwDestSize = 0;
      if (acmStreamSize (m_hACMStream, dwBlock, &dwDestSize, ACM_STREAMSIZEF_SOURCE)) {
         acmStreamClose (m_hACMStream, 0);
         m_hACMStream = NULL;
         return FALSE;
      }

      // allocate the memory
      if (!m_memAudioSrc.Required (dwBlock) || !m_memAudioDest.Required (dwDestSize*2)) {  // BUGFIX - ACM seemed to be overwriting beyond
         acmStreamClose (m_hACMStream, 0);
         m_hACMStream = NULL;
         return FALSE;
      }
      m_memAudioSrc.m_dwCurPosn = 0;
      BYTE *pNew;
      pNew = (BYTE*) m_memAudioDest.p;

      // prepare the stream header
      memset (&m_ACMHeader, 0, sizeof(m_ACMHeader));
      m_ACMHeader.cbStruct = sizeof(m_ACMHeader);
      m_ACMHeader.pbSrc = (LPBYTE) m_memAudioSrc.p;
      m_dwACMSrcLength = m_ACMHeader.cbSrcLength = dwBlock;
      m_ACMHeader.pbDst = pNew;
      m_dwACMDstLength = m_ACMHeader.cbDstLength = dwDestSize;
      mm = acmStreamPrepareHeader (m_hACMStream, &m_ACMHeader, 0);

   }


   char szTemp[512];

   // open wave file to write audio out to
   if ((m_dwFileFormat == 1) || (m_dwFileFormat == 2)) { // for bmp and jpg
      int iLen;
      WideCharToMultiByte (CP_ACP, 0, m_AI.szFile, -1, szTemp, sizeof(szTemp), 0,0);
      iLen = (DWORD)strlen(szTemp);
         // safe to use iLen-4 because have generated file name
      strcpy (szTemp + (iLen - 4), ".wav");

      m_hWriteWave = mmioOpen (szTemp, NULL, MMIO_WRITE | MMIO_CREATE | MMIO_EXCLUSIVE );
      if (!m_hWriteWave)
         return FALSE;

      // creat the main chunk
      memset (&m_mmckinfoMain, 0, sizeof(m_mmckinfoMain));
      m_mmckinfoMain.fccType = mmioFOURCC('W', 'A', 'V', 'E');
      if (mmioCreateChunk (m_hWriteWave, &m_mmckinfoMain, MMIO_CREATERIFF)) {
         return FALSE;
      }


      // create the format
      MMCKINFO mmckinfoFmt;
      memset (&mmckinfoFmt, 0, sizeof(mmckinfoFmt));
      mmckinfoFmt.ckid = mmioFOURCC('f', 'm', 't', ' ');
      if (mmioCreateChunk (m_hWriteWave, &mmckinfoFmt, 0)) {
         return FALSE;
      }

      // write the header
      mmioWrite (m_hWriteWave, (char*) m_pWFEX, sizeof(WAVEFORMATEX) + m_pWFEX->cbSize);

      // ascent the format
      mmioAscend (m_hWriteWave, &mmckinfoFmt, 0);

      memset (&m_mmckinfoData, 0, sizeof(m_mmckinfoData));
      m_mmckinfoData.ckid = mmioFOURCC('d', 'a', 't', 'a');
      if (mmioCreateChunk (m_hWriteWave, &m_mmckinfoData, 0)) {
         return FALSE;
      }
   }


   // open AVI file
   if (m_dwFileFormat == 0) { // AVI
      HRESULT hRes;
      WideCharToMultiByte (CP_ACP, 0, m_AI.szFile, -1, szTemp, sizeof(szTemp), 0,0);
      AVIFileInit ();
      m_pAVIFile = NULL;
      hRes = AVIFileOpen (&m_pAVIFile, szTemp, OF_CREATE | OF_SHARE_EXCLUSIVE | OF_WRITE, NULL);
      if (!m_pAVIFile) {
         // error
         AVIFileExit();
         return FALSE;
      }

      memset (&m_bi, 0, sizeof(m_bi));
      m_bi.biSize = sizeof(m_bi);
      m_bi.biWidth = m_AI.dwPixelsX;
      m_bi.biHeight = m_AI.dwPixelsY;
      m_bi.biPlanes = 1;
      m_bi.biBitCount = 32;
      m_bi.biCompression = BI_RGB;
      m_bi.biSizeImage = m_AI.dwPixelsX * m_AI.dwPixelsY * 4;
      m_hIC = ICLocate (ICTYPE_VIDEO, m_AI.dwAVICompressor, &m_bi, NULL, ICMODE_COMPRESS);
      if (!m_hIC)
         return FALSE;

      // get the info since need to create streme
      ICINFO icInfo;
      memset (&icInfo, 0, sizeof(icInfo));
      icInfo.dwSize = sizeof(icInfo);
      if (!ICGetInfo (m_hIC, &icInfo, sizeof(icInfo)))
         return FALSE;

      // figurte out format
      if (!m_memCompFmt.Required (m_dwCompFmt = ICCompressGetFormatSize (m_hIC, &m_bi)))
         return FALSE;
      if (ICERR_OK != ICCompressGetFormat (m_hIC, &m_bi, (PBITMAPINFO) m_memCompFmt.p))
         return FALSE;

      // start compression
      if (ICERR_OK != ICCompressBegin (m_hIC, &m_bi, m_memCompFmt.p))
         return FALSE;
      m_fSentICCompressBegin = TRUE;
      m_dwQuality = ICGetDefaultQuality (m_hIC);
      m_fCrunch = (icInfo.dwFlags & VIDCF_CRUNCH) ? TRUE : FALSE;

      if (!m_memCompBuff.Required(m_dwCompBuff = ICCompressGetSize (m_hIC, &m_bi, m_memCompFmt.p)))
         return FALSE;

      // allocate enough for frame bits
      if (!m_amemFrameBits[0].Required (m_AI.dwPixelsX * m_AI.dwPixelsY * 4))
         return FALSE;
      if (!m_amemFrameBits[1].Required (m_AI.dwPixelsX * m_AI.dwPixelsY * 4))
         return FALSE;

      // stream
      AVISTREAMINFO si;
      memset (&si, 0, sizeof(si));
      si.fccType = streamtypeVIDEO;
      si.fccHandler = icInfo.fccHandler;
      si.dwScale = 1;
      si.dwRate = m_AI.dwFPS;
      si.dwLength = m_dwFrames; // think this is correct
      si.dwSuggestedBufferSize = 0; // might want to set this but dont know how
      si.dwQuality = m_dwQuality;
      si.rcFrame.right = m_AI.dwPixelsX;
      si.rcFrame.bottom = m_AI.dwPixelsY;
      m_pAVIStreamVideo = NULL;
      hRes = AVIFileCreateStream (m_pAVIFile, &m_pAVIStreamVideo, &si);
      if (hRes || !m_pAVIStreamVideo)
         return FALSE;

      // format
      hRes = AVIStreamSetFormat (m_pAVIStreamVideo, 0, m_memCompFmt.p, m_dwCompFmt);
      if (hRes)
         return FALSE;

      // create audio?
      m_pAVIStreamAudio = NULL;
      if (m_pWFEX) {
         memset (&si, 0, sizeof(si));
         si.fccType = streamtypeAUDIO;
         si.dwScale = m_pWFEX->nBlockAlign;
         si.dwRate = m_pWFEX->nAvgBytesPerSec;
         si.dwLength = (DWORD)((__int64) m_dwFrames * (__int64) m_pWFEX->nAvgBytesPerSec /
            (__int64) m_pWFEX->nBlockAlign / (__int64) m_AI.dwFPS); // think this is correct
         si.dwSuggestedBufferSize = 0; // might want to set this but dont know how
         si.dwQuality = -1;
         si.dwSampleSize  = m_pWFEX->nBlockAlign;
         si.dwInitialFrames = m_AI.dwFPS * 3 / 4;
            // Not sure setting intiial frames correctly but documenatation sucks.
            // setting to .75 sec
         m_pAVIStreamAudio = NULL;
         hRes = AVIFileCreateStream (m_pAVIFile, &m_pAVIStreamAudio, &si);
         if (hRes || !m_pAVIStreamAudio)
            return FALSE;

         // format
         hRes = AVIStreamSetFormat (m_pAVIStreamAudio, 0, m_pWFEX, sizeof(WAVEFORMATEX) + m_pWFEX->cbSize);
         if (hRes)
            return FALSE;
      } // include audio

   }

   // load the effect
   if (!IsEqualGUID (m_AI.gEffectCode, GUID_NULL))
      m_pEffect = EffectCreate (m_dwRenderShard, &m_AI.gEffectCode, &m_AI.gEffectSub);

   // so know what has changed when do scnene set
   if (m_AI.pWorld)
      m_AI.pWorld->NotifySocketAdd (this);

   // register window
   static BOOL gfIsRegistered = FALSE;
   if (!gfIsRegistered) {
      WNDCLASS wc;
      gfIsRegistered = TRUE;
      memset (&wc, 0, sizeof(wc));
      wc.hIcon = LoadIcon (ghInstance, MAKEINTRESOURCE(IDI_ANIMICON));
      wc.lpfnWndProc = AnimSceneWndProc;
      wc.style = CS_HREDRAW | CS_VREDRAW;
      wc.hInstance = ghInstance;
      wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
      wc.lpszClassName = "AnimScene";
      wc.hCursor = LoadCursor (NULL, IDC_NO);
      RegisterClass (&wc);

      // image
      wc.hIcon = NULL;
      wc.lpfnWndProc = AnimSceneImageWndProc;
      wc.hbrBackground = NULL;
      wc.lpszClassName = "AnimSceneImage";
      RegisterClass (&wc);

   }


   // size
   int iX, iY;
   RECT rLoc;
   iX = GetSystemMetrics (SM_CXSCREEN);
   iY = GetSystemMetrics (SM_CYSCREEN);
   rLoc.left = iX / 16;
   rLoc.top = iX / 16;
   rLoc.right = iX * 15 / 16;
   rLoc.bottom = iY * 15 / 16;

   // title
   if (m_dwFileFormat == 3)
      strcpy (szTemp, "Drawing...");
   else {
      strcpy (szTemp, "Animating ");
      WideCharToMultiByte (CP_ACP, 0, m_AI.szFile, -1, szTemp + strlen(szTemp), sizeof(szTemp), 0, 0);
   }

	MALLOCOPT_RESTORE;
   // create window
   m_hWndAnim = CreateWindowEx (
      WS_EX_APPWINDOW
      , "AnimScene", szTemp,
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
      rLoc.left, rLoc.top, rLoc.right - rLoc.left, rLoc.bottom - rLoc.top,
      NULL, NULL, ghInstance, (PVOID) this);
   ShowWindow (m_hWndAnim, SW_SHOWNORMAL);

   return TRUE;
}



/******************************************************************************
CAnimWindow::WantToCancel - Returns TRUE if the user has pressed cancel and the
operation should exit.
*/
BOOL CAnimWindow::WantToCancel (void)
{
   return m_fWantToQuit;
}

/******************************************************************************
CAnimWindow::CanRedraw - Called by the renderer (or whatever) to tell the caller
that the display can be updated with a new image. This is used by the ray-tracer
to indicate it has finished drawing another bucket.
*/
void CAnimWindow::CanRedraw (void)
{
   DWORD dwTime = GetTickCount();
   if (dwTime - m_dwLastRedraw < 2000)
      return;  // ignore if it's been less than a second since last redraw
   m_dwLastRedraw = dwTime;

   IntegrateImage (FALSE);
}

/****************************************************************************
CAnimWindow::WriteFrame - Writes the frame either to an AVI file or bmp/jpg.
This assumes that the frame's size is as specific in m_AI.dwWidth and dwHeight.
Writes m_ImageDown.

returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CAnimWindow::WriteFrame (void)
{
   // convert the audio
   if (m_hACMStream) {
      // convert
      MMRESULT mm;
      DWORD dwAudioLen = m_dwAudioSamples * m_pWFEX->nChannels * sizeof(short);
      if (m_memAudioSrc.m_dwCurPosn + dwAudioLen >= m_memAudioSrc.m_dwAllocated)
         m_memAudioSrc.m_dwCurPosn = 0;   // BUGFIX - Prevent overflows
      memcpy ((PBYTE)m_memAudioSrc.p + m_memAudioSrc.m_dwCurPosn,
         m_pasAudioBuf, dwAudioLen);
      m_memAudioSrc.m_dwCurPosn += dwAudioLen;
      m_ACMHeader.cbSrcLength = (DWORD)m_memAudioSrc.m_dwCurPosn;
      m_ACMHeader.cbDstLengthUsed = 0;
      mm = acmStreamConvert (m_hACMStream, &m_ACMHeader,
         (m_fAudioStart ? ACM_STREAMCONVERTF_START : 0) |
         ACM_STREAMCONVERTF_BLOCKALIGN);
      m_fAudioStart = FALSE;
      if (m_ACMHeader.cbSrcLengthUsed <= m_memAudioSrc.m_dwCurPosn) {
         memmove (m_memAudioSrc.p, (PBYTE)m_memAudioSrc.p + m_ACMHeader.cbSrcLengthUsed,
            m_memAudioSrc.m_dwCurPosn - m_ACMHeader.cbSrcLengthUsed);
         m_memAudioSrc.m_dwCurPosn -= m_ACMHeader.cbSrcLengthUsed;
      }
      else
         m_memAudioSrc.m_dwCurPosn = 0;

      // amount of data = m_ACMHeader.cbSrcLengthUsed;
   }

   // write the audio
   if (m_hWriteWave) {
      if (m_hACMStream)
         mmioWrite (m_hWriteWave, (char*) m_ACMHeader.pbDst, m_ACMHeader.cbDstLengthUsed);
      else
         mmioWrite (m_hWriteWave, (char*) m_pasAudioBuf, m_dwAudioSamples * m_pWFEX->nChannels * sizeof(short));
   }
   if (m_pAVIStreamAudio) {
      // save it out
      LONG lSamp, lByte;
      DWORD dwWritten = m_hACMStream ? (m_ACMHeader.cbDstLengthUsed / m_pWFEX->nBlockAlign) : m_dwAudioSamples;
      if (AVIStreamWrite (m_pAVIStreamAudio, m_dwAudioAVICurBlock, dwWritten,
         m_hACMStream ? (LPBYTE)m_ACMHeader.pbDst : (LPBYTE)m_pasAudioBuf,
         m_hACMStream ? m_ACMHeader.cbDstLengthUsed : m_dwAudioSamples * m_pWFEX->nChannels * sizeof(short),
         0, &lSamp, &lByte))
         return FALSE;
      if (dwWritten != (DWORD)lSamp)
         dwWritten = (DWORD)lSamp;
      m_dwAudioAVICurBlock += dwWritten;
   }

   if ((m_dwFileFormat == 1) || (m_dwFileFormat == 2)) {
      // NOTE: Assuming that UpdateBitmap() has already been called and the bitmpa
      // in it si the one we want to save

      // if we have more than 1 frame than write frame numbers
      if (m_dwFrames > 1) {
         // how many digits?
         DWORD i, dwDigits;
         WCHAR szTemp[16];
         i = m_dwFrames;
         for (dwDigits = 0; i; dwDigits++, i /= 10);

         wcscpy (szTemp, L"%.xd");
         szTemp[2] = L'0' + (WCHAR)dwDigits;
         swprintf (m_AI.szFile + m_dwFilenameExt, szTemp, (int) m_dwCurFrame+1);
         
         // append format
         wcscat (m_AI.szFile, (m_dwFileFormat == 1) ? L".bmp" : L".jpg");
      }

      if (!m_hBmpDisplay)
         return FALSE;

      char szTemp[256];
      WideCharToMultiByte (CP_ACP, 0, m_AI.szFile, -1, szTemp, sizeof(szTemp), 0, 0);

      // save it
      if (m_dwFileFormat == 1)
         return BitmapSave (m_hBmpDisplay, szTemp);
      else
         return BitmapToJPegNoMegaFile (m_hBmpDisplay, szTemp);
   }
   else if (m_dwFileFormat == 3) {
      // it's to the hbmp
      if (!m_hBmpDisplay || !phBitFromAI)
         return FALSE;

      if (*phBitFromAI)
         DeleteObject (*phBitFromAI);
      *phBitFromAI = m_hBmpDisplay;
      m_hBmpDisplay = NULL;
      return TRUE;
   }

   // else, it's a AVI
   // fill this into a frame
   DWORD *padw;
   DWORD i, y;
   padw = (DWORD*) m_amemFrameBits[m_dwCurFrame%2].p;
   // BUGFIX - For some-reason image flipped vertically
   if (m_fFloatImage) { // BUGFIX - Was only for ray traing
      PFIMAGEPIXEL pip;
      float afc[3];
      for (y = 0; y < m_AI.dwPixelsY; y++) {
         pip = m_FImageDown.Pixel(0,m_AI.dwPixelsY - y - 1);
         for (i = 0; i < m_AI.dwPixelsX; i++, padw++, pip++) {
            // BUGFIX - Move afc[x] = xyz; so that in the right place
            afc[0] = max(min(pip->fRed, (fp)0xffff), 0);
            afc[1] = max(min(pip->fGreen, (fp)0xffff), 0);
            afc[2] = max(min(pip->fBlue, (fp)0xffff), 0);

            *padw = RGB(UnGamma ((WORD)afc[2]), UnGamma ((WORD)afc[1]), UnGamma ((WORD)afc[0]));
         }
      }
   }
   else {
      PIMAGEPIXEL pip;
      for (y = 0; y < m_AI.dwPixelsY; y++) {
         pip = m_ImageDown.Pixel(0,m_AI.dwPixelsY - y - 1);
         for (i = 0; i < m_AI.dwPixelsX; i++, padw++, pip++)
            *padw = RGB(m_ImageDown.UnGamma (pip->wBlue), m_ImageDown.UnGamma (pip->wGreen), m_ImageDown.UnGamma (pip->wRed));
      }
   }

   // use AVI to compress
   DWORD dwFlags;
   dwFlags = 0;
   LPBITMAPINFOHEADER pbmi;
   pbmi = (LPBITMAPINFOHEADER) m_memCompFmt.p;
   if (ICERR_OK != ICCompress (m_hIC, 0, pbmi, m_memCompBuff.p,
      &m_bi, m_amemFrameBits[m_dwCurFrame%2].p, 0, &dwFlags, m_dwCurFrame, 0, m_dwQuality,
      m_dwCurFrame ? &m_bi : NULL, m_dwCurFrame ? m_amemFrameBits[(m_dwCurFrame-1)%2].p : NULL))
      return FALSE;

   // save it out
   LONG lSamp, lByte;
   if (AVIStreamWrite (m_pAVIStreamVideo, m_dwCurFrame, 1, m_memCompBuff.p, pbmi->biSizeImage,
      dwFlags, &lSamp, &lByte))
      return FALSE;

   // done
   return TRUE;
}




/****************************************************************************
CAnimWindow::SetRenderCamera - Sets the render camera based on the camera
specified in m_AI.gCamera
*/
void CAnimWindow::SetRenderCamera (void)
{
   if (IsEqualGUID(m_AI.gCamera, GUID_NULL))
      return; // no camera

   // get object
   PCObjectSocket pos;
   pos = m_AI.pWorld->ObjectGet(m_AI.pWorld->ObjectFind(&m_AI.gCamera));
   if (!pos)
      return;

   // get FOV from camera
   // BUGBUG - in future get more info from camera - such as after effects
   OSMANIMCAMERA oac;
   fp fFOV;
   memset (&oac, 0, sizeof(oac));
   pos->Message (OSM_ANIMCAMERA, &oac);
   fFOV = PI/4;
   if (oac.poac)
      fFOV = oac.poac->m_fFOV;
   fFOV = max(fFOV, .001);
   fFOV = min(fFOV, PI * .99);

   // set the exposure
   if (oac.poac) {
      if (m_fUseRay)
         m_pRay->ExposureSet (CM_LUMENSSUN / exp(oac.poac->m_fExposure));
      else
         m_pRender->ExposureSet (CM_LUMENSSUN / exp(oac.poac->m_fExposure));
   }

   // get the matrix
   CMatrix m;
   pos->ObjectMatrixGet (&m);

   // since we are looking at 0,-1,0 but the normal camera looks at 0,1,0,
   // invert y
   CMatrix mFlip;
   mFlip.Scale (-1, -1, 1);   // BUGFIX - Flip more than just y
   m.MultiplyLeft (&mFlip);

   // convert this
   // CMatrix mInv;
   // CPoint pCenter;
   fp fLong, fLat, fTilt; //, f2;
   // fp fZ, fX, fY;
   // m.Invert4 (&mInv);
   CPoint p, p2;
   p.Zero();
   p.MultiplyLeft (&m);

   m.ToXYZLLT (&p2, &fLong, &fLat, &fTilt);
   if (m_fUseRay)
      m_pRay->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);
   else
      m_pRender->CameraPerspWalkthrough (&p, fLong, fLat, fTilt, fFOV);

}

/****************************************************************************
CAnimWindow::UpdateBitmap - Call this when a new image has been filled
into pImage. This will create a bitmap out of it and display that bitmap

pImage is assumed to be the same size as m_ImageDown
*/
void CAnimWindow::UpdateBitmap (PCImage pImage)
{
   if (m_hBmpDisplay)
      DeleteObject (m_hBmpDisplay);

   HDC hDC;
   hDC = GetDC (m_hWndAnim);
   m_hBmpDisplay = pImage->ToBitmap (hDC);
   ReleaseDC (m_hWndAnim, hDC);

   InvalidateRect (m_hWndViewport, NULL, FALSE);
   UpdateWindow (m_hWndViewport);
}


/****************************************************************************
CAnimWindow::UpdateBitmap - Call this when a new image has been filled
into pImage. This will create a bitmap out of it and display that bitmap

pImage is assumed to be the same size as m_ImageDown
*/
void CAnimWindow::UpdateBitmap (PCFImage pImage)
{
   if (m_hBmpDisplay)
      DeleteObject (m_hBmpDisplay);

   HDC hDC;
   hDC = GetDC (m_hWndAnim);
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_hBmpDisplay = pImage->ToBitmap (hDC);
	MALLOCOPT_RESTORE;
   ReleaseDC (m_hWndAnim, hDC);

   InvalidateRect (m_hWndViewport, NULL, FALSE);
   UpdateWindow (m_hWndViewport);
}


/****************************************************************************
CAnimWindow::UpdateThisDisplay - Updates the display of the this window so that
it displays the right progress.

inputs
   fp       fProgress - from 0 to 1
*/
void CAnimWindow::UpdateThisDisplay (fp fProgress)
{
   // progress
   fProgress = max(0,fProgress);
   fProgress = min(1,fProgress);
   fProgress *= 1000;
   SendMessage(m_hWndProgressThis, PBM_SETPOS, (DWORD) fProgress, 0);
   UpdateWindow (m_hWndProgressThis);
}

/****************************************************************************
CAnimWindow::UpdateTotalDisplay - Updates the display for the total number of frames
used so far.
*/
void CAnimWindow::UpdateTotalDisplay (void)
{
   // progress
   if (m_hWndProgressAll) {
      SendMessage(m_hWndProgressAll, PBM_SETPOS, m_dwCurFrame, 0);
      UpdateWindow (m_hWndProgressAll);
   }

   // progress text
   char szTemp[128];
   if (m_hWndStaticAll) {
      sprintf (szTemp, "Total: (%d/%d frames)", (int) m_dwCurFrame+1, (int) m_dwFrames);
      SetWindowText (m_hWndStaticAll, szTemp);
      InvalidateRect (m_hWndStaticAll, NULL, FALSE);
      UpdateWindow (m_hWndStaticAll);
   }

   // what's left
   if (m_dwCurFrame && (m_dwCurFrame <= m_dwFrames) && m_fTimeUsedForFrames) {
      fp fLeft = (fp) (m_dwFrames - m_dwCurFrame) * m_fTimeUsedForFrames / (fp)m_dwCurFrame;
      BOOL fShow = FALSE;

      szTemp[0] = 0;
      if (fShow || (fLeft >= 60.0 * 60.0)) {
         sprintf (szTemp + strlen(szTemp), "%d hr ", (int) (fLeft / 60.0 / 60.0));
         fLeft = myfmod(fLeft, 60.0 * 60.0);
         fShow = TRUE;
      }
      if (fShow || (fLeft >= 60.0)) {
         sprintf (szTemp + strlen(szTemp), "%d min ", (int) (fLeft / 60.0));
         fLeft = myfmod(fLeft, 60.0);
         fShow = TRUE;
      }

      // seconds
      sprintf (szTemp + strlen(szTemp), "%d sec", (int) fLeft);
   }
   else
      strcpy (szTemp, "");
   if (m_hWndStaticLeftInfo) {
      SetWindowText (m_hWndStaticLeftInfo, szTemp);
      InvalidateRect (m_hWndStaticLeftInfo, NULL, FALSE);
      UpdateWindow (m_hWndStaticLeftInfo);
   }
}



/****************************************************************************
CAnimWindow::IntegrateImage - Takes an image from the renderer and (if necessary)
integrates it with the motion blur image. THis also updates the display.

inputs
   BOOL        fFinal - If TRUE this is for the final image, else it's for a temporary
               image caused by CProgressSocket::CanRedraw()
*/
void CAnimWindow::IntegrateImage (BOOL fFinal)
{
   DWORD dwNum = m_AI.dwExposureStrobe;
   PCImage pScratch = fFinal ? &m_ImageMotion : &m_ImageMotionScratch;
   PCFImage pFScratch = fFinal ? &m_FImageMotion : &m_FImageMotionScratch;

   // downsample
   if (m_fFloatImage)   // BUGFIX - Was m_fImageRay
      m_FImageRenderTo.Downsample (&m_FImageDown, m_AI.dwAnti);
   else
      m_ImageRenderTo.Downsample (&m_ImageDown, m_AI.dwAnti);

   // if motion blur and first time then just copy the downsample into
   // the motion blur memory
   if (dwNum >= 2) {
      if (!m_dwStrobe) {
         if (m_fFloatImage) // BUGFIX - Was m_fImageRay
            m_FImageDown.Downsample (pFScratch, 1);  // use this as copy
         else
            m_ImageDown.Downsample (pScratch, 1);  // use this as copy
      }
      else if (!fFinal) {
         // if this isn't the first time (strobe > 0) and we're drawing a scratch
         // image, then copy into scratch
         if (m_fFloatImage) // BUGFIX - Was m_fImageRay
            m_FImageMotion.Downsample (pFScratch, 1);
         else
            m_ImageMotion.Downsample (pScratch, 1);
      }
         // do this so that will have some object info


      // need to add and scale
      DWORD x, dwMax;
      if (m_fFloatImage) { // BUGFIX - Was m_fUseRay
         PFIMAGEPIXEL pip, pip2;
         dwMax = m_FImageDown.Width() * m_FImageDown.Height();
         pip = m_FImageDown.Pixel(0,0);
         pip2 = pFScratch->Pixel(0,0);
         if (!m_dwStrobe) {
            // scale down the one just added
            for (x = 0; x < dwMax; x++, pip2++) {
               pip2->fRed /= (fp)dwNum;
               pip2->fGreen /= (fp)dwNum;
               pip2->fBlue /= (fp)dwNum;
            } // x
         }
         else {
            // add in new one
            for (x = 0; x < dwMax; x++, pip++, pip2++) {
               pip2->fRed += pip->fRed / (fp)dwNum;
               pip2->fGreen += pip->fGreen / (fp)dwNum;
               pip2->fBlue += pip->fBlue / (fp)dwNum;
            }  // x
         }
      }
      else { // not ray
         PIMAGEPIXEL pip, pip2;
         dwMax = m_ImageDown.Width() * m_ImageDown.Height();
         pip = m_ImageDown.Pixel(0,0);
         pip2 = pScratch->Pixel(0,0);
         if (!m_dwStrobe) {
            // scale down the one just added
            for (x = 0; x < dwMax; x++, pip2++) {
               pip2->wRed /= dwNum;
               pip2->wGreen /= dwNum;
               pip2->wBlue /= dwNum;
            } // x
         }
         else {
            // add in new one
            for (x = 0; x < dwMax; x++, pip++, pip2++) {
               pip2->wRed += pip->wRed / dwNum;
               pip2->wGreen += pip->wGreen / dwNum;
               pip2->wBlue += pip->wBlue / dwNum;
            }  // x
         }
      } // else if useray

      // update display - except for last time, since will update that
      // in a sec anyway
      if (m_dwStrobe + 1 < dwNum) {
         if (m_fFloatImage) // BUGFIX - Was m_fUseRay
            UpdateBitmap(pFScratch);
         else
            UpdateBitmap(pScratch);
      }
   } // if dwNum >= 2

   // if we're going to be finished with this image then write into image down
   if (m_dwStrobe+1 >= dwNum) {
      // might want to update imagedown
      if (dwNum >= 2) {
         if (m_fFloatImage) // BUGFIX - Was m_fUseRay
            pFScratch->Downsample (&m_FImageDown, 1);   // use this as a quick copy
         else
            pScratch->Downsample (&m_ImageDown, 1);   // use this as a quick copy
      }

      // update view
      if (m_fFloatImage) // BUGFIX - Was m_fUseRay
         UpdateBitmap(&m_FImageDown);
      else
         UpdateBitmap(&m_ImageDown);
   }
}


/****************************************************************************
CAnimWindow::CalcAudio - Calculates the audio for the frame (if this needs to be done)
*/
void CAnimWindow::CalcAudio (void)
{
   if (!m_pWFEX)
      return;  // nothing

   // how large is this buffer
   DWORD dwSub, dwSamples, dwSmall, dwExtra;
   dwSub = m_dwCurFrame % m_AI.dwFPS;
   dwSmall = dwSamples = m_pWFEX->nSamplesPerSec / m_AI.dwFPS;
   dwExtra = m_pWFEX->nSamplesPerSec - dwSamples * m_AI.dwFPS;
   if (dwSub < dwExtra)
      dwSamples++;   // add an extra one so get all accounted for

   // which time and sample is it
   int iTimeSec, iTimeSamp;
   iTimeSec = (int)floor(m_AI.fTimeStart);
   iTimeSamp = (int)((m_AI.fTimeStart - (fp)iTimeSec) * (fp)m_pWFEX->nSamplesPerSec);
   iTimeSec += (int)(m_dwCurFrame / m_AI.dwFPS);
   iTimeSamp += (int)(dwSmall * dwSub + min(dwSub,dwExtra));
   // NOTE: Assuming that AVI files show the first frame EXACTLY at sample number 0,
   // and continue this. If not, I'd need to add the following in
   //iTimeSamp -= (int)dwSmall/2;  // so that doing 1/2 frame before and after image

   // zero it out
   memset (m_pafAudioSum, 0, dwSamples * m_pWFEX->nChannels * sizeof(float));

   // loop through all the objects
   DWORD i, j, k;
   PCSceneObj pso;
   PCAnimSocket pas;
   fp fVolume;
   for (i = 0; i < m_AI.pScene->ObjectNum(); i++) {
      pso = m_AI.pScene->ObjectGet (i);
      if (!pso)
         continue;
      for (j = 0; j < pso->ObjectNum(); j++) {
         pas = pso->ObjectGet (j);
         if (!pas)
            continue;
         if (!pas->WaveGet (m_pWFEX->nSamplesPerSec, m_pWFEX->nChannels, iTimeSec, iTimeSamp,
            dwSamples, m_pasAudioBuf, &fVolume))
            continue;   // nothing

         // else, add in
         for (k = 0; k < dwSamples * m_pWFEX->nChannels; k++) {
            m_pafAudioSum[k] += (fp)m_pasAudioBuf[k] * fVolume;
         } // k
      } // j
   } // i

   // store away
   m_dwAudioSamples = dwSamples;
   for (i = 0; i < dwSamples * m_pWFEX->nChannels; i++) {
      m_pafAudioSum[i] = max(m_pafAudioSum[i], -32768);
      m_pafAudioSum[i] = min(m_pafAudioSum[i], 32767);
      m_pasAudioBuf[i] = (short) m_pafAudioSum[i];
   } // i
   // done
}

/****************************************************************************
CAnimWindow::WndProc - Window procedure
*/
LRESULT CAnimWindow::WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {

      case WM_CLOSE:
         // wont work if gfCanCancel == FALSE, but sets gfWantCancel...
         if (m_fCantQuit) {
            m_fWantToQuit = TRUE;
            EnableWindow (m_hWndButtonCancel, FALSE);
            return 0;
         }

         delete this;
         return 0;

      case WM_COMMAND:
         switch (LOWORD(wParam)) {
         case IDC_CANCEL:
            // just send a close mssage
            SendMessage (hWnd, WM_CLOSE, 0, 0);
            return 0;

         case IDC_PAUSE:
            {
               // remember how much time used
               DWORD dwTime;
               dwTime = GetTickCount();
               if (!m_fPaused && (dwTime > m_dwLastTimeSum))
                  m_fTimeUsedForFrames += (fp) (dwTime - m_dwLastTimeSum) / 1000.0;
               m_dwLastTimeSum = dwTime;

               // set the state of the button
               m_fPaused = !m_fPaused;
               SetWindowText (m_hWndButtonPause, m_fPaused ? "Resume" : "Pause");
            }
            return 0;

         }

         break;

      case WM_CREATE:
         // two buttons
         m_hWndButtonPause = CreateWindow ("BUTTON", "Pause",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0,0,10,10, hWnd, (HMENU) IDC_PAUSE, ghInstance, NULL);
         m_hWndButtonCancel = CreateWindow ("BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0,0,10,10, hWnd, (HMENU) IDC_CANCEL, ghInstance, NULL);

         // three statics
         if (m_dwFrames >= 2)
            m_hWndStaticAll = CreateWindow ("STATIC", "StaticAll",
               WS_CHILD | WS_VISIBLE,
               0,0,10,10, hWnd, (HMENU) IDC_STATICALL, ghInstance, NULL);
         else
            m_hWndStaticAll = NULL;
         m_hWndStaticThis = CreateWindow ("STATIC", "Current frame:",
            WS_CHILD | WS_VISIBLE,
            0,0,10,10, hWnd, (HMENU) IDC_STATICTHIS, ghInstance, NULL);

         if (m_dwFrames >= 2) {
            m_hWndStaticLeft = CreateWindow ("STATIC", "Estimated time left:",
               WS_CHILD | WS_VISIBLE,
               0,0,10,10, hWnd, (HMENU) IDC_STATICLEFT, ghInstance, NULL);
            m_hWndStaticLeftInfo = CreateWindow ("STATIC", "StaticLeft",
               WS_CHILD | WS_VISIBLE,
               0,0,10,10, hWnd, (HMENU) IDC_STATICLEFT, ghInstance, NULL);
         }
         else
            m_hWndStaticLeft = m_hWndStaticLeftInfo = NULL;

         // progress bars
         if (m_dwFrames >= 2)
            m_hWndProgressAll = CreateWindow (PROGRESS_CLASS, "",
               WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
               0,0,10,10, hWnd, (HMENU) IDC_PROGRESSALL, ghInstance, NULL);
         else
            m_hWndProgressAll = NULL;
         m_hWndProgressThis = CreateWindow (PROGRESS_CLASS, "",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            0,0,10,10, hWnd, (HMENU) IDC_PROGRESSTHIS, ghInstance, NULL);

         // Set min/max in progress bars
         if (m_hWndProgressAll)
            SendMessage(m_hWndProgressAll, PBM_SETRANGE, 0, MAKELPARAM(0, m_dwFrames));
         SendMessage(m_hWndProgressThis, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));

         // viewport for drawing bitmap
         m_hWndViewport = CreateWindowEx (WS_EX_CLIENTEDGE, "AnimSceneImage", "",
            WS_CHILD | WS_VISIBLE,
            0,0,10,10, hWnd, NULL, ghInstance, this);

         // update current progress
         UpdateTotalDisplay();
         UpdateThisDisplay(0);

         // remember this time
         m_dwLastTimeSum = GetTickCount();

         // send a message so renders next frame
         SetTimer (hWnd, TIMER_BETWEEN, TIMERBETWEENTIME, 0);
         m_fTimerBetween = TRUE;
         break;

      case WM_TIMER:
         if (wParam == TIMER_BETWEEN) {
            if (m_fWantToQuit && !m_fCantQuit) {
               // all done.
               delete this;
               return 0;
            }

            // if we're paused then just return and come back a bit later and
            // see if pause has changed
            if (m_fPaused)
               return 0;

            KillTimer (hWnd, TIMER_BETWEEN);
            m_fTimerBetween = FALSE;
            PostMessage (hWnd, WM_USER+82, 0,0);
         }
         break;

      case WM_USER+82:  // render a new frame
         {
            // BUGFIX - Was m_dwCurFrame > m_dwFrames, but generated 1 too many frames
            if ((m_fWantToQuit && !m_fCantQuit) || (m_dwCurFrame >= m_dwFrames)) {
               // all done.
               delete this;
               return 0;
            }

            // set flag indicating that processing
            m_fCantQuit = TRUE;

            // clear out the progress settings
            m_dwLastUpdateTime = m_dwStartTime = GetTickCount();
            m_fProgress = 0;
            m_lStack.Clear();
            UpdateThisDisplay (m_fProgress);

            // calculate the audio
            CalcAudio ();

            // figure out the amount of motion blur
            DWORD dwNum;
            DWORD dwStartTime;
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            HANDLE hProcess = GetCurrentProcess();

            dwNum = m_AI.dwExposureStrobe;
            if ((dwNum >= 2) && (m_dwFrames <= 1)) {
               dwStartTime = GetTickCount();
               GetProcessTimes (hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser);
            }
            for (m_dwStrobe = 0; (m_dwStrobe < dwNum) && !m_fWantToQuit; m_dwStrobe++) {
               // time accounting for strobe
               fp fTime = m_fCurFrame - m_AI.fExposureTime / 2;
               if (dwNum >= 2) {
                  Push ((fp)m_dwStrobe / (fp)dwNum, (fp)(m_dwStrobe+1) / (fp)dwNum);
                  fTime += (fp)m_dwStrobe / (fp)(dwNum-1) * m_AI.fExposureTime;
                  fTime = max(fTime, 0);
               }

               // update world to slider
               if (m_AI.pSceneSet)
                  m_AI.pSceneSet->StateSet (m_AI.pScene, fTime);

               // Update renderer's camera position and intensity
               SetRenderCamera ();

               // progress bar less if post-processing
               if (m_pEffect)
                  Push (0, 0.9);

               if ((dwNum <= 1) && (m_dwFrames <= 1)) {
                  dwStartTime = GetTickCount();
                  GetProcessTimes (hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser);
               }

               // draw
               if (m_fUseRay)
                  m_pRay->Render (m_dwWorldChanged, NULL, this);
               else
                  m_pRender->Render (m_dwWorldChanged, NULL, this);
               m_dwWorldChanged = 0;

               // write to regitry
               if ((dwNum <= 1) && (m_dwFrames <= 1)) {
                  dwStartTime = GetTickCount() - dwStartTime;
                  KeySet ("Last image time(ms)", dwStartTime);

                  // and CPU used
                  FILETIME ftCreation2, ftExit2, ftKernel2, ftUser2;
                  GetProcessTimes (hProcess, &ftCreation2, &ftExit2, &ftKernel2, &ftUser2);
                  __int64 iTimeTotal = *((__int64*)&ftKernel2) + *((__int64*)&ftUser2) -
                     *((__int64*)&ftKernel) - *((__int64*)&ftUser);
                  iTimeTotal /= 10000; // 000;
                  DWORD dwCPUTime = (DWORD)iTimeTotal;
                  KeySet ("Last image CPU time(ms)", dwCPUTime);

#ifdef _DEBUG
                  char szTemp[128];
                  sprintf (szTemp, "RENDER TIME: %f sec\r\n", (double) dwStartTime / 1000.0);
                  OutputDebugString (szTemp);
                  sprintf (szTemp, "RENDER CPU TIME: %f sec\r\n", (double) dwCPUTime / 1000.0);
                  OutputDebugString (szTemp);
#endif
               }

               if (m_pEffect) {
                  Pop ();
                  Push (0.9, 1);

                  // get the effect and draw it...
                  if (m_fFloatImage) // BUGFIX - Was m_fUseRay
                     m_pEffect->Render (&m_FImageRenderTo, m_fUseRay ? (PCRenderSuper)m_pRay : (PCRenderSuper)m_pRender, m_AI.pWorld, TRUE, this);
                        // BUGFIX - Was always passing into m_pRay
                  else
                     m_pEffect->Render (&m_ImageRenderTo, m_pRender, m_AI.pWorld, TRUE, this);

                  Pop();
               }
               IntegrateImage (TRUE);

               if (dwNum >= 2)
                  Pop(); // undo the progress push
            } // over m_dwStrobe

            // write it out
            BOOL fRet;
            fRet = WriteFrame ();
            
            // if error then ask if want to quit
            if (!fRet) {
               int iRet;
               iRet = EscMessageBox (this->m_hWndAnim, ASPString(),
                  L"The frame didn't write. Do you want to continue?",
                  L"Writing the frame to disk failed, perhaps because the file "
                  L"is write protected. Id you press Yes the next frame will be "
                  L"written, otherwise the animation will quit.",
                  MB_ICONQUESTION | MB_YESNO);
               if (iRet != IDYES)
                  m_fWantToQuit = TRUE;
            }

            // unset flag indicating that processing
            m_fCantQuit = FALSE;

            // remember how much time used
            DWORD dwTime;
            dwTime = GetTickCount();
            if (dwTime > m_dwLastTimeSum)
               m_fTimeUsedForFrames += (fp) (dwTime - m_dwLastTimeSum) / 1000.0;
            m_dwLastTimeSum = dwTime;

            // increment
            m_dwCurFrame++;
            m_fCurFrame += 1.0 / (fp) m_AI.dwFPS;
            UpdateTotalDisplay();
            SetTimer (hWnd, TIMER_BETWEEN, TIMERBETWEENTIME, 0);
            m_fTimerBetween = TRUE;
         }
         return 0;;

      case WM_DESTROY:
         if (m_fTimerBetween)
            KillTimer (hWnd, TIMER_BETWEEN);
         break;

      case WM_SIZE:
         {
            RECT r;
            GetClientRect (hWnd, &r);

            // define unit
            int iUnit = 20;
            int iAll = (m_hWndStaticAll ? iUnit : 0);

            // buttons
            MoveWindow (m_hWndButtonPause, r.right - iUnit * 5,r.top + iUnit, 
               iUnit * 4, iUnit * 3/2, TRUE);
            MoveWindow (m_hWndButtonCancel, r.right - iUnit * 5,r.top + iUnit*3, 
               iUnit * 4, iUnit * 3/2, TRUE);
            r.right -= iUnit * 6;

            // text
            if (m_hWndStaticAll)
               MoveWindow (m_hWndStaticAll, r.left + iUnit, r.top + iUnit,
                  iUnit * 10, iUnit, TRUE);
            MoveWindow (m_hWndStaticThis, r.left + iUnit, r.top + iUnit + iAll,
               iUnit * 10, iUnit, TRUE);
            if (m_hWndStaticLeft)
               MoveWindow (m_hWndStaticLeft, r.left + iUnit, r.top + iUnit*2 + iAll,
                  iUnit * 10, iUnit, TRUE);
            r.left += iUnit * 12;

            // progress
            if (m_hWndProgressAll)
               MoveWindow (m_hWndProgressAll, r.left, r.top + iUnit+2,
                  r.right - r.left, iUnit-4, TRUE);
            if (m_hWndStaticLeftInfo)
               MoveWindow (m_hWndStaticLeftInfo, r.left, r.top + iUnit*2 + iAll,
                  r.right - r.left, iUnit, TRUE);
            MoveWindow (m_hWndProgressThis, r.left, r.top + iUnit + iAll +2,
               r.right - r.left, iUnit-4, TRUE);

            // diplay
            GetClientRect (hWnd, &r);
            r.top += iUnit * 6;
            r.bottom -= iUnit;
            r.left += iUnit;
            r.right -= iUnit;
            int iCX, iCY, iX, iY;
            iCX = (r.right + r.left) /2;
            iCY = (r.bottom + r.top) / 2;
            iX = r.right - r.left - 4;
            iY = r.bottom - r.top - 4;
            iX = max(2, iX);
            iY = max(2, iY);
            iX = min(iX, (int)m_AI.dwPixelsX);
            iY = min(iY, (int)m_AI.dwPixelsY);
            if ((iX < (int)m_AI.dwPixelsX) || (iY < (int)m_AI.dwPixelsY)) {
               fp fScale = 1;
               if (iX < (int)m_AI.dwPixelsX)
                  fScale = min(fScale, (fp)iX / (fp)m_AI.dwPixelsX);
               if (iY < (int)m_AI.dwPixelsY)
                  fScale = min(fScale, (fp)iY / (fp)m_AI.dwPixelsY);
               iX = (int) ((fp)m_AI.dwPixelsX * fScale);
               iY = (int) ((fp)m_AI.dwPixelsY * fScale);
            }
            iX += 4;
            iY += 4;
            MoveWindow (m_hWndViewport, iCX - iX/2, iCY - iY/2, iX, iY, TRUE);
         }
         break;

   };

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/******************************************************************************
CAnimWindow::WorldAboutToChange - Callback from world
*/
void CAnimWindow::WorldAboutToChange (GUID *pgObject)
{
   // do nothing
}

/******************************************************************************
CAnimWindow::WorldChanged - Callback from world
*/
void CAnimWindow::WorldChanged (DWORD dwChanged, GUID *pgObject)
{
   m_dwWorldChanged |= dwChanged;
}

/******************************************************************************
CAnimWindow::WorldUndoChanged - Callback from world
*/
void CAnimWindow::WorldUndoChanged (BOOL fUndo, BOOL fRedo)
{
   // ignore
}

/******************************************************************************
CAnimWindow::Push - Pushes a new minimum and maximum for the progress meter
so that any code following will think min and max are going from 0..1, but it
might really be going from .2 to .6 (if that's what fMin and fMax) are.

inputs
   fp      fMin,fMax - New min and max, from 0..1. (Afected by previous min and
                  max already on stack.
reutrns
   BOOL - TRUE if success
*/
BOOL CAnimWindow::Push (float fMin, float fMax)
{
   PTEXTUREPOINT ptp = m_lStack.Num() ? (PTEXTUREPOINT) (m_lStack.Get(m_lStack.Num()-1)) : NULL;

   if (ptp) {
      fMin = fMin * (ptp->v - ptp->h) + ptp->h;
      fMax = fMax * (ptp->v - ptp->h) + ptp->h;
   }

   TEXTUREPOINT tp;
   tp.h = fMin;
   tp.v = fMax;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_lStack.Add (&tp);
	MALLOCOPT_RESTORE;

   return TRUE;
}

/******************************************************************************
CAnimWindow::Pop - Removes the last changes for percent complete off the stack -
as added by Push()
*/
BOOL CAnimWindow::Pop (void)
{
   if (!m_lStack.Num())
      return FALSE;
   m_lStack.Remove (m_lStack.Num()-1);
   return TRUE;
}

/******************************************************************************
CAnimWindow::Update - Updates the progress bar. Should be called once in awhile
by the code.

inputs
   fp         fProgress - Value from 0..1 for the progress from 0% to 100%
returns
   int - 1 if in the future give progress indications more often (which means they
         were too far apart), 0 for the same amount, -1 for less often
*/
int  CAnimWindow::Update (float fProgress)
{
   // check to make sure we actually started
   if (!m_dwStartTime)
      return 0;

   // get the current time
   DWORD dwTime;
   int   iRet; // what should return
   dwTime = GetTickCount();
   if (dwTime - m_dwLastUpdateTime > 500)
      iRet = 1;
   else if (dwTime - m_dwLastUpdateTime < 100)
      iRet = -1;
   else
      iRet = 0;
   m_dwLastUpdateTime = dwTime;

   // progress
   if (m_lStack.Num()) {
      PTEXTUREPOINT ptp = (PTEXTUREPOINT) m_lStack.Get(m_lStack.Num()-1);
      fProgress = fProgress * (ptp->v - ptp->h) + ptp->h;
   }

   if (fabs(fProgress - m_fProgress) > .02) {
      m_fProgress = fProgress;
      UpdateThisDisplay (m_fProgress);
   }
   // BUGFIX - Dont do m_fProgress since if update very slowly will never move slider
   //else {
      // just remember this and do nothing else
   //   m_fProgress = fProgress;
   //}

   // go through messages so user can click and whatnot
   MSG msg;
   while (!m_fWantToQuit) {
      while( (m_fPaused && !m_fWantToQuit) ? GetMessage (&msg, NULL, 0, 0) : PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
         ASPHelpCheckPage ();
      }

      if (!m_fPaused)
         break;
   }

   return iRet;
}




/****************************************************************************
CAnimWindow::ImageWndProc - Window procedure - for drawing the image
*/
LRESULT CAnimWindow::ImageWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
   case WM_PAINT:
      {
         PAINTSTRUCT ps;
         HDC hDC = BeginPaint (hWnd, &ps);
         RECT r;
         GetClientRect (hWnd, &r);
         if (m_hBmpDisplay) {
            HDC hNew;
            hNew = CreateCompatibleDC (hDC);
            SelectObject (hNew, m_hBmpDisplay);
            // BUGFIX - Changed from "<" to ">"
            // BUGFIX - Changed back because busted, but moved +4 to other side
            if ((r.right+4 < (int)m_AI.dwPixelsX+4) || (r.bottom+4 < (int)m_AI.dwPixelsY)) {
               SetStretchBltMode (hDC, COLORONCOLOR); // BUGFIX - So stretched bitmap looks better
               StretchBlt (hDC, 0, 0, r.right, r.bottom, hNew, 0, 0,
                  (int) m_AI.dwPixelsX, (int)m_AI.dwPixelsY, SRCCOPY);
            }
            else
               BitBlt (hDC, 0, 0, r.right, r.bottom, hNew, 0, 0, SRCCOPY);
            DeleteDC (hNew);
         }
         else {
            HBRUSH hbr = CreateSolidBrush (RGB(0x20,0x20,0x20));
            FillRect (hDC, &r, hbr);
            DeleteObject (hbr);
         }
         EndPaint (hWnd, &ps);
      }
      return 0;
   }

   return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/****************************************************************************
AIScenePage
*/
BOOL AIScenePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PANIMINFO pai = (PANIMINFO) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list boxes
         PCEscControl pControl;
         DWORD i;
         ESCMLISTBOXADD lba;

         // scene
         PCScene pSceneCur;
         pSceneCur = NULL;
         pai->pSceneSet->StateGet (&pSceneCur, NULL);
         pControl = pPage->ControlFind (L"scene");
         for (i = 0; i < pai->pSceneSet->SceneNum(); i++) {
            PCScene pScene = pai->pSceneSet->SceneGet(i);
            if (!pScene)
               continue;

            PWSTR pszName;
            pszName = pScene->NameGet ();
            if (!pszName || !pszName[0])
               pszName = L"Unnamed";
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = pszName;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set selection?
            if (!i || (pScene == pSceneCur)) {
               pControl->AttribSetInt (CurSel(), i);
               pai->pScene = pScene;
            }
         }

         // camera
         GUID  gCurCamera;
         pControl = pPage->ControlFind (L"camera");
         if (!pai->pSceneSet->CameraGet (&gCurCamera))
            memset (&gCurCamera, 0, sizeof(gCurCamera));
         CListFixed lCamera;
         lCamera.Init (sizeof(OSMANIMCAMERA));
         PCScene pScene;
         pScene = pai->pSceneSet->SceneGet(0);
         pScene->CameraEnum(&lCamera);

         // add the items
         POSMANIMCAMERA pc;
         GUID g;
         BOOL fSet;
         pc = (POSMANIMCAMERA) lCamera.Get(0);
         fSet = FALSE;
         for (i = 0; i < lCamera.Num(); i++, pc++) {
            PWSTR psz = pc->poac->StringGet(OSSTRING_NAME);
            if (!psz)
               psz = L"Unnnamed";
            pc->poac->GUIDGet (&g);
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = psz;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            // set selection?
            if (IsEqualGUID (g, gCurCamera)) {
               pControl->AttribSetInt (CurSel(), i);
               fSet = TRUE;
               pai->gCamera = g;
            }
         }  // i

         // add in the render camera
         if (pai->pRender) {
            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszText = L"View camera";
            pControl->Message (ESCM_LISTBOXADD, &lba);

            if (!fSet) {
               pControl->AttribSetInt (CurSel(), i); // i = lCamera.NUm()
               fSet = TRUE;
               memset (&pai->gCamera, 0, sizeof(pai->gCamera));
            }
         }
         if (!fSet) {
            pControl->AttribSetInt (CurSel(), 0);
            pc = (POSMANIMCAMERA) lCamera.Get(0);
            pc->poac->GUIDGet (&pai->gCamera);
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         if (!p->pControl->m_pszName)
            break;   // no name

         if (!_wcsicmp(p->pControl->m_pszName, L"scene")) {
            PCScene pScene = pai->pSceneSet->SceneGet (p->dwCurSel);
            if (pScene)
               pai->pScene = pScene;
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"camera")) {
            CListFixed lCamera;
            lCamera.Init (sizeof(OSMANIMCAMERA));
            PCScene pScene;
            pScene = pai->pSceneSet->SceneGet(0);
            pScene->CameraEnum(&lCamera);

            POSMANIMCAMERA pc;
            pc = (POSMANIMCAMERA) lCamera.Get(p->dwCurSel);
            if (pc)
               pc->poac->GUIDGet (&pai->gCamera);
            else
               memset (&pai->gCamera, 0, sizeof(pai->gCamera));
            break;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Animation scene and camera";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}




/****************************************************************************
AITimePage
*/
BOOL AITimePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PANIMINFO pai = (PANIMINFO) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fll in the items
         PCEscControl pControl;
         DWORD i;
         ESCMLISTBOXADD lba;

         // which radio button is checked
         if ((pai->fTimeStart == 0) && (pai->fTimeEnd == pai->pScene->DurationGet()))
            pControl = pPage->ControlFind (L"rball");
         else
            pControl = pPage->ControlFind (L"rbtime");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), TRUE);

         // set the other controls
         DoubleToControl (pPage, L"timeedit1", pai->fTimeStart);
         DoubleToControl (pPage, L"timeedit2", pai->fTimeEnd);
         DWORD dwFPS;
         dwFPS = pai->pScene->FPSGet();
         if (dwFPS) {
            DoubleToControl (pPage, L"frameedit1", floor((double)pai->fTimeStart * (double) dwFPS + .5));
            DoubleToControl (pPage, L"frameedit2", floor((double)pai->fTimeEnd * (double) dwFPS + .5));
         }
         else {
            pControl = pPage->ControlFind (L"frameedit1");
            if (pControl)
               pControl->Enable(FALSE);
            pControl = pPage->ControlFind (L"frameedit2");
            if (pControl)
               pControl->Enable(FALSE);
            pControl = pPage->ControlFind (L"rbframe");
            if (pControl)
               pControl->Enable(FALSE);
         }

         // bookmarks
         CListFixed lBookmark;
         POSMBOOKMARK pb;
         lBookmark.Init (sizeof(OSMBOOKMARK));

         // over time
         lBookmark.Clear();
         pai->pScene->BookmarkEnum (&lBookmark, FALSE, TRUE);
         if (lBookmark.Num()) {
            pControl = pPage->ControlFind (L"bookmarkrange");
            pb = (POSMBOOKMARK) lBookmark.Get(0);

            for (i = 0; i < lBookmark.Num(); i++, pb++) {
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pb->szName;
               pControl->Message (ESCM_LISTBOXADD, &lba);
               }

            // set selection?
            pControl->AttribSetInt (CurSel(), 0);
         }
         else {
            // diable
            pControl = pPage->ControlFind (L"rbbookmarkrange");
            if (pControl)
               pControl->Enable(FALSE);
         }

         // zero-length bookmarks
         PCEscControl pControl2;
         lBookmark.Clear();
         pai->pScene->BookmarkEnum (&lBookmark, TRUE, FALSE);
         if (lBookmark.Num()) {
            pControl = pPage->ControlFind (L"bookmarkzero1");
            pControl2 = pPage->ControlFind (L"bookmarkzero2");
            pb = (POSMBOOKMARK) lBookmark.Get(0);

            for (i = 0; i < lBookmark.Num(); i++, pb++) {
               memset (&lba, 0, sizeof(lba));
               lba.dwInsertBefore = -1;
               lba.pszText = pb->szName;
               pControl->Message (ESCM_LISTBOXADD, &lba);
               pControl2->Message (ESCM_LISTBOXADD, &lba);
               }

            // set selection?
            pControl->AttribSetInt (CurSel(), 0);
            pControl2->AttribSetInt (CurSel(), 0);
         }
         else {
            // diable
            pControl = pPage->ControlFind (L"rbbookmarkzero");
            if (pControl)
               pControl->Enable(FALSE);
         }

      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz || _wcsicmp(p->psz, gpszNext))
            break;   // only care about next

         // which radio button?
         DWORD dwButton;
         PCEscControl pControl;
         DWORD dwFPS;
         POSMBOOKMARK pb;
         CListFixed lBookmark;
         lBookmark.Init (sizeof(OSMBOOKMARK));
         dwFPS = pai->pScene->FPSGet();
         dwButton = 0;  // all
         if ((pControl = pPage->ControlFind (L"rbtime")) && pControl->AttribGetBOOL (Checked())) {
            dwButton = 1;  // time
         }
         else if ((pControl = pPage->ControlFind (L"rbframe")) && pControl->AttribGetBOOL (Checked())) {
            if (dwFPS)
               dwButton = 2;  // frame
         }
         else if ((pControl = pPage->ControlFind (L"rbbookmarkrange")) && pControl->AttribGetBOOL (Checked())) {
            pai->pScene->BookmarkEnum (&lBookmark, FALSE, TRUE);
            if (lBookmark.Num())
               dwButton = 3;  // bookmark range
         }
         else if ((pControl = pPage->ControlFind (L"rbbookmarkzero")) && pControl->AttribGetBOOL (Checked())) {
            pai->pScene->BookmarkEnum (&lBookmark, TRUE, FALSE);
            if (lBookmark.Num())
               dwButton = 4;  // bookmark time
         }
         
         // based on the type
         if (dwButton == 0) {
            pai->fTimeStart = 0;
            pai->fTimeEnd = pai->pScene->DurationGet();
         }
         else if (dwButton == 1) {
            // range
            pai->fTimeStart = DoubleFromControl (pPage, L"timeedit1");
            pai->fTimeEnd = DoubleFromControl (pPage, L"timeedit2");
         }
         else if (dwButton == 2) {
            // frames
            pai->fTimeStart = DoubleFromControl (pPage, L"frameedit1");
            pai->fTimeEnd = DoubleFromControl (pPage, L"frameedit2");
            pai->fTimeStart = pai->pScene->ApplyGrid ((double)pai->fTimeStart / (double)dwFPS);
            pai->fTimeEnd = pai->pScene->ApplyGrid ((double)pai->fTimeEnd / (double)dwFPS);
         }
         else if (dwButton == 3) {
            // continuous bookmark
            DWORD dwIndex;
            pControl = pPage->ControlFind (L"bookmarkrange");
            dwIndex = pControl ? (DWORD)pControl->AttribGetInt (CurSel()) : 0;
            pb = (POSMBOOKMARK) lBookmark.Get(dwIndex);
            if (pb) {
               pai->fTimeStart = pb->fStart;
               pai->fTimeEnd = pb->fEnd;
            }
         }
         else if (dwButton == 4) {
            // continuous bookmark
            DWORD dwIndex;
            pControl = pPage->ControlFind (L"bookmarkzero1");
            dwIndex = pControl ? (DWORD)pControl->AttribGetInt (CurSel()) : 0;
            pb = (POSMBOOKMARK) lBookmark.Get(dwIndex);
            if (pb)
               pai->fTimeStart = pb->fStart;

            pControl = pPage->ControlFind (L"bookmarkzero2");
            dwIndex = pControl ? (DWORD)pControl->AttribGetInt (CurSel()) : 0;
            pb = (POSMBOOKMARK) lBookmark.Get(dwIndex);
            if (pb)
               pai->fTimeEnd = pb->fStart;
         }

         // finally
         pai->fTimeStart = max(pai->fTimeStart, 0);
         pai->fTimeEnd = max(pai->fTimeStart, pai->fTimeEnd);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Animation timespan";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/****************************************************************************
AIQualityPage
*/
BOOL AIQualityPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PANIMINFO pai = (PANIMINFO) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fll in the items
         ComboBoxSet (pPage, L"quality", pai->dwQuality);
         DoubleToControl (pPage, L"width", pai->dwPixelsX);
         DoubleToControl (pPage, L"height", pai->dwPixelsY);
         ComboBoxSet (pPage, L"anti", pai->dwAnti);
         DoubleToControl (pPage, L"fps", pai->dwFPS);

         DoubleToControl (pPage, L"exposuretime", pai->fExposureTime);
         DoubleToControl (pPage, L"exposurestrobe", pai->dwExposureStrobe);

         // add to the combobox
         DWORD i;
         ICINFO icInfo;
         memset (&icInfo, 0, sizeof(icInfo));
         icInfo.dwSize = sizeof(icInfo);
         ESCMLISTBOXADD lba;
         PCEscControl pControl;
         BOOL fFound;
         fFound = FALSE;
         DWORD dwCompress;
         dwCompress = 0;
         pControl = pPage->ControlFind (L"compress");
         BITMAPINFOHEADER bmi;
         memset (&bmi, 0, sizeof(bmi));
         bmi.biSize = sizeof(bmi);
         bmi.biWidth = max(pai->dwPixelsX,1);
         bmi.biHeight = max(pai->dwPixelsY,1);
         bmi.biPlanes = 1;
         bmi.biBitCount = 32;
         bmi.biCompression = BI_RGB;
         bmi.biSizeImage = bmi.biWidth * bmi.biHeight * 4;
         for (i = 0; ICInfo (ICTYPE_VIDEO, i, &icInfo); i++) {
            HIC hIC = ICOpen (icInfo.fccType, icInfo.fccHandler, ICMODE_QUERY);
            if (!hIC)
               continue;

            // make sure it can compress
            if (ICERR_OK != ICCompressQuery (hIC, &bmi, NULL)) {
               ICClose (hIC);
               continue;
            }

            icInfo.dwSize = sizeof(ICINFO);
            ICGetInfo (hIC, &icInfo, sizeof(icInfo));

            // store compressor away
            dwCompress = icInfo.fccHandler;
            if (icInfo.fccHandler == pai->dwAVICompressor)
               fFound = TRUE;

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int) icInfo.fccHandler);
            MemCat (&gMemTemp, L"><bold>");
            MemCatSanitize (&gMemTemp, icInfo.szName);
            MemCat (&gMemTemp, L"</bold> - ");
            MemCatSanitize (&gMemTemp, icInfo.szDescription);
            MemCat (&gMemTemp, L"</elem>");

            memset (&lba, 0, sizeof(lba));
            lba.dwInsertBefore = -1;
            lba.pszMML = (PWSTR) gMemTemp.p;
            pControl->Message (ESCM_LISTBOXADD, &lba);

            ICClose (hIC);
         }

         // if not found choose one
         if (!fFound)
            pai->dwAVICompressor = dwCompress;

         ListBoxSet (pPage, L"compress", pai->dwAVICompressor);
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"compress")) {
            if (p->pszName)
               pai->dwAVICompressor = _wtoi(p->pszName);
            break;
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

         if (!_wcsicmp(psz, L"changeeffect")) {
            if (EffectSelDialog (pai->pWorld->RenderShardGet(), pPage->m_pWindow->m_hWnd, &pai->gEffectCode, &pai->gEffectSub))
               pPage->Exit (RedoSamePage());

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"quality")) {
            if (p->pszName)
               pai->dwQuality = _wtoi(p->pszName);
            return TRUE;
         }
         else if (!_wcsicmp(p->pControl->m_pszName, L"anti")) {
            if (p->pszName)
               pai->dwAnti = _wtoi(p->pszName);
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz || _wcsicmp(p->psz, gpszNext))
            break;   // only care about next

         pai->dwPixelsX = (DWORD) max(1,DoubleFromControl (pPage, L"width"));
         pai->dwPixelsY = (DWORD) max(1,DoubleFromControl (pPage, L"height"));
         pai->dwFPS = (DWORD) max(1,DoubleFromControl (pPage, L"fps"));
         pai->fExposureTime = max(0,DoubleFromControl (pPage, L"exposuretime"));
         pai->dwExposureStrobe = (DWORD) max(1, DoubleFromControl (pPage, L"exposurestrobe"));

         // get name to save as
         OPENFILENAME   ofn;
         char  szTemp[256];
         szTemp[0] = 0;
         memset (&ofn, 0, sizeof(ofn));
         char szInitial[256];
         GetLastDirectory(szInitial, sizeof(szInitial));
         // BUGBUG - Might want to save to directory where writing file
         ofn.lpstrInitialDir = szInitial;
         ofn.lStructSize = sizeof(ofn);
         ofn.hwndOwner = pPage->m_pWindow->m_hWnd;
         ofn.hInstance = ghInstance;
         ofn.lpstrFilter = 
            "Movie file (*.avi)\0*.avi\0"
            "JPEG file (single frame) (*.jpg)\0*.jpg\0"
            "Bitmap file (single frame) (*.bmp)\0*.bmp\0"
            "\0\0";
         ofn.lpstrFile = szTemp;
         ofn.nMaxFile = sizeof(szTemp);
         ofn.lpstrTitle = "Save movie file";
         ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
         ofn.lpstrDefExt = ".avi";
         // nFileExtension 
         if (!GetSaveFileName(&ofn))
            return TRUE;   // pressed cancel
         MultiByteToWideChar (CP_ACP, 0, szTemp, -1, pai->szFile, sizeof(pai->szFile)/2);
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Animation quality";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"EFFECTBITMAP")) {
            WCHAR szTemp[32];
            swprintf (szTemp, L"%lx", (__int64)pai->hBitEffect);

            MemZero (&gMemTemp);
            MemCat (&gMemTemp, szTemp);
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/***************************************************************************************
ACM callbacks */
// SPINFO - For waveformat choice page
typedef struct {
   PANIMINFO         pAI;           // pointer to animation info
   DWORD             dwSamplesPerSec;  // samples per sec chosen
   DWORD             dwChannels;    // channels chosen
   DWORD             dwWFEX;        // index into plWFEX chosen
   PCListVariable    plFormat;      // fills with a list of ACMFORMATDETAILS
   PCListVariable    plDriver;      // fills with a list of ACMDRIVERDETAILS
   PCListVariable    plWFEX;        // filled with a list of WAVEFORMATEX for the details
} SPINFO, *PSPINFO;


static BOOL CALLBACK FormatCallback (
  HACMDRIVERID       hadid,      
  LPACMFORMATDETAILS pafd,       
  DWORD_PTR          dwInstance, 
  DWORD              fdwSupport  
)
{
   PSPINFO ps = (PSPINFO) dwInstance;
   
   // if this is just PCM then ignore
   if ((pafd->pwfx->wFormatTag == WAVE_FORMAT_PCM) && (pafd->pwfx->wBitsPerSample == 16))
      return TRUE;

   // if wave format matches what have then remember this
   PWAVEFORMATEX pwfex = (PWAVEFORMATEX) ps->pAI->pmemWFEX->p;
   if ((pwfex->cbSize == pafd->pwfx->cbSize) && !memcmp(pwfex, pafd->pwfx, sizeof(WAVEFORMATEX)+pwfex->cbSize))
      ps->dwWFEX = ps->plWFEX->Num();

   // add this
   ps->plFormat->Add (pafd, sizeof(ACMFORMATDETAILS));
   ps->plWFEX->Add (pafd->pwfx, sizeof(WAVEFORMATEX) + pafd->pwfx->cbSize);

   return TRUE;
}

static BOOL CALLBACK DriverCallback(
  HACMDRIVERID hadid,     
  DWORD_PTR    dwInstance,
  DWORD        fdwSupport 
)
{
   HACMDRIVER hDriver = NULL;
   PSPINFO ps = (PSPINFO) dwInstance;
   if (acmDriverOpen (&hDriver, hadid, 0))
      return TRUE;

   ACMFORMATDETAILS fd;
   BYTE abTemp[1000];
   memset (&fd, 0, sizeof(fd));
   fd.cbStruct = sizeof(fd);
   fd.pwfx = (PWAVEFORMATEX) &abTemp[0];
   fd.cbwfx = sizeof(abTemp);
   fd.dwFormatTag = WAVE_FORMAT_UNKNOWN;

   // cheesy hack - know that the first entry in the list is the PCM data
   // so just just that as initial wave format
   memcpy (fd.pwfx, ps->plWFEX->Get(0), ps->plWFEX->Size(0));

   MMRESULT mm;
   mm = acmFormatEnum (hDriver, &fd, FormatCallback, dwInstance,
      ACM_FORMATENUMF_CONVERT | ACM_FORMATENUMF_NCHANNELS | ACM_FORMATENUMF_NSAMPLESPERSEC);

   // add copy of diver details
   ACMDRIVERDETAILS dd;
   memset (&dd, 0, sizeof(dd));
   dd.cbStruct = sizeof(dd);
   acmDriverDetails (hadid, &dd, 0);
   while (ps->plDriver->Num() < ps->plFormat->Num())
      ps->plDriver->Add (&dd, sizeof(dd));

   acmDriverClose (hDriver,0);
   return TRUE;
}


/****************************************************************************
AIWavePage
*/
BOOL AIWavePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PSPINFO ps = (PSPINFO) pPage->m_pUserData;
   PANIMINFO pai = ps->pAI;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;
         ESCMCOMBOBOXADD add;
         WCHAR szTemp[128];

         pControl = pPage->ControlFind (L"useaudio");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pai->fIncludeAudio);

         if (!ComboBoxSet (pPage,L"samples", ps->dwSamplesPerSec)) {
            pControl = pPage->ControlFind (L"samples");
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            swprintf (szTemp, L"<elem name=%d>%d kHz</elem>",
               (int)ps->dwSamplesPerSec,
               (int)ps->dwSamplesPerSec / 1000);
            add.pszMML = szTemp;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXADD, &add);
            ComboBoxSet (pPage, L"samples", ps->dwSamplesPerSec);
         }

         if (!ComboBoxSet (pPage,L"channels", ps->dwChannels)) {
            pControl = pPage->ControlFind (L"channels");
            memset (&add, 0, sizeof(add));
            add.dwInsertBefore = -1;
            swprintf (szTemp, L"<elem name=%d>%d channels</elem>",
               (int)ps->dwChannels,
               (int)ps->dwChannels);
            add.pszMML = szTemp;
            if (pControl)
               pControl->Message (ESCM_COMBOBOXADD, &add);
            ComboBoxSet (pPage, L"channels", ps->dwChannels);
         }

         pPage->Message (ESCM_USER+82);   // so update the list of choices for the
            // channels and sampling rate
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"useaudio")) {
            pai->fIncludeAudio = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz || _wcsicmp(p->psz, gpszNext))
            break;   // only care about next

         PWAVEFORMATEX pwfex;
         pwfex = (PWAVEFORMATEX) ps->plWFEX->Get(ps->dwWFEX);
         if (!pwfex)
            return TRUE;   // shouldnt happen

         // copy over
         if (!pai->pmemWFEX->Required (pwfex->cbSize + sizeof(WAVEFORMATEX)))
            return TRUE;   // shouldnt happen
         pai->pmemWFEX->m_dwCurPosn = pwfex->cbSize + sizeof(WAVEFORMATEX);
         memcpy (pai->pmemWFEX->p, pwfex, pai->pmemWFEX->m_dwCurPosn);

         pPage->Exit (gpszNext);
         return TRUE;
      }
      break;

   case ESCM_USER+83:   // set the status to indicate bytes per second, and total bytes
      {
         PCEscControl pControl = pPage->ControlFind (L"status");
         if (!pControl)
            return TRUE;

         CMem gMemTemp;
         MemZero (&gMemTemp);

         // find the wave format
         PWAVEFORMATEX pwfex;
         pwfex = (PWAVEFORMATEX) ps->plWFEX->Get(ps->dwWFEX);
         if (pwfex) {
            MemCat (&gMemTemp, L"<p><bold>");
            MemCat (&gMemTemp, (int)pwfex->nAvgBytesPerSec / 1000);
            MemCat (&gMemTemp, L".");
            MemCat (&gMemTemp, (int)(pwfex->nAvgBytesPerSec / 100) % 10);
            MemCat (&gMemTemp, L" KBytes/sec</bold> for ");
            MemCat (&gMemTemp, (int)(pai->fTimeEnd - pai->fTimeStart));
            MemCat (&gMemTemp, L" seconds =<br/>");
            MemCat (&gMemTemp, (int)((fp)pwfex->nAvgBytesPerSec * (pai->fTimeEnd - pai->fTimeStart)
               / 1000.0));
            MemCat (&gMemTemp, L" KBytes total");
            MemCat (&gMemTemp, L"</p>");
         }
         else
            MemCat (&gMemTemp, L"Can't seem to convert this wave.");

         ESCMSTATUSTEXT s;
         memset (&s, 0, sizeof(s));
         s.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_STATUSTEXT, &s);
      }
      return TRUE;

   case ESCM_USER+82:   // called so list of choices for channels and sampling rate updated
      {
         PCEscControl pControl = pPage->ControlFind (L"compression");
         if (!pControl)
            return TRUE;

         pControl->Message (ESCM_COMBOBOXRESETCONTENT, 0);

         
         // enumerate the formats into a big list
         ps->plFormat->Clear();
         ps->plWFEX->Clear();
         ps->plDriver->Clear();

         // add no-compression
         ACMFORMATDETAILS fd;
         WAVEFORMATEX wfex;
         ACMDRIVERDETAILS dd;
         memset (&fd, 0, sizeof(fd));
         memset (&wfex, 0, sizeof(wfex));
         memset (&dd, 0, sizeof(dd));
         fd.dwFormatTag = WAVE_FORMAT_PCM;
         // strcpy (fd.szFormat, "None");
         strcpy (dd.szLongName, "No compression");
         wfex.cbSize = 0;
         wfex.wFormatTag = WAVE_FORMAT_PCM;
         wfex.nChannels = ps->dwChannels;
         wfex.nSamplesPerSec = ps->dwSamplesPerSec;
         wfex.wBitsPerSample = 16;
         wfex.nBlockAlign  = wfex.nChannels * wfex.wBitsPerSample / 8;
         wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;
         ps->plFormat->Add (&fd, sizeof(fd));
         ps->plWFEX->Add (&wfex, sizeof(wfex));
         ps->plDriver->Add (&dd, sizeof(dd));

         // just in case dont match anything else, pretend matches pcm
         ps->dwWFEX = 0;

         // enumerate the formats
         acmDriverEnum (DriverCallback, (DWORD_PTR) ps, ACM_DRIVERENUMF_NOLOCAL);

         // add all the formats
         // add no compression
         ESCMCOMBOBOXADD add;
         DWORD i;
         CMem gMemTemp;
         MemZero (&gMemTemp);
         for (i = 0; i < ps->plFormat->Num(); i++) {
            PACMFORMATDETAILS pfd = (PACMFORMATDETAILS) ps->plFormat->Get(i);
            PACMDRIVERDETAILS pdd = (PACMDRIVERDETAILS) ps->plDriver->Get(i);
            if (!pdd->szLongName[0])
               continue;   // blank name


            // add 
            WCHAR szName[256];
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L"><bold>");
            MultiByteToWideChar (CP_ACP, 0, pdd->szLongName, -1, szName, sizeof(szName)/2);
            MemCatSanitize (&gMemTemp, szName);
            MemCat (&gMemTemp, L"</bold> ");
            MultiByteToWideChar (CP_ACP, 0, pfd->szFormat, -1, szName, sizeof(szName)/2);
            MemCatSanitize (&gMemTemp, szName);
            MemCat (&gMemTemp, L"</elem>");

         }

         memset (&add, 0, sizeof(add));
         add.dwInsertBefore = -1;
         add.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_COMBOBOXADD, &add);
         // select the string
         ComboBoxSet (pPage, L"compression", ps->dwWFEX);

         // may need to set status control (or delete it)
         pPage->Message (ESCM_USER+83);
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         DWORD dwVal;
         psz = p->pControl->m_pszName;
         dwVal = p->pszName ? _wtoi(p->pszName) : 0;

         if (!_wcsicmp(psz, L"samples")) {
            if ((dwVal == ps->dwSamplesPerSec) || !dwVal)
               return TRUE;   // no change
            ps->dwSamplesPerSec = dwVal;
            pPage->Message (ESCM_USER+82);   // to update list of possible rates
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"channels")) {
            if ((dwVal == ps->dwChannels) || !dwVal)
               return TRUE;   // no change
            ps->dwChannels = dwVal;
            pPage->Message (ESCM_USER+82);   // to update list of possible rates
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"compression")) {
            if (dwVal == ps->dwWFEX)
               return TRUE;   // no change
            ps->dwWFEX = dwVal;
            pPage->Message (ESCM_USER+83);   // to update size
            return TRUE;
         }
      }
      return 0;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Animation audio";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

static PSTR gpszAnimImageHeight = "AnimImageHeight";
static PSTR gpszAnimImageWidth = "AnimImageWidth";
static PSTR gpszAnimImageAnti = "AnimImageAnti";
static PSTR gpszAnimImageQuality = "AnimImageQuality";
static PSTR gpszAnimFPS = "AnimFPS";
static PSTR gpszAnimExposureStrobe = "AnimExposureStrobe";
static PSTR gpszAnimExposureOverride = "AnimExposureOverride";
static PSTR gpszAnimExposureTime = "AnimExposureTime";
static PSTR gpszAnimAVICompressor = "AnimAVICompressor";
static PSTR gpszAnimEffectCode = "AnimEffectCode";
static PSTR gpszAnimEffectSub = "AnimEffectSub";

/*************************************************************************
AnimInfoDialog - Brings up a dialog asking which scene the user wishes
to animate, etc. This fills in the ANIMINFO structure.

inputs
   HWND                 hWnd - To bring dialog up from
   PCWorldSocket        pWorld - World to use
   PCSceneSet           pSceneSet - SceneSet to use
   PCRenderTraditional  pRender - If exists, the render to use (for some defaults)
   PCMem                pmemWFEX - Memory for WAVEFORMATEX
   PANIMINFO            pAI - Filled in
returns
   BOOL - TRUE if user got all the way through. FALSE if not
*/
BOOL AnimInfoDialog (HWND hWnd, PCWorldSocket pWorld, PCSceneSet pSceneSet, PCRenderTraditional pRender,
                     PCMem pmemWFEX, PANIMINFO pAI)
{
   pAI->hBitEffect = NULL;

   CListVariable lDriver, lWFEX, lFormat;
   // fill in with what have
   memset (pAI, 0, sizeof(*pAI));
   pAI->pWorld = pWorld;
   pAI->pSceneSet = pSceneSet;
   pAI->pRender = pRender;
   pAI->pmemWFEX = pmemWFEX;

   // if there aren't any scenes or no cameras then exit here
   if (!pAI->pSceneSet->SceneNum()) {
      EscMessageBox (hWnd, ASPString(),
         L"You haven't created any scenes.",
         L"You must create a scene before you can animate.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   DWORD dwNumCamera;
   CListFixed lCamera;
   PCScene pScene;
   lCamera.Init (sizeof(OSMANIMCAMERA));
   pScene = pAI->pSceneSet->SceneGet(0);
   pScene->CameraEnum(&lCamera);
   dwNumCamera = lCamera.Num();
   if (pAI->pRender)
      dwNumCamera++;
   if (!dwNumCamera) {
      EscMessageBox (hWnd, ASPString(),
         L"You haven't created any animation cameras.",
         L"The scene cannot be animated unless there is a camera to film it with.",
         MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   CEscWindow cWindow;
   RECT r;
   DialogBoxLocation (hWnd, &r);

   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   PWSTR pszRet;

   // set up some values from registry
   pAI->dwPixelsX = KeyGet (gpszAnimImageHeight, 640);
   pAI->dwPixelsY = KeyGet (gpszAnimImageWidth, 480);
   pAI->dwQuality = KeyGet (gpszAnimImageQuality, 6); // BUGFIX - Default to ray trace 1
   pAI->dwAnti = KeyGet (gpszAnimImageAnti, 2);
   pAI->dwFPS = KeyGet (gpszAnimFPS, 15);
   pAI->dwExposureStrobe = KeyGet (gpszAnimExposureStrobe, 8);
   pAI->fExposureTime = (fp) KeyGet (gpszAnimExposureTime, (DWORD) 0) / 1000.0;
   pAI->dwAVICompressor = KeyGet (gpszAnimAVICompressor, (DWORD) 0);
   KeyGet (gpszAnimEffectCode, &pAI->gEffectCode);
   KeyGet (gpszAnimEffectSub, &pAI->gEffectSub);

askscene:
   // if only one scene and only one camera then skip the first page
   if ((pAI->pSceneSet->SceneNum() >= 2) || (dwNumCamera >= 2)) {
      // start with the first page
      pszRet = cWindow.PageDialog (ghInstance, IDR_MMLAISCENE, AIScenePage, pAI);
      if (!pszRet)
         return FALSE;  // went back
      if (!_wcsicmp(pszRet, Back()))
         return FALSE;  // went back
      if (_wcsicmp(pszRet, gpszNext))
         return FALSE;  // didn't press next
   }
   else {
      pAI->pScene = pAI->pSceneSet->SceneGet (0);
      memset (&pAI->gCamera, 0, sizeof(pAI->gCamera));
   }

   // set the duration to the entire scene
   pAI->fTimeStart = 0;
   pAI->fTimeEnd = pAI->pScene->DurationGet();
   if (pAI->pScene->FPSGet())
      pAI->dwFPS = pAI->pScene->FPSGet();

asktime:
   // ask the time
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLAITIME, AITimePage, pAI);
   if (!pszRet)
      return FALSE;  // went back
   if (!_wcsicmp(pszRet, Back())) {
      if ((pAI->pSceneSet->SceneNum() < 2) && (dwNumCamera < 2))
         return FALSE;  // went back but there's no scene to go back to
      goto askscene;
   }
   if (_wcsicmp(pszRet, gpszNext))
      return FALSE;  // didn't press next

askquality:
   // get the quality info
   if (pAI->hBitEffect) {
      DeleteObject (pAI->hBitEffect);
   }
   COLORREF cTrans;  // ignoring this
   pAI->hBitEffect = EffectGetThumbnail (pWorld->RenderShardGet(), &pAI->gEffectCode, &pAI->gEffectSub,
      cWindow.m_hWnd, &cTrans, TRUE);

   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLAIQUALITY, AIQualityPage, pAI);
   if (pAI->hBitEffect) {
      DeleteObject (pAI->hBitEffect);
      pAI->hBitEffect = NULL;
   }
   if (!pszRet)
      return FALSE;  // went back
   if (!_wcsicmp(pszRet, RedoSamePage()))
      goto askquality;
   if (!_wcsicmp(pszRet, Back()))
      goto asktime;
   if (_wcsicmp(pszRet, gpszNext))
      return FALSE;  // didn't press next

//askwave:
   // if there isn't a wave format specified then set one up
   DWORD dwSamplesPerSec, dwChannels;
   CM3DWave Wave;
   pAI->fIncludeAudio = pAI->pScene && pAI->pScene->WaveFormatGet(&dwSamplesPerSec, &dwChannels);

   // if only doing one frame then dont bother
   if (pAI->fTimeEnd <= pAI->fTimeStart) {
      pAI->fIncludeAudio = FALSE;
      goto skipwave;
   }

   if (!pAI->fIncludeAudio) {
      // put something in
      dwSamplesPerSec = 11025;
      dwChannels = 1;
   }
   PWAVEFORMATEX pwfex;
   if (pAI->pmemWFEX->m_dwCurPosn < sizeof(WAVEFORMATEX)) {
      BYTE abHuge[1000];
      Wave.DefaultWFEXGet ((PWAVEFORMATEX) abHuge, sizeof(abHuge));
      pwfex = (PWAVEFORMATEX) abHuge;

      // just get default wave format
      if (!pAI->pmemWFEX->Required (pwfex->cbSize + sizeof(WAVEFORMATEX)))
         return FALSE;
      pAI->pmemWFEX->m_dwCurPosn = pwfex->cbSize + sizeof(WAVEFORMATEX);
      memcpy (pAI->pmemWFEX->p, pwfex, pAI->pmemWFEX->m_dwCurPosn);
      pwfex = (PWAVEFORMATEX) pAI->pmemWFEX->p;

#if 0
      pAI->pmemWFEX->m_dwCurPosn = sizeof(WAVEFORMATEX);
      memset (pwfex, 0, sizeof(*pwfex));
      pwfex->wFormatTag = WAVE_FORMAT_PCM;
      pwfex->nSamplesPerSec = dwSamplesPerSec;
      pwfex->nChannels = dwChannels;
      pwfex->wBitsPerSample = sizeof(short) * 8;
      pwfex->nBlockAlign = pwfex->wBitsPerSample / 8 * pwfex->nChannels;
      pwfex->nAvgBytesPerSec = pwfex->nBlockAlign * pwfex->nSamplesPerSec;
#endif // 0
   }

   SPINFO sp;
   memset (&sp, 0, sizeof(sp));
   pwfex = (PWAVEFORMATEX) pAI->pmemWFEX->p;
   sp.dwChannels = pwfex->nChannels;
   sp.dwSamplesPerSec = pwfex->nSamplesPerSec;
   sp.dwWFEX = 0;
   sp.plFormat = &lFormat;
   sp.plWFEX = &lWFEX;
   sp.plDriver = &lDriver;
   sp.pAI = pAI;
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLAIWAVE, AIWavePage, &sp);
   Wave.DefaultWFEXSet ((PWAVEFORMATEX) pAI->pmemWFEX->p);
   if (!pszRet)
      return FALSE;  // went back
   if (!_wcsicmp(pszRet, Back()))
      goto askquality;
   if (_wcsicmp(pszRet, gpszNext))
      return FALSE;  // didn't press next

skipwave:
   // write out
   KeySet (gpszAnimImageHeight, pAI->dwPixelsX);
   KeySet (gpszAnimImageWidth, pAI->dwPixelsY);
   KeySet (gpszAnimImageQuality, pAI->dwQuality);
   KeySet (gpszAnimImageAnti, pAI->dwAnti);
   KeySet (gpszAnimFPS, pAI->dwFPS);
   KeySet (gpszAnimExposureStrobe, pAI->dwExposureStrobe);
   KeySet (gpszAnimExposureTime, (DWORD)(pAI->fExposureTime * 1000.0));
   KeySet (gpszAnimAVICompressor, pAI->dwAVICompressor);
   KeySet (gpszAnimEffectCode, &pAI->gEffectCode);
   KeySet (gpszAnimEffectSub, &pAI->gEffectSub);
   return TRUE;
}


/*************************************************************************
AnimHideAllWindows - Hides all 3dob windows so the animation window is the
only one visible. This is necessary because will be mucking with the world
*/
void AnimHideAllWindows (void)
{
   // hide all view windows
   DWORD i;
   PCHouseView ph;
   for (i = 0; i < ListPCHouseView()->Num(); i++) {
      ph = *((PCHouseView*)ListPCHouseView()->Get(i));
      ShowWindow (ph->m_hWnd, SW_HIDE);

      // BUGFIX - Tell the world to hide all editors
      ph->m_pWorld->ObjEditorShowWindow (FALSE);
   }
   for (i = 0; i < ListVIEWOBJ()->Num(); i++) {
      PVIEWOBJ pvo = (PVIEWOBJ) ListVIEWOBJ()->Get(i);
      ShowWindow (pvo->pView->m_hWnd, SW_HIDE);

      // BUGFIX - Tell the world to hide all editors
      pvo->pView->m_pWorld->ObjEditorShowWindow (FALSE);
   }


   // hide all the list views
   ListViewShowAll (FALSE);
   PaintViewShowHide (FALSE);

   // hide all timelines
   for (i = 0; i < ListPCSceneView()->Num(); i++) {
      PCSceneView pv = *((PCSceneView*) ListPCSceneView()->Get(i));
      ShowWindow (pv->m_hWnd, SW_HIDE);
   }

   // dont bother with help because cant affect the world

   // set thread priority
   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_BELOW_NORMAL));

}


/*************************************************************************
AnimShowAllWindows - Called when the animation is finished or the user
pressed cancels. Shows all 3dob windows so the animation window.
*/
void AnimShowAllWindows (DWORD dwRenderShard)
{
   gapAnim[dwRenderShard] = NULL;

   // show all view windows
   DWORD i;
   PCHouseView ph;
   for (i = 0; i < ListPCHouseView()->Num(); i++) {
      ph = *((PCHouseView*)ListPCHouseView()->Get(i));
      ShowWindow (ph->m_hWnd, SW_SHOW);

      // BUGFIX - Tell the world to show all editors
      ph->m_pWorld->ObjEditorShowWindow (TRUE);
   }
   for (i = 0; i < ListVIEWOBJ()->Num(); i++) {
      PVIEWOBJ pvo = (PVIEWOBJ) ListVIEWOBJ()->Get(i);
      ShowWindow (pvo->pView->m_hWnd, SW_SHOW);

      // BUGFIX - Tell the world to show all editors
      pvo->pView->m_pWorld->ObjEditorShowWindow (TRUE);
   }

   // show all the list views
   ListViewShowAll (TRUE);
   PaintViewShowHide (TRUE);

   // show all timelines
   for (i = 0; i < ListPCSceneView()->Num(); i++) {
      PCSceneView pv = *((PCSceneView*) ListPCSceneView()->Get(i));
      ShowWindow (pv->m_hWnd, SW_SHOW);
   }


   // set thread priority
   SetThreadPriority (GetCurrentThread(), VistaThreadPriorityHack(THREAD_PRIORITY_NORMAL));
}

/*******************************************************************************************
AnimSceneWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK AnimSceneWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCAnimWindow p = (PCAnimWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCAnimWindow p = (PCAnimWindow) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCAnimWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->WndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*******************************************************************************************
AnimSceneImageWndProc - Window proc. Just calls into object
*/
LRESULT CALLBACK AnimSceneImageWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN64
   PCAnimWindow p = (PCAnimWindow) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#else
   PCAnimWindow p = (PCAnimWindow) (size_t) GetWindowLongPtr (hWnd, GWLP_USERDATA);
#endif

   switch (uMsg) {
   case WM_CREATE: 
      {
         // store away the user data
         LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
         SetWindowLongPtr (hWnd, GWLP_USERDATA, (LONG_PTR) lpcs->lpCreateParams);
         p = (PCAnimWindow) lpcs->lpCreateParams;
      }
      break;
   };

   if (p)
      return p->ImageWndProc (hWnd, uMsg, wParam, lParam);
   else
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
}


/*************************************************************************
AnimAnimate - Animates a scene.

This:
   1) Remembers the animation info
   2) Hides all 3dob's windows
   3) Shows the animation window
   4) Animates
   5) Closes animation window
   6) Shows 3dob's windows

inputs
   PANIMINFO      pai - animation info
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL AnimAnimate (DWORD dwRenderShard, PANIMINFO pai)
{
   if (gapAnim[dwRenderShard])
      return FALSE;  // animating already

   MALLOCOPT_INIT;
   MALLOCOPT_OKTOMALLOC;
   gapAnim[dwRenderShard] = new CAnimWindow(dwRenderShard);
   MALLOCOPT_RESTORE;

   if (!gapAnim[dwRenderShard])
      return FALSE;

   if (!gapAnim[dwRenderShard]->Init (pai)) {
      MALLOCOPT_RESTORE;
      delete gapAnim[dwRenderShard];
      gapAnim[dwRenderShard] = NULL;
   }

   return TRUE;
   // done
}


/*************************************************************************
AnimAnimateWait - Animates a scene. Waits until the animation is finished

inputs
   PANIMINFO      pai - animation info
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL AnimAnimateWait (DWORD dwRenderShard, PANIMINFO pai)
{
   if (!AnimAnimate(dwRenderShard, pai))
      return FALSE;

   MSG msg;
   while (gapAnim[dwRenderShard]) {
      Sleep (10);
      while( gapAnim[dwRenderShard] && PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
         TranslateMessage( &msg );
         DispatchMessage( &msg );
         ASPHelpCheckPage ();
      }
   }
   return TRUE;
   // done
}


// FUTURE RELEASE - Generate AVI didn't seem to work on WinME. Decided that won't really
// support it (since by the time full release it will be dead), so don't bother.