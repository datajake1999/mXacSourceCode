/*============================================================================
  Class:         C3DSound (body)

  Please see accompanying definition of class.
   
  Written by:    Toby Murray, April 2002
  Last Modified: Toby Murray, 2nd April 2002
============================================================================*/
#include "stdafx.h"
#include "C3DSound.h"


C3DSound::C3DSound(){
   m_pPerformance = NULL;
   m_pLoader = NULL;
   m_pSegment = NULL;
   m_p3DAudioPath = NULL;
   m_pDSB = NULL;
}

C3DSound::~C3DSound(){
   Kill();
}

// sets up a sound
HRESULT C3DSound::Setup(WCHAR *pwsFileName,
                      IDirectMusicPerformance8* pPerformance,
                      IDirectMusicLoader8* pLoader){
   HRESULT hr;

   m_pPerformance = pPerformance;
   m_pLoader = pLoader;

   // load the soundfile
   if( FAILED( hr = m_pLoader->LoadObjectFromFile( CLSID_DirectMusicSegment,
                                                   IID_IDirectMusicSegment8,
                                                   pwsFileName,
                                                   (LPVOID*) &m_pSegment ) ) )
      return hr;

   // Download the segment's instruments to the synthesizer
   if (FAILED(hr = m_pSegment->Download( m_pPerformance )))
      return hr;

   // by default don't repeat soundss
   if (FAILED(hr = m_pSegment->SetRepeats( 0 )))
      return hr;

   
   // Create the 3D audiopath with a 3d buffer.
   // We can then play this segment through this audiopath (and hence the buffer)
   // and alter its 3D parameters.
   if (FAILED(hr = m_pPerformance->CreateStandardAudioPath( DMUS_APATH_DYNAMIC_3D, 
                                                            64, TRUE, &m_p3DAudioPath )))
      return hr;

   // Get the IDirectSound3DBuffer8 from the 3D audiopath
   if (FAILED(hr = m_p3DAudioPath->GetObjectInPath( DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, 
                                                    GUID_NULL, 0, IID_IDirectSound3DBuffer, 
                                                    (LPVOID*) &m_pDSB )))
      return hr;

   // get the listener from the 3d audiopath
   if (FAILED(m_p3DAudioPath->GetObjectInPath(0, DMUS_PATH_PRIMARY_BUFFER,
                                              0, GUID_All_Objects, 0, 
                                              IID_IDirectSound3DListener,
                                              (void **)&m_pListener)))
      return hr;

   return S_OK;


}


void C3DSound::Kill(){
    // Cleanup all interfaces
    if (m_pDSB)
       m_pDSB->Release();
    m_pDSB = NULL;
    if (m_pListener)
       m_pListener->Release();
    m_pListener = NULL;
    if (m_p3DAudioPath)
       m_p3DAudioPath->Release();
    m_p3DAudioPath = NULL;
   
    if (m_pSegment){       
       m_pSegment->Release();
    }
    m_pSegment = NULL;
}

// play the sound
HRESULT C3DSound::Play(bool bLoop){
   HRESULT hr;
   if (bLoop)
      // Tell DirectMusic to repeat this segment forever
      if (FAILED(hr = m_pSegment->SetRepeats( DMUS_SEG_REPEAT_INFINITE )))
         return hr;

   // Play segment on the 3D audiopath
   if (FAILED(hr = m_pPerformance->PlaySegmentEx( m_pSegment, NULL, NULL, DMUS_SEGF_SECONDARY, 
                                                  0, NULL, NULL, m_p3DAudioPath )))
      return hr;

   return S_OK;
}

HRESULT C3DSound::setPos(float fX, float fY, float fZ){
   HRESULT hr;
   // Set the position of sound
   if (FAILED(hr = m_pDSB->SetPosition( fX, fY, fZ, DS3D_IMMEDIATE )))
      return hr;

   return S_OK;
}

HRESULT C3DSound::setListenerPos(float fX, float fY, float fZ){
   HRESULT hr;

   if (FAILED(hr = m_pListener->SetPosition(fX, fY, fZ, DS3D_IMMEDIATE)))
      return hr;

   return S_OK;
}
