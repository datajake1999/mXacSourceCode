/*============================================================================
  Class:         CDXAudio (body)

  Please see accompanying definition of class.
   
  Written by:    Toby Murray, April 2002
  Last Modified: Toby Murray, 2nd April 2002
============================================================================*/
#include "stdafx.h"
#define INITGUID
#include "CDXAudio.h"
#include <cguid.h>

// constructor
CDXAudio::CDXAudio(){
   m_pPerformance = NULL;
   m_pLoader = NULL;
}

// destructor
CDXAudio::~CDXAudio(){
   Kill();
}

BOOL AppCreateBasicBuffer( 
    LPDIRECTSOUND8 lpDirectSound, 
    LPDIRECTSOUNDBUFFER *lplpDsb) 
{ 
  PCMWAVEFORMAT pcmwf; 
  DSBUFFERDESC dsbdesc; 
  HRESULT hr; 
 
  // Set up wave format structure. 
  memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT)); 
  pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM; 
  pcmwf.wf.nChannels = 2; 
  pcmwf.wf.nSamplesPerSec = 22050; 
  pcmwf.wf.nBlockAlign = 4; 
  pcmwf.wf.nAvgBytesPerSec = 
    pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign; 
  pcmwf.wBitsPerSample = 16; 

 
  // Set up DSBUFFERDESC structure. 
 
  memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 
  dsbdesc.dwSize = sizeof(DSBUFFERDESC); 
  dsbdesc.dwFlags = 
    DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY; 
 
  dsbdesc.dwBufferBytes = 3 * pcmwf.wf.nAvgBytesPerSec; 
  dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf; 
 
  // Create buffer. 
 
  hr = lpDirectSound->CreateSoundBuffer(&dsbdesc, lplpDsb, NULL); 
  if SUCCEEDED(hr) 
  { 
    // IDirectSoundBuffer interface is in *lplpDsb. 
    // Use QueryInterface to obtain IDirectSoundBuffer8.
    return TRUE; 
  } 
  else 
  { 
    // Failed. 
    *lplpDsb = NULL; 
    return FALSE; 
  } 
} 
 
HRESULT CDXAudio::Setup(){
   HRESULT hr;
   // Initialize COM
   if (FAILED(hr = CoInitialize(NULL)))
      return hr;
    
   // Create loader object
   if (FAILED(hr = CoCreateInstance( CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC, 
                                     IID_IDirectMusicLoader8, (void**)&m_pLoader )))
      return hr;

   // Create performance object
   if (FAILED(hr = CoCreateInstance( CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC, 
                                     IID_IDirectMusicPerformance8, (void**)&m_pPerformance )))
      return hr;

   // This initializes both DirectMusic and DirectSound and 
   // sets up the synthesizer. 
   if (FAILED(hr = m_pPerformance->InitAudio( NULL, NULL, NULL, 
                                              DMUS_APATH_DYNAMIC_3D, 64,
                                              DMUS_AUDIOF_ALL, NULL )))
      return hr;

   // BUGBUG
   LPDIRECTSOUND8 pDS8 = NULL;
   HRESULT hRes;
   hRes = DirectSoundCreate8 (&DSDEVID_DefaultPlayback, &pDS8, NULL);
   LPDIRECTSOUNDBUFFER pDSB = NULL;
   AppCreateBasicBuffer (pDS8, &pDSB);
   if (pDSB)
      pDSB->Release();
   if (pDS8)
      pDS8->Release();
   // BUGBUG

   return S_OK;
}

// release everything
void CDXAudio::Kill(){
   // Stop the music
   if (m_pPerformance)
      m_pPerformance->Stop( NULL, NULL, 0, 0 );

   if (m_pLoader)
      m_pLoader->Release(); 
   m_pLoader = NULL;   
   
   if (m_pPerformance)
      m_pPerformance->CloseDown();

   if (m_pPerformance)
      m_pPerformance->Release();
   m_pPerformance = NULL;

   // Close down COM
   CoUninitialize();

}
