/*************************************************************************************
CMidiFile.cpp - Code for reading in MIDI files.

begun 18/3/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"



#define DEFAULTBEATDUR     (1.0 / 120.0)



/*************************************************************************************
CMidiFile::Constructor and destructor
*/
CMidiFile::CMidiFile (void)
{
   m_pMidiInstance = NULL;
   m_fTimeTotal = NULL;
   m_pUserData = NULL;
   QueryPerformanceFrequency ((LARGE_INTEGER*)&m_iPerfFreq);
   m_awVolume[0] = m_awVolume[1] = AQ_NOVOLCHANGE;
}

CMidiFile::~CMidiFile (void)
{
   // close any open midi
   if (m_pMidiInstance)
      delete m_pMidiInstance;
   m_pMidiInstance = NULL;
}


/*************************************************************************************
CMidiFile::Open - Opens from a file (using MegaFileOpen(), so it can be remapped)

inputs
   PWSTR          pszFile - File name
returns
   BOOL - TRUE if it worked
*/
BOOL CMidiFile::Open (PWSTR pszFile)
{
   PMEGAFILE pmf = MegaFileOpen (pszFile);
   if (!pmf)
      return FALSE;

   // temproary memory to handle
   int iSize;
   MegaFileSeek (pmf, 0, SEEK_END);
   iSize = MegaFileTell (pmf);
   MegaFileSeek (pmf, 0, SEEK_SET);

   if (!m_memData.Required ((DWORD)iSize)) {
      MegaFileClose (pmf);
      return FALSE;
   }
   m_memData.m_dwCurPosn = (DWORD) iSize;

   MegaFileRead (m_memData.p, 1, iSize, pmf);
   MegaFileClose (pmf);

   return OpenInternal ();
}


/*************************************************************************************
CMidiFile::Open - Opens from memory. The memory is copied

inputs
   PWSTR          pszFile - File name
returns
   BOOL - TRUE if it worked
*/
BOOL CMidiFile::Open (PVOID pData, DWORD dwSize)
{
   if (!m_memData.Required (dwSize))
      return FALSE;
   m_memData.m_dwCurPosn = dwSize;
   memcpy (m_memData.p, pData, dwSize);

   return OpenInternal ();
}


/*************************************************************************************
CMidiFile::GetHeader - Gets the header chunk.

inputs
   DWORD          *pdwOffset - Current offset into m_memData. This is modified in place
   DWORD          *pdwID - Filled with the ID
   DWORD          *pdwSize - Filled with the size of the header
returns
   BOOL - TRUE if success
*/
BOOL CMidiFile::GetHeader (DWORD *pdwOffset, DWORD *pdwID, DWORD *pdwSize)
{
   if (!GetDWORD (pdwOffset, pdwID, TRUE))
      return FALSE;
   if (!GetDWORD (pdwOffset, pdwSize))
      return FALSE;


   // if would be too large then error
   if (*pdwOffset + *pdwSize > m_memData.m_dwCurPosn)
      return FALSE;

   return TRUE;
}


/*************************************************************************************
CMidiFile::GetWORD - Reads in a word from the info.

inputs
   DWORD          *pdwOffset - Current offset into m_memData. This is modified in place
   WORD           *pw - Filled with the word
   BOOL           fBigEndIn - If TRUE then get in PC format, FALSE (default) in mac format
returns
   BOOL - TRUE if success
*/
__inline BOOL CMidiFile::GetWORD (DWORD *pdwOffset, WORD *pw, BOOL fBigEndIn)
{
   DWORD dwOffset = *pdwOffset;
   DWORD dwNeed = sizeof(WORD);
   if (dwOffset + dwNeed > m_memData.m_dwCurPosn)
      return FALSE;

   // flip byte order
   PBYTE pbFrom = (PBYTE)m_memData.p + dwOffset;
   PBYTE pbTo = (PBYTE)pw;
   if (fBigEndIn)
      memcpy (pbTo, pbFrom, dwNeed);
   else {
      pbTo[0] = pbFrom[1];
      pbTo[1] = pbFrom[0];
   }

   // advance
   *pdwOffset = *pdwOffset + dwNeed;
   return TRUE;
}


/*************************************************************************************
CMidiFile::GetDWORD - Reads in a word from the info.

inputs
   DWORD          *pdwOffset - Current offset into m_memData. This is modified in place
   DWORD           *pdw - Filled with the dword
   BOOL           fBigEndIn - If TRUE then get in PC format, FALSE (default) in mac format
returns
   BOOL - TRUE if success
*/
__inline BOOL CMidiFile::GetDWORD (DWORD *pdwOffset, DWORD *pdw, BOOL fBigEndIn)
{
   DWORD dwOffset = *pdwOffset;
   DWORD dwNeed = sizeof(DWORD);
   if (dwOffset + dwNeed > m_memData.m_dwCurPosn)
      return FALSE;

   // flip byte order
   PBYTE pbFrom = (PBYTE)m_memData.p + dwOffset;
   PBYTE pbTo = (PBYTE)pdw;
   if (fBigEndIn)
      memcpy (pbTo, pbFrom, dwNeed);
   else {
      pbTo[0] = pbFrom[3];
      pbTo[1] = pbFrom[2];
      pbTo[2] = pbFrom[1];
      pbTo[3] = pbFrom[0];
   }

   // advance
   *pdwOffset = *pdwOffset + dwNeed;
   return TRUE;
}



/*************************************************************************************
CMidiFile::GetVarLength - Reads in a variable length number

inputs
   DWORD          *pdwOffset - Current offset into m_memData. This is modified in place
   DWORD          *pdwValue - Filled with the value
returns
   BOOL - TRUE if success
*/
__inline BOOL CMidiFile::GetVarLength (DWORD *pdwOffset, DWORD *pdwValue)
{
   DWORD dwOffset = *pdwOffset;
   DWORD dwLeft = (DWORD)m_memData.m_dwCurPosn;
   DWORD dwValue = 0;
   PBYTE pb = (PBYTE)m_memData.p + dwOffset;

   while (TRUE) {
      if (!dwLeft)
         return FALSE;

      if (pb[0] & 0x80) {
         dwValue = (dwValue << 7) | (pb[0] & 0x7f);
         pb++;
         dwLeft--;
         dwOffset++;
         continue;
      }

      // else end of info
      dwValue = (dwValue << 7) | pb[0];
      *pdwOffset = dwOffset+1;
      *pdwValue = dwValue;
      return TRUE;
   }
}



/*************************************************************************************
CMidiFile::GetDeltaTime - Reads in a delta time from the current track location.
It also adveces pti->dwOffsetCur (but not pti->fTimeUsed).

inputs
   PMFTRACKINFO         pti - Track information
returns
   double - Delta time, or negative value if error
*/
double CMidiFile::GetDeltaTime (PMFTRACKINFO pti)
{
   DWORD dwDelta;
   if (!GetVarLength (&pti->dwOffsetCur, &dwDelta))
      return -1;
   if (!dwDelta)
      return 0;   // fast out

   if (m_dwTicksPerBeat)
      return (double)dwDelta * m_fBeatDur / (double)m_dwTicksPerBeat;
   else
      return (double)dwDelta * m_fTickDurHeader;
}


/*************************************************************************************
CMidiFile::ActOnMidi - Acts on a MIDI event in the track, and the current track
location. It assumed the delta time has already been read, and that the pointer
is right on the MIDI event. This will mobe the pointer (dwOffsetCur) to just past
the MIDI event, at the next delta time.

inputs
   PMFTRACKINFO         pti - Track information
   PCMidiInstance       pInstance - MIDI to play this out to. If NULL then it
                        won't actually play any MIDI, but will still do stuff
                        like change the tempo
returns
   BOOL - TRUE if OK, FALSE if came to the end-of-track marker (or an error)
*/
BOOL CMidiFile::ActOnMidi (PMFTRACKINFO pti, PCMidiInstance pInstance)
{
   // read in the event
   if (pti->dwOffsetCur + 1 > m_memData.m_dwCurPosn)
      return FALSE;
   PBYTE pb = (PBYTE)m_memData.p + pti->dwOffsetCur;
   BYTE bEvent = pb[0];
   if (bEvent & 0x80) {
      pb++;
      pti->dwOffsetCur++;
      pti->bLastCommand = bEvent;
   }
   else if (pti->bLastCommand)
      bEvent = pti->bLastCommand;
   else
      return FALSE;  // error

   // switch based on the event
   switch (bEvent & 0xf0) {
      case 0x80: // note off
      case 0x90: // note on
      case 0xa0: // aftertouch
      case 0xb0: // controller
      case 0xe0: // pitch bend
         // these take two params
         if (pti->dwOffsetCur + 2 > m_memData.m_dwCurPosn)
            return FALSE;  // error
         if (pInstance)
            pInstance->MidiMessage (bEvent, pb[0], pb[1]);
         pti->dwOffsetCur += 2;
         pb += 2;

         return TRUE;

      case 0xc0: // program change
      case 0xd0: // aftertouch
         // tkes one byte
         if (pti->dwOffsetCur + 1 > m_memData.m_dwCurPosn)
            return FALSE;  // error
         if (pInstance)
            pInstance->MidiMessage (bEvent, pb[0]);
         pti->dwOffsetCur++;
         pb++;


         return TRUE;

      case 0xf0:
         // fall through
         break;
      default:
         return FALSE;
   } // switch

   // got an 0xf0 for the high nibble.. now need to see whaat that means

   // if it's not 0xff then it's some sort of system exsluve, so handle that
   if (bEvent != 0xff) {
      DWORD dwLen;
      if (!GetVarLength(&pti->dwOffsetCur, &dwLen))
         return FALSE;

      if (dwLen + pti->dwOffsetCur > m_memData.m_dwCurPosn)
         return FALSE;
      pti->dwOffsetCur += dwLen;

      return TRUE;
   }

   // else, it's a meta event... get the type
   if (pti->dwOffsetCur + 1 > m_memData.m_dwCurPosn)
      return FALSE;
   BYTE bType = pb[0];
   pb++;
   pti->dwOffsetCur++;

   // get the size
   DWORD dwLen;
   if (!GetVarLength(&pti->dwOffsetCur, &dwLen))
      return FALSE;
   pb = (PBYTE)m_memData.p + pti->dwOffsetCur;
   if (dwLen + pti->dwOffsetCur > m_memData.m_dwCurPosn)
      return FALSE;
   pti->dwOffsetCur += dwLen;

   switch (bType) {
   case 0x2f:  // end of track
      return FALSE;
   case 0x51:  // tempo
      if (dwLen != 3)
         return FALSE; // should be 3
      m_fBeatDur = (double)(pb[2] | ((DWORD)pb[1] << 8) | ((DWORD)pb[0] << 16)) / 1000000.0;
      // fall on through
      break;
   }

   return TRUE;

}



/*************************************************************************************
CMidiFile::OpenInternal - Handles the opening once all the data has been loaded into
m_memData.

returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CMidiFile::OpenInternal ()
{
   DWORD dwPosn = 0;
   DWORD dwID, dwSize;

   // read in the first chunk
   WORD w;
   DWORD dwTrackCount;
   if (!GetHeader (&dwPosn, &dwID, &dwSize))
      return FALSE;
   if (dwID != mmioFOURCC('M', 'T', 'h', 'd'))
      return FALSE;
   if (dwSize != 3*sizeof(WORD))  // midi header must be 6 bytes long
      return FALSE;
   if (!GetWORD (&dwPosn, &w))
      return FALSE;
   m_dwTrackFormat = w;
   if (m_dwTrackFormat >= 3)
      return FALSE;
   if (!GetWORD (&dwPosn, &w))
      return FALSE;
   dwTrackCount = w;
   if (!dwTrackCount)
      return FALSE;  // error
   if (!GetWORD (&dwPosn, &w))
      return FALSE;

   m_fBeatDur = DEFAULTBEATDUR;
   if (!(w & 0x8000)) {
      m_dwTicksPerBeat = w & 0x7fff;
      if (!m_dwTicksPerBeat)
         return FALSE;  //error
      m_fTickDurHeader = 0;
   }
   else {
      m_dwTicksPerBeat = 0;   // so know use the original tick duration

      double fFrame;

      // SMPTE
      switch ((w >> 8) & 0x7f) {
      case 24:
         fFrame = 24;
         break;
      case 25:
         fFrame = 25;
         break;
      case 29:
         fFrame = 29.97;
         break;
      case 30:
         fFrame = 30;
         break;
      default:
         return FALSE;  // unknown
      }
      w = w & 0xff;
      if (!w)
         return FALSE;  // cant have this
      m_fTickDurHeader = 1.0 / fFrame / (double)w;
   }

   // read in all the tracks
   DWORD i;
   MFTRACKINFO ti;
   memset (&ti, 0, sizeof(ti));
   m_lMFTRACKINFO.Init (sizeof(MFTRACKINFO));
   for (i = 0; i < dwTrackCount; i++) {
      if (!GetHeader(&dwPosn, &dwID, &dwSize))
         return FALSE;
      if (dwID != mmioFOURCC('M', 'T', 'r', 'k'))
         return FALSE;

      // add it
      ti.dwOffsetCur = ti.dwOffsetStart = dwPosn;
      ti.dwSize = dwSize;
      m_lMFTRACKINFO.Add (&ti);

      // jump to the next one
      dwPosn += dwSize;
   } // i

   // determine the length of each track
   PMFTRACKINFO pti = (PMFTRACKINFO)m_lMFTRACKINFO.Get(0);
   double fTime;
   m_fTimeTotal = 0;
   for (i = 0; i < m_lMFTRACKINFO.Num(); i++, pti++) {
      // clear out
      pti->fTimeTotal = 0;
      pti->dwOffsetCur = pti->dwOffsetStart;
      pti->bLastCommand = 0;

      // repeat
      while (TRUE) {
         fTime = GetDeltaTime (pti);
         if (fTime < 0)
            break;
         pti->fTimeTotal += fTime;

         // skip over the info
         if (!ActOnMidi(pti, NULL))
            break;
      } // while TRUE

      // restore the current offset
      pti->dwOffsetCur = pti->dwOffsetStart;

      // adjust the total time
      if (m_dwTrackFormat == 2) // syncrhonous
         m_fTimeTotal += pti->fTimeTotal;
      else
         m_fTimeTotal = max(m_fTimeTotal, pti->fTimeTotal);
   } // i

   // reset the beat duration
   m_fBeatDur = DEFAULTBEATDUR;

   // else done
   return TRUE;
}



/*************************************************************************************
CMidiFile::TrackPullOutDelay - Pulls out the delay to the next time and adds it
to pti->fDelayTillNext.

inputs
   PMFTRACKINFO         pti - Track information
returns
   none
*/
void CMidiFile::TrackPullOutDelay (PMFTRACKINFO pti)
{
   // if at the end of the track do nothing
   if (pti->fReachedEOT)
      return;

   double fTime = GetDeltaTime (pti);
   if (fTime < 0) {
      // error
      pti->fReachedEOT = TRUE;
      return;
   }

   // increase
   pti->fDelayTillNext += fTime;
}


/*************************************************************************************
CMidiFile::Start - Start playing

inputs
   PCMidiMix         pMidiMix - MIDI mixer to use
returns
   BOOL - TRUE if success
*/
BOOL CMidiFile::Start (PCMidiMix pMidiMix)
{
   if (m_pMidiInstance || !m_lMFTRACKINFO.Num())
      return FALSE;

   m_pMidiInstance = pMidiMix->InstanceNew ();
   if (!m_pMidiInstance)
      return FALSE;
   m_pMidiInstance->VolumeSet (m_awVolume[0], m_awVolume[1]);

   // wipe out some settings
   m_fBeatDur = DEFAULTBEATDUR;
   PMFTRACKINFO pti = (PMFTRACKINFO)m_lMFTRACKINFO.Get(0);
   DWORD i;
   for (i = 0; i < m_lMFTRACKINFO.Num(); i++, pti++) {
      pti->dwOffsetCur = pti->dwOffsetStart;
      pti->bLastCommand = 0;
      pti->fReachedEOT = FALSE;
      pti->fDelayTillNext = 0;

      // pull out the delay for each one
      TrackPullOutDelay (pti);
   }
   m_dwCurTrack = 0;

   QueryPerformanceCounter ((LARGE_INTEGER*)&m_iLastTime);

   // done
   return TRUE;
}


/*************************************************************************************
CMidiFile::Stop - Stops playing.

retunrs
   BOOL - TRUE if success, FALSE if already stopped
*/
BOOL CMidiFile::Stop (void)
{
   if (m_pMidiInstance) {
      delete m_pMidiInstance;
      m_pMidiInstance = NULL;
      return TRUE;
   }
   else
      return FALSE;
}



/*************************************************************************************
CMidiFile::PlayAdvanceTrack - Advances a specific track.

inputs
   PMFTRACKINFO         pti - Track information
   double   fDelta - Amount of time (in seconds) since the last time this was called.
returns
   double - Amount of the time NOT used up because the track has reached an end point.
*/
double CMidiFile::PlayAdvanceTrack (PMFTRACKINFO pti, double fDelta)
{
   // repeat
   double fChange;
   while (!pti->fReachedEOT) {
      // if we have time left for an event, subtract some time from it so it occurs
      if (pti->fDelayTillNext > 0) {
         fChange = min(pti->fDelayTillNext, fDelta);
         pti->fDelayTillNext -= fChange;
         fDelta -= fChange;
      }
      if (pti->fDelayTillNext > EPSILON)
         break;

      // act on this
      if (!ActOnMidi (pti, m_pMidiInstance))
         pti->fReachedEOT = TRUE;   // end

      // get the next oen
      TrackPullOutDelay (pti);

      // stop if still have a delay till next and no delta
      if ((pti->fDelayTillNext > 0) && (fDelta <= 0))
         break;
   } // while not end of track

   return fDelta;
}


/*************************************************************************************
CMidiFile::PlayAdvance - Advances playing of the MIDI file.

inputs
   double   fDelta - Amount of time (in seconds) since the last time this was called.
            If < 0 then do own time calculations
returns
   BOOL - TRUE if played some more, FALSE if has come to the end of the MIDI
            file (and therefore has automatically stopped)
*/
BOOL CMidiFile::PlayAdvance (double fDelta)
{
   // if not started then error
   if (!m_pMidiInstance)
      return FALSE;

   // if fDelta < 0 then do own time calculations
   if (fDelta < 0) {
      __int64 iLast = m_iLastTime;
      QueryPerformanceCounter ((LARGE_INTEGER*)&m_iLastTime);
      iLast = m_iLastTime - iLast;
      iLast = (iLast * (256 * 256)) / m_iPerfFreq;
      fDelta = (double)iLast / (double)(256 * 256);
   }

   PMFTRACKINFO pti = (PMFTRACKINFO)m_lMFTRACKINFO.Get(0);

   // if sequential then one thing
   if (m_dwTrackFormat == 2) {
      // NOTE: I dont think this will ever be hit

      // one after the other
      while (m_dwCurTrack < m_lMFTRACKINFO.Num()) {
         fDelta = PlayAdvanceTrack (pti + m_dwCurTrack, fDelta);
         if (pti[m_dwCurTrack].fReachedEOT)
            m_dwCurTrack++;
         if (fDelta <= 0)
            return TRUE;   // nothing left
      }
      if (m_dwCurTrack >= m_lMFTRACKINFO.Num())
         Stop();
      return FALSE;
   }

   // else, play all at once
   DWORD i;
   BOOL fStillGoing = FALSE;
   for (i = 0; i < m_lMFTRACKINFO.Num(); i++, pti++) {
      PlayAdvanceTrack (pti, fDelta);
      if (!pti->fReachedEOT)
         fStillGoing = TRUE;
   } // i
  
   if (!fStillGoing)
      Stop();
   return fStillGoing;
}

/*************************************************************************************
CMidiFile::VolumeSet - Sets the volume to play at.

inputs
   WORD        wLeft - Left volume, AQ_NOVOLCHANGE is full volume
   WORD        wRight - Rightvolume
returns
   none
*/
void CMidiFile::VolumeSet (WORD wLeft, WORD wRight)
{
   if ((wLeft == m_awVolume[0]) && (wRight == m_awVolume[1]))
      return; // no change

   m_awVolume[0] = wLeft;
   m_awVolume[1] = wRight;

   // set the internal volume for all the channels
   if (m_pMidiInstance)
      m_pMidiInstance->VolumeSet (m_awVolume[0], m_awVolume[1]);
}
