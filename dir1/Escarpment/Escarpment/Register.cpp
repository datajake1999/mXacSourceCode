/***************************************************************************
Register.cpp - Handle registration.

begun 5/11/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <speech.h>
#include "escarpment.h"
#include "mmlinterpret.h"
#include "resleak.h"

#define  RANDNUM1    0x45359854     // change for every app
#define  RANDNUM2    0xef89af98     // change for every app
#define  RANDNUM3    0xa6f2da8c     // change for every app


DWORD     gdwInitialized = 0; // init count
PITTSCENTRALW  gpITTSCentral = NULL;
DWORD          gdwChimeState = 0;   // 0->not playing, 1->playng, 2->stopped & timing down for MIDI
DWORD          gdwChimePosn = 0;     // current position with in the chimee state
DWORD          gdwChimeNum = 0;     // number of elements in gMemChime
UINT_PTR          gdwChimeTimer = 0;   // current chime timer
HMIDIOUT       ghChimeMIDI = 0;     // midi
CMem           gMemChime;           // memory to store current chime in

CRITICAL_SECTION gcsEscInit;

/**********************************************************************************
MySRand, MyRand - Personal random functions.
*/
static DWORD   gdwRandSeed;

void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

DWORD MyRand (void)
{
   gdwRandSeed = (gdwRandSeed ^ RANDNUM1) * (gdwRandSeed ^ RANDNUM2) +
      (gdwRandSeed ^ RANDNUM3);

   return gdwRandSeed;
}



/**********************************************************************************
HashString - Hash an E-mail (or other string) to a DWORD number. Use this as the
registration key.

inputs
   char  *psz
returns
   DWORD
*/
DWORD HashString (char *psz)
{
   DWORD dwSum;

   DWORD i;
   dwSum = 324233;
   for (i = 0; psz[i]; i++) {
      MySRand ((DWORD) tolower (psz[i]));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}




/**********************************************************************************
EscInitialize - Initializes the DLL


inputs
   PWSTR    pszEmail - Email of the person that registerd. Can use "Sample"
            to indicate that it's a sample in Escarpment
   DWORD    dwRegKey - Registration key for the E-mail.
   DWORD    dwFlags - Not yet used
*/
BOOL EscInitialize (PWSTR pszEmail, DWORD dwRegKey, DWORD dwFlags)
{
   EnterCriticalSection (&gcsEscInit);
   if (gdwInitialized) {
      gdwInitialized++;
      LeaveCriticalSection (&gcsEscInit);
      return TRUE;
   }

#if 0 // def _WIN64
   char szaTemp[256];
   sprintf (szaTemp, "Sizeof(PVOID)=%ld", (__int64) sizeof(PVOID));
   MessageBox (NULL, szaTemp, szaTemp, MB_OK);

#if 0
#define MEMCHUNKSTOALLOC      100
   PVOID apv[MEMCHUNKSTOALLOC];
   memset (apv, 0, sizeof(apv));
   DWORD i;
   for (i = 0; i < MEMCHUNKSTOALLOC; i++) {
      apv[i] = VirtualAlloc (NULL, 100000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (!apv[i]) {
         sprintf (szaTemp, "VirtualAlloc failed at %d", i);
         MessageBox (NULL, szaTemp, szaTemp, MB_OK);
         break;
      }
      else {
         sprintf (szaTemp, "VirtualAlloc succeded at %d", i);
         MessageBox (NULL, szaTemp, szaTemp, MB_OK);
      }
   }
   for (i = 0; i < MEMCHUNKSTOALLOC; i++)
      if (apv[i])
         VirtualFree (apv[i], 0, MEM_RELEASE);
#endif // 0
   __int64 i;
   for (i = 100000000; i < 10000000000; i *= 2) {
      PVOID pNew = VirtualAlloc (NULL, i, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (pNew)
         VirtualFree (pNew, 0, MEM_RELEASE);
      if (!pNew) {
         sprintf (szaTemp, "VirtualAlloc (%ld) failed", i);
         MessageBox (NULL, szaTemp, szaTemp, MB_OK);
      }
   }

#endif

   gdwInitialized++;

   // initialize com
   CoInitialize (NULL);

   // make sure multithreaded is initialized
   EscMultiThreadedInit();

   // is it a sample
   if (!_wcsicmp(pszEmail, L"sample")) {
      EscMessageBox (NULL,
         L"Escarpment Registration",

         L"This application is using a sample version of Escarpment.",

         L"The application is not licensed to use Escarpment. It is using Escarpment "
         L"for test purposes."
         L"\r\n"
         L"If this is a shipping application then it is illegally using Escarpment. "
         L"Please report the illegal usage to MikeRozak@bigpond.com."
         L"\r\n"
         L"If you are shipping an application you should register "
         L"your copy of Escarpment; see the Escarpment SDK for more details.",

         MB_OK | MB_ICONWARNING);
      LeaveCriticalSection (&gcsEscInit);
      return TRUE;
   }

   // else verify it's real
   char  szTemp[512];
   WideCharToMultiByte (CP_ACP, 0, pszEmail, -1, szTemp, sizeof(szTemp), 0, 0);
   if (dwRegKey != HashString(szTemp)) {
      EscMessageBox (NULL,
         L"Escarpment Registration",
         L"This application is using an illegal copy of Escarpment!",
         L"The application is using Escarpment but has not paid for it. "
         L"Please report the illegal usage of Escarpment "
         L"to MikeRozak@bigpond.com.",
         MB_OK | MB_ICONERROR);
   };

   LeaveCriticalSection (&gcsEscInit);
   return TRUE;
}


/**********************************************************************************
EscUninitialize - Uninitialized the DLL
*/
void EscUninitialize (void)
{
   EnterCriticalSection (&gcsEscInit);

   // BUGFIX - If not intiialize at all, just return
   if (!gdwInitialized) {
      LeaveCriticalSection (&gcsEscInit);
      return;
   }

   // BUGFIX - if still initialized after decrement then exit
   gdwInitialized--;
   if (gdwInitialized) {
      LeaveCriticalSection (&gcsEscInit);
      return;
   }

   // if just freed everything then get rid of TTS
   if (gpITTSCentral) {

      gpITTSCentral->Release();
   }

   // kill the chime timer
   if (gdwChimeTimer) {

      KillTimer (NULL, gdwChimeTimer);
   }

   // shut down midi
   MIDIShutdown();

   EscMultiThreadedEnd();

   // make sure the memory threads are freed because if they're
   // to be freed from DllMain(poc detatch), dont' seem to work
   EscMemoryFreeThreads();

   // uninitialze com each time
   CoUninitialize ();

   LeaveCriticalSection (&gcsEscInit);

   return;
}



/**********************************************************************************
EscSpeak - Speak using text-to-speech. This returns an error if TTS is disabled
dues to EscSoundsSet(), or it's not installed on the system.

inputs
   PWSTR    psz  - String to speak
   DWORD    dwFlags - One or more of the following
         ESCSPEAK_STOPPREVIOUS - If it was already speaking then stop the previous
                  one immediately and start speaking the new one.
         ESCSPEAK_WAITFORCHIME - Wait until after the currently playing chime plays
returns
   BOOL - TRUE if success
*/
BOOL EscSpeak (PWSTR psz, DWORD dwFlags)
{
   PITTSCENTRALW  pITTS = (PITTSCENTRALW) EscSpeakTTS ();
   if (!pITTS)
      return NULL;

   if (dwFlags & ESCSPEAK_STOPPREVIOUS)
      pITTS->AudioReset();

   // resume just in case
   if ((dwFlags & ESCSPEAK_WAITFORCHIME) && (gdwChimeState == 1))
      pITTS->AudioPause();
   else
      pITTS->AudioResume();

   // speak
   SDATA sd;
   sd.pData = psz;
   sd.dwSize = ((DWORD)wcslen(psz)+1)*2;
   if (pITTS->TextData (CHARSET_TEXT, 0, sd, NULL, IID_ITTSBufNotifySink))
      return FALSE;

   return TRUE;
}


/**********************************************************************************
EscSpeakTTS - Returns a LPUNKNOWN to the SAPI text-to-speech object being used.
This returns NULL if TTS is disabled due to EscSoundsSet(), or it's not installed
on the system.

Internal node - Acutally return PITTSCENTRALW.

returns
   PVOID - NOTE: This breaks from COM convention. The interface is NOT
               adref-ed prior to this, so don't release it.
*/
PVOID EscSpeakTTS (void)
{
   // if not speaking then return NULL
   if (!(EscSoundsGet () & ESCS_SPEAK))
      return NULL;
   // if already open then return that
   if (gpITTSCentral)
      return (PVOID) gpITTSCentral;

   // else create
   // create TTS
   PIAUDIO  pIAudio;
   PITTSFINDW   pITTSFind;
   CoCreateInstance (CLSID_MMAudioDest, NULL, CLSCTX_ALL,
      IID_IAudio, (void**)&pIAudio);
   CoCreateInstance (CLSID_TTSEnumerator, NULL, CLSCTX_ALL, IID_ITTSFindW, (void**)&pITTSFind);

   if (pITTSFind) {
      TTSMODEINFOW mi, mi2;
      memset (&mi, 0 ,sizeof(mi));
      pITTSFind->Find (&mi, NULL, &mi2);
      pITTSFind->Select(mi2.gModeID, &gpITTSCentral, pIAudio);
   }
   
   if (pITTSFind)
      pITTSFind->Release();
   if (pIAudio)
      pIAudio->Release();

   return (PVOID) gpITTSCentral;
}


/*************************************************************************************
ChimeTimer - Handle the chimes
*/
void CALLBACK ChimeTimer (HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
   // kill the old timer
   if (gdwChimeTimer)
      KillTimer (NULL, gdwChimeTimer);
   gdwChimeTimer = 0;

   // if we're in state 0, shouldn't be here
   if (gdwChimeState != 1) {
      // if we're in state 2, wating to shut down, then do so
      if (ghChimeMIDI)
         MIDIRelease();
      ghChimeMIDI = NULL;
      gdwChimeState = 0;
      return;
   }

   // repeat while time off is 0
   PESCMIDIEVENT  p;
   while (gdwChimePosn < gdwChimeNum) {
      p = ((PESCMIDIEVENT)gMemChime.p) + gdwChimePosn;
      // if there's a time on this one then zero it out and set a timer
      if (p->dwDelay) {
         gdwChimeTimer = SetTimer (NULL, NULL, p->dwDelay, ChimeTimer);
         p->dwDelay = 0;
         return;
      }

      // else send it
      if (ghChimeMIDI)
         MIDIShortMsg (ghChimeMIDI, p->dwMIDI);

      // next one
      gdwChimePosn++;
   }

   // if got here, got to the end of the chime data, so set
   // the state to 2 and a 10 second off timer
   gdwChimeState = 2;
   gdwChimeTimer = SetTimer (NULL, NULL, 10000, ChimeTimer);

   // if tts object, resume just in case was paused waiting
   // for the chime to finish
   PITTSCENTRALW  pITTS;
   pITTS = (PITTSCENTRALW) EscSpeakTTS ();
   if (pITTS)
      pITTS->AudioResume();
}


/*************************************************************************************
EscChime - Plays a chime (AKA MIDI). If a chime is already playing this fails.
   This will not play a chime if they're disabled with EscSoundsSet().

inputs
   PESCMIDIEVENT  paMidi - Pointer to an array of notes to play
   DWORD          dwNum - Number of elements in paMidi
returns
   BOOL - TRUE if success. FALSE if fail.
*/
BOOL EscChime (PESCMIDIEVENT paMidi, DWORD dwNum)
{
   // dont play if have disabled
   if (!(ESCS_CHIME & EscSoundsGet()))
      return FALSE;

   // if already playing don't play
   if (gdwChimeState == 1)
      return FALSE;

   // kill timer if there is any
   if (gdwChimeTimer)
      KillTimer (NULL, gdwChimeTimer);

   // if state was 0 then start up midi count
   if (gdwChimeState == 0) {
      ghChimeMIDI = MIDIClaim ();
      if (!ghChimeMIDI)
         return FALSE;
   }

   // keep this
   if (!gMemChime.Required (dwNum * sizeof(ESCMIDIEVENT)))
      return FALSE;
   memcpy (gMemChime.p, paMidi, dwNum * sizeof(ESCMIDIEVENT));
   gdwChimeState = 1;
   gdwChimePosn = 0;
   gdwChimeNum = dwNum;
   gdwChimeTimer = 0;

   // just call the chime timer function right out of the box (without
   // having a real time going) because it will play the first note
   // and then set up a timer
   ChimeTimer(NULL, NULL, NULL, NULL);

   return TRUE;
}


/*************************************************************************************
EscChime - Plays a built in chime.
   This will not play a chime if they're disabled with EscSoundsSet().


inputs
   DWORD    dwNum - Chime number
         ESCCHIME_INFORMATION - Tell user there's information
         ESCCHIME_WARNING - Tell user there's a problem
         ESCCHIME_ERROR - Tell user there's an error
         ESCCHIME_QUESTION - Tell user there's an error
         ESCCHIME_HOVERFHELP - For help
returns
   BOOL - TRUE if success. FALSE if error
*/
BOOL EscChime (DWORD dwNum)
{
#define  VOLUME         0x30
#if 0
#define  BASE           56
#define  TESTNOTE       32
   ESCMIDIEVENT aTest[] = {
      {0, MIDIINSTRUMENT (0, BASE+0)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+1)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+2)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+3)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+4)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+5)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+6)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)},
      {0, MIDIINSTRUMENT (0, BASE+7)},
      {0, MIDINOTEON (0, TESTNOTE,VOLUME)},
      {500, MIDINOTEOFF (0, TESTNOTE)}
   };
#endif // 0
#define  INFORMATIONBASE     70
   ESCMIDIEVENT aInformation[] = {
      {0, MIDIINSTRUMENT (0, 112)}, // bell
      {0, MIDINOTEON (0, INFORMATIONBASE+0,VOLUME)},
      {25, MIDINOTEOFF (0, INFORMATIONBASE+0)},
      {0, MIDINOTEON (0, INFORMATIONBASE+4,VOLUME)},
      {25, MIDINOTEOFF (0, INFORMATIONBASE+4)},
      {0, MIDINOTEON (0, INFORMATIONBASE+7,VOLUME)},
      {25, MIDINOTEOFF (0, INFORMATIONBASE+7)}
   };
#define  WARNINGBASE     56
#define  WARNINGVOLUME   min(VOLUME*3/2, 127)
   ESCMIDIEVENT aWarning[] = {
      {0, MIDIINSTRUMENT (0, 72+3)}, // breakthy flute
      {0, MIDINOTEON (0, WARNINGBASE+0,WARNINGVOLUME)},
      {250, MIDINOTEOFF (0, WARNINGBASE+0)},
      {0, MIDINOTEON (0, WARNINGBASE-6,WARNINGVOLUME)},
      {500, MIDINOTEOFF (0, WARNINGBASE-6)},
   };
#define  ERRORBASE         32
#define  ERRORVOLUME       min(VOLUME*2, 127)
   ESCMIDIEVENT aError[] = {
      {0, MIDIINSTRUMENT (0, 56+1)}, // tuba

      {0, MIDINOTEON (0, ERRORBASE+0,ERRORVOLUME)},
      {500, MIDINOTEOFF (0, ERRORBASE+0)},

      {50, MIDINOTEON (0, ERRORBASE-6,ERRORVOLUME)},
      {750, MIDINOTEOFF (0, ERRORBASE-6)},

   };
#define  QUESTIONBASE         64
#define  QUESTIONVOLUME       min(VOLUME*3/2, 127)
   ESCMIDIEVENT aQuestion[] = {
      {0, MIDIINSTRUMENT (0, 40+5)}, // pluck

      {0, MIDINOTEON (0, QUESTIONBASE+0,QUESTIONVOLUME)},
      {250, MIDINOTEOFF (0, QUESTIONBASE+0)},

      {0, MIDINOTEON (0, QUESTIONBASE+6,QUESTIONVOLUME)},
      {250, MIDINOTEOFF (0, QUESTIONBASE+6)},

   };
#define  HOVERBASE     70
   ESCMIDIEVENT aHover[] = {
      {0, MIDIINSTRUMENT (0, 112)}, // bell
      {0, MIDINOTEON (0, HOVERBASE+0,VOLUME)},
      {25, MIDINOTEOFF (0, HOVERBASE+0)},
      {0, MIDINOTEON (0, HOVERBASE+6,VOLUME)},
      {25, MIDINOTEOFF (0, HOVERBASE+6)},
      {0, MIDINOTEON (0, HOVERBASE+12,VOLUME)},
      {25, MIDINOTEOFF (0, HOVERBASE+12)}
   };

   switch (dwNum) {
   case ESCCHIME_INFORMATION:
      return EscChime (aInformation, sizeof(aInformation) / sizeof(ESCMIDIEVENT));
   case ESCCHIME_WARNING:
      return EscChime (aWarning, sizeof(aWarning) / sizeof(ESCMIDIEVENT));
   case ESCCHIME_ERROR:
      return EscChime (aError, sizeof(aError) / sizeof(ESCMIDIEVENT));
   case ESCCHIME_QUESTION:
      return EscChime (aQuestion, sizeof(aQuestion) / sizeof(ESCMIDIEVENT));
   case ESCCHIME_HOVERFHELP:
      return EscChime (aHover, sizeof(aHover) / sizeof(ESCMIDIEVENT));
   default:
      return FALSE;
   }

}
