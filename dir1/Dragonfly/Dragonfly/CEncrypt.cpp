/**********************************************************************
CEncrypt.cpp - Encryption code used in dragonfly.

begun 6/2/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "dragonfly.h"
#include <crtdbg.h>


#define  RANDNUM1    0x45af5498     // change for every app
#define  RANDNUM2    0xef82594a     // change for every app
#define  RANDNUM3    0x34a634f2     // change for every app

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
   HANGFUNCIN;
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


/*****************************************************************************
Constructor & destructor */
CRandSequence::CRandSequence (void)
{
   // this space intentionally left blank
}

CRandSequence::~CRandSequence (void)
{
   // this space intentionally left blank
}

/****************************************************************************
Init - Initializes the encrypt object using the given key.
*/
void CRandSequence::Init (DWORD dwKey)
{
   HANGFUNCIN;
   DWORD i;

   MySRand (dwKey);
   for (i = 0; i < ENCRYPTSIZE; i++)
      m_adwEncrypt[i] = MyRand();
}


/****************************************************************************
GetValue - Given a DWORD index into a data stream, this returns the encryption
value that's xored with the data to encrypt it.

inputs
   DWORD    dwPosn - position (in DWORDs)
returns
   DWORD    dwValue - Value to xor
*/
DWORD CRandSequence::GetValue (DWORD dwPosn)
{
   DWORD dwLoc;
   dwLoc = dwPosn % ENCRYPTSIZE;
   return m_adwEncrypt[dwLoc];
}




/****************************************************************************
Constructor & destructor */
CEncrypt::CEncrypt (void)
{
   HANGFUNCIN;
   m_szPassword[0] = 0;
   m_dwChars = 0;
   memset (m_apRS, 0, sizeof(m_apRS));
}


CEncrypt::~CEncrypt (void)
{
   HANGFUNCIN;
   DWORD i;
   for (i = 0; i < MAXENCRYPTPW; i++)
      if (m_apRS[i])
         delete m_apRS[i];
}


/****************************************************************************
Init - Initializes the encryption object using the given password

inputs
   PWSTR pszPassword - password
returns
   BOOL - FALSE if error
*/
BOOL CEncrypt::Init (PWSTR pszPassword)
{
   HANGFUNCIN;
   DWORD dwLen;
   dwLen = wcslen(pszPassword);
   if (dwLen > MAXENCRYPTPW)
      dwLen = MAXENCRYPTPW;

   DWORD i;
   for (i = 0; i < MAXENCRYPTPW; i++) {
      // free the old one
      if (m_apRS[i])
         delete m_apRS[i];
      m_apRS[i] = 0;
   }

   memcpy (m_szPassword, pszPassword, dwLen*2);
   m_dwChars = dwLen;
   for (i = 0; i < dwLen; i++) {
      m_apRS[i] = new CRandSequence;
      if (!m_apRS[i])
         return FALSE;
      m_apRS[i]->Init ((DWORD) m_szPassword[i]);
   }

   return TRUE;
}


/****************************************************************************
Encrypt - Encrypts the data and returns the non-encrypted checksum

inputs
   PBYTE    pData - data
   DWORD    dwSize - number of bytes
returns
   DWORD - checksum
*/
DWORD CEncrypt::Encrypt (PBYTE pData, DWORD dwSize)
{
   HANGFUNCIN;
#ifdef _DEBUG
   if (! _CrtCheckMemory())
      OutputDebugString ("*** Memory corruption before CEncrypt::Encrypt\r\n");
#endif

   DWORD dwCheckSum;
   dwCheckSum = CheckSum (pData, dwSize);

   // encrypt using shuffle then XOR
   ShuffleEncrypt (pData, dwSize);
   XOREnDecrypt (pData, dwSize);

#ifdef _DEBUG
   if (! _CrtCheckMemory())
      OutputDebugString ("*** Memory corruption after CEncrypt::Encrypt\r\n");
#endif
   return dwCheckSum;
}

/****************************************************************************
Encrypt - Decrypts the data.

inputs
   PBYTE    pData - data
   DWORD    dwSize - number of bytes
   DWORD    dwCheckSum - checksum from encryption.
returns
   BOOL - TRUE if checksum matched
*/
BOOL  CEncrypt::Decrypt (PBYTE pData, DWORD dwSize, DWORD dwCheckSum)
{
   HANGFUNCIN;
#ifdef _DEBUG
   if (! _CrtCheckMemory())
      OutputDebugString ("*** Memory corruption before CDecrypt::Encrypt\r\n");
#endif
   // XOR decrypt followed by ShuffleDecrypt
   XOREnDecrypt (pData, dwSize);
   ShuffleDecrypt (pData, dwSize);

#ifdef _DEBUG
   if (! _CrtCheckMemory())
      OutputDebugString ("*** Memory corruption after CDecrypt::Encrypt\r\n");
#endif
   return CheckSum (pData,dwSize) == dwCheckSum;
}


/****************************************************************************
CheckSum - Does a checksum on the data.

inputs
   PBYTE    pData - data
   DWORD    dwSize - number of bytes
returns
   DWORD - checksum
*/
DWORD CEncrypt::CheckSum (PBYTE pData, DWORD dwSize)
{
   HANGFUNCIN;
   DWORD dwSum, i;
   dwSum = 0;
   for (i = 0; i < dwSize; i++)
      dwSum += ((DWORD)pData[i] * (i+1));

   return dwSum;
}


/****************************************************************************
XOREnDecrypt - Encrypts an entire buffer. Not that the values are in bytes,
   so encyrption can be on odd DWORD alignments

inputs
   PBYTE    *pData - Data to encrypt (in place)
   DWORD    dwSize - size (in bytes) of the data
returns
   none
*/

void  CEncrypt::XOREnDecrypt (PBYTE pData, DWORD dwSize)
{
   HANGFUNCIN;
   DWORD dwTemp, dwVal, dwValInv;
   DWORD dwPos;
   DWORD dwNumSequence;
   PCRandSequence *pr;
   dwNumSequence = m_dwChars / 2;
   if (!dwNumSequence)
      return;  // nothing to encrypt
   pr = m_apRS;

   DWORD dwStartPosn = 0;
   //DWORD    dwStartPosn - start of the encryption position, used in GetValue().
   //            This is automatically converted to DWORDs, and odd alignments are
   //            dealt with
   // if the start position is not DWORD align then get it aligned.
   if (dwStartPosn % sizeof(DWORD)) {
      dwTemp = 0;
      dwVal = dwStartPosn % sizeof(DWORD);
      dwValInv = sizeof(DWORD) - dwVal;
      memcpy ((PBYTE) (&dwTemp) + dwVal, pData, min(dwValInv, dwSize));
      
      // encrypt
      dwPos = (dwStartPosn - dwVal) / sizeof(DWORD);
      dwTemp ^= pr[dwPos % dwNumSequence]->GetValue (dwPos / dwNumSequence);

      // write back
      memcpy (pData, (PBYTE) (&dwTemp) + dwVal, min(dwValInv, dwSize));

      // adjust the offsets
      pData += min(dwValInv, dwSize);
      dwSize -= min(dwValInv, dwSize);
      dwStartPosn += dwValInv;
   }

   // loop through data
   for (; dwSize >= sizeof(DWORD); dwSize -= sizeof(DWORD), pData += sizeof(DWORD), dwStartPosn += sizeof(DWORD)) {
      DWORD *pdw;
      pdw = (DWORD*) pData;
      dwPos = dwStartPosn / sizeof(DWORD);
      *pdw = *pdw ^ pr[dwPos % dwNumSequence]->GetValue (dwPos / dwNumSequence);
   }

   // if there's remaining then special case
   if (dwSize) {
      dwTemp = 0;
      memcpy (&dwTemp, pData, dwSize);
      dwPos = dwStartPosn / sizeof(DWORD);
      dwTemp ^= pr[dwPos % dwNumSequence]->GetValue (dwPos / dwNumSequence);
      memcpy (pData, &dwTemp, dwSize);
   }

   // done

}

/****************************************************************************
SuffleEncrypt - Encrypts an entire buffer. Not that the values are in bytes.

inputs
   PBYTE    pData - Data to encrypt (in place)
   DWORD    dwSize - size (in bytes) of the data
returns
   none
*/

void  CEncrypt::ShuffleEncrypt (PBYTE pData, DWORD dwSize)
{
   HANGFUNCIN;
   DWORD dwPos;
   DWORD dwNumSequence;
   PCRandSequence *pr;
   dwNumSequence = m_dwChars - (m_dwChars / 2);
   if (!dwNumSequence)
      return;  // nothing to encrypt
   pr = m_apRS + (m_dwChars/2);

   DWORD dwFlipWith;
   BYTE  bTemp;
   for (dwPos = 0; dwPos < dwSize; dwPos++) {
      dwFlipWith = dwPos + pr[dwPos % dwNumSequence]->GetValue(dwPos / dwNumSequence);
      dwFlipWith = dwFlipWith % dwSize;

      bTemp = pData[dwPos];
      pData[dwPos] = pData[dwFlipWith];
      pData[dwFlipWith] = bTemp;
   }

}

/****************************************************************************
SuffleDecrypt - Decrypts an entire buffer. Not that the values are in bytes.

inputs
   PBYTE    pData - Data to encrypt (in place)
   DWORD    dwSize - size (in bytes) of the data
returns
   none
*/

void  CEncrypt::ShuffleDecrypt (PBYTE pData, DWORD dwSize)
{
   HANGFUNCIN;
   DWORD dwPos;
   DWORD dwNumSequence;
   PCRandSequence *pr;
   dwNumSequence = m_dwChars - (m_dwChars / 2);
   if (!dwNumSequence)
      return;  // nothing to encrypt
   pr = m_apRS + (m_dwChars/2);

   DWORD dwFlipWith;
   BYTE  bTemp;
   // work backwards for decrypt
   for (dwPos = dwSize - 1; dwPos < dwSize; dwPos--) {
      dwFlipWith = dwPos + pr[dwPos % dwNumSequence]->GetValue(dwPos / dwNumSequence);
      dwFlipWith = dwFlipWith % dwSize;

      bTemp = pData[dwPos];
      pData[dwPos] = pData[dwFlipWith];
      pData[dwFlipWith] = bTemp;
   }

}
