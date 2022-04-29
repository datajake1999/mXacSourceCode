// 3DSoundTutorial.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

// include the 2 classes for DirectX Audio and 3D Sounds
#include "CDXAudio.h"
#include "C3DSound.h"

CDXAudio Audio;
C3DSound SoundEffect;

void FatalError(char* pszString){
   MessageBox(NULL, pszString, "3D Sound Tutorial",MB_OK);

   SoundEffect.Kill();
   Audio.Kill();
   exit(5);
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{

   // setup DirectX Audio
   if (FAILED(Audio.Setup()))
      FatalError("Couldn't setup DirectX Audio");

   if (FAILED(SoundEffect.Setup(L"explo1.wav", Audio.getPerformance(), Audio.getLoader())))
      FatalError("Couldn't load 3d sound effect.");

redo:
   SoundEffect.setPos(5.0f, 0.0f, 0.0f);
   SoundEffect.Play();
   MessageBox(NULL,"The sound effect just played to your right.","3D Sound Tutorial",MB_OK);


   SoundEffect.setPos(0.0f, 5.0f, 0.0f);
   SoundEffect.Play();
   MessageBox(NULL,"The sound effect just played in front of you.","3D Sound Tutorial",MB_OK);

   SoundEffect.setPos(-5.0f, 0.0f, 0.0f);
   SoundEffect.Play();
   MessageBox(NULL,"The sound effect just played to your left.","3D Sound Tutorial",MB_OK);

   SoundEffect.setPos(0.0f, -5.0f, 0.0f);
   SoundEffect.Play();
   MessageBox(NULL,"The sound effect just played behind you.","3D Sound Tutorial",MB_OK);
   goto redo;

   // kill the sound
   SoundEffect.Kill();

   // kill directX Audio
   Audio.Kill();
	return 0;
}



