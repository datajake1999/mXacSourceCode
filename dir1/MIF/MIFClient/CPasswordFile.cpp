/*************************************************************************************
CPasswordFile.cpp - For storing passwords

begun 10/11/06 by Mike Rozak.
Copyright 2006 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include <zmouse.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"




// globals
static PCPasswordFile gpPasswordFile = NULL;     // password file
static DWORD gdwPasswordFileCount = 0;          // password file ref count
static WCHAR gszPasswordFile[256] = L"";         // password file name
static WCHAR gszPasswordFilePassword[64] = L"";  // password file password
static DWORD gdwPasswordUniqueID = 0;           // unique ID

/*************************************************************************************
CPasswordFileAccount:: Constructor and destructor
*/
CPasswordFileAccount::CPasswordFileAccount (void)
{
   m_pCPasswordFile = NULL;
   m_pCResTitleInfoSet = NULL;
   m_pMMLNodeMisc = NULL;

   Clear();
}

CPasswordFileAccount::~CPasswordFileAccount (void)
{
   Clear();

   if (m_pMMLNodeMisc)
      delete m_pMMLNodeMisc;
   m_pMMLNodeMisc = NULL;
}



/*************************************************************************************
CPasswordFileAccount::Init - You must call this to set the password file.

returns
   BOOL - TRUE if success. FALSE if already initialized
*/
BOOL CPasswordFileAccount::Init (PCPasswordFile pPasswordFile)
{
   if (m_pCPasswordFile)
      return FALSE;

   m_pCPasswordFile = pPasswordFile;
   
   return TRUE;
}


static PWSTR gpszPasswordFileAccount = L"PasswordFileAccount";
static PWSTR gpszUser = L"User";
static PWSTR gpszPassword = L"Password";
static PWSTR gpszLastUsed = L"LastUsed";
static PWSTR gpszResTitleInfoSet = L"ResTitleInfoSet";
static PWSTR gpszShard = L"Shard";
static PWSTR gpszFileName = L"FileName";
static PWSTR gpszUniqueID = L"UniqueID";
static PWSTR gpszName = L"Name";
static PWSTR gpszNewUser = L"NewUser";
static PWSTR gpszMMLNodeMisc = L"MMLNodeMisc";

/*************************************************************************************
CPasswordFileAccount::MMLTo - Standard API
*/
PCMMLNode2 CPasswordFileAccount::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszPasswordFileAccount);

   // encode the user name
   BYTE abTemp[max(sizeof(m_szUser), sizeof(m_szPassword))];
   DWORD dwSize;
   dwSize = (DWORD)wcslen(m_szUser) * sizeof(WCHAR);
   memcpy (abTemp, m_szUser, dwSize);
   if (m_pCPasswordFile)
      EncodeDecode (m_pCPasswordFile->m_szPassword, abTemp, dwSize);
   MMLValueSet (pNode, gpszUser, abTemp, dwSize);

   // encode the password
   dwSize = (DWORD)wcslen(m_szPassword) * sizeof(WCHAR);
   memcpy (abTemp, m_szPassword, dwSize);
   if (m_pCPasswordFile)
      EncodeDecode (m_pCPasswordFile->m_szPassword, abTemp, dwSize);
   MMLValueSet (pNode, gpszPassword, abTemp, dwSize);

   MMLValueSet (pNode, gpszLastUsed, (int)m_dwLastUsed);
   MMLValueSet (pNode, gpszNewUser, (int)m_fNewUser);
   MMLValueSet (pNode, gpszShard, (int)m_dwShard);
   MMLValueSet (pNode, gpszUniqueID, (int)m_dwUniqueID);

   if (m_pCResTitleInfoSet) {
      PCMMLNode2 pSub = m_pCResTitleInfoSet->MMLTo ();
      if (pSub) {
         pSub->NameSet (gpszResTitleInfoSet);
         pNode->ContentAdd (pSub);
      }
   }

   PWSTR psz = (PWSTR)m_memFileName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszFileName, psz);

   psz = (PWSTR)m_memName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszName, psz);

   if (m_pMMLNodeMisc) {
      PCMMLNode2 pSub = m_pMMLNodeMisc->Clone();
      pSub->NameSet (gpszMMLNodeMisc);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}


/*************************************************************************************
CPasswordFileAccount::MMLFrom - Standard API
*/
BOOL CPasswordFileAccount::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   // get the user name and decode
   BYTE abTemp[max(sizeof(m_szUser), sizeof(m_szPassword))];
   PWSTR psz = (PWSTR) &abTemp[0];
   DWORD dwSize;
   dwSize = (DWORD) MMLValueGetBinary (pNode, gpszUser, abTemp, sizeof(abTemp));
   if (m_pCPasswordFile)
      EncodeDecode (m_pCPasswordFile->m_szPassword, abTemp, dwSize);
   psz[dwSize / sizeof(WCHAR)] = 0; // NULL terminate
   wcscpy (m_szUser, psz);

   // get the password and recode
   dwSize = (DWORD) MMLValueGetBinary (pNode, gpszPassword, abTemp, sizeof(abTemp));
   if (m_pCPasswordFile)
      EncodeDecode (m_pCPasswordFile->m_szPassword, abTemp, dwSize);
   psz[dwSize / sizeof(WCHAR)] = 0; // NULL terminate
   wcscpy (m_szPassword, psz);


   m_dwLastUsed = (DWORD) MMLValueGetInt (pNode, gpszLastUsed, (int)m_dwLastUsed);
   m_fNewUser = (BOOL) MMLValueGetInt (pNode, gpszNewUser, (int)m_fNewUser);
   m_dwShard = (DWORD)MMLValueGetInt (pNode, gpszShard, (int)m_dwShard);
   m_dwUniqueID = (DWORD) MMLValueGetInt (pNode, gpszUniqueID, (int)m_dwUniqueID);

   // find the title info
   PCMMLNode2 pSub;
   pSub = NULL;
   psz = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszResTitleInfoSet), &psz, &pSub);
   if (pSub) {
      m_pCResTitleInfoSet = new CResTitleInfoSet;
      if (m_pCResTitleInfoSet)
         m_pCResTitleInfoSet->MMLFrom (pSub);
      else {
         delete m_pCResTitleInfoSet;
         m_pCResTitleInfoSet = NULL;
      }
   }

   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszMMLNodeMisc), &psz, &pSub);
   if (pSub) {
      if (m_pMMLNodeMisc)
         delete m_pMMLNodeMisc;
      m_pMMLNodeMisc = pSub->Clone();
   }

   psz = MMLValueGet (pNode, gpszFileName);
   if (psz)
      MemCat (&m_memFileName, psz);

   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   return TRUE;
}



/*************************************************************************************
CPasswordFileAccount::CloneTo - Standard API
*/
BOOL CPasswordFileAccount::CloneTo (CPasswordFileAccount *pTo)
{
   pTo->Clear();

   wcscpy (pTo->m_szUser, m_szUser);
   wcscpy (pTo->m_szPassword, m_szPassword);
   pTo->m_dwLastUsed = m_dwLastUsed;
   pTo->m_fNewUser = m_fNewUser;
   pTo->m_dwShard = m_dwShard;
   pTo->m_dwUniqueID = m_dwUniqueID;
   if (m_pCResTitleInfoSet) {
      pTo->m_pCResTitleInfoSet = m_pCResTitleInfoSet->Clone();
      if (!pTo->m_pCResTitleInfoSet)
         return FALSE;
   }
   pTo->m_pCPasswordFile = m_pCPasswordFile;

   MemCat (&pTo->m_memFileName, (PWSTR)m_memFileName.p);
   MemCat (&pTo->m_memName, (PWSTR)m_memName.p);

   if (pTo->m_pMMLNodeMisc)
      delete pTo->m_pMMLNodeMisc;
   pTo->m_pMMLNodeMisc = NULL;
   if (m_pMMLNodeMisc)
      pTo->m_pMMLNodeMisc = m_pMMLNodeMisc->Clone();

   return TRUE;
}


/*************************************************************************************
CPasswordFileAccount::Clone - Standard API
*/
CPasswordFileAccount *CPasswordFileAccount::Clone (void)
{
   PCPasswordFileAccount pNew = new CPasswordFileAccount;
   if (!pNew)
      return NULL;

   if (!CloneTo(pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}

/*************************************************************************************
CPasswordFileAccount::Clear - Standard API
*/
void CPasswordFileAccount::Clear (void)
{
   if (m_pMMLNodeMisc)
      delete m_pMMLNodeMisc;
   m_pMMLNodeMisc = new CMMLNode2;

   m_szUser[0] = 0;
   m_szPassword[0] = 0;
   m_dwLastUsed = DFDATEToday(TRUE);
   m_fNewUser = TRUE;
   m_dwShard = 0;
   m_dwUniqueID = 0;
   if (m_pCResTitleInfoSet) {
      delete m_pCResTitleInfoSet;
      m_pCResTitleInfoSet = NULL;
   }

   MemZero (&m_memFileName);
   MemZero (&m_memName);
}


/*************************************************************************************
CPasswordFileAccount::Dirty - Set the dirty flag so this will be saved
when the object is deleted.
*/
void CPasswordFileAccount::Dirty (void)
{
   if (m_pCPasswordFile)
      m_pCPasswordFile->Dirty ();
}


/*************************************************************************************
CPasswordFileAccount::GenerateUniqueID - Come up with a unique ID.
This requires m_pCPasswordFile to be filled in.
*/
void CPasswordFileAccount::GenerateUniqueID (void)
{
   DWORD i;

   while (TRUE) {
      DWORD dwTry = m_pCPasswordFile->m_dwUniqueIDCur;
      m_pCPasswordFile->m_dwUniqueIDCur++;
      m_pCPasswordFile->Dirty();

      PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)m_pCPasswordFile->m_lPCPasswordFileAccount.Get(0);
      for (i = 0; i < m_pCPasswordFile->m_lPCPasswordFileAccount.Num(); i++)
         if (ppfa[i]->m_dwUniqueID == dwTry)
            break;
      if (i < m_pCPasswordFile->m_lPCPasswordFileAccount.Num())
         continue;   // not unique

      // else, create
      m_dwUniqueID = dwTry;
      Dirty();
      return;
   } // while TRUE
}

/*************************************************************************************
CPasswordFileAccount::GenerateUserPassword - Call this method to generate
a random user and password. This fills in m_szUser and m_szPassword.

inputs
   PWSTR          pszPasswordFile - Password file, from PasswordFileGet.
*/
void CPasswordFileAccount::GenerateUserPassword (PWSTR pszPasswordFile)
{
   DWORD dwString, i;
   DWORD dwPasswordFileLen = (DWORD)wcslen(pszPasswordFile);

   // time since start
   LARGE_INTEGER liCount;
   QueryPerformanceCounter (&liCount);
   __int64 iCount;

   // filetime
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);

   // disk space
   ULARGE_INTEGER iAvail, iTotal, iTotalFree;
   GetDiskFreeSpaceEx ("c:\\", &iAvail, &iTotal, &iTotalFree);

   for (dwString = 0; dwString < 2; dwString++) {
      // XOR of password file
      // use every other character so some is applied to user and some applied to password
      DWORD dwXOR = 0;
      for (i = dwString; i < dwPasswordFileLen; i += 2)
         dwXOR ^= ((DWORD)pszPasswordFile[i] << (((i/2)%4) * 8));

      // get the time and use as value
      DWORD dwTime = 0;
      iCount = *((__int64*) &liCount);
      if (dwString)
         iCount >>= 1;  // take off a bit
      for (i = 0; i < 32; i++, iCount >>= 2) {
         if (iCount % 2)
            dwTime |= 1;
         dwTime <<= 1;
      }

      // include filetime
      dwXOR ^= ft.dwHighDateTime;
      dwTime ^= ft.dwLowDateTime;

      // include disk space
      dwTime ^= iTotal.HighPart;
      dwTime ^= iTotalFree.LowPart;
      dwXOR ^= iTotal.LowPart;
      dwXOR ^= iTotalFree.HighPart;

      // one really large number
      unsigned __int64 qHuge = ((__int64)dwTime << 32) | (__int64)dwXOR;

      // generate characters from this
      BOOL fFirstChar = TRUE;
      PWSTR psz = dwString ? m_szUser : m_szPassword;
      while (qHuge) {
         unsigned __int64 qMax = fFirstChar ? 26 : 36;

         WCHAR wValue = (WCHAR)(qHuge % qMax);
         if (wValue < 26)
            psz[0] = L'a' + wValue;
         else
            psz[0] = L'0' + (wValue - 26);

         // advance
         psz++;
         qHuge /= qMax;
         fFirstChar = FALSE;
      }
      psz[0] = 0;
   } // dwString
}


/*************************************************************************************
CPasswordFileAccount::EncodeDecode - Encodes/decodes some binary data based
on a password.

inputs
   PWSTR          pszPassword - Password
   PBYTE          pabData - Data to encode/decode
   DWORD          dwSize - Number of bytes
*/
void CPasswordFileAccount::EncodeDecode (PWSTR pszPassword, PBYTE pabData, DWORD dwSize)
{
   // if empty password then do nothing
   if (!pszPassword[0])
      return;

   DWORD    dwSeed = 0;
   DWORD dwLen = (DWORD)wcslen(pszPassword);
   DWORD i;
   for (i = 0; i < dwLen; i++)
      dwSeed ^= ((DWORD) pszPassword[i] << ((i % 4) * 8));

   srand (dwSeed);

   for (i = 0; i < dwSize; i++)
      pabData[i] ^= (BYTE) rand();
}




/*************************************************************************************
CPasswordFile::Constructor and destructor */
CPasswordFile::CPasswordFile (void)
{
   m_lPCPasswordFileAccount.Init (sizeof(PCPasswordFileAccount));
   m_pMMLNodeMisc = NULL;

   Clear();
}

CPasswordFile::~CPasswordFile (void)
{
   if (m_fDirty && m_szFile[0])
      Save ();

   Clear();

   if (m_pMMLNodeMisc)
      delete m_pMMLNodeMisc;
   m_pMMLNodeMisc = NULL;
}


/*************************************************************************************
CPasswordFile::Clear - Clears out the contents
*/
void CPasswordFile::Clear (void)
{
   if (m_pMMLNodeMisc)
      delete m_pMMLNodeMisc;
   m_pMMLNodeMisc = new CMMLNode2;

   m_fDirty = FALSE;
   m_szFile[0] = 0;
   m_szPassword[0] = 0;

   m_szEmail[0] = 0;
   m_szDescLink[0] = 0;
   m_szDescLink[0] = 0;
   m_szPrefName[0] = 0;
   MemZero (&m_memPrefDesc);
   m_dwAge = 0;
   m_dwHoursPlayed = 5;
   m_dwPasswordChecksum = 0;
   m_dwUniqueIDCur = 1;
   m_fExposeTimeZone = TRUE;
   m_fDisableTutorial = FALSE;
   m_dwTimesSinceRemind = DEFAULTTIMESSINCEREMIND;
   m_iPrefGender = 0;

   m_fSimplifiedUI = FALSE;

   DWORD i;
   PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)m_lPCPasswordFileAccount.Get(0);
   for (i = 0; i < m_lPCPasswordFileAccount.Num(); i++)
      delete ppfa[i];
   m_lPCPasswordFileAccount.Clear();
}

static PWSTR gpszEmail = L"Email";
static PWSTR gpszPrefName = L"PrefName";
static PWSTR gpszPrefDesc = L"PrefDesc";
static PWSTR gpszAge = L"Age";
static PWSTR gpszHoursPlayed = L"HoursPlayed";
static PWSTR gpszExposeTimeZone = L"ExposeTimeZone";
static PWSTR gpszDisableTutorial = L"DisableTutorial";
static PWSTR gpszPrefGender = L"PrefGender";
static PWSTR gpszPasswordFile = L"PasswordFile";
static PWSTR gpszPasswordChecksum = L"PasswordChecksum";
static PWSTR gpszUniqueIDCur = L"UniqueIDCur";
static PWSTR gpszDescLink = L"DescLink";
static PWSTR gpszTimesSinceRemind = L"TimesSinceRemind";

/*************************************************************************************
CPasswordFile::MMLTo - Standard API
*/
PCMMLNode2 CPasswordFile::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszPasswordFile);

   if (m_szEmail[0])
      MMLValueSet (pNode, gpszEmail, m_szEmail);
   if (m_szDescLink[0])
      MMLValueSet (pNode, gpszDescLink, m_szDescLink);
   if (m_szPrefName[0])
      MMLValueSet (pNode, gpszPrefName, m_szPrefName);
   PWSTR psz = (PWSTR)m_memPrefDesc.p;
   if (psz && psz[0])
      MMLValueSet (pNode, gpszPrefDesc, psz);

   MMLValueSet (pNode, gpszAge, (int)m_dwAge);
   MMLValueSet (pNode, gpszHoursPlayed, (int)m_dwHoursPlayed);
   MMLValueSet (pNode, gpszUniqueIDCur, (int)m_dwUniqueIDCur);
   MMLValueSet (pNode, gpszExposeTimeZone, (int)m_fExposeTimeZone);
   MMLValueSet (pNode, gpszDisableTutorial, (int)m_fDisableTutorial);
   MMLValueSet (pNode, gpszTimesSinceRemind, (int)m_dwTimesSinceRemind);
   MMLValueSet (pNode, gpszPrefGender, m_iPrefGender);
   MMLValueSet (pNode, gpszPasswordChecksum, (int)m_dwPasswordChecksum);

   // password file accounts
   DWORD i;
   PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)m_lPCPasswordFileAccount.Get(0);
   PCMMLNode2 pSub;
   for (i = 0; i < m_lPCPasswordFileAccount.Num(); i++) {
      pSub = ppfa[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   if (m_pMMLNodeMisc) {
      PCMMLNode2 pSub = m_pMMLNodeMisc->Clone();
      pSub->NameSet (gpszMMLNodeMisc);
      pNode->ContentAdd (pSub);
   }
   return pNode;
}


/*************************************************************************************
CPasswordFile::MMLFrom - Standard API
*/
BOOL CPasswordFile::MMLFrom (PWSTR pszPassword, PCMMLNode2 pNode)
{
   Clear();

   wcscpy (m_szPassword, pszPassword);

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszEmail);
   if (psz && (wcslen(psz) < sizeof(m_szEmail) / sizeof(WCHAR)-1))
      wcscpy (m_szEmail, psz);

   psz = MMLValueGet (pNode, gpszDescLink);
   if (psz && (wcslen(psz) < sizeof(m_szDescLink) / sizeof(WCHAR)-1))
      wcscpy (m_szDescLink, psz);

   psz = MMLValueGet (pNode, gpszPrefName);
   if (psz && (wcslen(psz) < sizeof(m_szPrefName) / sizeof(WCHAR)-1))
      wcscpy (m_szPrefName, psz);

   psz = MMLValueGet (pNode, gpszPrefDesc);
   if (psz)
      MemCat (&m_memPrefDesc, psz);

   m_dwAge = (DWORD) MMLValueGetInt (pNode, gpszAge, (int)m_dwAge);
   m_dwHoursPlayed = (DWORD) MMLValueGetInt (pNode, gpszHoursPlayed, (int)m_dwHoursPlayed);
   m_dwPasswordChecksum = (DWORD) MMLValueGetInt (pNode, gpszPasswordChecksum, (int)m_dwPasswordChecksum);
   m_dwUniqueIDCur = (DWORD) MMLValueGetInt (pNode, gpszUniqueIDCur, (int)m_dwUniqueIDCur);
   m_fExposeTimeZone = (BOOL) MMLValueGetInt (pNode, gpszExposeTimeZone, (int)m_fExposeTimeZone);
   m_fDisableTutorial = (BOOL) MMLValueGetInt (pNode, gpszDisableTutorial, (int)m_fDisableTutorial);
   m_dwTimesSinceRemind = (DWORD) MMLValueGetInt (pNode, gpszTimesSinceRemind, DEFAULTTIMESSINCEREMIND);
   m_iPrefGender = MMLValueGetInt (pNode, gpszPrefGender, m_iPrefGender);

   // password file accounts
   PCMMLNode2 pSub;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      psz = pSub->NameGet ();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszPasswordFileAccount)) {
         PCPasswordFileAccount pNew = new CPasswordFileAccount;
         if (!pNew)
            continue;
         pNew->Init (this);
         if (!pNew->MMLFrom (pSub)) {
            delete pSub;
            continue;
         }
         m_lPCPasswordFileAccount.Add (&pNew);
         continue;
      }
      else if (!_wcsicmp(psz, gpszMMLNodeMisc)) {
         if (m_pMMLNodeMisc)
            delete m_pMMLNodeMisc;
         m_pMMLNodeMisc = pSub->Clone();
      }
   } // i

   return TRUE;
}


/*************************************************************************************
CPasswordFile::CloneTo - Standard API
*/
BOOL CPasswordFile::CloneTo (CPasswordFile *pTo)
{
   pTo->Clear();

   pTo->m_fDirty = m_fDirty;
   wcscpy (pTo->m_szFile, m_szFile);
   wcscpy (pTo->m_szPassword, m_szPassword);


   wcscpy (pTo->m_szEmail, m_szEmail);
   wcscpy (pTo->m_szDescLink, m_szDescLink);
   wcscpy (pTo->m_szPrefName, m_szPrefName);
   MemCat (&pTo->m_memPrefDesc, (PWSTR)m_memPrefDesc.p);

   pTo->m_dwAge = m_dwAge;
   pTo->m_dwHoursPlayed = m_dwHoursPlayed;
   pTo->m_dwPasswordChecksum = m_dwPasswordChecksum;
   pTo->m_dwUniqueIDCur = m_dwUniqueIDCur;
   pTo->m_iPrefGender = m_iPrefGender;
   pTo->m_fExposeTimeZone = m_fExposeTimeZone;
   pTo->m_fDisableTutorial = m_fDisableTutorial;
   pTo->m_dwTimesSinceRemind = m_dwTimesSinceRemind;

   // dont do m_fSimplifiedUI

   pTo->m_lPCPasswordFileAccount.Init (sizeof(PCPasswordFileAccount), m_lPCPasswordFileAccount.Get(0),m_lPCPasswordFileAccount.Num());
   PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)pTo->m_lPCPasswordFileAccount.Get(0);
   DWORD i;
   for (i = 0; i < pTo->m_lPCPasswordFileAccount.Num(); i++)
      ppfa[i] = ppfa[i]->Clone();

   if (pTo->m_pMMLNodeMisc)
      delete pTo->m_pMMLNodeMisc;
   pTo->m_pMMLNodeMisc = NULL;
   if (m_pMMLNodeMisc)
      pTo->m_pMMLNodeMisc = m_pMMLNodeMisc->Clone();

   return TRUE;
}



/*************************************************************************************
CPasswordFile::Clone - Standard API
*/
CPasswordFile *CPasswordFile::Clone (void)
{
   PCPasswordFile pNew = new CPasswordFile;
   if (!pNew)
      return NULL;

   if (!CloneTo(pNew)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}




/*************************************************************************************
CPasswordFile::Open - This opens the given file.

inputs
   PWSTR             pszFile - FIle
   PWSTR             pszPassword - Password used.
   BOOL              fCreateIfNotExist - If the file doesn't exist then it's created.
returns
   BOOL - TRUE if successfully opened (or created). FALSE if failed to open (or create).
*/
BOOL CPasswordFile::Open (PWSTR pszFile, PWSTR pszPassword, BOOL fCreateIfNotExist)
{
   if (wcslen(pszFile) > sizeof(m_szFile)/sizeof(WCHAR)-1)
      return FALSE;

   PCMMLNode2 pNode = MMLFileOpen (pszFile, &GUID_PasswordFile, NULL, FALSE, TRUE);

   if (pNode) {
      if (!MMLFrom (pszPassword, pNode)) {
         delete pNode;
         return FALSE;
      }

      wcscpy (m_szFile, pszFile);

      delete pNode;
      return TRUE;
   }

   // doesn't exist. see if want to create
   if (!fCreateIfNotExist)
      return FALSE;

   // create
   Clear ();
   wcscpy (m_szFile, pszFile);
   wcscpy (m_szPassword, pszPassword);

   // remember the password checksum
   m_dwPasswordChecksum = Checksum (pszPassword);

   return Save();
}



/*************************************************************************************
CPasswordFile::Save - Saves the file using the existing m_szFile name.

This is AUTOMATICALLY called on the destruction of the object IF it's dirty and
there's a file name,
*/
BOOL CPasswordFile::Save (void)
{
   if (!m_szFile[0])
      return FALSE;

   PCMMLNode2 pNode = MMLTo ();
   if (!pNode)
      return FALSE;

   BOOL fRet = MMLFileSave (m_szFile, &GUID_PasswordFile, pNode, 1, NULL, TRUE);

   delete pNode;

   m_fDirty = FALSE;

   return fRet;
}


/*************************************************************************************
CPasswordFile::Checksum - Calculates the checksum for a password.

If changing the password then set m_dwPasswordChecksum to this.

If trying to log on, compare m_dwPasswordChecksum to this.
*/
DWORD CPasswordFile::Checksum (PWSTR pszPassword)
{
   DWORD dwSum = 0;
   DWORD dwXOR = 0;
   DWORD i;
   for (i = 0; pszPassword[i]; i++) {
      dwSum += (DWORD)pszPassword[i];
      dwXOR ^= (DWORD)pszPassword[i];
   }

   return (dwSum & 0xff) | ((dwXOR & 0xff) << 8);
}



/*************************************************************************************
CPasswordFile::AccountFind - Given a unique ID, finds it in the password file.

inputs
   DWORD                   dwID - Unique ID
returns
   DWORD - Index into password file list, or -1 if can't find
*/
DWORD CPasswordFile::AccountFind (DWORD dwID)
{
   PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)m_lPCPasswordFileAccount.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCPasswordFileAccount.Num(); i++)
      if (ppfa[i]->m_dwUniqueID == dwID)
         return i;

   return (DWORD)-1;
}


/*************************************************************************************
CPasswordFile::AccountGet - Given an index, this returns the account object.

inputs
   DWORD          dwIndex - Index to accounts. Can get from AccountFind() or AccountNum().

returns
   PCPasswordFileAccount - Returns the account, or NULL if error
*/
PCPasswordFileAccount CPasswordFile::AccountGet (DWORD dwIndex)
{
   PCPasswordFileAccount *ppfa = (PCPasswordFileAccount*)m_lPCPasswordFileAccount.Get(dwIndex);
   if (!ppfa)
      return NULL;

   return ppfa[0];
}



/*************************************************************************************
CPasswordFile::AccountNum - Returns the number of accounts.

returns
   DWORD - Number
*/
DWORD CPasswordFile::AccountNum (void)
{
   return m_lPCPasswordFileAccount.Num();
}


/*************************************************************************************
CPasswordFile::AccountRemove - Given an index, this deletes an account object.

inputs
   DWORD          dwIndex - Index to accounts. Can get from AccountFind() or AccountNum().

returns
   BOOL - TRUE if success
*/
BOOL CPasswordFile::AccountRemove (DWORD dwIndex)
{
   PCPasswordFileAccount pfa = AccountGet (dwIndex);
   if (!pfa)
      return FALSE;

   delete pfa;
   m_lPCPasswordFileAccount.Remove (dwIndex);

   Dirty();

   return TRUE;
}


/*************************************************************************************
CPasswordFile::AccountNew - Create a new account.

inputs
   PCResTitleInfoSet       pInfoSet - Info set to use. This is cloned
   DWORD                   dwShard - Shard number, or 0 if not needed
   LANGID                  LID - Language ID to use for the name
   PWSTR                   pszPasswordFile - Where the password file is saved (from
                                 PasswordFileGet()). Used to create a hash for a unique user ID/password.
   PWSTR                   pszFile - If the account if for a .crf (NOT a link) then set this
                                 to the file name. If this is for a .crk (link) then
                                 use NULL
returns
   DWORD - Unique ID for the new account, or 0 if error
*/
DWORD CPasswordFile::AccountNew (PCResTitleInfoSet pInfoSet, DWORD dwShard, LANGID LID,
                                 PWSTR pszPasswordFile, PWSTR pszFile)
{
   PCPasswordFileAccount pNew = new CPasswordFileAccount;
   if (!pNew)
      return 0;

   if (!pNew->Init (this)) {
      delete pNew;
      return 0;
   }
   pNew->GenerateUniqueID ();
   pNew->GenerateUserPassword (pszPasswordFile);

   pNew->m_dwLastUsed = DFDATEToday(TRUE);
   pNew->m_dwShard = dwShard;
   pNew->m_fNewUser = TRUE;
   MemZero (&pNew->m_memFileName);
   if (pszFile)
      MemCat (&pNew->m_memFileName, pszFile);

   // make up a name
   PCResTitleInfo pInfo = pInfoSet->Find (LID);
   MemZero (&pNew->m_memName);
   if (pInfo)
      MemCat (&pNew->m_memName, (PWSTR)pInfo->m_memName.p);
   
   // title
   if (pNew->m_pCResTitleInfoSet)
      delete pNew->m_pCResTitleInfoSet;
   pNew->m_pCResTitleInfoSet = pInfoSet->Clone();

   // add this to own list
   m_lPCPasswordFileAccount.Add (&pNew);
   Dirty();

   return pNew->m_dwUniqueID;
}


static int _cdecl PCPasswordFileAccountSort (const void *elem1, const void *elem2)
{
   PCPasswordFileAccount *pdw1, *pdw2;
   pdw1 = (PCPasswordFileAccount*) elem1;
   pdw2 = (PCPasswordFileAccount*) elem2;

   if (pdw1[0]->m_dwLastUsed != pdw2[0]->m_dwLastUsed)
      return (int)pdw2[0]->m_dwLastUsed - (int)pdw1[0]->m_dwLastUsed;

   return _wcsicmp((PWSTR) pdw1[0]->m_memName.p, (PWSTR) pdw2[0]->m_memName.p);
}

/*************************************************************************************
CPasswordFile::AccountSort - Sorts the accounts by last used.

NOTE: This DOESN'T set the dirty flag.
*/
void CPasswordFile::AccountSort (void)
{
   qsort (m_lPCPasswordFileAccount.Get(0), m_lPCPasswordFileAccount.Num(), sizeof(PCPasswordFileAccount), PCPasswordFileAccountSort);
}

/*************************************************************************************
CPasswordFile::Dirty - Set the dirty flag so this will be saved
when the object is deleted.
*/
void CPasswordFile::Dirty (void)
{
   m_fDirty = TRUE;
}


/*************************************************************************************
CPasswordFile::PasswordChange- Call this to change the password

inputs
   PWSTR       pszPassword - New passsword
returns
   BOOL - TRUE if success
*/
BOOL CPasswordFile::PasswordChange (PWSTR pszPassword)
{
   if (!pszPassword || (wcslen(pszPassword) > sizeof(m_szPassword)/sizeof(WCHAR) - 1))
      return FALSE;

   wcscpy (m_szPassword, pszPassword);
   m_dwPasswordChecksum = Checksum (m_szPassword);

   // set dirty so will save
   Dirty ();

   return TRUE;
}



/*************************************************************************************
PasswordFileCache - Causes the password file to be cached if it isn't alredy.
You MUST call PasswordFileRelease() to free the memory (and potentially save the file)

inputs
   BOOL           fCreateIfNotExist - If no file the create.
*/
PCPasswordFile PasswordFileCache (BOOL fCreateIfNotExist)
{
   if (gdwPasswordFileCount) {
      gdwPasswordFileCount++;
      return gpPasswordFile;
   }

   // make sure there's a password file
   if (!gszPasswordFile[0])
      return NULL;

   gpPasswordFile = new CPasswordFile;
   if (!gpPasswordFile)
      return NULL;
   if (!gpPasswordFile->Open (gszPasswordFile, gszPasswordFilePassword, fCreateIfNotExist)) {
      delete gpPasswordFile;
      return NULL;
   }

   gdwPasswordFileCount++;
   return gpPasswordFile;
}


/*************************************************************************************
PasswordFileAccountCache - Caches the password file, but automatically gets
the PCPasswordFileAccount based on the unqiue ID set in PasswordFileUniqueIDSet().

You MUST call PasswordFileRelease() after this if it succedes.

returns
   PCPasswordFileAccount - Account
*/
PCPasswordFileAccount PasswordFileAccountCache (void)
{
   PCPasswordFile pPF = PasswordFileCache (FALSE);
   if (!pPF)
      return NULL;

   PCPasswordFileAccount pPFA = pPF->AccountGet(pPF->AccountFind(gdwPasswordUniqueID));
   if (!pPFA)
      PasswordFileRelease();
    
   return pPFA;
}

   


/*************************************************************************************
PasswordFileUniqueIDSet - Sets the unique ID to be used for the password file.
This ID is for PCPasswordFileAccount.
*/
void PasswordFileUniqueIDSet (DWORD dwID)
{
   gdwPasswordUniqueID = dwID;
}

/*************************************************************************************
PasswordFileRelease - Call this for ever call to cache.

returns
   BOOL - TRUE if success
*/
BOOL PasswordFileRelease (void)
{
   if (!gdwPasswordFileCount)
      return FALSE;

   gdwPasswordFileCount--;
   if (!gdwPasswordFileCount)
      delete gpPasswordFile;
   return TRUE;
}



/*************************************************************************************
PasswordFileGet - Returns the current password file

inputs
   PWSTR          pszFile - Fills in the filename. Must be 256 chars. This might
                  be filled in with NULL to indicate no user
   PWSTR          pszPassword - Fills in the password. Must be 64 chars.
returns
   none
*/
void PasswordFileGet (PWSTR pszFile, PWSTR pszPassword)
{
   if (pszFile)
      wcscpy (pszFile, gszPasswordFile);
   if (pszPassword)
      wcscpy (pszPassword, gszPasswordFilePassword);
}


/*************************************************************************************
PasswordFileSet - Sets the password file.

inputs
   PWSTR          pszFile - File
   PWSTR          pszPassword - Password to use
   BOOL           fCreateIfNotExist - If TRUE then create if doesn't already exist
returns
   DWORD - 0 if no error
            1 if password file already cached
            2 if couldnt open file
            3 if password didn't match checksum
            4 if strings too long
*/
DWORD PasswordFileSet (PWSTR pszFile, PWSTR pszPassword, BOOL fCreateIfNotExist)
{
   if (gdwPasswordFileCount)
      return 1;   // already cached

   // copy over
   if (wcslen(pszFile) > (sizeof(gszPasswordFile)/sizeof(WCHAR)-1))
      return 4;
   if (wcslen(pszPassword) > (sizeof(gszPasswordFilePassword)/sizeof(WCHAR)-1))
      return 4;

   wcscpy (gszPasswordFile, pszFile);
   wcscpy (gszPasswordFilePassword, pszPassword);

   if (!gszPasswordFile[0])
      return 0;   // set to nothing

   // try to open
   PCPasswordFile pFile = PasswordFileCache (fCreateIfNotExist);
   if (!pFile) {
      gszPasswordFile[0] = 0;
      return 2;
   }

   // check checksum
   if (pFile->m_dwPasswordChecksum != pFile->Checksum(pszPassword)) {
      PasswordFileRelease ();
      gszPasswordFile[0] = 0;
      return 3;
   }

   // release this since done with it
   PasswordFileRelease ();

   return 0;

}

/*************************************************************************************
RemoveIllegalFilenameChars - Removes illegal filename characters.

inputs
   PWSTR       psz - String. To be modified in place
*/
void RemoveIllegalFilenameChars (PWSTR psz)
{
   // loop
   DWORD i;
   for (i = 0; psz[i]; i++) {
      // alphabetic and numeric ok
      if (iswalnum(psz[i]))
         continue;
      switch (psz[i]) {
         // other character ok
         case L' ':
         case L'\'':
         case L'_':
            continue;
      } // switch
      
      // else, remove
      psz[i] = L' ';
   } // i

   // remove leading spaces
   for (i = 0; psz[i]; i++)
      if (psz[i] != L' ')
         break;
   if (i)
      memmove (psz, psz + i, (wcslen(psz) + 1 - i) * sizeof(WCHAR));

   // remove trailing spaces
   DWORD dwLen = (DWORD)wcslen(psz);
   for (i = dwLen-1; i < dwLen; i--)
      if (psz[i] != L' ')
         break;
      else
         psz[i] = 0;
}
