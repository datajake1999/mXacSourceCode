/*============================================================================
  Class:         CDXAudio (definition)

  Summary:       Used to setup DirectX Audio 8 for an application to use to
                 play sounds with.

  Methods:       See class definition below.

  Written by:    Toby Murray, April 2002
  Last Modified: Toby Murray, 2nd April 2002
============================================================================*/

#ifndef CDXAUDIO_H
#define CDXAUDIO_H

#include <windows.h>
#include <dmusicc.h>
#include <dmusici.h>



class CDXAudio{

public:

   /*-------------------------------------------------------
   Constructor:  CDXAudio(void);
   Purpose: Initialises member variables to default values.
            Doesn't actually do any of the setting up of
            DirectMusic 8.
   -------------------------------------------------------*/
   CDXAudio();

   /*-------------------------------------------------------
   Destructor:  ~CDXAudio(void);
   Purpose: Will release any resources not already released
            However should not be relied upon to do so.
            Kill() should be used to do all releasing.
   -------------------------------------------------------*/
   ~CDXAudio();

   /*-------------------------------------------------------
   Method:  HRESULT Setup(void);
   Purpose: Sets up DirectMusic and DirectAudio. Also sets
            up a directMusic loader object which can then
            be used to load sounds.
   -------------------------------------------------------*/
   HRESULT Setup(void);

   /*-------------------------------------------------------
   Method:  void Kill(void);
   Purpose: Stops the performance, and shuts it down, releases
            all interfaces used including performance and loader.
            Does not release individual
            sounds, these should be released first.
   -------------------------------------------------------*/
   void Kill();

   /*-------------------------------------------------------
   Method:  IDirectMusicPerformance8* getPerformance(void);
   Purpose: Returns a pointer to the performance interface.
   -------------------------------------------------------*/
   IDirectMusicPerformance8* getPerformance(void){
      return m_pPerformance;
   }

   /*-------------------------------------------------------
   Method:  IDirectMusicLoader8* getLoader(void);
   Purpose: Returns a pointer to the loader interface.
   -------------------------------------------------------*/
   IDirectMusicLoader8* getLoader(void){
      return m_pLoader;
   }

private:

   // directmusic performance object
   IDirectMusicPerformance8* m_pPerformance;

   // directmusic loader object
   IDirectMusicLoader8* m_pLoader;

};


#endif
