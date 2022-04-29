/*************************************************************************************
CMidiMix.cpp - Code for managing MIDI.

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


#define PANVOLUMEMAX       (128 * 128 - 1)
#define DEFMAINVOLUME      PANVOLUMEMAX
#define DEFPAN             (PANVOLUMEMAX / 2 + 1)


/*************************************************************************************
CMidiMix::Constructor and destructor
*/
CMidiMix::CMidiMix (void)
{
   m_hMidi = FALSE;
   m_dwTimeCount = 1;
   memset (m_afClaimed, 0, sizeof(m_afClaimed));
   memset (m_adwLastClaimed, 0, sizeof(m_adwLastClaimed));

   m_afClaimed[DRUMCHANNEL] = TRUE; // always pretend this is claimed

   // try to open the midi devide
   midiOutOpen (&m_hMidi, MIDI_MAPPER, NULL, 0, CALLBACK_NULL);
}

CMidiMix::~CMidiMix (void)
{
   if (m_hMidi)
      midiOutClose (m_hMidi);
   m_hMidi = NULL;
}


/*************************************************************************************
CMidiMix::ChannelClaim - Claims a channel for exclusive use

inputs
   DWORD          dwOrigChannel - Original channel (0..15) in the MIDI file
returns
   DWORD - Mapped channel, what it's mapped to. -1 if cant get one
*/
DWORD CMidiMix::ChannelClaim (DWORD dwOrigChannel)
{
   // first try to open the device if it isn't already
   if (!m_hMidi) {
      midiOutOpen (&m_hMidi, MIDI_MAPPER, NULL, 0, CALLBACK_NULL);
      if (!m_hMidi)
         return -1;
   }

   // if it's the drum channel, always return the drum channel back, so it's shared
   if (dwOrigChannel == DRUMCHANNEL)
      return DRUMCHANNEL;

   // else, find one to claim
   DWORD i;
   DWORD dwBestIndex = -1;
   DWORD dwBestTime;
   for (i = 0; i < MIDICHANNELS; i++) {
      if (m_afClaimed[i])
         continue;
      if ((dwBestIndex == -1) || (m_adwLastClaimed[i] < dwBestTime)) {
         dwBestIndex = i;
         dwBestTime = m_adwLastClaimed[i];
      }
   } // i

   if (dwBestIndex == -1)
      return -1;  // cant claim

   // else claim
   m_afClaimed[dwBestIndex] = TRUE;
   m_adwLastClaimed[dwBestIndex] = m_dwTimeCount++;
   return dwBestIndex;
}


/*************************************************************************************
CMidiMix::ChannelRelease - Releases a channel that was successfully claimed by
ChannelClaim.

inputs
   DWORD          dwChannel - Value returned by ChannelClaim()
*/
void CMidiMix::ChannelRelease (DWORD dwChannel)
{
   if (dwChannel == DRUMCHANNEL)
      return;  // this doenst get released

   // else release
   m_afClaimed[dwChannel] = FALSE;
   m_adwLastClaimed[dwChannel] = m_dwTimeCount++;

}


/*************************************************************************************
CMidiMix::InstanceNew - Creates a new MIDI instance that will automatically map
channels into the MIDI device.

returns
   PCMidiInstance - Midi intance. Do NOT delete the CMidiMix until all MIDI isntances
                     are delted
*/
PCMidiInstance CMidiMix::InstanceNew (void)
{
   return new CMidiInstance (this);
}



/*************************************************************************************
CMidiMix::MidiMessage - Sends a MidiMessage.

inputs
   BYTE        bStatus - Midi status
   BYTE        bData1 - Data 1
   BYTE        bData2 - Data 2;
returns
   BOOL - TRUE if it was send
*/
__inline BOOL CMidiMix::MidiMessage (BYTE bStatus, BYTE bData1, BYTE bData2)
{
   if (!m_hMidi) {
      midiOutOpen (&m_hMidi, MIDI_MAPPER, NULL, 0, CALLBACK_NULL);
      if (!m_hMidi)
         return FALSE;
   }

   DWORD dw;
   PBYTE pb = (PBYTE)&dw;
   pb[0] = bStatus;
   pb[1] = bData1;
   pb[2] = bData2;
   pb[3] = 0;

   return !midiOutShortMsg (m_hMidi, dw);
}


/*************************************************************************************
CMidiInstance::Constructor and destructor
*/
CMidiInstance::CMidiInstance (PCMidiMix pMidiMix)
{
   m_pMidiMix = pMidiMix;

   m_awVolume[0] = m_awVolume[1] = AQ_NOVOLCHANGE;

   // clear out channel info
   DWORD i;
   memset (m_aChannel, 0, sizeof(m_aChannel));
   for (i = 0; i < MIDICHANNELS; i++)
      m_aChannel[i].dwChannelMap = -1;
}


CMidiInstance::~CMidiInstance (void)
{
   // free up all the channels
   DWORD i;
   for (i = 0; i < MIDICHANNELS; i++)
      if (m_aChannel[i].dwChannelMap != -1)
         ChannelRelease (i);
}


/*************************************************************************************
CMidiInstance::ChannelClaimIfNotAlready - Claims a channel for the MIDI device
if it isn't already claimed.
*/
__inline BOOL CMidiInstance::ChannelClaimIfNotAlready (DWORD dwChannel)
{
   if (m_aChannel[dwChannel].dwChannelMap != -1)
      return TRUE;

   // else claim
   m_aChannel[dwChannel].dwChannelMap = m_pMidiMix->ChannelClaim (dwChannel);
   if (m_aChannel[dwChannel].dwChannelMap == -1)
      return FALSE;  // not claimed

   // BUGFIX - Move these calls from ChannelRelease()
   // restore some values
   DWORD dwMap = m_aChannel[dwChannel].dwChannelMap;
   BYTE bChannelMode = 0xb0 | (BYTE)dwMap;
   m_pMidiMix->MidiMessage (0xe0 | (BYTE)dwMap, 0, 0x40); // pitch bend off
   m_pMidiMix->MidiMessage (bChannelMode, 121, 63); // reset all controllers

   // else, default volume and pan
   m_aChannel[dwChannel].wMidiVolume = DEFMAINVOLUME;
   m_aChannel[dwChannel].wMidiPan = DEFPAN;
   VolumeSetInternal (dwChannel, 0xffff);

   return TRUE;
}


/*************************************************************************************
CMidiInstance::ChannelRelease - Releases a channel that's no longer in use
*/
BOOL CMidiInstance::ChannelRelease (DWORD dwChannel)
{
   if (m_aChannel[dwChannel].dwChannelMap == -1)
      return FALSE;  // not taken


   // turn off all the notes
   AllNotesOff (dwChannel);

   // BUGFIX - Moved code to restore defaults to the ChannelClaim part

   m_pMidiMix->ChannelRelease (m_aChannel[dwChannel].dwChannelMap);
   m_aChannel[dwChannel].dwChannelMap = -1;

   return TRUE;
}


/*************************************************************************************
CMidiMix::MidiMessage - Sends a MidiMessage.

inputs
   BYTE        bStatus - Midi status
   BYTE        bData1 - Data 1
   BYTE        bData2 - Data 2;
returns
   BOOL - TRUE if it was send
*/
BOOL CMidiInstance::MidiMessage (BYTE bStatus, BYTE bData1, BYTE bData2)
{
   BYTE bNibble = bStatus & 0xf0;
   if (bNibble < 0xf0) {
      // channel-specific message
      DWORD dwChannel = bStatus & 0x0f;
      PMINSTCHANNEL pc = &m_aChannel[dwChannel];
      if (pc->dwChannelMap == -1) {
         // need to claim
         if (!ChannelClaimIfNotAlready(dwChannel))
            return FALSE;  // cant send
      }
      bStatus = bNibble | (BYTE)pc->dwChannelMap;

      // switch based on the nibble
      switch (bNibble) {
      case 0x80:  // note off
         // remember that it's ff
         pc->adwNoteOn[bData1 / 32] &= ~(1 << (bData1 % 32));
         break;
      case 0x90:  // note on
         // remember that it's on
         pc->adwNoteOn[bData1 / 32] |= (1 << (bData1 % 32));
         break;
      case 0xb0:  // channel mode message
         switch (bData1) {
         case 1: // modulation
         case 1+32: // modulation
         case 2: // breath contorller
         case 2+32: // breath contorller
         case 4: // foot controller
         case 4+32: // foot controller
         case 5: // portamento time
         case 5+32: // portamento time
         case 11: // expression controller
         case 11+32: // expression controller
         case 64: // damper
         case 65: // portamento
         case 66: // sostenuto
         case 67: // soft petal
         case 68: // legato
         case 69: // hold2
         case 84: // portamento
            break;   // allow to pass through

         case 7: // main volume
            m_aChannel[dwChannel].wMidiVolume = (m_aChannel[dwChannel].wMidiVolume & 0x7f) |
               ((DWORD)bData2 << 7);
            VolumeSetInternal (dwChannel, 0x01);
            return TRUE;

         case 7+32: // main volume
            m_aChannel[dwChannel].wMidiVolume = (m_aChannel[dwChannel].wMidiVolume & (0x7f << 7)) |
               bData2;
            VolumeSetInternal (dwChannel, 0x02);
            return TRUE;

         case 10: // pan
            m_aChannel[dwChannel].wMidiPan = (m_aChannel[dwChannel].wMidiPan & 0x7f) |
               ((DWORD)bData2 << 7);
            VolumeSetInternal (dwChannel, 0x04);
            return TRUE;

         case 10+32: // pan
            m_aChannel[dwChannel].wMidiPan = (m_aChannel[dwChannel].wMidiPan & (0x7f << 7)) |
               bData2;
            VolumeSetInternal (dwChannel, 0x08);
            return TRUE;


         case 121: // reset all controllers
            // first send reset, then set volume again
            if (!m_pMidiMix->MidiMessage (bStatus, bData1, bData2))
               return FALSE;
            // BUGFIX - Reset MIDI volume and pan for the channel
            m_aChannel[dwChannel].wMidiVolume = DEFMAINVOLUME;
            m_aChannel[dwChannel].wMidiPan = DEFPAN;
            VolumeSetInternal (dwChannel, 0xffff);
            return TRUE;

         case 123: // all notes off
            return AllNotesOff (dwChannel);

         default:
         case 122: // local control on/off
            return TRUE;   // dont send
         } // switch
         break;
      } // switch bNibble
   }
   else {
      // system message, not channel
      // trap all system messages
      return TRUE;
   }

   // if get here send it out
   return m_pMidiMix->MidiMessage (bStatus, bData1, bData2);
}



/*************************************************************************************
CMidiInstance::AllNotesOff - Turn all the notes off for the given channel.

inputs
   DWORD       dwChannel - Channel
returns
   BOOL - TRUE if success
*/
BOOL CMidiInstance::AllNotesOff (DWORD dwChannel)
{
   PMINSTCHANNEL pc = &m_aChannel[dwChannel];
   BYTE bNoteOff = 0x80 | (BYTE)pc->dwChannelMap;
   DWORD i, j;
   for (i = 0; i < 128/32; i++) {
      DWORD dw = pc->adwNoteOn[i];
      for (j = i*32; dw; j++, dw >>= 1) {
         if (!(dw & 0x01))
            continue;   // not on

         // else, it's on
         m_pMidiMix->MidiMessage (bNoteOff, (BYTE)j, 0x7f);
      } // j
   } // i

   // set flags
   memset (pc->adwNoteOn, 0, sizeof(pc->adwNoteOn));

   return TRUE;
}


/*************************************************************************************
CMidiInstance::VolumeSet - Sets the volume to play at.

inputs
   WORD        wLeft - Left volume, AQ_NOVOLCHANGE is full volume
   WORD        wRight - Rightvolume
returns
   none
*/
void CMidiInstance::VolumeSet (WORD wLeft, WORD wRight)
{
   if ((wLeft == m_awVolume[0]) && (wRight == m_awVolume[1]))
      return; // no change

   m_awVolume[0] = wLeft;
   m_awVolume[1] = wRight;

   // loop through all the claimed channels and set them
   DWORD i;
   for (i = 0; i < MIDICHANNELS; i++)
      if (m_aChannel[i].dwChannelMap != -1)
         VolumeSetInternal (i, 0xffff);
}



/*************************************************************************************
CMidiInstance::VolumeSetInternal - Sends the volume messages the main MIDI device.
NOTE: The channel MUST have been claimed

inputs
   DWORD       dwChannel - Midi channel
   DWORD       dwFlags - 0x01 to send hibyte of volume, 0x02 to send lowbyte of volume
                        0x04 to send hibyte of pan, 0x08 to send lowbyte of pan
*/
void CMidiInstance::VolumeSetInternal (DWORD dwChannel, DWORD dwFlags)
{
   // figure out main pain/volume
   WORD wVol = m_awVolume[0]/2 + m_awVolume[1]/2;
   int iPan;
   WORD wMin = min(m_awVolume[0], m_awVolume[1]);
   WORD wMax = max(m_awVolume[0], m_awVolume[1]);
   if (wMax) {
      iPan = (256 - (int)wMin * 256 / (int)wMax) * DEFPAN / 256;
            // BUGFIX - Was PANVOLUMEMAX but was too much panning
      if (m_awVolume[0] > m_awVolume[1])
         iPan *= -1;
   }
   else
      iPan = 0;

   // bits to send
   DWORD dwVolBits = (DWORD)m_aChannel[dwChannel].wMidiVolume * (DWORD) wVol / AQ_NOVOLCHANGE;
   dwVolBits = min(dwVolBits, PANVOLUMEMAX);
   int iPanBits = (int)(DWORD)m_aChannel[dwChannel].wMidiPan + iPan;
   iPanBits = min (iPanBits, PANVOLUMEMAX);
   iPanBits = max (iPanBits, 0);

   // send them
   BYTE bStatus = (BYTE)m_aChannel[dwChannel].dwChannelMap | 0xb0;
   if (dwFlags & 0x01)
      m_pMidiMix->MidiMessage (bStatus, 7, (BYTE)(dwVolBits >> 7));
   if (dwFlags & 0x02)
      m_pMidiMix->MidiMessage (bStatus, 7+32, (BYTE)(dwVolBits & 0x7f));
   if (dwFlags & 0x04)
      m_pMidiMix->MidiMessage (bStatus, 10, (BYTE)(iPanBits >> 7));
   if (dwFlags & 0x08)
      m_pMidiMix->MidiMessage (bStatus, 10+32, (BYTE)(iPanBits & 0x7f));

}
