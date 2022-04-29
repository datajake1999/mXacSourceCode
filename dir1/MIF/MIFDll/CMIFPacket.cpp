/*************************************************************************************
CMIFPacket - Code to send packets to/from the internet.

begun 28/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include "escarpment.h"
#include "..\buildnum.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "resource.h"

#define WORDALIGN(x)             (((x+1)/2)*2)

#define TIMEBEFOREPING           (60*1000)      // if dont hear anything from client for this long then ping
#define TIMEBEFOREDISCONNECT     (TIMEBEFOREPING*2)   // if dont hear anything from client for this long then send disconnect

#define COMP_MAXUNIQUEBITS       3     // 3 bits (=8) unique chars max
#define COMP_MAXUNIQUE           (1<<COMP_MAXUNIQUEBITS) // maximum unique chars
#define COMP_MAXREPEATBLOCK      (256 - COMP_MAXUNIQUE*2) // maximum chars in a repeat block
#define COMP_SAFEBUFFER          (COMP_MAXREPEATBLOCK * sizeof(WCHAR) + 16)

#define COMP_BUFFERSIZE          16384 // must be larger than largest compression packet ~ 512 bytes
         // BUGFIX - Just upped the size of the buffers so would be more efficient
         // when sending to telnet. But, if make too long too large a delay
         // between buffered data and interrup for more immediate info
         // So, went from 8192 to 4096

         // BUGFIX - Since dealing with high-speed broadband for most user, upped
         // the buffer size from 4096 to 16384


// CIRCUMREALITYPACKETHDR - Header data for data send over message
typedef struct {
   DWORD             dwType;     // type of packet, CIRCUMREALITYPACKET_XXX
   DWORD             dwSize;     // number of bytes of data
} CIRCUMREALITYPACKETHDR, *PCIRCUMREALITYPACKETHDR;

// CIRCUMREALITYPACKETRECEIVE - Info about packet that received
typedef struct {
   DWORD             dwType;     // type of packed, CIRCUMREALITYPACKET_XXX
} CIRCUMREALITYPACKETRECIEVE, *PCIRCUMREALITYPACKETRECEIVE;

// CIRCUMREALITYPACKETSEND - Info about packet being sent
typedef struct {
   DWORD             dwType;     // type of packed, CIRCUMREALITYPACKET_XXX
   PVOID             pExtraData; // if extra data is appended to this packet, not NULL
   DWORD             dwExtraData;// amount of extra data to send
   DWORD             dwSendElem; // which info is being sent, 0 = the header,
                                 // 1 = main data, 2 = pExtraData
   DWORD             dwSendOffset;   // offset into the data being sent, in bytes

   DWORD             dwTotalData;   // total amount of data to send (including header)
   DWORD             dwTotalOffset; // current offset into the total amount of data

   CIRCUMREALITYPACKETHDR      hdr;        // hdr, used for sending
} CIRCUMREALITYPACKETSEND, *PCIRCUMREALITYPACKETSEND;


#define  COMPDICTSIZE            0x8000      // number of characters in compression dictionary
   // BUGFIX - Works best at 0x10000, but slower (and lots of memory per user), so reduced size
   // NOTE: With optimizations that put in could increase again, but will keep at 0x8000 to keep
   // memory low

/*************************************************************************************
CCircumrealityPacket::Constructor and destructor
*/
CCircumrealityPacket::CCircumrealityPacket (void)
{
   m_fCompInSeparateThread = FALSE;
   m_pCritSecSeparateThread = NULL;
   m_plPCCircumrealityPacketSeparateThread = NULL;
   m_paSignalSeparateThread = NULL;
   m_lCompressOrNot.Init (sizeof(int));
   m_iMaxQueuedReceive = 1000000000;   // default to 1 GB max to receive


   LARGE_INTEGER     liPerCountFreq;
   QueryPerformanceFrequency (&liPerCountFreq);
   m_iSendQueueTimeWait = ( *((__int64*)&liPerCountFreq)) / 20;
      // wait 1/20th of second (50 ms) to queue up data before send, to minimize packets running
      // back and forth
   m_iSendQueueTimeStart = 0;

   InitializeCriticalSection (&m_CritSec);
   memset (&m_PI, 0, sizeof (m_PI));
   m_lReceive.Init (sizeof(CIRCUMREALITYPACKETRECIEVE));
   DWORD dwChannel;
   for (dwChannel = 0; dwChannel < NUMPACKCHAN; dwChannel++)
      m_alSend[dwChannel].Init (sizeof(CIRCUMREALITYPACKETSEND));
   m_iErrLast = m_iErrFirst = 0;
   m_pszErrLast = m_pszErrFirst = NULL;

   m_iStartTime = m_iStartCount = 0;

   m_PI.dwBytesPerSec = 28000 / 8;  // default to assumeing 28kbaud comm rate

   m_dwSendChan = (DWORD)0;   // but since chanleft == 0, counts as -1
   m_wSendChanLeft = 0;

   m_dwReceiveChan = -1;   // start receiving header
   m_wReceiveChanLeft = sizeof(m_ReceiveChanHeader);
   memset (&m_ReceiveChanHeader, 0, sizeof(m_ReceiveChanHeader));
   memset (&m_SendChanHeader, 0, sizeof(m_SendChanHeader));

   // compression
   DWORD i, j;
   for (i = 0; i < 2; i++) {
      m_amemDecomp[i].Required (COMP_BUFFERSIZE);
      m_amemComp[i].Required (COMP_BUFFERSIZE);
      m_amemDict[i].Required (COMPDICTSIZE * sizeof(WCHAR));
      m_adwDictLoc[i] = 0;
      m_fEndOfOut = TRUE;

      WCHAR *psz = (PWSTR)m_amemDict[i].p;
      if (i)
         memcpy (psz, m_amemDict[0].p, COMPDICTSIZE * sizeof(WCHAR));
      else
         for (j = 0; j < COMPDICTSIZE; j++, psz++)
            psz[0] = (WCHAR)j;
   } // i

   m_wCompTick = 0;
   m_wCompBitHistory = (WORD)-1;

   // remember the connect time
   LARGE_INTEGER     liCount;
   QueryPerformanceFrequency (&liPerCountFreq);
   QueryPerformanceCounter (&liCount);
   m_PI.iConnectTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);
   m_PI.iSendLast = m_PI.iReceiveLast = m_PI.iConnectTime;  // just so wont send ping for awhile
}

CCircumrealityPacket::~CCircumrealityPacket (void)
{
   // end of data
   CompSendEndOfData();

   DeleteCriticalSection (&m_CritSec);
}



/************************************************************************************
CCircumrealityPacket::Init - Call this to initialze the object to act as either
a server socket, or a client socket.

NOTE: This MUST be called in the creator thread.

inputs
   SOCKET         iSocket - This is the socket to use, or INVALID_SOCKET if no
                  socket and use the window name instead. NOTE: This socket
                  will be freed when the object is deleted
   char           *pszNameThis - Window name to use for the end of the socket
                     If iSocket == INVALID_SOCKET, then this should be
                     a string representation of the IP address.
   char           *pszNameServer - Name for the socket to connect to. If this is NULL
                     then this socket will merely sit around and wait for a connection
                     to happen. If the server doesn't exist this will error out
returns
   BOOL - TRUE if success
*/
BOOL CCircumrealityPacket::Init (SOCKET iSocket, char *pszNameThis, char *pszNameServer)
{
   // since connecting on local host, set the name
   if (iSocket == INVALID_SOCKET)
      wcscpy (m_PI.szIP, L"Local");
   else
      MultiByteToWideChar (CP_ACP, 0, pszNameThis, -1, m_PI.szIP, sizeof(m_PI.szIP)/sizeof(WCHAR));

   return m_WinSock.Init (iSocket, pszNameThis, pszNameServer);
}


/************************************************************************************
CCircumrealityPacket::SocketGet - Returns the SOCKET that's used by the packet, or INVALID_SOCKET
if it's not using sockets (and is instead going through windows posts)
*/
SOCKET CCircumrealityPacket::SocketGet (void)
{
   return m_WinSock.m_iSocket;
}


/************************************************************************************
CCircumrealityPacket::MaintinenceSendIfCan - This tries calling WinSock and sends data.
It will send as much data as winsock will allow it to send.

NOTE: This must be called from the creator thread

inputs
   __int64        iTimeMilli - A time in milliseconds - counter so can calculate
                     the incoming data rate and other stuff
returns
   int - 0, or a winsock error
*/
int CCircumrealityPacket::MaintinenceSendIfCan (__int64 iTimeMilli)
{
   int iErr = 0;
   PCWSTR pszErr = NULL;
   EnterCriticalSection (&m_CritSec);

   BOOL fMoreChan = TRUE;
   while (fMoreChan) {
      // if there's no data left to send then blank out
      if (!m_wSendChanLeft) {
         if (m_dwSendChan == (DWORD)-1) {
            m_dwSendChan = m_SendChanHeader.wChannel; // will send actual data
            m_wSendChanLeft = m_SendChanHeader.wSize;
         }
         else {
            // try to find some data to send...
            DWORD dwChannel, dwTotal, i;
            for (dwChannel = 0; dwChannel < NUMPACKCHAN; dwChannel++)
               if (m_alSend[dwChannel].Num())
                  break;
            if (dwChannel >= NUMPACKCHAN) {
               CompSendEndOfData();
               break;   // no more data to send
            }
            PCIRCUMREALITYPACKETSEND pSend = (PCIRCUMREALITYPACKETSEND)m_alSend[dwChannel].Get(0);

            m_SendChanHeader.wChannel = (WORD)dwChannel;

            // if this is channel 0 then can be any size. Otherwise based on m_dwMaxChanBufSize
            m_SendChanHeader.wSize = dwChannel ? (WORD)m_WinSock.m_dwMaxChanBufSize : 0xfffe;

            // size can't be larger than total amount of data
            m_SendChanHeader.wSize = (WORD)min((DWORD)m_SendChanHeader.wSize, pSend->dwTotalData - pSend->dwTotalOffset);

            // figure out the total left in the queue
            dwTotal = 0;
            for (dwChannel = 0; dwChannel < NUMPACKCHAN; dwChannel++) {
               pSend = (PCIRCUMREALITYPACKETSEND)m_alSend[dwChannel].Get(0);
               for (i = 0; i < m_alSend[dwChannel].Num(); i++, pSend++)
                  dwTotal += pSend->dwTotalData - pSend->dwTotalOffset;
            }
            m_SendChanHeader.dwLeftInQueue = dwTotal;


            m_dwSendChan = (DWORD)-1;  // will send the channel header
            m_wSendChanLeft = sizeof(m_SendChanHeader);
         }
      } // if nothing left to send

      // try to send the header?
      int iSent;

      if (m_dwSendChan == (DWORD)-1) {
         iSent = CompSend ((PBYTE)&m_SendChanHeader + (sizeof(m_SendChanHeader)-(DWORD)m_wSendChanLeft),
            m_wSendChanLeft, TRUE);
         if (iSent == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceSendIfCan::CompSend #1";
            goto done;
         }
         if (!iSent) {
            fMoreChan = FALSE;
            break;   // not sending anything, so buffer must be full
         }
         m_wSendChanLeft -= (WORD) iSent;  // know how much sent
         continue;
      } // if send header



      // else, sending a chunk
      PCIRCUMREALITYPACKETSEND pSend = (PCIRCUMREALITYPACKETSEND)m_alSend[m_dwSendChan].Get(0);
      DWORD dwDataSize = (DWORD)m_alSendPacket[m_dwSendChan].Size(0);
      PBYTE pData = (PBYTE)m_alSendPacket[m_dwSendChan].Get(0);

      // try sending the header
      if (pSend->dwSendElem == 0) {
         iSent = CompSend ((PBYTE)&pSend->hdr + pSend->dwSendOffset,
            min((DWORD)m_wSendChanLeft, sizeof(pSend->hdr) - pSend->dwSendOffset), TRUE);
         if (iSent == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceSendIfCan::CompSend #2";
            goto done;
         }
         if (!iSent) {
            fMoreChan = FALSE;
            break;   // not sending anything, so buffer must be full
         }
         m_PI.iSendBytes += iSent;  // know how much sent
         m_PI.iSendLast = iTimeMilli;  // remember this
            // NOTE: when put in compression will count pre-compressed bytes

         pSend->dwSendOffset += (DWORD)iSent;
         pSend->dwTotalOffset += (DWORD)iSent;
         m_wSendChanLeft -= (WORD)iSent;
         if (pSend->dwSendOffset < sizeof(pSend->hdr))
            continue;   // still sending this, but no more this go around

         // else, next
         pSend->dwSendElem  = 1;
         pSend->dwSendOffset = 0;
      } // try sending header

      // try sending the data
      if (pSend->dwSendElem == 1) {

         // want to compress the data in most main blocks, except voice chat
         BOOL fTryCompress;
         switch (pSend->dwType) {
         case CIRCUMREALITYPACKET_VOICECHAT:
         case CIRCUMREALITYPACKET_SERVERCACHETTS:
         case CIRCUMREALITYPACKET_SERVERCACHERENDER:
         case CIRCUMREALITYPACKET_CLIENTCACHETTS:
         case CIRCUMREALITYPACKET_CLIENTCACHERENDER:
            fTryCompress = FALSE;
            break;
         default:
            fTryCompress = TRUE;
            break;
         } // switch type

         iSent = CompSend (pData + pSend->dwSendOffset,
            min((DWORD)m_wSendChanLeft, dwDataSize - pSend->dwSendOffset), fTryCompress);
         if (iSent == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceSendIfCan::CompSend #3";
            goto done;
         }
         if (!iSent && dwDataSize) {
            fMoreChan = FALSE;
            break;   // not sending anything, so buffer must be full
         }
         m_PI.iSendBytes += iSent;  // know how much sent
            // NOTE: when put in compression will count pre-compressed bytes

         pSend->dwSendOffset += (DWORD)iSent;
         pSend->dwTotalOffset += (DWORD)iSent;
         m_wSendChanLeft -= (WORD)iSent;
         if (pSend->dwSendOffset < dwDataSize)
            continue;   // still sending this, but no more this go around

         // else, next
         pSend->dwSendElem = 2;
         pSend->dwSendOffset = 0;
      } // try sending header

      // try sending the extra data
      if ((pSend->dwSendElem == 2) && !pSend->pExtraData)
         pSend->dwSendElem = 3;
      if (pSend->dwSendElem == 2) {
         iSent = CompSend ((PBYTE)pSend->pExtraData + pSend->dwSendOffset,
            min((DWORD)m_wSendChanLeft, pSend->dwExtraData - pSend->dwSendOffset), FALSE);
               // NOTE: Extra data is never compressed since it's always binary
         if (iSent == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceSendIfCan::CompSend #4";
            goto done;
         }
         if (!iSent && pSend->dwExtraData) {
            fMoreChan = FALSE;
            break;   // not sending anything, so buffer must be full
         }
         m_PI.iSendBytes += iSent;  // know how much sent
            // NOTE: when put in compression will count pre-compressed bytes

         pSend->dwSendOffset += (DWORD)iSent;
         pSend->dwTotalOffset += (DWORD)iSent;
         m_wSendChanLeft -= (WORD)iSent;
         if (pSend->dwSendOffset < pSend->dwExtraData)
            continue;   // still sending this, but no more this go around

         // else, next
         pSend->dwSendElem = 3;
         pSend->dwSendOffset = 0;
      } // try sending header

      // extra byte?
      if ((pSend->dwSendElem == 3) && !((pSend->dwExtraData + dwDataSize) % 2))
         pSend->dwSendElem = 4;
      if (pSend->dwSendElem == 3) {
         // send extra byte so word aligned
         BYTE bExtra = 0;
         iSent = m_wSendChanLeft ? CompSend (&bExtra, 1, TRUE) : 0;
         if (iSent == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceSendIfCan::CompSend #5";
            goto done;
         }
         if (!iSent) {
            fMoreChan = FALSE;
            break;   // not sending anything, so buffer must be full
         }
         m_PI.iSendBytes += 1;  // know how much sent
         pSend->dwTotalOffset += 1;
         m_wSendChanLeft -= 1;

         // else, next
         pSend->dwSendElem = 5;
         pSend->dwSendOffset = 0;
      }


#ifdef _DEBUG
      if (pSend->dwTotalData != pSend->dwTotalOffset)
         OutputDebugString ("\r\nERROR: Mismatch in data sent");
#endif
      // if done with sending then remove
      m_alSendPacket[m_dwSendChan].Remove (0);
      m_alSend[m_dwSendChan].Remove (0);
   }  // while have data to send

done:
   LeaveCriticalSection (&m_CritSec);
   // BUGFIX - Only set m_iErr if ecnountered an error, so dont overwrite existing error
   if (iErr) {
      m_iErrLast = iErr;
      m_pszErrLast = pszErr;

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }
   }
   return iErr;
}



/************************************************************************************
CCircumrealityPacket::MaintinenceReceiveIfCan - This tries calling WinSock and receives data.
It will receive as much data as winsock will allow it to receive.

NOTE: This must be called from the creator thread

inputs
   __int64        iTimeMilli - A time in milliseconds - counter so can calculate
                     the incoming data rate and other stuff
   BOOL           *pfIncoming - Set to TRUE if a new message has come in.
returns
   int - 0, or a winsock error
*/
int CCircumrealityPacket::MaintinenceReceiveIfCan (__int64 iTimeMilli, BOOL *pfIncoming)
{
   if (pfIncoming)
      *pfIncoming = FALSE;

   int iErr = 0;
   PCWSTR pszErr = NULL;
   EnterCriticalSection (&m_CritSec);

   while (TRUE) {
      // see where data should be received to
      if (!m_wReceiveChanLeft) {
         if (m_dwReceiveChan == (DWORD)-1) {
            // update the amount of data that expect to receive
            m_PI.iReceiveBytesExpect = m_PI.iReceiveBytes + (__int64)m_ReceiveChanHeader.dwLeftInQueue;

            m_dwReceiveChan = m_ReceiveChanHeader.wChannel; // will Receive actual data
            m_wReceiveChanLeft = m_ReceiveChanHeader.wSize;
         }
         else {
            m_dwReceiveChan = -1;   // finished, wait for new header
            m_wReceiveChanLeft = sizeof(m_ReceiveChanHeader);
         }
      } // if no data left in receive channel

      // if expecting header, try to read that
      if (m_dwReceiveChan == (DWORD)-1) {
         int iRec = CompReceive ((PBYTE)&m_ReceiveChanHeader + (sizeof(m_ReceiveChanHeader) - m_wReceiveChanLeft),
            m_wReceiveChanLeft);
         if (iRec == SOCKET_ERROR) {
            iErr = m_WinSock.GetLastError(&pszErr);
            // pszErr = L"MaintinenceReceiveIfCan::CompReceive #1";
            goto done;
         }
         if (!iRec)
            break;   // not receiving anything, so maybe no more to get

         m_wReceiveChanLeft -= (WORD)iRec;
         continue;
      } // if receive channel info


      // else packet
      DWORD dwWant = (DWORD)m_amemRecieve[m_dwReceiveChan].m_dwCurPosn + 1024; // 1k block extensions
      
      // figure out how much already have queued
      __int64 iTotal = (__int64)dwWant;
      DWORD i;
      for (i = 0; i < m_lReceivePacket.Num(); i++) {
         iTotal += (__int64)m_lReceivePacket.Size(i);
         iTotal += sizeof(CIRCUMREALITYPACKETRECIEVE);
      } // i
      if (iTotal >= m_iMaxQueuedReceive) {
         iErr = 1;   // random error number just so reports an error
         pszErr = L"iTotal >= m_iMaxQueueReceived";
         goto done;
      }

      if ((dwWant > m_amemRecieve[m_dwReceiveChan].m_dwAllocated) && !m_amemRecieve[m_dwReceiveChan].Required (dwWant))
         goto done;  // error. no more memory

      dwWant = (DWORD)(m_amemRecieve[m_dwReceiveChan].m_dwAllocated - m_amemRecieve[m_dwReceiveChan].m_dwCurPosn);
      dwWant = min(dwWant, (DWORD)m_wReceiveChanLeft);
      int iRec = CompReceive ((PBYTE)m_amemRecieve[m_dwReceiveChan].p + m_amemRecieve[m_dwReceiveChan].m_dwCurPosn,
         dwWant);
      if (iRec == SOCKET_ERROR) {
         iErr = m_WinSock.GetLastError(&pszErr);
         // pszErr = L"MaintinenceReceiveIfCan::CompReceive #2";
         goto done;
      }
      if (!iRec)
         break;   // not receiving anything, so maybe no more to get

      // keep track of number of bytes received
      m_PI.iReceiveBytes += iRec;
      m_PI.iReceiveLast = iTimeMilli;  // remember this
         // NOTE: When do compression will keep track of post-decompressed bytes

      // if don't already have a start time for a packet then set this as one
      if (!m_iStartTime) {
         m_iStartTime = iTimeMilli;
         m_iStartCount = m_PI.iReceiveBytes;
      }

      m_amemRecieve[m_dwReceiveChan].m_dwCurPosn += (DWORD)iRec;
      m_wReceiveChanLeft -= (WORD)iRec;

      // pull out as many packets as possible
      while (TRUE) {
         // if this isn't large enough for a header just continue because need more
         // data to tell
         if (m_amemRecieve[m_dwReceiveChan].m_dwCurPosn < sizeof(CIRCUMREALITYPACKETHDR))
            break;

         // else, see if have enough data
         PCIRCUMREALITYPACKETHDR ph = (PCIRCUMREALITYPACKETHDR)m_amemRecieve[m_dwReceiveChan].p;

         if (WORDALIGN(ph->dwSize) + sizeof(*ph) > m_amemRecieve[m_dwReceiveChan].m_dwCurPosn)
            break;   // not enough data to complete the packet

         // else, have an entire packet, so pull out
         CIRCUMREALITYPACKETRECIEVE rc;
         rc.dwType = ph->dwType;
         m_lReceivePacket.Add (ph + 1, ph->dwSize);
         m_lReceive.Add (&rc);

         // note that done with packet
         m_iStartTime = 0;

         // note that incoming
         if (pfIncoming)
            *pfIncoming = TRUE;

         // move down
         DWORD dwMove = sizeof(*ph) + WORDALIGN(ph->dwSize);
         memmove (m_amemRecieve[m_dwReceiveChan].p, (PBYTE)m_amemRecieve[m_dwReceiveChan].p + dwMove,
            m_amemRecieve[m_dwReceiveChan].m_dwCurPosn - dwMove);
         m_amemRecieve[m_dwReceiveChan].m_dwCurPosn -= dwMove;

         // special - if the packet was a ping, then automatically reply
         if (rc.dwType == CIRCUMREALITYPACKET_PINGSEND) {
            // free up
            m_lReceive.Remove(0);
            m_lReceivePacket.Remove (0);

            // add to queue
            // NOTE: Need to code separately here so don't try to reenter
            // critical seciton
            // structure for this
            CIRCUMREALITYPACKETSEND ps;
            memset (&ps, 0, sizeof(ps));
            ps.dwType = CIRCUMREALITYPACKET_PINGRECEIVED;
            ps.hdr.dwType = ps.dwType;
            ps.dwTotalData = WORDALIGN(ps.hdr.dwSize) + sizeof(ps.hdr);

            // add this in, always in channel 0
            m_alSendPacket[PACKCHAN_HIGH].Add (NULL, 0);
            m_alSend[PACKCHAN_HIGH].Add (&ps);

            // keep track of number of bytes expect to send
            m_PI.iSendBytesExpect += (__int64)ps.dwTotalData;
         }
         else if (rc.dwType == CIRCUMREALITYPACKET_PINGRECEIVED) {
            // just delet this since it's a NOOP
            m_lReceive.Remove(0);
            m_lReceivePacket.Remove (0);
         }
      } // while - pulling out packets
   } // while getting data

done:
#define CALCRES         1000     // milliseconds
   // if we've been in the same packet for more than N seconds then can start
   // guestimating receive speed
   if (m_iStartTime && (iTimeMilli >= m_iStartTime + CALCRES)) {
      // how much data have gotten in, and how much time
      DWORD dwData = (DWORD)(m_PI.iReceiveBytes - m_iStartCount);
      DWORD dwTime = (DWORD)(iTimeMilli - m_iStartTime);
      DWORD dwRate = dwData * 1000 / dwTime;

      // blend this in with existing rate
      m_PI.dwBytesPerSec = (m_PI.dwBytesPerSec * 2 + dwRate) / 3;

      // update settings so wont include same data next time
      m_iStartTime = iTimeMilli;
      m_iStartCount = m_PI.iReceiveBytes;
   }

   LeaveCriticalSection (&m_CritSec);
   // BUGFIX - Only set m_iErr if ecnountered an error, so dont overwrite existing error
   if (iErr) {
      m_iErrLast = iErr;
      m_pszErrLast = pszErr;

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }
   }
   return iErr;
}


/************************************************************************************
CCircumrealityPacket:MaintinenceCheckForSilence - This tests to see if the connection
has been silent for awhile (approx 1 minute). If it has then it sends a ping.
If no ping is received then it will return TRUE to indicate that should
disconnect this port.

An application should call this regularly.

Call GetLastError() to get the error.

inputs
   __int64        iTimeMilli - A time in milliseconds - counter so can calculate
                     the incoming data rate and other stuff
returns
   BOOL - TRUE if should disconnect because silent for too long (aka: other machine
            isn't responding), FALSE if OK
*/
BOOL CCircumrealityPacket::MaintinenceCheckForSilence (__int64 iTimeMilli)
{
   // NOTE: Doing an UNSAFE check here, but OK, since if it's wrong once then
   // the next time that run will proably be ok
   if (max(m_PI.iSendLast, m_PI.iReceiveLast)+TIMEBEFOREPING >= iTimeMilli)
      return FALSE;  // quick exit

   // now do a threadsafe version
   BOOL fRet = FALSE;
   EnterCriticalSection (&m_CritSec);

   // threadsafe check
   __int64 iMax = max(m_PI.iSendLast, m_PI.iReceiveLast);
   if (iMax + TIMEBEFOREPING >= iTimeMilli)
      goto done;

   // if has been way too long then just exit
   if (iMax + TIMEBEFOREDISCONNECT <= iTimeMilli) {
      fRet = TRUE;
      m_iErrLast = WSAETIMEDOUT;
      m_pszErrLast = L"MaintinenceCheckForSilence::iMax + TIMEBEFOREDISCONNECT <= iTimeMilli";

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }
      goto done;
   }

   // else, want to send a ping
   // first, make sure one isn't already on the list
   // pings are always in channel 0
   PCIRCUMREALITYPACKETSEND pSend = (PCIRCUMREALITYPACKETSEND)m_alSend[0].Get(0);
   DWORD i;
   for (i = 0; i < m_alSend[0].Num(); i++, pSend++)
      if (pSend->dwType == CIRCUMREALITYPACKET_PINGSEND)
         goto done;  // already sent it so don't bother
   
   // else, add
   LeaveCriticalSection (&m_CritSec);
#ifdef _DEBUG
         OutputDebugString ("\r\nSent ping");
#endif
   // BUGFIX - If packet send fails then wantt o error. Otherwise
   // sends an infinite number of packetsends
   if (!PacketSend (CIRCUMREALITYPACKET_PINGSEND, 0, 0, NULL, 0)) {
      // make sure to set iSendLast so that dont call these forever,
      // since if packetsend() fails, wont set m_PI.iSendLast
      m_PI.iSendLast = iTimeMilli;
      return TRUE;
   }
   return FALSE;

   // done
done:
   LeaveCriticalSection (&m_CritSec);
   return fRet;

}



/************************************************************************************
CCircumrealityPacket::GetLastError - Returns the last WinSock error

NOTE: Thread safe

returns
   int - Winsockerr, or 0 if none
*/
int CCircumrealityPacket::GetLastError (PCWSTR *ppszErr)
{
   int iRet;
   EnterCriticalSection (&m_CritSec);
   iRet = m_iErrLast;
   *ppszErr = m_pszErrLast;
   LeaveCriticalSection (&m_CritSec);
   return iRet;
}




/************************************************************************************
CCircumrealityPacket::GetFirstError - Returns the First WinSock error

NOTE: Thread safe

returns
   int - Winsockerr, or 0 if none
*/
int CCircumrealityPacket::GetFirstError (PCWSTR *ppszErr)
{
   int iRet;
   EnterCriticalSection (&m_CritSec);
   iRet = m_iErrFirst;
   *ppszErr = m_pszErrFirst;
   LeaveCriticalSection (&m_CritSec);
   return iRet;
}



/************************************************************************************
CCircumrealityPacket::GetSendReceive - Returns the amount of data sent/received.

NOTE: Thread safe

inputs
   __int64        *piSent - Filled in with the number of bytes sent. Can be NULL.
   __int64        *piSentExpect - Expects to send this many bytes
   __int64        *piSendComp - Compressed bytes sent
   __int64        *piReceived - Bytes received.
   __int64        *piReceivedExpect - Expects to receive
   __int64        *piReceivedComp - Compressed bytes received
   DWORD          *pdwBytesPerSec - Filled in with average received bytes per second
*/
void CCircumrealityPacket::GetSendReceive (__int64 *piSent, __int64 *piSentExpect, __int64 *piSendComp,
                                 __int64 *piReceived, __int64 *piReceivedExpect, __int64 *piReceivedComp,
                                 DWORD *pdwBytesPerSec)
{
   EnterCriticalSection (&m_CritSec);
   
   if (piSent)
      *piSent = m_PI.iSendBytes;
   if (piSentExpect)
      *piSentExpect = m_PI.iSendBytesExpect;
   if (piSendComp)
      *piSendComp = m_PI.iSendBytesComp;
   if (piReceived)
      *piReceived = m_PI.iReceiveBytes;
   if (piReceivedExpect)
      *piReceivedExpect = m_PI.iReceiveBytesExpect;
   if (piReceivedComp)
      *piReceivedComp = m_PI.iReceiveBytesComp;
   if (pdwBytesPerSec)
      *pdwBytesPerSec = m_PI.dwBytesPerSec;

   LeaveCriticalSection (&m_CritSec);
}


/************************************************************************************
CCircumrealityPacket::PacketSend - This sends a packet across the internet.
Technically, it just queues it up, and the packet is send later by another thread.

NOTE: It can be called from any thread.

inputs
   DWORD          dwType - Type of packet, CIRCUMREALITYPACKET_XXX
   PVOID          pData - Data in the packet. This data will be copied into an internal buffer.
   DWORD          dwSize - size of the data
   PVOID          pExtra - You can append extra data onto the pData. This extra data is not
                           copied, but assume to remain permenatly (such as a cache of the
                           send-on-demand files.) If there is extra data pass in non-NULL pExtra
   DWORD          dwExtraSize - Number of bytes in pExtra, or 0 if pExtra==NULL
returns
   BOOL - TRUE if successs, FALSE if error. Call GetLastError()
*/
BOOL CCircumrealityPacket::PacketSend (DWORD dwType, PVOID pData, DWORD dwSize, PVOID pExtra, DWORD dwExtraSize)
{
   BOOL fRet = TRUE;
   EnterCriticalSection (&m_CritSec);

   if (m_iErrLast) {
      // if has had an eror then report false
      fRet = FALSE;
      goto done;
   }

   // determine the channel to send it on
   DWORD dwChannel = dwSize ? PACKCHAN_LOW : PACKCHAN_HIGH;
   if (dwChannel) switch (dwType ) {
   case CIRCUMREALITYPACKET_PINGSEND:
   case CIRCUMREALITYPACKET_PINGRECEIVED:
   case CIRCUMREALITYPACKET_FILEDELETE:
   case CIRCUMREALITYPACKET_CLIENTVERSIONNEED:
   case CIRCUMREALITYPACKET_FILEDATEQUERY:
   case CIRCUMREALITYPACKET_CLIENTLOGOFF:
   case CIRCUMREALITYPACKET_MMLIMMEDIATE:
   case CIRCUMREALITYPACKET_MMLQUEUE:
   case CIRCUMREALITYPACKET_VOICECHAT:
   case CIRCUMREALITYPACKET_UNIQUEID:
   case CIRCUMREALITYPACKET_INFOFORSERVER:
   case CIRCUMREALITYPACKET_SERVERQUERYWANTTTS:
   case CIRCUMREALITYPACKET_SERVERQUERYWANTRENDER:
   case CIRCUMREALITYPACKET_CLIENTREPLYWANTTTS:
   case CIRCUMREALITYPACKET_CLIENTREPLYWANTRENDER:
   case CIRCUMREALITYPACKET_SERVERREQUESTTTS:
   case CIRCUMREALITYPACKET_SERVERREQUESTRENDER:
   case CIRCUMREALITYPACKET_CLIENTCACHEFAILTTS:
   case CIRCUMREALITYPACKET_CLIENTCACHEFAILRENDER:
      dwChannel = PACKCHAN_HIGH; // high priority
      break;

   case CIRCUMREALITYPACKET_SERVERCACHETTS:
   case CIRCUMREALITYPACKET_CLIENTCACHETTS:
      dwChannel = PACKCHAN_TTS;
      break;

   case CIRCUMREALITYPACKET_SERVERCACHERENDER:
   case CIRCUMREALITYPACKET_CLIENTCACHERENDER:
      dwChannel = PACKCHAN_RENDER;
      break;
   }

   // structure for this
   CIRCUMREALITYPACKETSEND ps;
   memset (&ps, 0, sizeof(ps));
   ps.dwExtraData = dwExtraSize;
   ps.pExtraData = pExtra;
   ps.dwType = dwType;
   ps.hdr.dwType = dwType;
   ps.hdr.dwSize = dwSize + dwExtraSize;
   ps.dwTotalData = WORDALIGN(ps.hdr.dwSize) + sizeof(ps.hdr);

   // add this in
   m_alSendPacket[dwChannel].Add (pData, dwSize);
   m_alSend[dwChannel].Add (&ps);

   // keep track of number of bytes expect to send
   m_PI.iSendBytesExpect += (__int64)ps.dwTotalData;

done:
   LeaveCriticalSection (&m_CritSec);
   return fRet;
}



/************************************************************************************
CCircumrealityWinSockData::PacketSend - This sends a packet across the internet.
Technically, it just queues it up, and the packet is send later by another thread.

NOTE: It can be called from any thread.

inputs
   DWORD          dwType - Type of packet, CIRCUMREALITYPACKET_XXX
   PCMMLNode2      pNode - Node being sent. This is converted into unicode
                  and that is sent
   PVOID          pData - Additional memory appended onto the MML
   DWORD          dwSize - Size of the additional memory... when the packet is received, a
                  few bytes extra might be added for round-off reasons
returns
   BOOL - TRUE if success. If FALSE call GetLast error
*/
BOOL CCircumrealityPacket::PacketSend (DWORD dwType, PCMMLNode2 pNode, PVOID pData, DWORD dwSize)
{
   // convert to text
   CMem mem;
   if (!MMLToMem (pNode, &mem, FALSE, 0, FALSE)) {
      m_iErrLast = 1; // memory. dont know what else to put
      m_pszErrLast = L"PacketSend::!MMLToMem";

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }
      return FALSE;
   }
   mem.CharCat (0);  // null terminate

   if (dwSize) {
      if (!mem.Required (mem.m_dwCurPosn + dwSize))
         return FALSE;
      memcpy ((PBYTE)mem.p + mem.m_dwCurPosn, pData, dwSize);
      mem.m_dwCurPosn += dwSize;
   }

   return PacketSend (dwType, mem.p, (DWORD)mem.m_dwCurPosn);
      // BUGFIX - Was, but cant do when add extra data... (wcslen((PWSTR)mem.p)+1)*sizeof(WCHAR));
}



/************************************************************************************
CCircumrealityWinSockData::PacketGetType - Returns the type of packet at the given index.

inputs
   DWORD          dwIndex - Index into the packets.
returns
   DWORD - Packet type, or -1 if error (perhaps packet doesnt exist)
*/
DWORD CCircumrealityPacket::PacketGetType (DWORD dwIndex)
{
   DWORD dwRet = -1;
   EnterCriticalSection (&m_CritSec);

   PCIRCUMREALITYPACKETRECEIVE pRec = (PCIRCUMREALITYPACKETRECEIVE) m_lReceive.Get(dwIndex);
   if (!pRec)
      goto done;
   dwRet = pRec->dwType;

done:
   LeaveCriticalSection (&m_CritSec);
   return dwRet;
}


/************************************************************************************
CCircumrealityWinSockData::PacketGetFind - Finds the first packet of the given type.
If found, it returns the index to the packet, or -1.

inputs
   DWORD          dwType - Type of packet that looking for
returns
   DWORD - Index into packets, or -1 if cant find
*/
DWORD CCircumrealityPacket::PacketGetFind (DWORD dwType)
{
   DWORD dwRet = -1;
   EnterCriticalSection (&m_CritSec);

   PCIRCUMREALITYPACKETRECEIVE pRec = (PCIRCUMREALITYPACKETRECEIVE) m_lReceive.Get(0);
   DWORD i;
   for (i = 0; i < m_lReceive.Num(); i++, pRec++)
      if (pRec->dwType == dwType) {
         dwRet = i;
         break;
      }

   LeaveCriticalSection (&m_CritSec);
   return dwRet;
}



/************************************************************************************
CCircumrealityWinSockData::PacketGetMem - Given a packet index, the gets (and removes) the
packet. It returns a PCMem with the packet memory.

inputs
   DWORD          dwIndex - Index to get. If this isn't valid then NULL will be returned
returns
   PCMem - Memory containing the packet information. NULL if error. This must be
         freed by the caller. m_dwCurPosn is set with the packet length
*/
PCMem CCircumrealityPacket::PacketGetMem (DWORD dwIndex)
{
   PCMem pRet = NULL;
   EnterCriticalSection (&m_CritSec);

   PCIRCUMREALITYPACKETRECEIVE pRec = (PCIRCUMREALITYPACKETRECEIVE) m_lReceive.Get(dwIndex);
   if (!pRec)
      goto done;
   
   // fill in the memory
   pRet = new CMem;
   if (!pRet)
      goto done;
   DWORD dwSize = (DWORD)m_lReceivePacket.Size(dwIndex);
   PVOID pData = m_lReceivePacket.Get(dwIndex);
   if (!pRet->Required (dwSize)) {
      delete pRet;
      pRet = NULL;
      goto done;
   }
   memcpy (pRet->p, pData, dwSize);
   pRet->m_dwCurPosn = dwSize;

   // remove
   m_lReceivePacket.Remove (dwIndex);
   m_lReceive.Remove (dwIndex);

done:
   LeaveCriticalSection (&m_CritSec);
   return pRet;
}



/************************************************************************************
CCircumrealityWinSockData::PacketGetMML - This takes a packet that the caller knows it MML
and interprets it as MML.

inputs
   DWORD          dwIndex - Index to get. If this isn't valid then NULL will be returned
   PCMem          pMem - If not NULL, then any extra data at the end of the MML
                  will be copied into here. Use this to tack binary data onto the
                  end of MML. pMem->m_dwCurPosn will be set with the data
returns
   PCMMLNode2 - MML for the packet. The MML must be freed by the caller. If this
      returns NULL then index was bad. If the index was good, but the MML was bad,
      the data will be packet data will be freed anyway.

      BUGFIX - Because a packet can have several nodes in it, such as
      "<iconwindow>...</iconwindow><iconwindow>...</iconwindow>", this returns
      a pNode. THe subnodes of this node are acutally the packets.
*/
PCMMLNode2 CCircumrealityPacket::PacketGetMML (DWORD dwIndex, PCMem pMem)
{
   // clear out if have
   if (pMem)
      pMem->m_dwCurPosn = 0;

   PCMMLNode2 pRet = NULL;
   EnterCriticalSection (&m_CritSec);

   PCIRCUMREALITYPACKETRECEIVE pRec = (PCIRCUMREALITYPACKETRECEIVE) m_lReceive.Get(dwIndex);
   if (!pRec)
      goto done;
   
   // get memory
   DWORD dwSize = (DWORD)m_lReceivePacket.Size(dwIndex);
   PVOID pData = m_lReceivePacket.Get(dwIndex);
   // BUGFIX - Was looking for null termination here, but pushed below...
   // if ((dwSize < sizeof(WCHAR)) || ((PWSTR)pData)[dwSize/sizeof(WCHAR)-1]) {
   if (dwSize < sizeof(WCHAR)) {
      // BUGFIX - Remove the packet first
      m_lReceivePacket.Remove (dwIndex);
      m_lReceive.Remove (dwIndex);

      goto done;  // obviously too small or not null terminated
   }

   // find null termination
   PWSTR psz = (PWSTR)pData;
   DWORD i;
   for (i = 0; i < dwSize/sizeof(WCHAR); i++, psz++)
      if (!psz[0])
         break;
   if (i >= dwSize/sizeof(WCHAR)) {
      m_lReceivePacket.Remove (dwIndex);
      m_lReceive.Remove (dwIndex);
      goto done;  // obviously too small or not null terminated
   }
   if (pMem) {
      i++;
      DWORD dwNeed = dwSize - i * sizeof(WCHAR);
      if (pMem->Required (dwNeed)) {
         memcpy (pMem->p, (PWSTR)pData+i, dwNeed);
         pMem->m_dwCurPosn = dwNeed;
      }
   }

   // BUGFIX - Was calling CircumrealityParseMML, but that only assumed one sub-node.
   // pRet = CircumrealityParseMML ((PWSTR)pData);
   {
      CEscError err;
      PCMMLNode pRet1;
      // BUGFIX - Changed fDoMacros to TRUE so that could pass macros through
      // to MML displays in client
      pRet1 = ParseMML ((PWSTR)pData, ghInstance, NULL, NULL, &err, TRUE);
      if (pRet1) {
         pRet = pRet1->CloneAsCMMLNode2();
         delete pRet1;
      }
      else
         pRet = NULL;
   }

   // remove
   m_lReceivePacket.Remove (dwIndex);
   m_lReceive.Remove (dwIndex);

done:
   LeaveCriticalSection (&m_CritSec);
   return pRet;
}



/************************************************************************************
CCircumrealityPacket::WindowGet - Gets the window used by the winsock data.

returns
   HWND - This window
*/
HWND CCircumrealityPacket::WindowGet (void)
{
   return m_WinSock.WindowGet();
}

/************************************************************************************
CCircumrealityPacket::WindowSendToSet - Sets the window that will be sending to.

inputs
   HWND     hWnd - Send to
returns
   none
*/
void CCircumrealityPacket::WindowSentToSet (HWND hWnd)
{
   m_WinSock.WindowSentToSet (hWnd);
}



/************************************************************************************
CCircumrealityPacket::InfoGet - Gets misc. information about the packet.

inputs
   PCIRCUMREALITYPACKETINFO       pInfo - Filled in with information
returns
   none
*/
void CCircumrealityPacket::InfoGet (PCIRCUMREALITYPACKETINFO pInfo)
{
   EnterCriticalSection (&m_CritSec);
   memcpy (pInfo, &m_PI, sizeof(m_PI));
   LeaveCriticalSection (&m_CritSec);
}



/************************************************************************************
CCircumrealityPacket::QueuedToSend - Returns the number of bytes that are queued up
to be sent out, excluding data that in pre-existing memory. This is used to
ensure that too much data isn't queued up, causing the server to crash.

inputs
   DWORD       dwChannel - -1 for all, else PACKCHAN_XXX
returns
   __int64 - Number of bytes queued
*/
__int64 CCircumrealityPacket::QueuedToSend (DWORD dwChannel)
{
   __int64 iTotal = 0;
   EnterCriticalSection (&m_CritSec);
   DWORD i, j;
   DWORD dwStart = 0, dwStop = NUMPACKCHAN;
   if (dwChannel != (DWORD)-1) {
      dwStart = dwChannel;
      dwStop = dwChannel+1;
   }
   for (i = dwStart; i < dwStop; i++) {
      for (j = 0; j < m_alSendPacket[i].Num(); j++) {
         iTotal += (__int64)m_alSendPacket[i].Size(j);
         iTotal += sizeof(CIRCUMREALITYPACKETSEND); // size of packet info
      } // j
   } // i

   LeaveCriticalSection (&m_CritSec);

   return iTotal;
}

/************************************************************************************
CCircumrealityPacket::InfoSet - Sets misc. information about the packet. NOTE: The
later part of the pInfo structure is NOT transferred over because that
is automatically updated info, like bytes used, etc.

inputs
   PCIRCUMREALITYPACKETINFO       pInfo - Information to set
returns
   none
*/
void CCircumrealityPacket::InfoSet (PCIRCUMREALITYPACKETINFO pInfo)
{
   EnterCriticalSection (&m_CritSec);

   wcscpy (m_PI.szIP, pInfo->szIP);
   wcscpy (m_PI.szStatus, pInfo->szStatus);
   wcscpy (m_PI.szUser, pInfo->szUser);
   wcscpy (m_PI.szCharacter, pInfo->szCharacter);
   wcscpy (m_PI.szUniqueID, pInfo->szUniqueID);
   m_PI.gObject = pInfo->gObject;

   LeaveCriticalSection (&m_CritSec);
}




/************************************************************************************
CCircumrealityPacket::CompSend - Send information using the compression setting.

inputs
   PVOID          pData - Data to send
   DWORD          dwSize - Size in bytes
   BOOL           fTryCompress - Set TRUE if this data is compressable, FALSE
                     if just binary should be sent
returns
   int - Number of bytes of data actually send, or SOCKET_ERROR if an error has occured.
      Often times not as much data will be sent as is provided
*/
int CCircumrealityPacket::CompSend (PVOID pData, DWORD dwSize, BOOL fTryCompress)
{
   // m_fEndOfOut = TRUE; return m_WinSock.Send (pData, dwSize); // to test

   // first of all, assume no longer at the end
   m_fEndOfOut = FALSE;

   // copy as much data as can into the output buffer
   DWORD dwAvail = (DWORD)(m_amemDecomp[0].m_dwAllocated - m_amemDecomp[0].m_dwCurPosn);
   dwSize = min(dwSize, dwAvail);
   // BUGFIX - had exited if no dwSize, but keep going and try to send anyway
   if (dwSize) {
      memcpy ((PBYTE)m_amemDecomp[0].p + m_amemDecomp[0].m_dwCurPosn, pData, dwSize);
      m_amemDecomp[0].m_dwCurPosn += dwSize;

      // remember that wanted compressed or not
      int iAdd = (int)dwSize * (fTryCompress ? 1 : -1);
      m_lCompressOrNot.Add (&iAdd);
   }

   // if any data is left then add this to a separate thread
   if (m_fCompInSeparateThread)
      AddToSeparateThreadQueue ();
   else {   // compress now
      if (SOCKET_ERROR == CompCompressAndSend (TRUE))
         return SOCKET_ERROR;
   }

   return dwSize;
}
   

/************************************************************************************
CCircumrealityPacket::CompSendEndOfData - Called if the last of the data has been reached.
*/
int CCircumrealityPacket::CompSendEndOfData (void)
{
   m_fEndOfOut = TRUE;

#ifdef _DEBUG
   if (m_amemDecomp[0].m_dwCurPosn % 2)
      OutputDebugString ("ERROR: Odd number of bytes in CompSendEndOfData\r\n");
#endif

   // if any data is left then add this to a separate thread
   if (m_fCompInSeparateThread) {
      AddToSeparateThreadQueue ();
      return 0;
   }
   else  // locally
      return CompCompressAndSend (TRUE);
}



/************************************************************************************
CompCompare - Compares a string against an offset in the main
output data. It returns the number of cahracters that match.

inputs
   PWSTR          pszData - Data to search for
   DWORD          dwCount - Number of CHARACTERS of data
   PWSTR          pszDict - Output dictionary
   DWORD          dwOffset - Offset into output dictionary
   DWORD          dwBestMatch - Best match length. If 0 then no best match
returns
   DWORD - Number of characters of match
*/
static __inline DWORD CompCompare (PWSTR pszData, DWORD dwCount, PWSTR pszDict, DWORD dwOffset) //,
                                   // DWORD dwBestMatch)
{
   // trivial reject - not needed because compare before
   //if (pszDict[dwOffset] != pszData[0])
   //   return 0;

   // trivial test against best match
   // NOTE: Don't test (dwBestMatch > 0) since will always be
   // NOTE: should test dwCount > dwBestMatch
   // NOTE: Move up before call compcompare
   //if (pszDict[(dwOffset+dwBestMatch-1)%COMPDICTSIZE] != pszData[dwBestMatch-1])
   //   return 0;

   // more complex
   DWORD dwModuloAt = COMPDICTSIZE - dwOffset;
   DWORD dwPreMod = min(dwCount, dwModuloAt);
   DWORD dwPostMod = (dwCount > dwModuloAt) ? (dwCount - dwModuloAt) : 0;

   // premodulo
   DWORD i;
   PWSTR pszCurDict = pszDict + (dwOffset+1);
   pszData++;
   for (i = 1; i < dwPreMod; i++, pszData++, pszCurDict++)
      if (*pszCurDict != *pszData)
         return i;   // found end

   // else may have gone post-modulo
   if (dwPostMod) {
      pszCurDict = pszDict;
      for (; i < dwCount; i++, pszData++, pszCurDict++)
         if (*pszCurDict != *pszData)
            return i;   // found end
   }

   // else, totla match
   return dwCount;
}

/************************************************************************************
CCircumrealityPacket::CompFindLongestMatch - Finds the longest match for some data in the
output dictionary.

inputs
   PWSTR          pszData - Data to search for
   DWORD          dwCount - Nubmber of CHARACTERS of data
   DWORD          *pdwMatched - Number of CHARACTERS matched.
returns
   DWORD - Index (character-based) into the output dictionary, or -1 if can't find
*/
DWORD CCircumrealityPacket::CompFindLongestMatch (PWSTR pszData, DWORD dwCount, DWORD *pdwMatched)
{
   // *pdwMatched = 0; return -1;  // to test - to make easier

   DWORD i;
   PWSTR pszDict = (PWSTR)m_amemDict[0].p;
   DWORD dwMatched = min(dwCount, COMP_MAXUNIQUE/2);  // minimum number of chars that must match for a fit
   DWORD dwRet = (DWORD)-1;
   WCHAR cStart = pszData[0];
   WCHAR cEnd = pszData[dwMatched-1];
   PWSTR pszComp;
   for (i = 0, pszComp = pszDict; i < COMPDICTSIZE; i++, pszComp++) {
      // trivial reject
      if (*pszComp != cStart)
         continue;

      // trivial reject
      if (pszDict[(i+dwMatched-1)%COMPDICTSIZE] != cEnd)
         continue;

      DWORD dwMatch = CompCompare (pszData, dwCount, pszDict, i); //, dwMatched);
      if (dwMatch && (dwMatch > dwMatched)) {
         dwMatched = dwMatch;
         cEnd = pszData[dwMatched-1];
         dwRet = i;

         // if already reached full length then dont bother
         if (dwMatched >= dwCount)
            break;
      }
   } // i

   if (dwRet == (DWORD)-1) {
      *pdwMatched = 0;
      return 0;
   }
   else {
      *pdwMatched = dwMatched;
      return dwRet;
   }
}


/************************************************************************************
CCircumrealityPacket::CompBuffer - Compresses a buffer of data.

inputs
   PWSTR          pszComp - Data to compress
   DWORD          dwMaxCount - Maximum amount of data can get from this (in WCHAR units)
   BOOL           fTryCompress - Set to TRUE if try to compress. If FALSE will just
                  sent binary without compression. BUFIX - Used as optimization so doesn't
                  try to compressed non-text data.
returns
   DWORD - Amount compressed (in chars
*/
DWORD CCircumrealityPacket::CompBuffer (PWSTR pszComp, DWORD dwMaxCount, BOOL fTryCompress)
{
   // to test - to test
   //if (!dwMaxCount || (m_amemComp[0].m_dwCurPosn + 2 >= m_amemComp[0].m_dwAllocated))
   //   return 0;
   //memcpy ((PBYTE)m_amemComp[0].p + m_amemComp[0].m_dwCurPosn, pszComp, 2);
   //m_amemComp[0].m_dwCurPosn += 2;
   //return 1;
   // to test

   // figure out how well compression is working. If it hasn't been working
   // well lately then don't try to compress
   DWORD dwCompCount, dwTemp;
   // BUGFIX - Made as parameter BOOL fTryCompress = TRUE;
   m_wCompTick++;
   if (fTryCompress) {
      for (dwCompCount=0, dwTemp = m_wCompBitHistory; dwTemp; dwTemp &= (dwTemp-1), dwCompCount++);
      if ((dwCompCount < sizeof(m_wCompBitHistory)*8 / 2) && (m_wCompTick % 16))
         fTryCompress = FALSE;   // we haven't had much luck lately, so don't try to compress
   }


   // find the longest match
   DWORD dwCount, dwOffset;
   dwCount = dwMaxCount;
   if (fTryCompress)
      dwOffset = CompFindLongestMatch (pszComp, min(dwCount,COMP_MAXREPEATBLOCK-1), &dwCount);
   else
      dwOffset = dwCount = 0;
   dwCount = min(dwCount, COMP_MAXREPEATBLOCK-1);  // if repeat, can only repeat so much

   DWORD dwCompIndex = (DWORD)m_amemComp[0].m_dwCurPosn;
   PBYTE pbComp = (PBYTE) m_amemComp[0].p + dwCompIndex;

   // if match count < 4 then shorter to send 8 chars down
   if (dwCount < (COMP_MAXUNIQUE/2)) {
      dwCount = min(dwMaxCount, COMP_MAXUNIQUE);

      // jump ahead 4 and see if can find a pattern after this
      // if can find a good pattern then use a short exception pattern,
      // otherwise use a long exception pattern
      if (dwMaxCount >= COMP_MAXUNIQUE) {
         DWORD dwOffset2, dwCount2;
         if (fTryCompress) {
            dwOffset2 = CompFindLongestMatch (pszComp + (COMP_MAXUNIQUE/2),
               min((dwMaxCount - COMP_MAXUNIQUE/2),COMP_MAXUNIQUE), &dwCount2);
               // NOTE: Min uses COMP_MAXUNIQUE, so won't go for really long strings
               // since cant use the output of this anyway
            if (dwCount2 < COMP_MAXUNIQUE/2)
               dwCount = COMP_MAXUNIQUE;  // since going another step wouldnt compress either
         }
         else
            dwCount = COMP_MAXUNIQUE;
      }


      // see if they're all ANSI
      BOOL fANSI = TRUE;
      DWORD i;
      for (i = 0; i < dwCount; i++)
         if (pszComp[i] >= 256) {
            fANSI = FALSE;
            break;
         }

      // fANSI = FALSE; // to test

      // write the data... know that have plenty of space
      if (fANSI) {
         *(pbComp++) = COMP_MAXREPEATBLOCK + ((dwCount-1)<<1); // low bit for unicode
         for (i = 0; i < dwCount; i++)
            *(pbComp++) = (BYTE)pszComp[i];
         dwCompIndex += (1 + dwCount);
      }
      else {   // unicode
         *(pbComp++) = COMP_MAXREPEATBLOCK + ((dwCount-1)<<1) + 1;   // low bit for unicode
         memcpy (pbComp, pszComp, dwCount * sizeof(WCHAR));
         // NOT needed: pbComp += (dwCount*sizeof(WCHAR));
         dwCompIndex += (1 + dwCount*sizeof(WCHAR));
      }

      // note that didn't compress
      if (fTryCompress)
         m_wCompBitHistory = (m_wCompBitHistory << 1);
   }
   else {
      // repeat
      *(pbComp++) = dwCount-1;
      *((WORD*)pbComp) = (WORD) dwOffset;
      dwCompIndex += 3;

      // note that compressed
      if (fTryCompress)
         m_wCompBitHistory = (m_wCompBitHistory << 1) + 1;
   }
   m_amemComp[0].m_dwCurPosn = dwCompIndex;  // so store

   // if have added data, and wasn't any there before, then set the time
   if (m_amemComp[0].m_dwCurPosn && !m_iSendQueueTimeStart) {
      LARGE_INTEGER     liCount;
      QueryPerformanceCounter (&liCount);
      m_iSendQueueTimeStart = *((__int64*)&liCount);
   }


   // copy what just compressed into the dictionary
   DWORD dwModuloAt = COMPDICTSIZE - m_adwDictLoc[0];
   DWORD dwPreMod = min(dwCount, dwModuloAt);
   DWORD dwPostMod = (dwCount > dwModuloAt) ? (dwCount - dwModuloAt) : 0;
   memcpy ((PWSTR)m_amemDict[0].p + m_adwDictLoc[0], pszComp, dwPreMod*sizeof(WCHAR));
   if (dwPostMod)
      memcpy ((PWSTR)m_amemDict[0].p, pszComp + dwPreMod, dwPostMod*sizeof(WCHAR));
   m_adwDictLoc[0] = (m_adwDictLoc[0] + dwCount) % COMPDICTSIZE;

   return dwCount;
}


/************************************************************************************
CCircumrealityPacket::CompressInSeparateThread - This should be called immediately after
the packet is created, if it is to be called. It tells the packet that compression
will be handled by a separate thread.

inputs
   CRITICAL_SECTION     *pCritSec - Critical section to access the list
   PCListFixed          plPCCircumrealityPacket - If there's data to be written, this object
                           should add itself to the end of the list. This can be NULL
   PHANDLE              paSignal - Pointer to an array of MAXRAYTHREAD handles, some
                           of which can be NULL. If the packet is actually added
                           then all of the non-null signals should have SetEvent()
                           called.. This can be NULL
returns
   none
*/
void CCircumrealityPacket::CompressInSeparateThread (CRITICAL_SECTION *pCritSec,
                                                PCListFixed plPCCircumrealityPacket, PHANDLE paSignal)
{
   EnterCriticalSection (&m_CritSec);

   m_fCompInSeparateThread = TRUE;
   m_pCritSecSeparateThread = pCritSec;
   m_plPCCircumrealityPacketSeparateThread = plPCCircumrealityPacket;
   m_paSignalSeparateThread = paSignal;

   LeaveCriticalSection (&m_CritSec);
}


/************************************************************************************
CCircumrealityPacket::AddToSeparateThreadQueue - Adds this packet to the separate thread
queue.
*/
void CCircumrealityPacket::AddToSeparateThreadQueue (void)
{
   if (!m_fCompInSeparateThread)
      return;  // shouldnt happen

   EnterCriticalSection (m_pCritSecSeparateThread);

   if (!m_plPCCircumrealityPacketSeparateThread) {
      LeaveCriticalSection (m_pCritSecSeparateThread);
      return;
   }

   // see if match
   PCCircumrealityPacket *ppp = (PCCircumrealityPacket*)m_plPCCircumrealityPacketSeparateThread->Get(0);
   DWORD i;
   for (i = m_plPCCircumrealityPacketSeparateThread->Num()-1; i < m_plPCCircumrealityPacketSeparateThread->Num(); i--)
      if (ppp[i] == this) {
         // already there
         LeaveCriticalSection (m_pCritSecSeparateThread);
         return;
      }

   // else, add
   PCCircumrealityPacket pThis = this;
   m_plPCCircumrealityPacketSeparateThread->Add (&pThis);

   // set all the events
   if (m_paSignalSeparateThread) for (i = 0; i < MAXRAYTHREAD; i++)
      if (m_paSignalSeparateThread[i])
         SetEvent (m_paSignalSeparateThread[i]);

   LeaveCriticalSection (m_pCritSecSeparateThread);
}

/************************************************************************************
CCircumrealityPacket::CompressedDataToWinsock - This actually sends the compressed data
to winsock.

If the packets are being compressed by a seperate thread then call this
once in awhile to actually send the data.

NOTE: If *pdwRemaining is NOT 0, and doing compression in separate thread,
   then AddToSeparateThreadQueue() should probably be called because there
   might be more data that needs compressing.

inputs
   BOOL           fInCritSec - If calling externally, pass in FALSE. If called
                     internally, pass in TRUE, since we're already calling
                     from a critical section
   DWORD          *pdwRemaining - Filled with the Number of bytes of data REMAINING
                     to be sent in the buffer after this
                     is called. Thus, if there is data remaining then should call it again later
                     on.
returns
   int - Number of bytes of data actually send, or SOCKET_ERROR if an error has occured.
      Often times not as much data will be sent as is provided
*/
int CCircumrealityPacket::CompressedDataToWinsock (BOOL fInCritSec, DWORD *pdwRemaining)
{
   int iRet = 0;
   if (!fInCritSec)
      EnterCriticalSection (&m_CritSec);

   // BUGFIX - *pdwRemaining moved into critical section, was before it
   *pdwRemaining = (DWORD)m_amemComp[0].m_dwCurPosn;

   if (!m_amemComp[0].m_dwCurPosn)
      goto done;  // nothing

   // if not enough time has elapsed then don't send
   LARGE_INTEGER     liCount;
   QueryPerformanceCounter (&liCount);
   __int64 iTime = *((__int64*)&liCount);
   if ((iTime < m_iSendQueueTimeStart + m_iSendQueueTimeWait) && (iTime > m_iSendQueueTimeStart - m_iSendQueueTimeWait)) {
      // new data was just sent, and still in the wait period
      
      // if there isn't enough data then just stop here and wait to see
      // if more appears
      if (m_amemComp[0].m_dwCurPosn < COMP_BUFFERSIZE/2)
         goto done;

      // else, there's enough data queued that will send anyway, even if it's early days
      m_iSendQueueTimeStart = iTime - 2*m_iSendQueueTimeWait; // so that next time it won't bother to wait
   }

   iRet = m_WinSock.Send (m_amemComp[0].p, (DWORD)m_amemComp[0].m_dwCurPosn);

#if 0 // to test def _DEBUG
   WCHAR szTemp[256];
   _swprintf (szTemp, L"\r\nm_WinSock.Send (%d)", (int)m_amemComp[0].m_dwCurPosn);
   OutputDebugStringW (szTemp);
#endif

   if (iRet == SOCKET_ERROR) {
      m_iErrLast = m_WinSock.GetLastError(&m_pszErrLast);
      // m_pszErr = L"CompressedDataToWinsock::Send()";

      if (!m_iErrFirst) {
         m_iErrFirst = m_iErrLast;
         m_pszErrFirst = m_pszErrLast;
      }

      goto done;
   }
   *pdwRemaining = (DWORD)m_amemComp[0].m_dwCurPosn - (DWORD)iRet;
   memmove (m_amemComp[0].p, (PBYTE)m_amemComp[0].p + iRet, m_amemComp[0].m_dwCurPosn - (DWORD)iRet);
   m_amemComp[0].m_dwCurPosn -= (DWORD)iRet;
   if (!m_amemComp[0].m_dwCurPosn)
      m_iSendQueueTimeStart = 0;
   
   m_PI.iSendBytesComp += iRet;

done:
   if (!fInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return iRet;
}


/************************************************************************************
CCircumrealityPacket::CompCompressAndSend - Tries to compress the data in the buffer down and
then send it out.

NOTE: This assumes that all data is ultimately word aligned, and wont be finished
with data with an odd number of bytes.

inputs
   BOOL           fInCritSec - If calling externally, pass in FALSE. If called
                     internally, pass in TRUE, since we're already calling
                     from a critical section

returns
   int - Number of bytes of data actually send, or SOCKET_ERROR if an error has occured.
      Often times not as much data will be sent as is provided
*/
int CCircumrealityPacket::CompCompressAndSend (BOOL fInCritSec)
{
   int iRet = 0;
   int iSent = 0;

   if (!fInCritSec)
      EnterCriticalSection (&m_CritSec);

   while (TRUE) {
      // try to send out what have if full compress buffer
      int iRet;
      if (!m_fCompInSeparateThread && ((m_fEndOfOut && m_amemComp[0].m_dwCurPosn) || (m_amemComp[0].m_dwCurPosn + COMP_SAFEBUFFER >= m_amemComp[0].m_dwAllocated))) {
         DWORD dwRemain;
         iRet = CompressedDataToWinsock (TRUE, &dwRemain);
         if (iRet == SOCKET_ERROR)
            goto done;
         
         iSent += iRet;

//#ifdef _DEBUG
//         char szTemp[64];
//         sprintf (szTemp, "Send total of %d comp bytes\r\n", (int)m_PI.iSendBytesComp);
//         OutputDebugString (szTemp);
//#endif
      }

      // if full compress buffer then just exit
      if (m_amemComp[0].m_dwCurPosn + COMP_SAFEBUFFER >= m_amemComp[0].m_dwAllocated) {
         iRet = iSent;
         goto done;
      }

      // if not enough data then exit
      if (m_amemDecomp[0].m_dwCurPosn < sizeof(WCHAR)) {
         iRet = iSent;
         goto done;
      }
      if (!m_fEndOfOut && (m_amemDecomp[0].m_dwCurPosn < COMP_SAFEBUFFER)) {
         iRet = iSent;   // could send out, but will wait since might be able to do better compression
         // NOTE: Since all data is ultimately word aligned, shouldnt get to case where last byte isnt sent
         goto done;
      }

      // compress as much as possible without moving
      DWORD dwCur = 0;
      while (dwCur + (m_fEndOfOut ? 0 : (COMP_SAFEBUFFER/sizeof(WCHAR))) < m_amemDecomp[0].m_dwCurPosn/sizeof(WCHAR)) {
         // if not enough safe space in output then also stop
         if (m_amemComp[0].m_dwCurPosn + COMP_SAFEBUFFER >= m_amemComp[0].m_dwAllocated)
            break;

         // figure out if want to compress or decompress
         int *paiComp;
         BOOL fTryCompress;
         if (m_lCompressOrNot.Num()) {
            paiComp = (int*) m_lCompressOrNot.Get(0);
            fTryCompress = (paiComp[0] > 0);
         }
         else
            fTryCompress = FALSE;   // shouldnt happen

         // compress this
         DWORD dwCount = CompBuffer ((PWSTR)m_amemDecomp[0].p + dwCur,
            (DWORD)m_amemDecomp[0].m_dwCurPosn/sizeof(WCHAR) - dwCur, fTryCompress);
         if (!dwCount)
            break;
         dwCur += dwCount;

         // loop through and eliminate compress or not from list
         dwCount *= sizeof(WCHAR);  // so back to bytes
         while (dwCount && m_lCompressOrNot.Num()) {
            paiComp = (int*) m_lCompressOrNot.Get(0);
            int iAbs = abs(paiComp[0]);
            iAbs = min (iAbs, (int)dwCount);

            // decrease
            dwCount -= (DWORD)iAbs;
            if (paiComp[0] > 0)
               paiComp[0] -= iAbs;  // fewer bytes left to compress
            else
               paiComp[0] += iAbs;  // fewer non-compress bytes to skip
            if (!paiComp[0])
               m_lCompressOrNot.Remove (0);  // nothing left, so remove
         }
      } // while can compress

      // if compressed any then move back
      if (dwCur) {
         memmove ((PWSTR)m_amemDecomp[0].p, (PWSTR)m_amemDecomp[0].p + dwCur,
            m_amemDecomp[0].m_dwCurPosn - dwCur * sizeof(WCHAR));
         m_amemDecomp[0].m_dwCurPosn -= dwCur * sizeof(WCHAR);
      }
      else
         break;   // couldnt actually compress anything

      // loop around and try to send out what have
   } // while TRUE

   iRet = iSent;
   // fall through

done:
   if (!fInCritSec)
      LeaveCriticalSection (&m_CritSec);
   return iRet;
}


/************************************************************************************
CCircumrealityPacket::CompDecompBuffer - Decompress from an individual buffer

inputs
   PBYTE          pbData - Data to decompress from
   DWORD          dwSize - Size of data in bytes
returns
   DWORD - Number of bytes used
*/
DWORD CCircumrealityPacket::CompDecompBuffer (PBYTE pbData, DWORD dwSize)
{
   // to test - to test
   //if (!dwSize || (m_amemDecomp[1].m_dwCurPosn+1 >=m_amemDecomp[1].m_dwAllocated))
   //   return 0;
   //((PBYTE)m_amemDecomp[1].p + m_amemDecomp[1].m_dwCurPosn)[0] = pbData[0];
   //m_amemDecomp[1].m_dwCurPosn++;
   //return 1;
   // to test

   // if not enough data for eaven a header then dont care
   if (!dwSize)
      return 0;

   // if decomp buffer isn't large enough for safe margin then don't
   if (m_amemDecomp[1].m_dwCurPosn + COMP_SAFEBUFFER >= m_amemDecomp[1].m_dwAllocated)
      return 0;

   PWSTR pszDest = (PWSTR)m_amemDecomp[1].p + (m_amemDecomp[1].m_dwCurPosn/sizeof(WCHAR));

   // how much pull from data
   DWORD dwUse;
   DWORD dwChars;
   if (pbData[0] < COMP_MAXREPEATBLOCK) {
      dwUse = 3;
      if (dwSize < dwUse)
         return 0;   // not large enough
      dwChars = (DWORD)pbData[0]+1;
      
      // index to decomp from
      DWORD dwIndex = *((WORD*) (pbData+1));

      // pull out
      dwIndex = min(dwIndex, COMPDICTSIZE);  // just to make sure hackers dont crash client
      DWORD dwModuloAt = COMPDICTSIZE - dwIndex;
      DWORD dwPreMod = min(dwChars, dwModuloAt);
      DWORD dwPostMod = (dwChars > dwModuloAt) ? (dwChars - dwModuloAt) : 0;

      memcpy (pszDest, (PWSTR)m_amemDict[1].p + dwIndex, dwPreMod * sizeof(WCHAR));
      if (dwPostMod)
         memcpy (pszDest + dwModuloAt, (PWSTR)m_amemDict[1].p, dwPostMod * sizeof(WCHAR));
   }
   else {
      DWORD dwSub = pbData[0] - COMP_MAXREPEATBLOCK;
      pbData++;
      dwChars = (dwSub >> 1) + 1;
      BOOL fAnsi = (dwSub & 0x01) ? FALSE : TRUE;

      dwUse = 1 + dwChars * (fAnsi ? 1 : sizeof(WCHAR));
      if (dwSize < dwUse)
         return 0;   // not large enough

      DWORD i;
      if (fAnsi)
         for (i = 0; i < dwChars; i++)
            pszDest[i] = (WCHAR)pbData[i];
      else
         memcpy (pszDest, pbData, dwChars * sizeof(WCHAR));
   }
   m_amemDecomp[1].m_dwCurPosn += dwChars * sizeof(WCHAR);  // increase position

   // fill in the dictionary entry
   DWORD dwModuloAt = COMPDICTSIZE - m_adwDictLoc[1];
   DWORD dwPreMod = min(dwModuloAt, dwChars);
   DWORD dwPostMod = (dwChars > dwModuloAt) ? (dwChars - dwModuloAt) : 0;
   memcpy ((PWSTR)m_amemDict[1].p + m_adwDictLoc[1], pszDest, dwPreMod * sizeof(WCHAR));
   if (dwPostMod)
      memcpy ((PWSTR)m_amemDict[1].p, pszDest + dwPreMod, dwPostMod * sizeof(WCHAR));

   m_adwDictLoc[1] = (m_adwDictLoc[1] + dwChars) % COMPDICTSIZE;

   return dwUse;
}


/************************************************************************************
CCircumrealityPacket::CompReceiveAndDecompress - Try to receive data and decompress

returns
   int - Number of bytes received (pre-decompress), or SOCKET_ERROR if error
*/
int CCircumrealityPacket::CompReceiveAndDecompress (void)
{
   int iRead = 0;
   while (TRUE) {
      // if there isn't enough space in decompress buffer for a safe decompress
      // then just exit here
      if (m_amemDecomp[1].m_dwCurPosn + COMP_SAFEBUFFER >= m_amemDecomp[1].m_dwAllocated)
         return iRead;

      // if we have space, try to read in some more
      if (m_amemComp[1].m_dwCurPosn < m_amemComp[1].m_dwAllocated) {
         int iRet = m_WinSock.Receive ((PBYTE)m_amemComp[1].p + m_amemComp[1].m_dwCurPosn,
            (DWORD)(m_amemComp[1].m_dwAllocated - m_amemComp[1].m_dwCurPosn));
         if (iRet == SOCKET_ERROR) {
            m_iErrLast = m_WinSock.GetLastError(&m_pszErrLast);
            // m_pszErr = L"CompReceiveAndDecompress::Receive()";

            if (!m_iErrFirst) {
               m_iErrFirst = m_iErrLast;
               m_pszErrFirst = m_pszErrLast;
            }
            return iRet;
         }

         iRead += iRet;
         m_PI.iReceiveBytesComp += iRet;
         m_amemComp[1].m_dwCurPosn += (DWORD)iRet;

//#ifdef _DEBUG
//         char szTemp[64];
//         sprintf (szTemp, "Received total of %d comp bytes\r\n", (int)m_PI.iReceiveBytesComp);
//         OutputDebugString (szTemp);
//#endif
      }

      DWORD dwCur, dwUsed;
      for (dwCur = 0; dwCur < m_amemComp[1].m_dwCurPosn; ) {
         dwUsed = CompDecompBuffer ((PBYTE)m_amemComp[1].p + dwCur,
            (DWORD)m_amemComp[1].m_dwCurPosn - dwCur);
         if (!dwUsed)
            break;   // nothing else to decompress

         // else decomrpressed
         dwCur += dwUsed;
      }

      // move everything down
      if (dwCur) {
         memmove ((PBYTE)m_amemComp[1].p, (PBYTE)m_amemComp[1].p + dwCur,
            m_amemComp[1].m_dwCurPosn - dwCur);
         m_amemComp[1].m_dwCurPosn -= dwCur;
      }
      else
         break;   // didn't actually decompress anything, so give up
   } // while TRUE

   return iRead;
}



/************************************************************************************
CCircumrealityPacket::CompReceive -Reads data thats queued up from the remove machine.

inputs
   PVOID          pData - To fill in
   DWORD          dwSize - Bytes available
returns
   int - Number of bytes actually filled in. SOCKET_ERROR if an error has occured,
      in which case GetLastError() should be called for the error
*/
int CCircumrealityPacket::CompReceive (PVOID pData, DWORD dwSize)
{
   //return m_WinSock.Receive (pData, dwSize); // to test
   
   int iTotal = 0;

   while (dwSize) {
      // try to get
      if (SOCKET_ERROR == CompReceiveAndDecompress ())
         return SOCKET_ERROR;

      // copy over
      DWORD dwCopy = min(dwSize, (DWORD)m_amemDecomp[1].m_dwCurPosn);
      if (!dwCopy)
         return iTotal; // all done
      memcpy (pData, m_amemDecomp[1].p, dwCopy);
      dwSize -= dwCopy;
      iTotal += (int)dwCopy;
      pData = ((PBYTE)pData + dwCopy); // BUGFIX

      // move
      memmove (m_amemDecomp[1].p, (PBYTE)m_amemDecomp[1].p + dwCopy,
         m_amemDecomp[1].m_dwCurPosn - dwCopy);
      m_amemDecomp[1].m_dwCurPosn -= dwCopy;
   }

   // done
   return iTotal;
}


// BUGBUG - will need to check send speed. If not sending fast then client
// is trying to backlog, so disconnect

// BUGBUG - if send buffer has gotten too large then something bad has happened
// and should disconnect

// BUGBUG - if the server is asked to receive a really huge packet then
// disconnect client

// BUGBUG - should keep track of total bytes send/received for whole system

