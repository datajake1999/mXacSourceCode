/*============================================================================
  Class:         C3DSound (definition)

  Summary:       Used to represent a single sound which may played and
                 positioned in 3D space relative to the origin.

  Methods:       See class definition below.

  Written by:    Toby Murray, April 2002
  Last Modified: Toby Murray, 2nd April 2002
============================================================================*/

#ifndef C3DSOUND_H
#define C3DSOUND_H

#include <windows.h>
#include <dmusicc.h>
#include <dmusici.h>


class C3DSound{

public:

   /*-------------------------------------------------------
   Constructor:  C3DSound();
   Purpose: Initialises member variables to default values.
            Doesn't actually do any of the creating or setup.
   -------------------------------------------------------*/
   C3DSound();

   /*-------------------------------------------------------
   Destructor:  ~C3DSound(void);
   Purpose: Will release any resources not already released
            However should not be relied upon to do so.
            Kill() should be used to do all releasing.
   -------------------------------------------------------*/
   ~C3DSound();

   /*-------------------------------------------------------
   Method:  HRESULT Setup(WCHAR *pwzFileName,
                          IDirectMusicPerformance8 *pPerformance,
                          IDirectMusicLoader8 *pLoader);
   Purpose: Sets up a sound. Loads the given soundfile
            (with the given loader)
            specified with a relative path from the current
            working directory. Also creates the 3d audiopath
            and gets the 3d sound buffer and listener 
            for the sound so that it can be positioned in
            3D space. Positions the sound at the origin.
            Returns S_OK on success or
            an error code on failure.
   -------------------------------------------------------*/
   HRESULT Setup(WCHAR *pwsFileName,
                 IDirectMusicPerformance8 *pPerformance,
                 IDirectMusicLoader8 *pLoader);

   /*-------------------------------------------------------
   Method:  void Kill(void);
   Purpose: Stops the sound playing and releases all resources
            used by the sound. Should be called when a sound
            is no longer needed to be played, and should be
            called when shutting down app before closing down
            DirectMusic.
   -------------------------------------------------------*/
   void Kill();

   /*-------------------------------------------------------
   Method:  HRESULT Play(bool bLoop = false);
   Purpose: Plays the given sound, specifying whether the
            sound is to loop or to only be played once.
            Returns S_OK on success or an error code on failure.
   -------------------------------------------------------*/
   HRESULT Play(bool bLoop = false);

   /*-------------------------------------------------------
   Method:  HRESULT setPos(float fX, float fY, float fZ);
   Purpose: Changes the position of a sound in 3D space.
            Returns S_OK on success, or an erro code on failure.            
   -------------------------------------------------------*/
   HRESULT setPos(float fX, float fY, float fZ);
   
   /*-------------------------------------------------------
   Method:  HRESULT setListenerPos(float fX, float fY, float fZ);
   Purpose: Changes the position of the listener in 3D space.
            Returns S_OK on success, or an erro code on failure.            
   -------------------------------------------------------*/
   HRESULT setListenerPos(float fX, float fY, float fZ);


private:
   // the performance this sound is associated with
   IDirectMusicPerformance8* m_pPerformance;

   // the loader to laod this sound wtih
   IDirectMusicLoader8* m_pLoader;

   // the segment for this sound
   IDirectMusicSegment8* m_pSegment;

   // the 3d audiopath for this sound
   IDirectMusicAudioPath8* m_p3DAudioPath;

   // the sound buffer for this sound - used to position sound in 3d space
   IDirectSound3DBuffer8* m_pDSB;

   // the listener for this sound
   IDirectSound3DListener8* m_pListener;
};

#endif
