/***************************************************************************
WaveOpen.cpp - Code that opens wave file.

begun 5/9/2003
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <shlwapi.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "m3dwave.h"
#include "resource.h"

#ifdef _DEBUG
#define BLIZZARDCHALLENGE              // special code for reading in blizzard challeng files
#endif




// CWaveDirCache - Caches information about the various directories that have
// looked throug, remember what wave file information there is
class CWaveDirCache : public CEscObject {
public:
   CWaveDirCache (void);
   ~CWaveDirCache (void);

   PCWaveDirInfo DirGet (PWSTR pszDir, PCProgressSocket pProgress);

   BOOL Open (void);
   BOOL Save (void);

private:
   PCMMLNode2 MMLTo (void);
   BOOL MMLFrom (PCMMLNode2 pNode);

   CListFixed        m_lPCWaveDirInfo;       // pointer of wave dirs
};
typedef CWaveDirCache *PCWaveDirCache;


// WOD
typedef struct {
   PCWaveDirCache       pCache;     // file cache
   WCHAR                szDir[256]; // current directory
   WCHAR                szFile[256];   // file
   WCHAR                szTextFilter[128];
   WCHAR                szSpeakerFilter[128];
   BOOL                 fSave;      // if TRUE then saving file
   PCListVariable       plMultiSel; // multiple selection
} WOD, *PWOD;


/******************************************************************************
CWaveFileInfo::Constuctor and destructor*/
CWaveFileInfo::CWaveFileInfo (void)
{
   m_pszFile = m_pszText = m_pszSpeaker = NULL;
   memset (&m_FileTime, 0 ,sizeof(m_FileTime));
   m_iFileSize = 0;
   m_fDuration = 0;
   m_dwSamplesPerSec = 0;
   m_dwChannels = 0;
}

CWaveFileInfo::~CWaveFileInfo (void)
{
   // do nothing for now
}

/******************************************************************************
CWaveFileInfo::SetText - Sets the text for the file name, text spoken, and
speaker. This basically makes sure the memory is large enough and allocates
it all.

inputs
   PWSTR          pszFile - File name
   PWSTR          pszText - Text. Can be null
   PWSTR          pszSpeaker - Speaker. Can be NULL
returns
   BOOL - TRUE if success
*/
BOOL CWaveFileInfo::SetText (PWSTR pszFile, PWSTR pszText, PWSTR pszSpeaker)
{
   DWORD dwNeedFile = (DWORD)wcslen(pszFile)+1;
   DWORD dwNeedText = (pszText ? ((DWORD)wcslen(pszText)+1) : 0);
   DWORD dwNeedSpeaker = (pszSpeaker ? ((DWORD)wcslen(pszSpeaker)+1) : 0);
   DWORD dwNeed = (dwNeedFile + dwNeedText + dwNeedSpeaker) * sizeof(WCHAR);
   if (!m_memInfo.Required (dwNeed))
      return FALSE;

   m_pszFile = (PWSTR) m_memInfo.p;
   memcpy (m_pszFile, pszFile, dwNeedFile * sizeof(WCHAR));
   if (dwNeedText) {
      m_pszText = m_pszFile + dwNeedFile;
      memcpy (m_pszText, pszText, dwNeedText * sizeof(WCHAR));
   }
   else
      m_pszText = NULL;
   if (dwNeedSpeaker) {
      m_pszSpeaker = m_pszFile + (dwNeedFile + dwNeedText);
      memcpy (m_pszSpeaker, pszSpeaker, dwNeedSpeaker * sizeof(WCHAR));
   }
   else
      m_pszSpeaker = NULL;
   return TRUE;
}

/******************************************************************************
CWaveFileInfo::FillFromFile - Reads in a wave file and uses it to fill in the
given information

NOTE: This doesn't fill in file size or last modified time.

inputs
   PWSTR          pszDir - Directory, such as "c:\hello"... no last slash...
   PWSTR          pszFile - File such as "hello.wav"
   BOOL           fFast - If TRUE, then make up some values instead of opening the file
returns
   BOOL - TRUE if success. FALSE if failed
*/
BOOL CWaveFileInfo::FillFromFile (PWSTR pszDir, PWSTR pszFile, BOOL fFast)
{
   CM3DWave Wave;
   char szFile[512];
   if (pszDir) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szFile, sizeof(szFile), 0 ,0);
      strcat (szFile, "\\");
   }
   else
      szFile[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);
   
   if (fFast) {
      // just some stock values so dont open all the files
      SetText (pszFile, L"", L"");
      m_fDuration = 0;
      m_dwSamplesPerSec = 22050;
      m_dwChannels = 1;
   }
   else {
      if (!Wave.Open (NULL, szFile, FALSE))
         return FALSE;

      // store the info
      if (!SetText (pszFile, (PWSTR)Wave.m_memSpoken.p, (PWSTR)Wave.m_memSpeaker.p))
         return FALSE;
      m_fDuration = (fp)Wave.m_dwSamples / (fp)Wave.m_dwSamplesPerSec;
      m_dwSamplesPerSec = Wave.m_dwSamplesPerSec;
      m_dwChannels = Wave.m_dwChannels;
   }

   return TRUE;
}

/******************************************************************************
CWaveFileInfo::FillFromFindFile - Fills in the information about the file from
the FindNextFile() call's return.

NOTE: Becayse there isn't any info about duration, etc, that is blanked out.

inputs
   PWIN32_FIND_DATA     pFF - Info about the file
returns
   BOOL - TRUE if success
*/
BOOL CWaveFileInfo::FillFromFindFile (PWIN32_FIND_DATA pFF)
{
   WCHAR szName[256];
   MultiByteToWideChar (CP_ACP, 0, pFF->cFileName, -1, szName, sizeof(szName)/sizeof(WCHAR));

   if (!SetText (szName, NULL, NULL))
      return FALSE;

   m_FileTime = pFF->ftLastWriteTime;
   m_iFileSize = ((__int64)pFF->nFileSizeHigh << 32) + (__int64)(DWORD)pFF->nFileSizeLow;
   m_fDuration = 0;
   m_dwSamplesPerSec = 0;
   m_dwChannels = 0;
   return TRUE;
}

/*********************************************************************************
CWaveFileInfo::Clone - standard clone
*/
CWaveFileInfo *CWaveFileInfo::Clone (void)
{
   PCWaveFileInfo pNew = new CWaveFileInfo;
   if (!pNew)
      return NULL;

   pNew->SetText (m_pszFile, m_pszText, m_pszSpeaker);
   pNew->m_dwChannels = m_dwChannels;
   pNew->m_dwSamplesPerSec = m_dwSamplesPerSec;
   pNew->m_fDuration = m_fDuration;
   pNew->m_FileTime = m_FileTime;
   pNew->m_iFileSize = m_iFileSize;

   return pNew;
}


static PWSTR gpszWaveFileInfo = L"WaveFileInfo";
static PWSTR gpszFile = L"File";
static PWSTR gpszText = L"Text";
static PWSTR gpszSpeaker = L"Speaker";
static PWSTR gpszFileTime = L"FileTime";
static PWSTR gpszFileSize = L"FileSize";
static PWSTR gpszDuration = L"Duration";
static PWSTR gpszSamplesPerSec = L"SamplesPerSec";
static PWSTR gpszChannels = L"Channels";

/******************************************************************************
CWaveFileInfo::MMLTo - Standard MML to
*/
PCMMLNode2 CWaveFileInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszWaveFileInfo);
   
   if (!m_pszFile[0]) {
      delete pNode;
      return FALSE;
   }
   MMLValueSet (pNode, gpszFile, m_pszFile);
   if (m_pszText && m_pszText[0])
      MMLValueSet (pNode, gpszText, m_pszText);
   if (m_pszSpeaker && m_pszSpeaker[0])
      MMLValueSet (pNode, gpszSpeaker, m_pszSpeaker);

   MMLValueSet (pNode, gpszFileTime, (PBYTE)&m_FileTime, sizeof(m_FileTime));
   MMLValueSet (pNode, gpszFileSize, (PBYTE)&m_iFileSize, sizeof(m_iFileSize));
   MMLValueSet (pNode, gpszDuration, m_fDuration);
   MMLValueSet (pNode, gpszSamplesPerSec, (int)m_dwSamplesPerSec);
   MMLValueSet (pNode, gpszChannels, (int)m_dwChannels);
   
   return pNode;
}

/******************************************************************************
CWaveFileInfo::MMLFrom - Standard MML from
*/
BOOL CWaveFileInfo::MMLFrom (PCMMLNode2 pNode)
{
   PWSTR pszFile, pszText, pszSpeaker;
   pszFile = MMLValueGet (pNode, gpszFile);
   if (!pszFile)
      return NULL;
   pszText = MMLValueGet (pNode, gpszText);
   pszSpeaker = MMLValueGet (pNode, gpszSpeaker);
   if (!SetText (pszFile, pszText, pszSpeaker))
      return FALSE;
   

   MMLValueGetBinary (pNode, gpszFileTime, (PBYTE)&m_FileTime, sizeof(m_FileTime));
   MMLValueGetBinary (pNode, gpszFileSize, (PBYTE)&m_iFileSize, sizeof(m_iFileSize));
   m_fDuration = MMLValueGetDouble (pNode, gpszDuration, 0);
   m_dwSamplesPerSec = (DWORD) MMLValueGetInt (pNode, gpszSamplesPerSec, 0);
   m_dwChannels = (DWORD) MMLValueGetInt (pNode, gpszChannels, 0);
   
   return TRUE;
}


/******************************************************************************
CWaveFileInfo::PlayFile - Plays the file.

inputs
   PWSTR       pszDir - directory, such as "c:\hello" - no final backslash
*/
BOOL CWaveFileInfo::PlayFile (PWSTR pszDir)
{
   char szFile[512];
   if (pszDir) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, 0, szFile, sizeof(szFile), 0 ,0);
      strcat (szFile, "\\");
   }
   else
      szFile[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, m_pszFile, 0, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);

   // play
   sndPlaySound (NULL, 0);
   return sndPlaySound (szFile, SND_ASYNC | SND_NODEFAULT);
}


/******************************************************************************
CWaveDirInfo::Contructor and destructor
*/
CWaveDirInfo::CWaveDirInfo(void)
{
   m_szDir[0] = 0;
   m_lPCWaveFileInfo.Init (sizeof(PCWaveFileInfo));
}

CWaveDirInfo::~CWaveDirInfo (void)
{
   DWORD i;
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_lPCWaveFileInfo.Num(); i++)
      delete ppfi[i];
}


static PWSTR gpszWaveDirInfo = L"WaveDirInfo";
static PWSTR gpszDir = L"Dir";

/******************************************************************************
CWaveDirInfo::MMLTo - Standard MML To
*/
PCMMLNode2 CWaveDirInfo::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return pNode;
   pNode->NameSet (gpszWaveDirInfo);

   if (m_szDir && m_szDir[0])
      MMLValueSet (pNode, gpszDir, m_szDir);

   DWORD i;
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_lPCWaveFileInfo.Num(); i++) {
      PCMMLNode2 pSub = ppfi[i]->MMLTo();
      if (pSub)
         pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CWaveDirInfo::Clone - Clones this

returns
   PCWaveDirInfo - clone
*/
PCWaveDirInfo CWaveDirInfo::Clone (void)
{
   PCWaveDirInfo pClone = new CWaveDirInfo;
   if (!pClone)
      return NULL;

   wcscpy (pClone->m_szDir, m_szDir);
   pClone->m_lPCWaveFileInfo.Init (sizeof(PCWaveFileInfo), m_lPCWaveFileInfo.Get(0), m_lPCWaveFileInfo.Num());
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) pClone->m_lPCWaveFileInfo.Get(0);
   DWORD i;
   for (i = 0; i < pClone->m_lPCWaveFileInfo.Num(); i++)
      ppfi[i] = ppfi[i]->Clone();

   return pClone;
}


/******************************************************************************
CWaveDirInfo::MMLFrom - Standard
*/
BOOL CWaveDirInfo::MMLFrom (PCMMLNode2 pNode)
{
   // clear out old
   DWORD i;
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_lPCWaveFileInfo.Num(); i++)
      delete ppfi[i];
   m_lPCWaveFileInfo.Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszDir);
   if (psz)
      wcscpy (m_szDir, psz);
   else
      m_szDir[0] = 0;

   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWaveFileInfo)) {
         PCWaveFileInfo pwi = new CWaveFileInfo;
         if (!pwi)
            return FALSE;
         if (!pwi->MMLFrom (pSub))
            return FALSE;
         m_lPCWaveFileInfo.Add (&pwi);
      }
   } // i

   return TRUE;
}


static int _cdecl PCWaveFileInfoSort (const void *elem1, const void *elem2)
{
   PCWaveFileInfo *pdw1, *pdw2;
   pdw1 = (PCWaveFileInfo*) elem1;
   pdw2 = (PCWaveFileInfo*) elem2;

   return _wcsicmp(pdw1[0]->m_pszFile, pdw2[0]->m_pszFile);
}

/******************************************************************************
CWaveDirInfo::SyncFiles - This goes through the existing files in the
list and synchronizes them compared against themselves. It's different from SyncWithDirectory()
because it doesn't add or remove any files AND the files can be in different
directories is pszDir is NULL

inputs
   PCProgressSocket     pProgress - To show the progress in case there are 1000s of files
   PCWSTR               pszDir - Directory to prepend, or NULL if non
returns
   BOOL - TRUE if any files have changed, FALSE if not
*/
BOOL CWaveDirInfo::SyncFiles (PCProgressSocket pProgress, PCWSTR pszDir)
{
   BOOL fChanged = FALSE;

   // loop through each of the files
   DWORD i;
   PCWaveFileInfo *ppwi = (PCWaveFileInfo*)m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_lPCWaveFileInfo.Num(); i++) {
      PCWaveFileInfo pwi = ppwi[i];
      if (pProgress && ((i%10) == 0))
         pProgress->Update ((fp)i / (fp)m_lPCWaveFileInfo.Num());

      // create the file name
      char szTemp[512];
      if (pszDir) {
         WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szTemp, sizeof(szTemp), 0, 0);
         strcat (szTemp, "\\");
      }
      else
         szTemp[0] = 0;
      WideCharToMultiByte (CP_ACP, 0, pwi->m_pszFile, -1, szTemp + strlen(szTemp),
         sizeof(szTemp) - (DWORD)strlen(szTemp), 0 ,0);

      WIN32_FIND_DATA ffd;
      HANDLE hFile;
      hFile = CreateFile (szTemp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
         NULL, OPEN_EXISTING, 0, 0);
      if (!hFile)
         continue;
      memset (&ffd, 0, sizeof(ffd));
      GetFileTime (hFile, &ffd.ftCreationTime, &ffd.ftLastAccessTime, &ffd.ftLastWriteTime);
      ffd.nFileSizeLow = GetFileSize (hFile, &ffd.nFileSizeHigh);
      __int64 iSize = ((__int64)ffd.nFileSizeHigh << 32) + (__int64)(DWORD)ffd.nFileSizeLow;
      CloseHandle (hFile);

      // if not save time or size then resync
      if (memcmp(&pwi->m_FileTime, &ffd.ftLastWriteTime, sizeof(pwi->m_FileTime)) || (iSize != pwi->m_iFileSize)) {
         WCHAR szFile[256];
         wcscpy (szFile, pwi->m_pszFile);
         pwi->FillFromFile ((PWSTR)pszDir, szFile);
         pwi->m_FileTime = ffd.ftLastWriteTime;
         pwi->m_iFileSize = iSize;

         fChanged = TRUE;
      }
   } // i

   return fChanged;
}



/**************************************************************************
WaveDirInfoAddFilesPage
*/
BOOL WaveDirInfoAddFilesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PCM3DWave pWave = (PCM3DWave) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         PWSTR pszWord = (PWSTR)pWave->m_memSpoken.p;
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pszWord);

         // start playing
         pWave->QuickPlay ();
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"play")) {
            pWave->QuickPlayStop ();
            pWave->QuickPlay ();
            return TRUE;
         }
      }
      break;

   case ESCM_LINK:
      {
         // stop playing
         pWave->QuickPlayStop ();

         PESCMLINK p = (PESCMLINK) pParam;
         if (!p->psz)
            break;
         if (!_wcsicmp(p->psz, L"ok")) {
            PCEscControl pControl;
            pControl = pPage->ControlFind (L"name");
            if (!pControl)
               return TRUE;

            WCHAR szHuge[1024];
            szHuge[0] = 0;
            DWORD dwNeeded;
            pControl->AttribGet (Text(), szHuge, sizeof(szHuge), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBInformation (L"You must type in some text.");
               return TRUE;
            }

            MemZero (&pWave->m_memSpoken);
            MemCat (&pWave->m_memSpoken, szHuge);
            break;
         }
      }
      break;
   }

   return DefPage (pPage, dwMessage, pParam);
}



/******************************************************************************
CWaveDirInfo::AddFilesUI - Pull up UI so user can add new files to the list

NOTE: m_szDirectory is ignored, and it's assumed the entire directory is stored
in the file's name.

inputs
   HWND           hWnd - For the user interface
   BOOL           fRequireText - If TRUE require text for the wave files. If no text then
                  ask for it.
retursn
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::AddFilesUI (HWND hWnd, BOOL fRequireText)
{
   WCHAR szFile[256];
   szFile[0] = 0;
   CListVariable lMultiSel;
   if (!WaveFileOpen(hWnd, FALSE, szFile, &lMultiSel))
      return FALSE;

   // go through all the items
   DWORD i,j;
   PCWaveFileInfo *ppwi;
   PCWaveFileInfo pwi;
   CM3DWave Wave;
   for (i = 0; i < lMultiSel.Num(); i++) {
      PWSTR pszAdd = (PWSTR)lMultiSel.Get(i);

      // make sure not dups
      ppwi = (PCWaveFileInfo*)m_lPCWaveFileInfo.Get(0);
      for (j = 0; j < m_lPCWaveFileInfo.Num(); j++) {
         pwi = ppwi[j];
         if (!_wcsicmp(pwi->m_pszFile, pszAdd))
            break;
      } // j
      if (j < m_lPCWaveFileInfo.Num())
         continue;   // file already exists so dont add again

      // if fRequireText
      if (fRequireText) {
         char szaFile[512];
         WideCharToMultiByte (CP_ACP, 0, pszAdd, -1, szaFile, sizeof(szaFile), 0, 0);
         if (!Wave.Open (NULL, szaFile)) {
            EscMessageBox (hWnd, NULL, L"The file isn't a wave file, so won't be added", pszAdd, MB_OK);
            continue;
         }

         PWSTR pszSpoken = (PWSTR)Wave.m_memSpoken.p;
         if (pszSpoken[0])
            goto hastext;

#ifdef BLIZZARDCHALLENGE // BUGBUG
         // try opening a text file
         WCHAR szTextFile[256];
         wcscpy (szTextFile, pszAdd);
         DWORD dwLen = (DWORD)wcslen(szTextFile);
         if ((dwLen >= 4) && !_wcsicmp(szTextFile + (dwLen-4), L".wav"))
            wcscpy (szTextFile + (dwLen-4), L".txt");
         FILE *pf = _wfopen (szTextFile, L"rt");
         if (pf) {
            WCHAR szSent[512];
            szSent[0] = 0;
            fgetws (szSent, sizeof(szSent)/sizeof(WCHAR), pf);
            PWSTR pszChar;
            if (pszChar = wcschr (szSent, L'\r'))
               *pszChar = 0;
            if (pszChar = wcschr (szSent, L'\n'))
               *pszChar = 0;
            if (szSent[0] != 0) {
               MemZero (&Wave.m_memSpoken);
               MemCat (&Wave.m_memSpoken, szSent);

               FILE *pfAppend = _wfopen (L"c:\\Sentences.txt", L"at");
               if (pfAppend) {
                  fputws (szSent, pfAppend);
                  fputws (L"\r\n", pfAppend);
                  fclose (pfAppend);
               }
            }
            fclose (pf);
         }
#endif

         // BUGFIX - While at it, remove sub-voice, noise, and noramlize
         Wave.FXRemoveDCOffset (TRUE, NULL);
         short sMax = Wave.FindMax();
         fp f;
         sMax = max(1,sMax);
         f = 32767.0 / (fp)sMax;
         Wave.FXVolume (f, f, NULL);
         Wave.FXNoiseReduce (NULL);

         // else, ask
         PWSTR pszRet;
         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation2 (hWnd, &r);
         cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE | EWS_AUTOHEIGHT | EWS_NOTITLE, &r);
         pszRet = cWindow.PageDialog (ghInstance, IDR_MMLWAVEDIRINFOADDFILES, WaveDirInfoAddFilesPage, &Wave);
         if (!pszRet || !_wcsicmp(pszRet, L"cancel"))
            break;   // not added

         Wave.Save (TRUE, NULL);
      } // fRequireText

hastext:
      // else, add
      PCWaveFileInfo pNew = new CWaveFileInfo;
      if (!pNew)
         continue;
      pNew->SetText (pszAdd, NULL, NULL);
      m_lPCWaveFileInfo.Add (&pNew);
   } // i

   // re-sync the newly added files
   CProgress Progress;
   Progress.Start (hWnd, "Updating list...");
   SyncFiles (&Progress, NULL);

   return TRUE;
}

/******************************************************************************
CWaveDirInfo::RemoveFile - Searches through the list for the given file name
and removes it from the list; the file is NOT deleted

inputs
   PCWSTR         pszFile - File to remove
retursn
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::RemoveFile (PCWSTR pszFile)
{
   DWORD j;
   PCWaveFileInfo *ppwi;
   PCWaveFileInfo pwi;
   ppwi = (PCWaveFileInfo*)m_lPCWaveFileInfo.Get(0);
   for (j = 0; j < m_lPCWaveFileInfo.Num(); j++) {
      pwi = ppwi[j];
      if (!_wcsicmp(pwi->m_pszFile, pszFile)) {
         delete pwi;
         m_lPCWaveFileInfo.Remove (j);
         return TRUE;
      }
   } // j

   return FALSE; // cant find
}

/******************************************************************************
CWaveDirInfo::PlayFile - This does a sndplaysound() to play a file.

inputs
   PCWSTR            pszDir - Directory. Can be null
   PCWSTR            pszFile - File.
returns
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::PlayFile (PCWSTR pszDir, PCWSTR pszFile)
{
   char szFile[512];
   if (pszDir) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szFile, sizeof(szFile), 0 ,0);
      strcat (szFile, "\\");
   }
   else
      szFile[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);

   // play
   sndPlaySound (NULL, 0);
   sndPlaySound (szFile, SND_ASYNC | SND_NODEFAULT);
   
   return TRUE;
}

/******************************************************************************
CWaveDirInfo::ClearFiles - Clears all the files from the list
*/
void CWaveDirInfo::ClearFiles (void)
{
   // delete
   DWORD i;
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
   for (i = 0; i < m_lPCWaveFileInfo.Num(); i++)
      delete ppfi[i];
   m_lPCWaveFileInfo.Clear();
}

/******************************************************************************
CWaveDirInfo::EditFile - Pulls up the wave editor for the file.

inputs
   PCWSTR            pszDir - Directory. Can be null
   PCWSTR            pszFile - File.
   HWND              hWnd - To run from
returns
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::EditFile (PCWSTR pszDir, PCWSTR pszFile, HWND hWnd)
{
   char szFile[512];
   if (pszDir) {
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szFile, sizeof(szFile), 0 ,0);
      strcat (szFile, "\\");
   }
   else
      szFile[0] = 0;
   WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);

   // create the filename
   char szRun[1024];
   char szCurDir[256];
   DWORD dwLen;
   strcpy (szRun, "\"");
   dwLen = (DWORD) strlen(szRun);
   strcpy (szCurDir, gszAppDir);
   strcat (szRun, gszAppDir);
   strcat (szRun, "m3dwave.exe");
   strcat (szRun, "\"");
   if (szFile[0]) {
      strcat (szRun, " ");
      strcat (szRun, szFile);
   }

   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   memset (&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset (&pi, 0, sizeof(pi));
   if (!CreateProcess (NULL, szRun, NULL, NULL, NULL, NULL, NULL, szCurDir, &si, &pi))
      return FALSE;

   // Close process and thread handles. 
   CloseHandle( pi.hProcess );
   CloseHandle( pi.hThread );

   return TRUE;
}

/******************************************************************************
CWaveDirInfo::InventFileName - This invents a file name (and speaker) so that
another file in the sequence can be recorded. It manages this invention by
looking at the last file added and using it's directory and speaker as
inspiration. Failing that it uses the pszMasterFile for inspiration.

inputs
   PCWSTR         pszMasterFile - File that this is stored in, such as "c:\hello\test.mtf".
                     The directory might be used for where to store the fiels, and
                     the name (test.mtf) might be used for the speaker name
   PCWSTR         pszStartName - Starting point of name such as "SpeechRecog". A number
                     is appended to this for a new file.
   PWSTR          pszFile - Filled in with the file to use.
   DWORD          dwChars - Number of characters in szFIle
   PWSTR          pszSpeaker - Filled in with the recommended speakr
   DWORD          dwSpeakerChars - Number of characters in pszSpeaker
returns
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::InventFileName (PCWSTR pszMasterFile, PCWSTR pszStartName,
                                   PWSTR pszFile, DWORD dwChars, PWSTR pszSpeaker, DWORD dwSpeakerChars)
{
   pszFile[0] = 0;
   pszSpeaker[0] = 0;

   // look in masterfile and determine where the file name begins...
   PCWSTR pszSlash, pszCur;
   pszSlash = NULL;
   for (pszCur = pszMasterFile; pszCur[0]; pszCur++)
      if (pszCur[0] == L'\\')
         pszSlash = pszCur;

   // see if can get the last file
   PCWaveFileInfo *ppwi = (PCWaveFileInfo*)m_lPCWaveFileInfo.Get(0);
   PCWaveFileInfo pwi;
   DWORD dwNum = m_lPCWaveFileInfo.Num();
   if (dwNum) {
      // get into
      pwi = ppwi[dwNum-1];

      // find the slash...
      PWSTR pszS = NULL;
      for (pszCur = pwi->m_pszFile; pszCur[0]; pszCur++)
         if (pszCur[0] == L'\\')
            pszS = (PWSTR) pszCur;

      if (pszS) {
         DWORD dwLen = (DWORD)((PBYTE)pszS - (PBYTE)pwi->m_pszFile) / sizeof(WCHAR);
         if (dwLen+1 >= dwChars)
            return FALSE;
         memcpy (pszFile, pwi->m_pszFile, dwLen * sizeof(WCHAR));
         pszFile[dwLen] = 0;  // null terminate
      }
      
      // find the name
      if (pwi->m_pszSpeaker && pwi->m_pszSpeaker[0] && (wcslen(pwi->m_pszSpeaker) < dwSpeakerChars))
         wcscpy (pszSpeaker, pwi->m_pszSpeaker);
   } // if already file

   // if there isn't any file then make one up from source file
   if (!pszFile[0] && pszSlash) {
      DWORD dwLen = (DWORD)((PBYTE)pszSlash - (PBYTE)pszMasterFile) / sizeof(WCHAR);
      if (dwLen+1 >= dwChars)
         return FALSE;
      memcpy (pszFile, pszMasterFile, dwLen * sizeof(WCHAR));
      pszFile[dwLen] = 0;  // null terminate
   }

   // if there isn't any speaker then make one up from the source file
   if (!pszSpeaker[0] && pszSlash) {
      DWORD dwLen = (DWORD)wcslen(pszSlash+1);
      if (dwLen+1 >= dwSpeakerChars)
         return FALSE;
      wcscpy (pszSpeaker, pszSlash+1);

      // remove the .xyz
      PWSTR psz, pszDot;
      pszDot = 0;
      for (psz = pszSpeaker; psz[0]; psz++)
         if (psz[0] == L'.')
            pszDot = psz;
      if (pszDot)
         pszDot[0] = 0;
   }

   // come up with some names
   DWORD dwLen = (DWORD)wcslen(pszFile);
   if (dwLen + wcslen(pszStartName) + 15 >= dwChars)
      return FALSE;
   wcscat (pszFile, L"\\");
   wcscat (pszFile, pszStartName);
   dwLen = (DWORD)wcslen(pszFile);

   // loop through possible numbers
   DWORD i;
   char szTemp[512];
   FILE *f;
   for (i = m_lPCWaveFileInfo.Num(); ; i++) {
      swprintf (pszFile + dwLen, L"%05d.wav", (int) i+1);
      WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szTemp, sizeof(szTemp), 0, 0);
      OUTPUTDEBUGFILE (szTemp);
      f = fopen (szTemp, "rb");
      if (f) {
         fclose (f);
         continue;   // file exists
      }

      // else, doesn't exist
      return TRUE;
   }

   return TRUE;
}




/******************************************************************************
CWaveDirInfo::SyncWithDirectory - This enunmerates all the files in the directory
(m_szDir) and finds the wave files. This list of wave files is then compared against
the list in m_lPCWaveFileInfo. And files which no longer exist are removed, those
that have changed are reloaded, and new ones are added.

inputs
   PCProgressSocket     pProgress - To show the progress in case there are 1000s of files
   BOOL                 fFast - If TRUE, then dont bother opening the file and loading in details.
                           make it up instead
returns
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::SyncWithDirectory (PCProgressSocket pProgress, BOOL fFast)
{
   char szDir[256];
   WideCharToMultiByte (CP_ACP, 0, m_szDir, -1, szDir, sizeof(szDir), 0, 0);
   strcat (szDir, "\\*.wav");

   // make a list of wave files in the directory
   CListFixed lFound;
   lFound.Init (sizeof(PCWaveFileInfo));
   WIN32_FIND_DATA FindFileData;
   HANDLE hFind;

   hFind = FindFirstFile(szDir, &FindFileData);
   if (hFind != INVALID_HANDLE_VALUE) {
      while (TRUE) {
         PCWaveFileInfo pInfo = new CWaveFileInfo;
         if (!pInfo)
            break;
         if (!pInfo->FillFromFindFile (&FindFileData))
            delete pInfo;
         else
            lFound.Add (&pInfo);

         if (!FindNextFile (hFind, &FindFileData))
            break;
      }

      FindClose(hFind);
   }

   // sort the list
   PCWaveFileInfo *ppfFound = (PCWaveFileInfo*) lFound.Get(0);
   DWORD dwFound = lFound.Num();
   qsort (ppfFound, dwFound, sizeof(PCWaveFileInfo), PCWaveFileInfoSort);

   // existing list doest need to be sorted because already is
   PCWaveFileInfo *ppfList = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
   DWORD dwList = m_lPCWaveFileInfo.Num();


   // loop over them
   DWORD dwCurFound = 0, dwCurList = 0;
   WCHAR szTemp[256];
   while ((dwCurFound < dwFound) || (dwCurList < dwList)) {
      if (pProgress && (dwFound + dwList))
         pProgress->Update ((fp)(dwCurFound + dwCurList) / (fp)(dwFound+dwList));

      int iRet;

      if ((dwCurFound < dwFound) && (dwCurList >= dwList))
         iRet = -1;
      else if ((dwCurFound >= dwFound) && (dwCurList < dwList))
         iRet = 1;
      else
         iRet = _wcsicmp(ppfFound[dwCurFound]->m_pszFile, ppfList[dwCurList]->m_pszFile);

      // if they're the same the just make sure the file is up to date
      if (iRet == 0) {
         if (!memcmp(&ppfFound[dwCurFound]->m_FileTime, &ppfList[dwCurList]->m_FileTime, sizeof(ppfList[dwCurList]->m_FileTime)) &&
            (ppfFound[dwCurFound]->m_iFileSize == ppfList[dwCurList]->m_iFileSize)) {

            dwCurFound++;
            dwCurList++;
            continue;   // no change so dont chare
            }

         // else, changed
         wcscpy (szTemp, ppfList[dwCurList]->m_pszFile);
         if (!ppfList[dwCurList]->FillFromFile (m_szDir, szTemp, fFast)) {
            // cant see to open anymore
            delete ppfList[dwCurList];

            m_lPCWaveFileInfo.Remove (dwCurList);

            // reset...
            ppfList = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
            dwList = m_lPCWaveFileInfo.Num();
            continue;
         }

         // copy over some info
         ppfList[dwCurList]->m_FileTime = ppfFound[dwCurFound]->m_FileTime;
         ppfList[dwCurList]->m_iFileSize = ppfFound[dwCurFound]->m_iFileSize;

         dwCurList++;
         dwCurFound++;
         continue;
      } // if same name

      // if the current found is less than the current list then need to insert
      if (iRet < 0) {
         // load info
         wcscpy (szTemp, ppfFound[dwCurFound]->m_pszFile);
         if (!ppfFound[dwCurFound]->FillFromFile (m_szDir, szTemp, fFast)) {
            dwCurFound++;
            continue;   // error
         }

         // add to the current list
         m_lPCWaveFileInfo.Insert (dwCurList, &ppfFound[dwCurFound]);
         ppfFound[dwCurFound] = 0;
         dwCurList++;   // since don't want to look at the one just inserted

         // reset...
         ppfList = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
         dwList = m_lPCWaveFileInfo.Num();

         dwCurFound++;
         continue;
      } // less than

      // else, if get here then list exists, but not in found... therefore remove
      delete ppfList[dwCurList];
      m_lPCWaveFileInfo.Remove (dwCurList);

      // reset...
      ppfList = (PCWaveFileInfo*) m_lPCWaveFileInfo.Get(0);
      dwList = m_lPCWaveFileInfo.Num();
      continue;
   }

   // delete everything in the fFound list
   DWORD i;
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) lFound.Get(0);
   for (i = 0; i < lFound.Num(); i++)
      if (ppfi[i])
         delete ppfi[i];


   return TRUE;
}


/******************************************************************************
CWaveDirInfo::FillListBox - Fills a list box with the file names
in CWaveDirInfo. Each item has a name of "f:xxx" where xxx is the file name.
Directories can optionally be filled in using "d:xxx" for the relative directory

inputs
   PCEscPage         pPage - Page containing the list box
   PCWSTR            pszControl - Control name
   PCWSTR            pszTextFilter - Filter text out by this
   PCWSTR            pszSpeakerFilter - Speaker filter out by this
   PCWSTR            pszDir - Directory to use for enumeration. If NULL then dont fill directory into list box
returns
   BOOL - TRUE if success
*/
BOOL CWaveDirInfo::FillListBox (PCEscPage pPage, PCWSTR pszControl,
                                PCWSTR pszTextFilter, PCWSTR pszSpeakerFilter, PCWSTR pszDir)
{
   PCEscControl pControl = pPage->ControlFind ((PWSTR)pszControl);
   if (!pControl)
      return TRUE;
   pControl->Message (ESCM_LISTBOXRESETCONTENT);

   // sort the list of files alphabetically
   CListFixed lSort;
   lSort.Init (sizeof(PCWaveFileInfo), this ? m_lPCWaveFileInfo.Get(0) : NULL,
      this ? m_lPCWaveFileInfo.Num() : 0);
   qsort (lSort.Get(0), lSort.Num(), sizeof(PCWaveFileInfo), PCWaveFileInfoSort);

   // add the names
   MemZero (&gMemTemp);

   DWORD i;
   DWORD dwNum = lSort.Num();
   PCWaveFileInfo *ppfi = (PCWaveFileInfo*) lSort.Get(0);
   WCHAR szTemp[256];
   for (i = 0; i < dwNum; i++) {
      PCWaveFileInfo pfi = ppfi[i];

      // filter by text/speaker
      if (pszTextFilter && pszTextFilter[0]) {
         if (!pfi->m_pszText)
            continue;

         if (!MyStrIStr(pfi->m_pszText, pszTextFilter))
            continue;
      }

      // filter by speaker
      if (pszSpeakerFilter && pszSpeakerFilter[0]) {
         if (!pfi->m_pszSpeaker)
            continue;

         if (!MyStrIStr(pfi->m_pszSpeaker, pszSpeakerFilter))
            continue;
      }

      MemCat (&gMemTemp, L"<elem name=\"f:");
      MemCatSanitize (&gMemTemp, pfi->m_pszFile);
      MemCat (&gMemTemp, L"\"><small><table border=0 innerlines=0 tbmargin=2 width=100%%><tr><td width=40%%><bold>");
      MemCatSanitize (&gMemTemp, pfi->m_pszFile);
      MemCat (&gMemTemp, L"</bold></td><td width=20%%>");
      MemCat (&gMemTemp, (int)pfi->m_fDuration);
      MemCat (&gMemTemp, L" sec</td><td width=20%%>");
      MemCat (&gMemTemp, (int)pfi->m_dwSamplesPerSec / 1000);
      MemCat (&gMemTemp, L" kHz, ");
      if (pfi->m_dwChannels == 1)
         MemCat (&gMemTemp, L"mono");
      else if (pfi->m_dwChannels == 2)
         MemCat (&gMemTemp, L"stereo");
      else {
         MemCat (&gMemTemp, (int)pfi->m_dwChannels);
         MemCat (&gMemTemp, L" channels");
      }
      MemCat (&gMemTemp, L"</td><td width=20%%>");
      swprintf (szTemp, L"%.2g", (double)(int)(pfi->m_iFileSize / 1000 / 10) / 100.0);
      MemCat (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L" MB</td></tr>");
      if ((pfi->m_pszText && pfi->m_pszText[0]) || (pfi->m_pszSpeaker && pfi->m_pszSpeaker[0])) {
         MemCat (&gMemTemp, L"<tr><td width=10%%/><td width=90%%><bold><font color=#008000>");
         if (pfi->m_pszText && pfi->m_pszText[0]) {
            MemCatSanitize (&gMemTemp, pfi->m_pszText);

#if 0 // def _DEBUG // to get sentences for speaking
            OutputDebugStringW (L"\r\n");
            OutputDebugStringW (pfi->m_pszText);
#endif
         }
         MemCat (&gMemTemp, L"</font></bold> ");
         if (pfi->m_pszSpeaker && pfi->m_pszSpeaker[0]) {
            MemCat (&gMemTemp, L"(");
            MemCatSanitize (&gMemTemp, pfi->m_pszSpeaker);
            MemCat (&gMemTemp, L")");
         }
         MemCat (&gMemTemp, L"</td></tr>");
      }
      MemCat (&gMemTemp, L"</table></small><br/></elem>");
   } // i

   if (pszDir) {
      // will need directory names
      WIN32_FIND_DATA ffd;
      HANDLE hFind;
      char szSearch[256];
      WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szSearch, sizeof(szSearch), 0 ,0);
      strcat (szSearch, "\\*.*");

      hFind = FindFirstFile(szSearch, &ffd);
      if (hFind != INVALID_HANDLE_VALUE) {
         while (TRUE) {
            // only want directories
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
               goto next;

            // get rid of . and ..
            if (!strcmp(ffd.cFileName, ".") || !strcmp(ffd.cFileName, ".."))
               goto next;

            // convert name to unicode
            MultiByteToWideChar (CP_ACP, 0, ffd.cFileName, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));

            // add
            MemCat (&gMemTemp, L"<elem name=\"d:");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\">");
            MemCat (&gMemTemp, L"<image transparent=true bmpresource=");
            MemCat (&gMemTemp, (int)IDB_FOLDER);
            MemCat (&gMemTemp, L"/><bold><small>");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</small></bold></elem>");

   next:
            if (!FindNextFile (hFind, &ffd))
               break;
         }

         FindClose(hFind);
      }
   } // fDir


   // add this to the list box
   ESCMLISTBOXADD lba;
   memset (&lba, 0, sizeof(lba));
   lba.dwInsertBefore = -1;
   lba.pszMML = (PWSTR) gMemTemp.p;
   pControl->Message (ESCM_LISTBOXADD, &lba);

   // set the selection
   pControl->AttribSetInt (CurSel(), 0);

   return TRUE;
}




/******************************************************************************
CWaveDirCache::Constructor and destructor
*/
CWaveDirCache::CWaveDirCache (void)
{
   m_lPCWaveDirInfo.Init (sizeof(PCWaveDirInfo));
}

CWaveDirCache::~CWaveDirCache (void)
{
   // delete
   DWORD i;
   PCWaveDirInfo *ppwi = (PCWaveDirInfo*) m_lPCWaveDirInfo.Get(0);
   for (i = 0; i < m_lPCWaveDirInfo.Num(); i++)
      delete ppwi[i];
   m_lPCWaveDirInfo.Clear();
}

/******************************************************************************
CWaveDirCache::DirGet - Returns a pointer to the PCWaveDirInfo for the given
directory. It synchronizes in the process. If it can't be found in the internal
list then it's created. If it ends up not having any files then NULL is returned

inputs
   PWSTR          pszDir - Directory, like "c:\hello\bye"... not final \
   PCProgressSocket pProgress - Show progress in loading
*/
PCWaveDirInfo CWaveDirCache::DirGet (PWSTR pszDir, PCProgressSocket pProgress)
{
   // see if can find it
   DWORD i;
   PCWaveDirInfo *ppwi = (PCWaveDirInfo*) m_lPCWaveDirInfo.Get(0);
   for (i = 0; i < m_lPCWaveDirInfo.Num(); i++)
      if (!_wcsicmp(ppwi[i]->m_szDir, pszDir))
         break;
   
   // pointer
   PCWaveDirInfo pwi;
   if (i < m_lPCWaveDirInfo.Num())
      pwi = ppwi[i];
   else {
      pwi = new CWaveDirInfo;
      if (!pwi)
         return NULL;
      wcscpy (pwi->m_szDir, pszDir);
   }

   // sync
   pwi->SyncWithDirectory (pProgress);

   // if empty then delete and return
   if (!pwi->m_lPCWaveFileInfo.Num()) {
      delete pwi;
      if (i < m_lPCWaveDirInfo.Num())
         m_lPCWaveDirInfo.Remove (i);
      return NULL;
   }

   // may need to add
   if (i >= m_lPCWaveDirInfo.Num()) {
      // addd....

      // dlete and old one
      while (m_lPCWaveDirInfo.Num() > 100) {
         DWORD dwRand = (DWORD)rand() % m_lPCWaveDirInfo.Num();
         delete ppwi[dwRand];
         m_lPCWaveDirInfo.Remove (dwRand);
         ppwi = (PCWaveDirInfo*) m_lPCWaveDirInfo.Get(0);
      } // delete if too many cached

      // add it
      m_lPCWaveDirInfo.Add (&pwi);
   }

   // done
   return pwi;
}

/******************************************************************************
CWaveDirCache::MMLTo - Standard
*/
static PWSTR gpszWaveDirCache = L"WaveDirCache";

PCMMLNode2 CWaveDirCache::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszWaveDirCache);

   // write out entries
   DWORD i;
   PCWaveDirInfo *ppwi = (PCWaveDirInfo*) m_lPCWaveDirInfo.Get(0);
   PCMMLNode2 pSub;
   for (i = 0; i < m_lPCWaveDirInfo.Num(); i++) {
      pSub = ppwi[i]->MMLTo();
      if (!pSub)
         continue;
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CWaveDirCache::MMLFrom -Stanrdar
*/
BOOL CWaveDirCache::MMLFrom (PCMMLNode2 pNode)
{
   // clear the cache...
   DWORD i;
   PCWaveDirInfo *ppwi = (PCWaveDirInfo*) m_lPCWaveDirInfo.Get(0);
   for (i = 0; i < m_lPCWaveDirInfo.Num(); i++)
      delete ppwi[i];
   m_lPCWaveDirInfo.Clear();

   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszWaveDirInfo)) {
         PCWaveDirInfo pwi = new CWaveDirInfo;
         if (!pwi)
            return FALSE;
         if (!pwi->MMLFrom (pSub))
            return FALSE;
         m_lPCWaveDirInfo.Add (&pwi);
      }
   } // i

   return TRUE;

}


/******************************************************************************
CWaveDirCache::Open - reads the wave dir cache from disk.

returns
   BOOL - TRUE if succeess
*/
BOOL CWaveDirCache::Open (void)
{
   // make the name
   char szFile[256];
   WCHAR szw[256];
   strcpy (szFile, gszAppDir);
   strcat (szFile, "WaveDirCache.mml");

   MultiByteToWideChar (CP_ACP, 0, szFile, -1, szw, sizeof(szw)/sizeof(WCHAR));
   PCMMLNode2 pNode = MMLFileOpen (szw, &GUID_WaveCache);
   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode)) {
      delete pNode;
      return FALSE;
   }

   delete pNode;
   return TRUE;
}

/******************************************************************************
CWaveDirCache::Save - Writes the cache of wave file info out
*/
BOOL CWaveDirCache::Save (void)
{
   // make the name
   char szFile[256];
   WCHAR szw[256];
   strcpy (szFile, gszAppDir);
   strcat (szFile, "WaveDirCache.mml");
   MultiByteToWideChar (CP_ACP, 0, szFile, -1, szw, sizeof(szw)/sizeof(WCHAR));

   PCMMLNode2 pNode = MMLTo();
   if (!pNode)
      return FALSE;
   BOOL fRet;
   fRet = MMLFileSave (szw, &GUID_WaveCache, pNode);
   delete pNode;
   return fRet;
}


/*********************************************************************************
CleanDirName - This takes a directory name and a) verifies its valid. If it
isn't then a valid name is put in place. and b) removes any "\" at the end

inputs
   PWSTR       pszDir - Directory to modify
*/
static void CleanDirName (PWSTR pszDir)
{
   DWORD dwLen = (DWORD)wcslen(pszDir);
   if (dwLen && (pszDir[dwLen-1] == '\\'))
      pszDir[dwLen-1] = 0;

   // convert to ANSI
   char szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szTemp, sizeof(szTemp), 0,0);
   if (!PathIsDirectory (szTemp))
      wcscpy (pszDir, L"c:");
}


/*********************************************************************************
MyStrIStr - Look for a string in another one, ignoring case.

inputs
   PWSTR          pszLookIn - look in this
   PWSTR          pszLookFor - look for this
returns
   PWSTR - Pointer to first occurance, or NULL if cant find
*/
PCWSTR MyStrIStr (PCWSTR pszLookIn, PCWSTR pszLookFor)
{
   DWORD dwLen = (DWORD)wcslen(pszLookFor);
   if (!dwLen)
      return NULL;

   DWORD i;
   WCHAR wcLower = towlower (pszLookFor[0]);
   for (i = 0; pszLookIn[i]; i++) {
      if (towlower(pszLookIn[i]) != wcLower)
         continue;

      if (!_wcsnicmp(pszLookIn + i, pszLookFor, dwLen))
         return pszLookIn + i;
   }

   return NULL;
}


/*********************************************************************************
WaveOpenPage */

static BOOL WaveOpenPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PWOD pw = (PWOD) pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl = pPage->ControlFind (L"edit");

         // fill the list box
         pPage->Message (ESCM_USER+82);

         // BUGFIX - Move after ESCM_USER+83
         if (pControl)
            pControl->AttribSet (Text(), pw->szFile);
      }
      break;

   case ESCM_USER+82:   // fill the listbox
      {
         CleanDirName (pw->szDir);

         // get the directory
         PCWaveDirInfo pwd;
         {
            CProgress Progress;
            Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning files...");
            pwd = pw->pCache->DirGet(pw->szDir, &Progress);
         }

         pwd->FillListBox (pPage, L"list", pw->szTextFilter, pw->szSpeakerFilter, pw->szDir);


         // create a list of absolute paths...
         CListVariable lPath;
         char szaTemp[256];
         WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szaTemp, sizeof(szaTemp), 0, 0);
         char *pc, *pLast;
         pc = szaTemp + strlen(szaTemp);
         pc[0] = '\\';
         pc[1] = 0;
         lPath.Add (szaTemp, strlen(szaTemp)+1);
         pc[0] = 0;

         while (TRUE) {
            // find th elast backslash and cut off
            pLast = NULL;
            for (pc = szaTemp; *pc; pc++)
               if (pc[0] == '\\')
                  pLast = pc;
            if (!pLast)
               break;

            // wipe out
            pLast[1] = 0;

            // make sure valid directory
            if (!szaTemp[0] || !PathIsDirectory (szaTemp))
               break;

            // if its a root dont add
            if (PathIsRoot(szaTemp) && !PathIsUNC(szaTemp))
               break;

            // else, add
            lPath.Add (szaTemp, strlen(szaTemp)+1);

            pLast[0] = 0;
         }
         // loop through all the the possible roots and add these
         DWORD i;
         for (i = 0; i < 26; i++) {
            szaTemp[0] = 0;
            PathBuildRoot (szaTemp, i);
            if (!szaTemp[0])
               continue;
            if (GetDriveType (szaTemp) == DRIVE_NO_ROOT_DIR)
               continue;

            // if it's the same as the first entry then skip
            if (lPath.Num() && !_stricmp(szaTemp, (char*)lPath.Get(0)))
               continue;

            lPath.Add (szaTemp, strlen(szaTemp)+1);
         }

         // add these
         MemZero (&gMemTemp);
         WCHAR szTemp[256];
         for (i = 0; i < lPath.Num(); i++) {
            char *psz = (char*)lPath.Get(i);
            MultiByteToWideChar (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp)/sizeof(WCHAR));

            MemCat (&gMemTemp, L"<elem name=\"a:");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"\"><bold>");
            MemCatSanitize (&gMemTemp, szTemp);
            MemCat (&gMemTemp, L"</bold></elem>");
         }


         // add this to the combo box
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"dir");
         if (!pControl)
            return TRUE;

         ESCMCOMBOBOXADD cba;
         memset (&cba, 0, sizeof(cba));
         cba.dwInsertBefore = 0; // to add
         cba.pszMML = (PWSTR) gMemTemp.p;
         pControl->Message (ESCM_COMBOBOXRESETCONTENT);
         pControl->Message (ESCM_COMBOBOXADD, &cba);

         // set the selection
         pControl->AttribSetInt (CurSel(), 0);

         // clear edit
         pControl = pPage->ControlFind (L"edit");
         if (pControl)
            pControl->AttribSet (Text(), L"");
      }
      return TRUE;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         // only care about list
         if (!psz || _wcsicmp(psz, L"dir"))
            break;
         if (!p->pszName)
            return TRUE;

         // if item 0 ignore since just set that
         if (p->dwCurSel == 0)
            break;

         if ((p->pszName[0] == L'a') && (p->pszName[1] == L':')) {
            wcscpy (pw->szDir, p->pszName+2);

            // fill the list box
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;

   case ESCN_LISTBOXSELCHANGE:
      {
         PESCNLISTBOXSELCHANGE p = (PESCNLISTBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;

         // only care about list
         if (!psz || _wcsicmp(psz, L"list"))
            break;
         if (!p->pszName)
            return TRUE;

         // if click on file the play that file
         if ((p->pszName[0] == L'f') && (p->pszName[1] == L':')) {
            char szFile[512];
            WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szFile, sizeof(szFile), 0 ,0);
            strcat (szFile, "\\");
            WideCharToMultiByte (CP_ACP, 0, p->pszName + 2, -1, szFile + strlen(szFile), sizeof(szFile) - (DWORD)strlen(szFile), 0 ,0);

            // play
            sndPlaySound (NULL, 0);
            sndPlaySound (szFile, SND_ASYNC | SND_NODEFAULT);
            
            // set the text
            PCEscControl pControl = pPage->ControlFind (L"edit");
            if (pControl)
               pControl->AttribSet (Text(), p->pszName+2);
            return TRUE;
         }
         if ((p->pszName[0] == L'd') && (p->pszName[1] == L':')) {
            DWORD dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(p->pszName+2) + 2;
            if (dwLen >= sizeof(pw->szDir)/sizeof(WCHAR))
               return FALSE;

            // else, new dir
            wcscat (pw->szDir, L"\\");
            wcscat (pw->szDir, p->pszName+2);

            // fill the list box
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      return TRUE;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"textfilter")) {
            DWORD dwNeed;
            pw->szTextFilter[0] = 0;
            p->pControl->AttribGet (Text(), pw->szTextFilter, sizeof(pw->szTextFilter), &dwNeed);
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         if (!_wcsicmp(psz, L"speakerfilter")) {
            DWORD dwNeed;
            pw->szSpeakerFilter[0] = 0;
            p->pControl->AttribGet (Text(), pw->szSpeakerFilter, sizeof(pw->szSpeakerFilter), &dwNeed);
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp(psz, L"multisel")) {
            PCEscControl pControl = pPage->ControlFind (L"list");
            if (!pControl)
               return TRUE;
            pw->plMultiSel->Clear();

            // number of items
            ESCMLISTBOXGETCOUNT gc;
            memset (&gc, 0, sizeof(gc));
            pControl->Message (ESCM_LISTBOXGETCOUNT, &gc);

            DWORD i;
            ESCMLISTBOXGETITEM gi;
            for (i = 0; i < gc.dwNum; i++) {
               memset (&gi, 0, sizeof(gi));
               gi.dwIndex = i;
               if (!pControl->Message (ESCM_LISTBOXGETITEM, &gi))
                  break;

               if (!gi.pszName)
                  continue;

               if ((gi.pszName[0] == L'f') && (gi.pszName[1] == L':')) {
                  WCHAR szFile[256];
                  DWORD dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(gi.pszName) + 2;
                  if (dwLen * sizeof(WCHAR) > sizeof(szFile))
                     continue;   // too large

                  wcscpy (szFile, pw->szDir);
                  wcscat (szFile, L"\\");
                  wcscat (szFile, gi.pszName + 2);

                  pw->plMultiSel->Add (szFile, (wcslen(szFile)+1)*sizeof(WCHAR));
               }
            } // i

            if (!pw->plMultiSel->Num()) {
               pPage->MBWarning (L"No wave files are visible.");
               return TRUE;
            }

            pPage->Exit (L"ok");
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"ok")) {
            // stop playing
            sndPlaySound (NULL, 0);

            // get the text
            WCHAR szText[256];
            DWORD dwNeed;
            PCEscControl pControl = pPage->ControlFind (L"edit");
            if (!pControl)
               return FALSE;
            szText[0] = 0;
            pControl->AttribGet (Text(), szText, sizeof(szText), &dwNeed);

            // see if typed in a directory...
            char szTemp[512];
            WideCharToMultiByte (CP_ACP, 0, szText, -1, szTemp, sizeof(szTemp), 0,0);
            if (PathIsDirectory (szTemp)) {
               wcscpy (pw->szDir, szText);
               pPage->Message (ESCM_USER+82);
               return TRUE;
            }


            // see if it's an extension onto existing directory
            WideCharToMultiByte (CP_ACP, 0, pw->szDir, -1, szTemp, sizeof(szTemp), 0,0);
            strcat (szTemp, "\\");
            WideCharToMultiByte (CP_ACP, 0, szText, -1, szTemp + strlen(szTemp), sizeof(szTemp) - (DWORD)strlen(szTemp), 0,0);
            DWORD dwLen;
            if (PathIsDirectory (szTemp)) {
               dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(szText) + 2;
               if (dwLen * sizeof(WCHAR) < sizeof(pw->szDir)) {
                  wcscat (pw->szDir, L"\\");
                  wcscat (pw->szDir, szText);
               }
               pPage->Message (ESCM_USER+82);
               return TRUE;
            }

            // try appending .wav to it...
            dwLen = (DWORD)wcslen(szText);
            if ((dwLen < 4) || _wcsicmp(szText+(dwLen-4), L".wav"))
               wcscat (szText, L".wav");
            dwLen = (DWORD)wcslen(pw->szDir) + (DWORD)wcslen(szText) + 2;
            if (dwLen * sizeof(WCHAR) >= sizeof(pw->szFile)) {
               pPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
               return TRUE;
            }
            wcscpy (pw->szFile, pw->szDir);
            wcscat (pw->szFile, L"\\");
            wcscat (pw->szFile, szText);

            WideCharToMultiByte (CP_ACP, 0, pw->szFile, -1, szTemp, sizeof(szTemp), 0,0);

            // try opening this
            CM3DWave Wave;
            if (Wave.Open (NULL, szTemp, FALSE)) {
               if (pw->fSave) {
                  int iRet;
                  iRet = pPage->MBYesNo (L"Do you want to overwrite the existing file?",
                     L"The file already exists.");
                  if (iRet != IDYES)
                     return TRUE;
               }

               if (pw->plMultiSel) {
                  pw->plMultiSel->Clear();
                  pw->plMultiSel->Add (pw->szFile, (wcslen(pw->szFile)+1) * sizeof(WCHAR));
               }

               pPage->Exit (L"ok");
               return TRUE;
            }

            // if saving then accept the name
            if (pw->fSave) {
               // see if can save something
               OUTPUTDEBUGFILE (szTemp);
               FILE *f = fopen(szTemp, "wb");
               if (!f) {
                  pPage->MBWarning (L"The file couldn't be saved.",
                     L"You may not have typed in a correct name for the file.");
                  return TRUE;
               }
               else {
                  fclose (f);
                  DeleteFile (szTemp);
               }
               pPage->Exit (L"ok");
               return TRUE;
            }

            // else dont know
            pPage->m_pWindow->Beep (ESCBEEP_DONTCLICK);
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Wave open";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OPENSAVE")) {
            p->pszSubString = pw->fSave ? L"<font color=#ff8080>Save</font>" : L"Open";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFMULTISEL")) {
            p->pszSubString = pw->plMultiSel ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFMULTISEL")) {
            p->pszSubString = pw->plMultiSel ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;

   }

   return FALSE;
   // BUGFIX - Dont use DefPage (pPage, dwMessage, pParam); because trps enter
}



/*********************************************************************************
WaveFileOpen - This opens a wave file.

inputs
   HWND              hWnd - Window to display from
   BOOL              fSave - If TRUE then saving a file, else opening.
   PWSTR             pszFile - Filled with the file name if the user selects open
                     Must be 256 chars long. Iniitally, this is filled in with a
                     file name. If saving then the directory and text will come from
                     the original file name
   PCListVariable    plMultiSel - Normally this is NULL, however if it isn't,
                     and opening files, then it will be filled with a list of files to
                     be opened.
returns
   BOOL - TRUE if open, FALSE if not
*/
BOOL WaveFileOpen (HWND hWnd, BOOL fSave, PWSTR pszFile, PCListVariable plMultiSel)
{
   if (plMultiSel)
      plMultiSel->Clear();

   // load in the directory cache
   WOD wod;
   memset (&wod, 0, sizeof(wod));
   CWaveDirCache DirCache;
   DirCache.Open();
   wod.pCache = &DirCache;
   wod.fSave = fSave;
   if (!fSave)
      wod.plMultiSel = plMultiSel;

   // look in the directory and get the current working dir...
   char szTemp[256];
   GetLastDirectory (szTemp, sizeof(szTemp));
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, wod.szDir, sizeof(wod.szDir)/sizeof(WCHAR));

   // if saving or opening then get info from name
   if (pszFile[0]) {
      // find the last slash...
      WCHAR *pc, *pcLast;
      pcLast = pszFile;
      for (pc = pszFile; *pc; pc++)
         if (pc[0] == L'\\')
            pcLast = pc;
      wcscpy (wod.szFile, pcLast+1);

      if (pcLast != pszFile) {
         pcLast[0] = 0;
         wcscpy (wod.szDir, pszFile);
      }
   }

   // display the window
   CEscWindow cWindow;
   RECT r;
   PWSTR pszRet;
   BOOL fOpen = FALSE;
   DialogBoxLocation2 (hWnd, &r);
   cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);
   pszRet = cWindow.PageDialog (ghInstance, IDR_MMLWAVEOPEN, WaveOpenPage, &wod);
   if (pszRet && !_wcsicmp(pszRet, L"ok")) {
      fOpen = TRUE;
      wcscpy (pszFile, wod.szFile);
   }

   // set directory
   WideCharToMultiByte (CP_ACP, 0, wod.szDir, -1, szTemp, sizeof(szTemp), 0, 0);
   SetLastDirectory (szTemp);

   // save cahce
   DirCache.Save();

   return fOpen;
}



