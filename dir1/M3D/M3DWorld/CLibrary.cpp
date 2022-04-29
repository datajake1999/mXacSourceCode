/********************************************************************************
CLibrary.cpp - Library storage object

begun 2/6/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <math.h>
#include "resource.h"
#include "escarpment.h"
#include "..\M3D.h"


CLibrary gaLibraryInstalled[MAXRENDERSHARDS];    // installed library
CLibrary gaLibraryUser[MAXRENDERSHARDS];         // user library
PCLibraryCategory gapLCTexturesInstalled[MAXRENDERSHARDS];  // custom textures
PCLibraryCategory gapLCTexturesUser[MAXRENDERSHARDS];
PCLibraryCategory gapLCObjectsInstalled[MAXRENDERSHARDS];   // custom objects
PCLibraryCategory gapLCObjectsUser[MAXRENDERSHARDS];
PCLibraryCategory gapLCEffectsInstalled[MAXRENDERSHARDS];  // custom textures
PCLibraryCategory gapLCEffectsUser[MAXRENDERSHARDS];


static PWSTR gpszTextures = L"Textures";
static PWSTR gpszObjects = L"Objects";
static PWSTR gpszEffects = L"Effects";

typedef struct {
   WCHAR       szCategory[128];     // category name
   PCLibraryCategory pCategory;     // category pointer
} CATINFO, *PCATINFO;

//typedef struct {
//   GUID        gCode;               // main GUID
//   GUID        gSub;                // sub GUID
//   PCMMLNode2   pNode;               // node containing all the contents of the category
//} CATITEM, *PCATITEM;

// CCatItem - Class for storing category item
#define TOOLARGEFORTHISFILE            1024     // if size of object more than this then too large to store in this file

class CCatItem {
   friend class CLibraryCategory;

public:
   ESCNEWDELETE;

   CCatItem (PCMegaFile pmf, BOOL *pfDirty);
   ~CCatItem (void);

   PCMMLNode2 MMLTo (PWSTR pszName, PWSTR pszMajor, PWSTR pszMinor);
   BOOL MMLFrom (PCMMLNode2 pNode);
   CCatItem * Clone (PCMegaFile pmfNew, BOOL *pfDirty);

   PCMMLNode2 NodeGet (void);    // gets this node
   BOOL NodeSet (PCMMLNode2 pNode, BOOL fKeepNode); // sets the node
   BOOL Save (void);       // causes this to be saved to the megafile, if the memory is large enough
   BOOL Delete (void);

   GUID              m_gCode;       // main GUID
   GUID              m_gSub;        // sub GUID
   CMem              m_memName;     // name string
   CMem              m_memMajor;    // major string
   CMem              m_memMinor;    // minor string
   BOOL              m_fDirty;      // set to TRUE when dirty

private:
   BOOL Decompress (void); // decompressed from m_memNode to m_pNode
   BOOL Compress (void);   // compressed from m_pNode to m_memNode
   PWSTR NameString (PWSTR pszTemp); // returns the name sting

   BOOL              m_fResidesOnDisk; // if TRUE the node is large enough it resides on disk
                                    // in the megafile
   CMem              m_memNode;     // memory containing the compressed version of m_pNode
                                    // m_dwCurPosn is the size, or 0 if not filled in
   PCMMLNode2        m_pNode;       // node containing all the contents of the category
                                    // this may be NULL if no node is decompressed
   PCMegaFile        m_pmf;         // megafile that can write info out to
   BOOL              *m_pfDirty;    // dirty flag to set if changes
};
typedef CCatItem *PCCatItem;


/**************************************************************************************
CCatItem::Constructor and destructor.
*/
CCatItem::CCatItem (PCMegaFile pmf, BOOL *pfDirty)
{
   m_gCode = m_gSub = GUID_NULL;
   m_fDirty = FALSE;
   m_fResidesOnDisk = FALSE;
   m_pNode = NULL;
   m_pmf = pmf;
   m_pfDirty = pfDirty;
}

CCatItem::~CCatItem (void)
{
   if (m_pNode)
      delete m_pNode;
}



/**************************************************************************************
CCatItem::Clone - Clones the catitem

inputs
   PCMegaFile     pmfNew - Megatfile of new catitem
   BOOL           *pfDirty - To be filled in when cat item is dirty
returns
   PCCatItem - New cat item
*/
PCCatItem CCatItem::Clone (PCMegaFile pmfNew, BOOL *pfDirty)
{
   // copy this
   PCCatItem pNew = new CCatItem (pmfNew, pfDirty);
   if (!pNew)
      return NULL;
   pNew->m_fDirty = m_fDirty;
   pNew->m_fResidesOnDisk = m_fResidesOnDisk;
   pNew->m_gCode = m_gCode;
   pNew->m_gSub = m_gSub;
   MemZero (&pNew->m_memMajor);
   MemCat (&pNew->m_memMajor, (PWSTR)m_memMajor.p);
   MemZero (&pNew->m_memMinor);
   MemCat (&pNew->m_memMinor, (PWSTR)m_memMinor.p);
   MemZero (&pNew->m_memName);
   MemCat (&pNew->m_memName, (PWSTR)m_memName.p);
   if (m_memNode.m_dwCurPosn) {
      if (!pNew->m_memNode.Required (m_memNode.m_dwCurPosn)) {
         delete pNew;
         return NULL;
      };
      pNew->m_memNode.m_dwCurPosn = m_memNode.m_dwCurPosn;
      memcpy (pNew->m_memNode.p, m_memNode.p, m_memNode.m_dwCurPosn);
   }
   if (m_pNode) {
      pNew->m_pNode = m_pNode->Clone();
      if (!pNew->m_pNode) {
         delete pNew;
         return NULL;
      }
   }
   
   // copy if resides on disk
   if (m_fResidesOnDisk) {
      __int64 iSize;
      WCHAR szTemp[96];
      NameString (szTemp);

      // NOTE: Assuming m_pmf, so this only happens if megafile NOT remapped
      MFFILEINFO mffi;
      m_pmf->Exists (szTemp, &mffi);
      PVOID pData = m_pmf->Load (szTemp, &iSize, FALSE);

      if (pData) {
         pNew->m_pmf->Save (szTemp, pData, iSize, &mffi.iTimeCreate, &mffi.iTimeModify, &mffi.iTimeAccess);
         MegaFileFree (pData);
      }
   } // if resides on disk


   return pNew;
}

/**************************************************************************************
MMLCompressed - Returns the string "MMLCompressed", which indicates that some compressed
binary is saved in the node.
*/
DLLEXPORT PWSTR MMLCompressed (void)
{
   return L"MMLCompressed";
}

static PWSTR gpszCatItem = L"Item";
static PWSTR gpszResidesOnDisk = L"ResidesOnDisk";

/**************************************************************************************
CCatItem::MMLTo - Saves the node to MML.

inputs
   PWSTR          pszName - Name to use. If NULL then use m_memName
   PWSTR          pszMajor - Name to use. If NULL then use m_memMajor
   PWSTR          pszMinor - Name to use. If NULL then use m_memMinor
returns
   PCMMLNode - New node, or NULL if error
*/
PCMMLNode2 CCatItem::MMLTo (PWSTR pszName, PWSTR pszMajor, PWSTR pszMinor)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszCatItem);

   PWSTR psz;
   psz = pszMajor ? pszMajor : (PWSTR)m_memMajor.p;
   if (psz && psz[0])
      MMLValueSet (pNode, CatMajor(), psz);

   psz = pszMinor ? pszMinor : (PWSTR)m_memMinor.p;
   if (psz && psz[0])
      MMLValueSet (pNode, CatMinor(), psz);

   psz = pszName ? pszName : (PWSTR)m_memName.p;
   if (psz && psz[0])
      MMLValueSet (pNode, CatName(), psz);

   MMLValueSet (pNode, CatGUIDCode(), (PBYTE) &m_gCode, sizeof(m_gCode));
   MMLValueSet (pNode, CatGUIDSub(), (PBYTE) &m_gSub, sizeof(m_gSub));

   // contents
   if (m_fResidesOnDisk)
      MMLValueSet (pNode, gpszResidesOnDisk, (int)TRUE);
   else {
      if (!m_memNode.m_dwCurPosn) {
         if (!Compress()) {
            delete pNode;
            return NULL;
         }
      }
      MMLValueSet (pNode, MMLCompressed(), (PBYTE)m_memNode.p, m_memNode.m_dwCurPosn);
   }

   // done
   return pNode;
}


/**************************************************************************************
CCatItem::MMLFrom - Reads info from mml
*/
BOOL CCatItem::MMLFrom (PCMMLNode2 pNode)
{
   m_memNode.m_dwCurPosn = 0; // just to clear
   m_fDirty = FALSE;
   if (m_pNode) {
      delete m_pNode;
      m_pNode = NULL;
   }

   PWSTR psz;
   psz = MMLValueGet (pNode, CatMajor());
   if (!psz)
      return FALSE;
   MemZero (&m_memMajor);
   MemCat (&m_memMajor, psz);

   psz = MMLValueGet (pNode, CatMinor());
   if (!psz)
      return FALSE;
   MemZero (&m_memMinor);
   MemCat (&m_memMinor, psz);

   psz = MMLValueGet (pNode, CatName());
   if (!psz)
      return FALSE;
   MemZero (&m_memName);
   MemCat (&m_memName, psz);

   if (sizeof(m_gCode) != MMLValueGetBinary (pNode, CatGUIDCode(), (PBYTE) &m_gCode, sizeof(m_gCode)))
      return FALSE;
   if (sizeof(m_gSub) != MMLValueGetBinary (pNode, CatGUIDSub(), (PBYTE) &m_gSub, sizeof(m_gSub)))
      return FALSE;

   // get the small binary
   MMLValueGetBinary (pNode, MMLCompressed(), &m_memNode);
   m_fResidesOnDisk = FALSE;

   if (!m_memNode.m_dwCurPosn) {
      // see if stored on disk
      m_fResidesOnDisk = (BOOL) MMLValueGetInt (pNode, gpszResidesOnDisk, (int)FALSE);

      // if didn't get a reside on disk then this is an old format, so set up for
      // new format
      if (!m_fResidesOnDisk) {
         // old format
         m_pNode = pNode->Clone();
         if (!m_pNode)
            return FALSE;
         m_fDirty = *m_pfDirty = TRUE;  // so will save

         // remove some old settings
         DWORD dwIndex;
         dwIndex = m_pNode->ContentFind (CatMajor());
         if (dwIndex != -1)
            m_pNode->ContentRemove (dwIndex);
         dwIndex = m_pNode->ContentFind (CatMinor());
         if (dwIndex != -1)
            m_pNode->ContentRemove (dwIndex);
         dwIndex = m_pNode->ContentFind (CatName());
         if (dwIndex != -1)
            m_pNode->ContentRemove (dwIndex);
         dwIndex = m_pNode->ContentFind (CatGUIDCode());
         if (dwIndex != -1)
            m_pNode->ContentRemove (dwIndex);
         dwIndex = m_pNode->ContentFind (CatGUIDSub());
         if (dwIndex != -1)
            m_pNode->ContentRemove (dwIndex);

         // compress this down
         if (!Compress())
            return FALSE;

         // delete the m_pNode
         if (m_pNode)
            delete m_pNode;
         m_pNode = NULL;
      } // if not residing on disk and not already compressed data
   } // if no binary representation

   // done
   return TRUE;
}



/**************************************************************************************
CCatItem::NameString - Returns a name string that this can be saved into MML as.

inptus
   PWSTR       pszTemp - Filled in with the string. Must be 65 chars
returns
   PWSTR - pszTemp
*/
PWSTR CCatItem::NameString (PWSTR pszTemp)
{
   GUID ag[2];
   ag[0] = m_gSub;
   ag[1] = m_gCode;
   MMLBinaryToString ((PBYTE) &ag[0], sizeof(ag), pszTemp);

   return pszTemp;
}


/**************************************************************************************
CCatItem::NodeGet - Returns the MMLNode for the object. This loads it from the megafile
if necessary, or decompresses it.

returns
   PCMMLNode2 - Node, or NULL if can't get.
*/
PCMMLNode2 CCatItem::NodeGet (void)
{
   // maybe already cached
   if (m_pNode)
      return m_pNode;

   // if already have binary loaded the easy
   if (m_memNode.m_dwCurPosn) {
      if (Decompress())
         return m_pNode;
      return NULL;   // else error
   }

   // else, need to load from disk
   if (!m_fResidesOnDisk || !(m_pmf || MegaFileGet()))
      return NULL;

   __int64 iSize;
   WCHAR szTemp[96];
   if (m_pmf) {
      PVOID pData = m_pmf->Load (NameString(szTemp), &iSize, FALSE);
      if (!pData)
         return NULL;   // cant load this

      if (!m_memNode.Required ((DWORD)iSize)) {
         MegaFileFree (pData);
         return NULL;   // cant load
      }
      m_memNode.m_dwCurPosn = (DWORD)iSize;
      memcpy (m_memNode.p, pData, m_memNode.m_dwCurPosn);
      MegaFileFree (pData);
   }
   else {
      // this is only reached if have a megafile remap going on, so can open the
      // file directly
      m_pNode = MMLFileOpen (NameString(szTemp), &GUID_LibraryHeaderNewer, NULL, FALSE);
      if (!m_pNode)
         return NULL;
   }

   // decompress
   if (!Decompress())
      return NULL;

   return m_pNode;
}



/**************************************************************************************
CCatItem::NodeSet - Sets a node to a new value.

inputs
   PCMMLNode2        pNode - New node to use. This will be cloned and kept around
   BOOL              fKeepNode - If TRUE then pNode is kept
returns
   BOOL - TRUE if succeess
*/
BOOL CCatItem::NodeSet (PCMMLNode2 pNode, BOOL fKeepNode)
{
   m_fDirty = *m_pfDirty = TRUE;
   m_memNode.m_dwCurPosn = 0; // to indicate that compression is bad
   if (m_pNode)
      delete m_pNode;
   m_pNode = fKeepNode ? pNode : pNode->Clone();
   if (!m_pNode)
      return FALSE;

   // NOTE: Will need to comrpress when saving

   return TRUE;
}


/**************************************************************************************
CCatItem::Delete - Deletes the item from the megafile. NOTE: This does not actually
delete the item from memory.

returns
   BOOL - TRUE if success
*/
BOOL CCatItem::Delete (void)
{
   if (!m_fResidesOnDisk)
      return FALSE;  // doesnt exist in disk

   // get the name and delete
   WCHAR szTemp[96];
   m_fResidesOnDisk = FALSE;
   m_fDirty = *m_pfDirty = TRUE;
   return m_pmf ? m_pmf->Delete (NameString(szTemp)) : FALSE;
}


/**************************************************************************************
CCatItem::Save - Causes this to be saved to the megafile (if it needs it). This will
also delete it from the megafile if the new size is smaller than what can be stored
on a per-item basis.
*/
BOOL CCatItem::Save (void)
{
   if (!m_memNode.m_dwCurPosn) {
      if (!Compress())
         return FALSE;
   }

   if ((m_memNode.m_dwCurPosn > TOOLARGEFORTHISFILE) && (m_pmf || MegaFileGet())) {
      // this is too large to fit in this file, so need to save
      m_fResidesOnDisk = TRUE;
      m_fDirty = *m_pfDirty = TRUE;  // so know to save MML

      WCHAR szTemp[96];
      BOOL fRet;
      if (m_pmf)
         fRet = m_pmf->Save (NameString(szTemp), m_memNode.p, m_memNode.m_dwCurPosn);
      else if (MegaFileGet() && m_pNode)
         // called if in the megafile remap mode. Because I'm lazy will
         // only call if m_pNode is valud, not m_memNode like above. This
         // doesn't matter much because this shouldn't be called
         fRet = MMLFileSave (NameString(szTemp), &GUID_LibraryHeaderNewer, m_pNode);
      else
         fRet = FALSE;
      if (!fRet) {
         m_fResidesOnDisk = FALSE;
         return FALSE;
      }

      return TRUE;
   }

   // else, resides in master file
   // if it was on disk then delete that
   if (m_fResidesOnDisk) {
      Delete ();
      m_fResidesOnDisk = FALSE;
      m_fDirty = *m_pfDirty = TRUE;
   }

   // else, nothing to save
   return TRUE;
}


/**************************************************************************************
CCatItem::Decompress - Decompresses from m_memNode to m_pNode

returns
   BOOL - TRUE if success
*/
BOOL CCatItem::Decompress (void)
{
   if (m_pNode)
      return TRUE;   // already there

   m_pNode = MMLFileOpen ((PBYTE)m_memNode.p, (DWORD)m_memNode.m_dwCurPosn, &GUID_LibraryHeaderNewer);

   return m_pNode ? TRUE : FALSE;
}



/**************************************************************************************
CCatItem::Compress - Compresses from m_pNode to m_memNode.

returns
   BOOl - TRUE if success
*/
BOOL CCatItem::Compress (void)
{
   // if already there then dont bother
   if (m_memNode.m_dwCurPosn)
      return TRUE;
   if (!m_pNode)
      return FALSE;

   if (!MMLFileSave (&m_memNode, &GUID_LibraryHeaderNewer, m_pNode)) {
      m_memNode.m_dwCurPosn = 0;
      return FALSE;
   }

   // worked
   return TRUE;
}







/**************************************************************************************
LibraryObjects - Returns a pointer to the CLibraryCategory for objects.

inputs
   BOOL        fUser - If TRUE return the user library, else installed
*/
PCLibraryCategory LibraryObjects (DWORD dwRenderShard, BOOL fUser)
{
   if (fUser)
      return gapLCObjectsUser[dwRenderShard];
   else
      return gapLCObjectsInstalled[dwRenderShard];
}

/**************************************************************************************
LibraryTextures - Returns a pointer to the CLibraryCategory for textures.

inputs
   BOOL        fUser - If TRUE return the user library, else installed
*/
PCLibraryCategory LibraryTextures (DWORD dwRenderShard, BOOL fUser)
{
   if (fUser)
      return gapLCTexturesUser[dwRenderShard];
   else
      return gapLCTexturesInstalled[dwRenderShard];
}

/**************************************************************************************
LibraryEffects - Returns a pointer to the CLibraryCategory for effects.

inputs
   BOOL        fUser - If TRUE return the user library, else installed
*/
PCLibraryCategory LibraryEffects (DWORD dwRenderShard, BOOL fUser)
{
   if (fUser)
      return gapLCEffectsUser[dwRenderShard];
   else
      return gapLCEffectsInstalled[dwRenderShard];
}

/**************************************************************************************
CLibrary::Constructor and destructor */
CLibrary::CLibrary (void)
{
   m_fDirty = FALSE;
   m_lCATINFO.Init (sizeof(CATINFO));
   m_szFile[0] = m_szIndex[0] = 0;
   m_pmf = NULL;
}

CLibrary::~CLibrary (void)
{
   // first commit if dirty
   if (m_fDirty)
      Commit(NULL);

   PCATINFO pci;
   DWORD i;
   pci = (PCATINFO) m_lCATINFO.Get(0);
   for (i = 0; i < m_lCATINFO.Num(); i++)
      if (pci[i].pCategory)
         delete pci[i].pCategory;

   // BUGBUG - If using a shared megafile then don't want to delete this
   if (m_pmf)
      delete m_pmf;
}



/**************************************************************************************
CLibrary::MMLFrom - Reads its contents from a MMLNode.

inputs
   PCMMLNOde      pNode - node
   DWORD       *pdwNum - Filled with the number of elements
returns
   BOOL - TRUE if succeede
*/
BOOL CLibrary::MMLFrom (PCMMLNode2 pNode, DWORD *pdwNum)
{
   // go through the nodes...
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode2 pSub;
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;
      if (!pSub || !pSub->NameGet())
         continue;

      // new category
      CATINFO ci;
      memset (&ci, 0, sizeof(ci));
      wcscpy (ci.szCategory, pSub->NameGet());
      ci.pCategory = new CLibraryCategory(this);
      if (!ci.pCategory)
         continue;

      DWORD dwCat;
      if (!ci.pCategory->MMLFrom (pSub, &dwCat))
         continue;

      if (pdwNum)
         (*pdwNum) += dwCat;

#ifdef _DEBUG
      char szTemp[128];
      WideCharToMultiByte (CP_ACP, 0, ci.szCategory, -1, szTemp, 128, 0,0);
      sprintf (szTemp + strlen(szTemp), ": %d elements\r\n", (int) dwCat);
      OutputDebugString (szTemp);
#endif

      m_lCATINFO.Add (&ci);
   }

   return TRUE;
}

/**************************************************************************************
CLibrary::Init - Reads in the lirbary file.

inputs
   PWSTR       pszFile - file to load in
   PWSTR       pszIndex - Name of the index in the megafile.
   DWORD       *pdwNum - Filled with the number of elements
returns
   BOOL - TRUE if success
*/
BOOL CLibrary::Init (PWSTR pszFile, PWSTR pszIndex, PCProgressSocket pProgress, DWORD *pdwNum)
{
   if (pdwNum)
      *pdwNum = 0;
   if (m_szFile[0] || m_pmf)
      return FALSE;  // already have something

   wcscpy (m_szFile, pszFile);
   wcscpy (m_szIndex, pszIndex);

   // if the megafile override is set should use that.
   if (MegaFileGet())
      m_pmf = NULL;
   else {
      m_pmf = new CMegaFile;
      if (!m_pmf)
         return FALSE;
      if (!m_pmf->Init (m_szFile, &GUID_LibraryHeaderNewer, FALSE)) {
   #ifdef _DEBUG
         // load in old form and then save out
         PCMMLNode2 pNode;
         pNode = MMLFileOpen (pszFile, &GUID_LibraryHeaderNewer, pProgress, FALSE);
            // BUGFIX - Pass in ignoredir for megafiles
            // BUGFIX - Take out ignoredir since wont be using with megafiles
         if (pNode) {
            MMLFrom (pNode, pdwNum);
            delete pNode;
         }
   #endif // _DEBUG

         // try reloading
         if (!m_pmf->Init (m_szFile, &GUID_LibraryHeaderNewer, TRUE))
            return FALSE;

         // note that this is dirty
         m_fDirty = TRUE;

         // save away changes
         Commit (NULL);

         // done
         return TRUE;
      }
   } // if !MegaFileGet()

   // read the node from the megafile
   PCMMLNode2 pNode;
   if (m_pmf)
      pNode = MMLFileOpen (m_pmf, m_szIndex, &GUID_LibraryHeaderNewer, pProgress, FALSE);
   else if (MegaFileGet())
      // if using megafile call this to read from it
      pNode = MMLFileOpen (m_szIndex, &GUID_LibraryHeaderNewer, pProgress, FALSE);
   else
      pNode = NULL;

   if (!pNode)
      return FALSE;

   if (!MMLFrom (pNode, pdwNum)) {
      delete pNode;
      return FALSE;
   }
   delete pNode;

   m_fDirty = FALSE;
   return TRUE;

#if 0 // disable
   // read in old format
   if (!pNode) {
      PMEGAFILE f;
      f = MegaFileOpen(pszFile, TRUE, MFO_IGNOREDIR); // NOTE: Allowing any directory for megafile
      if (!f)
         return FALSE;  // cant ope

      CMem mem;
      MegaFileSeek (f, 0, SEEK_END);
      DWORD dwLen;
      dwLen = MegaFileTell (f);
      MegaFileSeek (f, 0, SEEK_SET);

      if (!mem.Required (dwLen)) {
         MegaFileClose (f);
         return FALSE;
      }

      MegaFileRead (mem.p, 1, dwLen, f);

      MegaFileClose (f);


      // verify right type
      GUID *pg;
      pg = (GUID*) mem.p;
      BOOL fToken;
      if (IsEqualGUID(*pg, GUID_LibraryHeaderOld))
         fToken = FALSE;
      else if (IsEqualGUID(*pg, GUID_LibraryHeaderNew))
         fToken = TRUE;
      else
         return FALSE;  // not right type

      // get the node
      DWORD dwRet;
      pNode = NULL;
      if (!fToken)
         dwRet = MMLDecompressFromANSI(pg+1, dwLen - sizeof(GUID), pProgress, &pNode);
      else
         dwRet = MMLDecompressFromToken(pg+1, dwLen - sizeof(GUID), pProgress, &pNode, FALSE);
            // NOTE - Eventually pass TRUE to dont encode binary
      if (dwRet)
         return FALSE;
   }
#endif // _DEBUG

   // convert from ANSI to unicode
   //CMem memUni;
   //if (!memUni.Required (mem.m_dwAllocated * 2))
   //   return FALSE;
   //MultiByteToWideChar (CP_ACP, 0, (char*)(pg + 1), -1, (PWSTR) memUni.p, memUni.m_dwAllocated/2);

   // get from world
   //PCMMLNode2 pNode;
   //pNode = MMLFromMem((PWSTR) memUni.p);
   //if (!pNode)
   //   return FALSE;

}

/*********************************************************************************
CLibrary::Commit - Saves the library information out to a file.

inputs
   PCProgressSocket     pProgress - Progress bar
   BOOL           fDebug - if TRUE, then write the long version (for debug purposes)
returns
   BOOL - TRUE if success
*/
BOOL CLibrary::Commit (PCProgressSocket pProgress, BOOL fDebug)
{
   // create the main MML node
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return FALSE;
   pNode->NameSet (L"Library");

   // go through all the categories
   DWORD i;
   PCATINFO pci;
   pci = (PCATINFO) m_lCATINFO.Get(0);
   if (pProgress)
      pProgress->Push (0, 0.5);
   for (i = 0; i < m_lCATINFO.Num(); i++) {
      if (pProgress)
         pProgress->Update ((fp)i / (fp)m_lCATINFO.Num());
      if (!pci[i].pCategory)
         continue;

      // have each category commit so that they'll save changes directly to MML
      pci[i].pCategory->Commit ();

      PCMMLNode2 pSub;
      pSub = pci[i].pCategory->MMLTo ();
      if (!pSub)
         continue;

      pSub->NameSet (pci[i].szCategory);
      pNode->ContentAdd (pSub);
   }

   if (pProgress) {
      pProgress->Pop();
      pProgress->Push (0.5, 1);
   }

   BOOL fRet;
   if (m_pmf)
      fRet = MMLFileSave (m_pmf, m_szIndex, &GUID_LibraryHeaderNewer, pNode, 1, pProgress);
   else if (MegaFileGet())
      // this is reached if in the megafile remap mode
      fRet = MMLFileSave (m_szIndex, &GUID_LibraryHeaderNewer, pNode, 1, pProgress);
   else
      fRet = FALSE;

   delete pNode;
   if (pProgress)
      pProgress->Pop();
   if (!fRet)
      return FALSE;

   // the following is old cod
   // conver to a string
//   CMem memANSI;
//   DWORD dwRet;
//   if (fDebug)
//      dwRet = MMLCompressToANSI(pNode, pProgress, &memANSI);
//   else
//      dwRet = MMLCompressToToken(pNode, pProgress, &memANSI, FALSE);
         // NOTE - Eventually pass TRUE to dont encode binary
//   delete pNode;
//   if (dwRet)
//      return FALSE;

   // write out
   //if (!MMLToMem (pNode, &mem, TRUE)) {
   //   delete pNode;
   //   return FALSE;
   //}
   // findally, delete node

   // convert from unicode to ANSI
   //CMem memANSI;
   //if (!memANSI.Required (mem.m_dwCurPosn+2))
   //   return FALSE;
   //int iLen;
   //iLen = WideCharToMultiByte (CP_ACP, 0, (PWSTR) mem.p, -1, (char*) memANSI.p, memANSI.m_dwAllocated, 0, 0);
   //memANSI.m_dwCurPosn = iLen;

   // save this to file
//   PMEGAFILE f;
//   char szFile[256];
//   strcpy (szFile, m_szFile);
//   if (fDebug)
//      strcat (szFile, "d");
//   f = MegaFileOpen (szFile, FALSE);
//   if (!f)
//      return FALSE;
//   if (fDebug)
//      MegaFileWrite ((PVOID) &GUID_LibraryHeaderOld, sizeof(GUID), 1, f);
//   else
//      MegaFileWrite ((PVOID) &GUID_LibraryHeaderNew, sizeof(GUID), 1, f);
//   MegaFileWrite (memANSI.p, 1, memANSI.m_dwCurPosn, f);
//   MegaFileClose (f);

   m_fDirty = FALSE;
   return TRUE;
}


/*********************************************************************************
CLibrary::Clone - Clones the library.

inputs
   PWSTR          pszFile - New megafile
   PWSTR          *papszInclude - List of categories to include. If NULL,
                  this includes all.
   DWORD          dwIncludeNum - Number of entries in papszInclude
returns
   PCLibrary - Library if success
*/
PCLibrary CLibrary::Clone (PWSTR pszFile, PWSTR *papszInclude, DWORD dwIncludeNum)
{
   PCLibrary pNew = new CLibrary;
   if (!pNew)
      return NULL;
   pNew->m_pmf = new CMegaFile;
   if (!pNew->m_pmf) {
      delete pNew;
      return NULL;
   }
   if (!pNew->m_pmf->Init (pszFile, &GUID_LibraryHeaderNewer, TRUE)) {
      delete pNew;
      return NULL;
   }
   pNew->m_fDirty = TRUE;  // so will commit
   wcscpy (pNew->m_szFile, pszFile);
   wcscpy (pNew->m_szIndex, m_szIndex);

   // categories
   DWORD i, j;
   PCATINFO pci = (PCATINFO)m_lCATINFO.Get(0);
   CATINFO ci;
   for (i = 0; i < m_lCATINFO.Num(); i++, pci++) {
      // see if it's valid
      if (papszInclude) {
         for (j = 0; j < dwIncludeNum; j++)
            if (!_wcsicmp(papszInclude[j], pci->szCategory))
               break;
         if (j >= dwIncludeNum)
            continue;   // dont include
      }

      // clone this
      memset (&ci, 0, sizeof(ci));
      wcscpy (ci.szCategory, pci->szCategory);
      ci.pCategory = pci->pCategory->Clone (pNew);
      if (!ci.pCategory)
         continue;   // error

      // add
      pNew->m_lCATINFO.Add (&ci);
   } // i

   return pNew;
}

/*********************************************************************************
CLibrary::DirtySet - Sets the state of the dirty flag

inputs
   BOOL     fDirty - New state
*/
void CLibrary::DirtySet (BOOL fDirty)
{
   m_fDirty = fDirty;
}


/*********************************************************************************
CLibrary::DirtyGet - Gets the state of the dirty flag.
*/
BOOL CLibrary::DirtyGet (void)
{
   return m_fDirty;
}


/*********************************************************************************
CLibrary::CategoryGet - Returns a pointer to a cateogry. DO NOT delte this. It
will remain valid until the list is deleted.

inputs
   PWSTR       pszCategory - Category name
   BOOL        fCreateIfNotExist - If it doesn't exist then create it
returns
   PCLibraryCategory - cateogyr pointer, NULL if error
*/
PCLibraryCategory CLibrary::CategoryGet (PWSTR pszCategory, BOOL fCreateIfNotExist)
{
   // find it
   DWORD i;
   PCATINFO pci;
   pci = (PCATINFO) m_lCATINFO.Get(0);
   for (i = 0; i < m_lCATINFO.Num(); i++) {
      if (!_wcsicmp(pszCategory, pci[i].szCategory))
         return pci[i].pCategory;
   }

   // if get here couldnt find
   if (!fCreateIfNotExist)
      return NULL;

   // create it
   CATINFO ci;
   memset (&ci, 0, sizeof(ci));
   wcscpy (ci.szCategory, pszCategory);
   ci.pCategory = new CLibraryCategory(this);
   if (!ci.pCategory)
      return NULL;
   m_lCATINFO.Add (&ci);
   m_fDirty = TRUE;
   return ci.pCategory;
}





/***********************************************************************************
CLibraryCategory::Constructor and destructor */
CLibraryCategory::CLibraryCategory(CLibrary *pLibrary)
{
   m_pLibrary = pLibrary;
}

CLibraryCategory::~CLibraryCategory()
{
   // delete the items stores in the trees
   DWORD i, j, k;
   for (i = 0; i < m_treeItems.Num(); i++) {
      PCBTree pMinor;
      pMinor = *((PCBTree*) m_treeItems.GetNum(i));
      if (!pMinor)
         continue;

      for (j = 0; j < pMinor->Num(); j++) {
         PCBTree pItem;
         pItem = *((PCBTree*) pMinor->GetNum(j));
         if (!pItem)
            continue;

         // loop over all the items
         for (k = 0; k < pItem->Num(); k++) {
            PCCatItem *ppci = (PCCatItem*)pItem->GetNum (k);
            if (!ppci)
               continue;
            PCCatItem pci = *ppci;

            delete pci;
         }  // over items

         delete pItem;

      }  // over minor

      delete pMinor;
   }  // over major

}


/***********************************************************************************
CLibraryCategory::Clone - Clones a library category

inputs
   PCLibrary         pLibrary - Library that this will belong to
returns
   PCLibraryCategory - Cloned object
*/
PCLibraryCategory CLibraryCategory::Clone (PCLibrary pLibrary)
{
   PCLibraryCategory pNew = new CLibraryCategory (pLibrary);
   if (!pNew)
      return NULL;
   
   // clone tree items
   DWORD i, j, k;
   for (i = 0; i < m_treeItems.Num(); i++) {
      PCBTree pMinor;
      pMinor = *((PCBTree*) m_treeItems.GetNum(i));
      if (!pMinor)
         continue;

      // create sub-tree
      PCBTree pNewMinor = new CBTree;
      if (!pNewMinor)
         continue;   // error

      for (j = 0; j < pMinor->Num(); j++) {
         PCBTree pItem;
         pItem = *((PCBTree*) pMinor->GetNum(j));
         if (!pItem)
            continue;

         // create sub-tree again
         PCBTree pNewItem = new CBTree;
         if (!pNewItem)
            continue;   // error

         // loop over all the items
         for (k = 0; k < pItem->Num(); k++) {
            PCCatItem *ppci = (PCCatItem*)pItem->GetNum (k);
            if (!ppci)
               continue;
            PCCatItem pci = *ppci;

            PCCatItem pNewCat = pci->Clone (pLibrary->m_pmf, &pLibrary->m_fDirty);
            if (pNewCat)
               pNewItem->Add (pItem->Enum(k), &pNewCat, sizeof(pNewCat));
         }  // over items

         // add new item
         pNewMinor->Add (pMinor->Enum(j), &pNewItem, sizeof(pNewItem));

      }  // over minor

      // add new minor
      pNew->m_treeItems.Add (m_treeItems.Enum(i), &pNewMinor, sizeof(pNewMinor));
   }  // over major

   return pNew;
}


/***********************************************************************************
CLibraryCategory::MMLTo - Writes category to MML.

NOTE: This does NOT actually write disk versions out to the megafile. For that
use Commit().

returns
   PCMMLNode2 - TRUE if success
*/
PCMMLNode2 CLibraryCategory::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (L"Category");

   // enumerate all the tiems
   DWORD i, j, k;
   PWSTR pszMajor, pszMinor, pszName;
   for (i = 0; i < m_treeItems.Num(); i++) {
      pszMajor = m_treeItems.Enum (i);
      if (!pszMajor)
         continue;
      PCBTree pMinor;
      pMinor = *((PCBTree*) m_treeItems.GetNum(i));
      if (!pMinor)
         continue;

      for (j = 0; j < pMinor->Num(); j++) {
         pszMinor = pMinor->Enum (j);
         if (!pszMinor)
            continue;
         PCBTree pItem;
         pItem = *((PCBTree*) pMinor->GetNum(j));
         if (!pItem)
            continue;

         // loop over all the items
         for (k = 0; k < pItem->Num(); k++) {
            pszName = pItem->Enum(k);
            if (!pszName)
               continue;
            PCCatItem *ppci = (PCCatItem*)pItem->GetNum(k);
            if (!ppci)
               continue;
            PCCatItem pci = *ppci;

            PCMMLNode2 pSub =  pci->MMLTo (pszName, pszMajor, pszMinor);
            if (!pSub) {
               delete pNode;
               return FALSE;
            }
            pNode->ContentAdd (pSub);
         }  // over items

      }  // over minor

   }  // over major

   return pNode;
}

/***********************************************************************************
CLibraryCategory::MMLFrom - Parses the PCMMLNode2 and fills in the cateogry information.
NOTE: This is different from most MMLFrom because it pulls all the contents out of pNode
and leaves only a shell.

inputs
   PCMMLNode2      pNode - node
   DWORD          *pdwNum - If Not NULL, filled with the number in the category
returns
   BOOL - TRUE if success
*/
BOOL CLibraryCategory::MMLFrom (PCMMLNode2 pNode, DWORD *pdwNum)
{
   if (pdwNum)
      *pdwNum = 0;

   // loop over all the contents
   PCMMLNode2 pSub;
   PWSTR psz;
   DWORD i;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      if (!pNode->ContentEnum (i, &psz, &pSub))
         continue;
      if (!pSub || !(psz = pSub->NameGet()))
         continue;
      if (_wcsicmp(psz, gpszCatItem))
         continue;

      PCCatItem pci = new CCatItem (m_pLibrary->m_pmf, &m_pLibrary->m_fDirty);
      if (!pci)
         return FALSE;
      if (!pci->MMLFrom (pSub))
         return FALSE;

      // add this
      PCBTree pMinor = FindMinor ((PWSTR)pci->m_memMajor.p, (PWSTR)pci->m_memMinor.p, TRUE);
      if (!pMinor || pMinor->Find((PWSTR)pci->m_memName.p)) {
         delete pci;
         return FALSE;  // already exists
      }

      if (!pMinor->Add ((PWSTR)pci->m_memName.p, &pci, sizeof(pci))) {
         delete pci;
         return FALSE;  // already exists
      }

      if (pdwNum)
         (*pdwNum)++;
   }
   // BUGFIX - Dont delete here or causes crash: delete pNode;
   return TRUE;
}

/***********************************************************************************
CLibraryCategory::FindMajor - Given a major string, finds the minor tree. Creates
the minor tree if the major doesnt exist.

inputs
   PWSTR    pszMajor - Major category
   BOOL     fCreateIfNotExist - If TRUE, create the major if it doesn't exist
returns
   PCBTree - Tree for minor. NULL if error or cant find
*/
PCBTree CLibraryCategory::FindMajor (PWSTR pszMajor, BOOL fCreateIfNotExist)
{
   PCBTree *ppFind = (PCBTree*) m_treeItems.Find (pszMajor);
   if (ppFind)
      return *ppFind;

   if (!fCreateIfNotExist)
      return FALSE;

   PCBTree pNew;
   pNew = new CBTree;
   if (!pNew)
      return NULL;

   m_treeItems.Add (pszMajor, &pNew, sizeof(pNew));
   return pNew;
}

/***********************************************************************************
CLibraryCategory::FindMinor - Given a major and minor string, finds the items tree. Creates
the items tree if the major/minor doesnt exist.

inputs
   PWSTR    pszMajor - Major category
   PWSTR    pszMinor - Minor cateogry
   BOOL     fCreateIfNotExist - If TRUE, create the major if it doesn't exist
returns
   PCBTree - Tree for items. NULL if error or cant find
*/
PCBTree CLibraryCategory::FindMinor (PWSTR pszMajor, PWSTR pszMinor, BOOL fCreateIfNotExist)
{
   PCBTree pMajor = FindMajor (pszMajor, fCreateIfNotExist);
   if (!pMajor)
      return NULL;

   PCBTree *ppFind;
   ppFind = (PCBTree*) pMajor->Find (pszMinor);
   if (ppFind)
      return *ppFind;

   if (!fCreateIfNotExist)
      return NULL;

   PCBTree pNew;
   pNew = new CBTree;
   if (!pNew)
      return NULL;

   pMajor->Add (pszMinor, &pNew, sizeof(pNew));
   return pNew;
}

/***********************************************************************************
CLibraryCategory::IntItemGet - Returns the PCCatItem for the item.

inputs
   PWSTR    pszMajor - Major category
   PWSTR    pszMinor - Minor cateogry
   PWSTR    pszName - Name
returns
   PCMMLNode2 - MML for item, or NULL if error
*/
PCCatItem CLibraryCategory::IntItemGet (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   PCBTree pItem = FindMinor (pszMajor, pszMinor);
   if (!pItem)
      return NULL;

   PCCatItem *ppci = (PCCatItem*) pItem->Find (pszName);
   if (!ppci)
      return NULL;
   return *ppci;
}


/***********************************************************************************
CLibraryCategory::ItemExists - Returns TRUE if the item exists.

inputs
   PWSTR    pszMajor - Major category
   PWSTR    pszMinor - Minor cateogry
   PWSTR    pszName - Name
returns
   BOOL - TRUE if exists
*/
BOOL CLibraryCategory::ItemExists (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   return IntItemGet (pszMajor, pszMinor, pszName) ? TRUE : FALSE;
}

/***********************************************************************************
CLibraryCategory::ItemGet - Returns the MML for the item,

inputs
   PWSTR    pszMajor - Major category
   PWSTR    pszMinor - Minor cateogry
   PWSTR    pszName - Name
returns
   PCMMLNode2 - MML for item, or NULL if error
*/
PCMMLNode2 CLibraryCategory::ItemGet (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   PCCatItem pci = IntItemGet (pszMajor, pszMinor, pszName);
   return pci ? pci->NodeGet() : NULL;
}

/***********************************************************************************
CLibraryCategory::ItemGet - Given a GUID, returns the item's MML node

inputs
   GUID        *pgCode - Major guid
   GUID        *pgSub - Minor guid
returns
   PCMMLNode2 - node
*/
PCMMLNode2 CLibraryCategory::ItemGet (const GUID *pgCode, const GUID *pgSub)
{
   WCHAR szMajor[256], szMinor[256], szName[256];
   if (!ItemNameFromGUID (pgCode, pgSub, szMajor, szMinor, szName))
      return NULL;

   return ItemGet (szMajor, szMinor, szName);
}

/***********************************************************************************
CLibraryCategory::ItemNameFromGUID - Given a guid, searches through the entire
list and fills in pszMajor, pszMinor, and pszName with the item name.

inputs
   GUID     *pgCode, *pgSub - GUIDS identifying object
   PWSTR    pszMajor, pszMinor, pszName - Filled in with name. Can be null
returns
   BOOL - TRUE if found
*/
BOOL CLibraryCategory::ItemNameFromGUID (const GUID *pgCode, const GUID *pgSub, PWSTR pszFillMajor, PWSTR pszFillMinor, PWSTR pszFillName)
{
   // enumerate all the tiems
   DWORD i, j, k;
   PWSTR pszMajor, pszMinor, pszName;
   for (i = 0; i < m_treeItems.Num(); i++) {
      pszMajor = m_treeItems.Enum (i);
      if (!pszMajor)
         continue;
      PCBTree pMinor;
      pMinor = *((PCBTree*) m_treeItems.GetNum(i));
      if (!pMinor)
         continue;

      for (j = 0; j < pMinor->Num(); j++) {
         pszMinor = pMinor->Enum (j);
         if (!pszMinor)
            continue;
         PCBTree pItem;
         pItem = *((PCBTree*) pMinor->GetNum(j));
         if (!pItem)
            continue;

         // loop over all the items
         for (k = 0; k < pItem->Num(); k++) {
            pszName = pItem->Enum(k);
            if (!pszName)
               continue;
            PCCatItem *ppci = (PCCatItem*) pItem->GetNum (k);
            if (!ppci)
               continue;
            PCCatItem pci = *ppci;

            if (IsEqualGUID(pci->m_gSub, *pgSub) && IsEqualGUID(pci->m_gCode, *pgCode)) {
               if (pszFillMajor)
                  wcscpy (pszFillMajor, pszMajor);
               if (pszFillMinor)
                  wcscpy (pszFillMinor, pszMinor);
               if (pszFillName)
                  wcscpy (pszFillName, pszName);
               return TRUE;
            }
         }  // over items

      }  // over minor

   }  // over major

   return FALSE;
}

/***********************************************************************************
CLibraryCategory::ItemGUIDFromName - Given a GUID, fills in a name.

inputs
   PWSTR    pszMajor, pszMinor, pszName - Name,
   GUID     *pgCode, *pgSub - Filled in GUIDS identifying object
returns
   BOOL - TRUE if found
*/
BOOL CLibraryCategory::ItemGUIDFromName (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, GUID *pgCode, GUID *pgSub)
{
   PCBTree pItem = FindMinor (pszMajor, pszMinor);
   if (!pItem)
      return FALSE;

   PCCatItem *ppci = (PCCatItem*)pItem->Find (pszName);
   if (!ppci)
      return FALSE;
   PCCatItem pci = *ppci;

   if (pgCode)
      *pgCode = pci->m_gCode;
   if (pgSub)
      *pgSub = pci->m_gSub;

   return TRUE;
}

/***********************************************************************************
CLibraryCategory::IntItemRemoveButDontDelete - Removes the item from the tree but
doesn't delete the PCMMLNode2 there. It also prunes dead branches.

inputs
   PWSTR       pszMajor, pszMinor, pszName - Full name
   BOOL        fDelete - If TRUE then should delte it anyway
returns
   BOOL - TRUE if found and removed. FALSE if cant find
*/
BOOL CLibraryCategory::IntItemRemoveButDontDelete (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName,
                                                   BOOL fDelete)
{
   PCBTree pMinor = FindMinor (pszMajor, pszMinor);
   PCBTree pMajor = FindMajor (pszMajor);

   if (!pMinor)
      return FALSE;  // cant find at all
   PCCatItem *ppci = (PCCatItem*) pMinor->Find (pszName);
   if (!ppci)
      return FALSE;  // cant find at all

   if (fDelete && ppci) {
      // if had written this to disk then want to delte the disk contents
      PCCatItem pci = *ppci;
      if (pci->m_fResidesOnDisk)
         pci->Delete();
      delete *ppci;
   }

   pMinor->Remove (pszName);

   // if there aren't any elements in minor then remove
   if (!pMinor->Num()) {
      delete pMinor;
      pMajor->Remove (pszMinor);
   }

   // if there arent any elements in the major then remove
   if (!pMajor->Num()) {
      delete pMajor;
      m_treeItems.Remove (pszMajor);
   }

   // set dirty flag
   m_pLibrary->DirtySet();

   return TRUE;
}


/***********************************************************************************
CLibraryCategory::ItemAdd - Adds a new item.

inptus
   PWSTR    pszMajor, pszMinor, pszName - To add
   GUID     *pgCode, *pgSub - GUID to itendity
   PCMMLNode2 pNode - Node to use. This pointer is stored directly and NOT cloned
returs
   BOOL - TRUE if success. FALSE if errror - like already exists
*/
BOOL CLibraryCategory::ItemAdd (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, const GUID *pgCode, const GUID *pgSub, PCMMLNode2 pNode)
{
   PCBTree pMinor = FindMinor (pszMajor, pszMinor, TRUE);
   if (!pMinor)
      return FALSE;
   if (pMinor->Find(pszName))
      return FALSE;  // already exists

   PCCatItem pci = new CCatItem (m_pLibrary->m_pmf, &m_pLibrary->m_fDirty);
   if (!pci)
      return FALSE;

   pci->m_gCode = *pgCode;
   pci->m_gSub = *pgSub;
   pci->NodeSet (pNode, TRUE);

   if (!pMinor->Add (pszName, &pci, sizeof(pci)))
      return FALSE;

   // set dirty flag
   m_pLibrary->DirtySet();

   return TRUE;
}


/***********************************************************************************
CLibraryCategory::ItemDirty - Call this after an item is changed, so the dirty
flag is set all around.
*/
void CLibraryCategory::ItemDirty (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName, BOOL fDirty)
{
   // ignoring the other params for now
   m_pLibrary->DirtySet (fDirty);
}

void CLibraryCategory::ItemDirty (const GUID *pgCode, const GUID *pgSub, BOOL fDirty)
{
   // ignoring the other params for now
   m_pLibrary->DirtySet (fDirty);
}

/***********************************************************************************
CLibraryCategory::ItemRename - Renames an item.

inputs
   PWSTR    pszOrigMajor, pszOrigMinor, pszOrigName - Original
   PWSTR    pszMajor, pszMinor, pszName - New
returns
   BOOL - TRUE if succes, FALSE if failure - either couldnt find the original, or
            the new one already exists
*/
BOOL CLibraryCategory::ItemRename (PWSTR pszOrigMajor, PWSTR pszOrigMinor, PWSTR pszOrigName, PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   // get the original
   GUID gCode, gSub;
   PCCatItem pci = IntItemGet (pszOrigMajor, pszOrigMinor, pszOrigName);
   if (!pci || !ItemGUIDFromName (pszOrigMajor, pszOrigMinor, pszOrigName, &gCode, &gSub))
      return FALSE;

   // if new one exists then return false
   if (ItemGet (pszMajor, pszMinor, pszName))
      return FALSE;

   // remove old one, add new one
   IntItemRemoveButDontDelete (pszOrigMajor, pszOrigMinor, pszOrigName, FALSE);

   // set dirty flag
   m_pLibrary->DirtySet();

   // add this
   PCBTree pMinor = FindMinor (pszMajor, pszMinor, TRUE);
   if (!pMinor || pMinor->Find(pszName)) {
      delete pci;
      return FALSE;  // already exists
   }

   if (!pMinor->Add (pszName, &pci, sizeof(pci))) {
      delete pci;
      return FALSE;  // already exists
   }
   return TRUE;
}

/***********************************************************************************
CLibraryCategory::ItemRemove - Deletes the item, including the MML associated with it.

inputs
   PWSTR    pszMajor, pszMinor, pszName - item
returns
   BOOL - TRUE if succes, FALSE if can't find
*/
BOOL CLibraryCategory::ItemRemove (PWSTR pszMajor, PWSTR pszMinor, PWSTR pszName)
{
   return IntItemRemoveButDontDelete (pszMajor, pszMinor, pszName, TRUE);
}

/***********************************************************************************
CLibraryCategory::EnumMajor - Fills pl with a list of PWSTR pointing to the names
for the major caterogy names. The pointers are valid until a major category
is removed.

inputs
   PCListFixed       pl - Will be initialized to sizeof(PWSTR), and contain pointers to string
*/
void CLibraryCategory::EnumMajor (PCListFixed pl)
{
   pl->Init (sizeof(PWSTR));
   pl->Clear();

   PCBTree pTree = &m_treeItems;
   if (!pTree)
      return;
   DWORD i;
   for (i = 0; i < pTree->Num(); i++) {
      PWSTR psz = pTree->Enum(i);
      if (!psz)
         continue;
      pl->Add (&psz);
   }
}

/***********************************************************************************
CLibraryCategory::EnumMinor - Fills pl with a list of PWSTR pointing to the names
for the major caterogy names. The pointers are valid until a major category
is removed.

inputs
   PCListFixed       pl - Will be initialized to sizeof(PWSTR), and contain pointers to string
   PWSTR             pszMajor - Major category
*/
void CLibraryCategory::EnumMinor (PCListFixed pl, PWSTR pszMajor)
{
   pl->Init (sizeof(PWSTR));
   pl->Clear();

   PCBTree pTree = FindMajor (pszMajor);
   if (!pTree)
      return;
   DWORD i;
   for (i = 0; i < pTree->Num(); i++) {
      PWSTR psz = pTree->Enum(i);
      if (!psz)
         continue;
      pl->Add (&psz);
   }
}


/***********************************************************************************
CLibraryCategory::EnumItems - Fills pl with a list of PWSTR pointing to the names
for the items. The pointers are valid until a major category
is removed.

inputs
   PCListFixed       pl - Will be initialized to sizeof(PWSTR), and contain pointers to string
   PWSTR             pszMajor - Major category
   PWSTR             pszMinor - Minor category
*/
void CLibraryCategory::EnumItems (PCListFixed pl, PWSTR pszMajor, PWSTR pszMinor)
{
   pl->Init (sizeof(PWSTR));
   pl->Clear();

   PCBTree pTree = FindMinor (pszMajor, pszMinor);
   if (!pTree)
      return;
   DWORD i;
   for (i = 0; i < pTree->Num(); i++) {
      PWSTR psz = pTree->Enum(i);
      if (!psz)
         continue;
      pl->Add (&psz);
   }
}


/*********************************************************************************
CLibraryCategory::Commit - Causes the category to compress all nodes that aren't compressed
and then so save any not saved
*/
BOOL CLibraryCategory::Commit (void)
{
   // enumerate all the tiems
   DWORD i, j, k;
   PWSTR pszMajor, pszMinor, pszName;
   for (i = 0; i < m_treeItems.Num(); i++) {
      pszMajor = m_treeItems.Enum (i);
      if (!pszMajor)
         continue;
      PCBTree pMinor;
      pMinor = *((PCBTree*) m_treeItems.GetNum(i));
      if (!pMinor)
         continue;

      for (j = 0; j < pMinor->Num(); j++) {
         pszMinor = pMinor->Enum (j);
         if (!pszMinor)
            continue;
         PCBTree pItem;
         pItem = *((PCBTree*) pMinor->GetNum(j));
         if (!pItem)
            continue;

         // loop over all the items
         for (k = 0; k < pItem->Num(); k++) {
            pszName = pItem->Enum(k);
            if (!pszName)
               continue;
            PCCatItem *ppci = (PCCatItem*) pItem->GetNum (k);
            if (!ppci)
               continue;
            PCCatItem pci = *ppci;

            if (pci->m_fDirty)
               pci->Save();
         }  // over items

      }  // over minor

   }  // over major

   return TRUE;
}



/************************************************************************************
LibraryOpenFiles - Open two files, the installed library and the user-defined library.
The globals gaLibraryInstalled, and gaLibraryUser are set up

inputs
   BOOL              fAppDir - If TRUE then always open from the app's directory.
                              If FALSE, then look at the registry entry for
                              where the library's should be installed
   PCProgressSocket  pProgress - Progress bar
*/
void LibraryOpenFiles (DWORD dwRenderShard, BOOL fAppDir, PCProgressSocket pProgress)
{
   char szaTemp[256];
   WCHAR szTemp[256];

   if (pProgress)
      pProgress->Push (0, .5);

   // BUGFIX - get library location from registry instead, so get from location of 3dob's exe
   if (fAppDir)
      strcpy (szaTemp, gszAppDir);
   else
      M3DLibraryDir (szaTemp, sizeof(szaTemp));
   MultiByteToWideChar (CP_ACP, 0, szaTemp, -1, szTemp, sizeof(szTemp) / sizeof(WCHAR));
   wcscat (szTemp, L"LibraryInstalled." LIBRARYFILEEXT);
   gaLibraryInstalled[dwRenderShard].Init (szTemp, L"LibraryInstalled", pProgress);
   if (pProgress) {
      pProgress->Pop();
      pProgress->Update (.5);
      pProgress->Push(.5,1);
   }

   // BUGFIX - get library location from registry instead, so get from location of 3dob's exe
   if (fAppDir)
      strcpy (szaTemp, gszAppDir);
   else
      M3DLibraryDir (szaTemp, sizeof(szaTemp));
   MultiByteToWideChar (CP_ACP, 0, szaTemp, -1, szTemp, sizeof(szTemp) / sizeof(WCHAR));
   wcscat (szTemp, L"LibraryUser." LIBRARYFILEEXT);
   gaLibraryUser[dwRenderShard].Init (szTemp, L"LibraryUser", pProgress);
   if (pProgress) {
      pProgress->Pop();
      pProgress->Update (1);
   }

#if 0
   // to test
   PWSTR apsz[2] = {gpszTextures, gpszEffects};
   PCLibrary pClone = gaLibraryInstalled[dwRenderShard].Clone (L"c:\\test.me3", apsz, 2);
   delete pClone;
#endif

   // fill in some categories
   gapLCTexturesInstalled[dwRenderShard] = gaLibraryInstalled[dwRenderShard].CategoryGet (gpszTextures);
   gapLCTexturesUser[dwRenderShard] = gaLibraryUser[dwRenderShard].CategoryGet (gpszTextures);
   gapLCEffectsInstalled[dwRenderShard] = gaLibraryInstalled[dwRenderShard].CategoryGet (gpszEffects);
   gapLCEffectsUser[dwRenderShard] = gaLibraryUser[dwRenderShard].CategoryGet (gpszEffects);

#if 0
   // if there aren't any elements, then fill in with defaults
   CListFixed l;
   gapLCTexturesInstalled[dwRenderShard]->EnumMajor (&l);
   if (!l.Num())
      HackTextureCreate();
#endif // 0

   // fill in the objects
   gapLCObjectsInstalled[dwRenderShard] = gaLibraryInstalled[dwRenderShard].CategoryGet (gpszObjects);
   gapLCObjectsUser[dwRenderShard] = gaLibraryUser[dwRenderShard].CategoryGet (gpszObjects);

#if 0
   // if there aren't any elements, then fill in with defaults
   CListFixed l;
   gapLCObjectsInstalled[dwRenderShard]->EnumMajor (&l);
   if (!l.Num())
      HackObjectsCreate();
#endif // 0
}

/************************************************************************************
LibrarySaveFiles - If the files are dirty, the library files are saved.

inputs
   BOOL              fAppDir - If TRUE then always open from the app's directory.
                              If FALSE, then look at the registry entry for
                              where the library's should be installed
   PCProgressSocket     pProgress - Progress bar
   BOOL                 fSkipInstalled - if TRUE then dont bother saving installed, even if changed
*/
void LibrarySaveFiles (DWORD dwRenderShard, BOOL fAppDir, PCProgressSocket pProgress, BOOL fSkipInstalled)
{
   // dont progress this since only gets saved for me
   if (pProgress)
      pProgress->Push (0,.5);

   if (!fSkipInstalled && gaLibraryInstalled[dwRenderShard].DirtyGet()) {
      gaLibraryInstalled[dwRenderShard].Commit(pProgress);
#if 0
      // when in debug mode always save full version so can recover
      gaLibraryInstalled[dwRenderShard].Commit(pProgress, TRUE);
#endif
   }

   if (pProgress) {
      pProgress->Pop();
      pProgress->Push (0,.5);
   }

   if (gaLibraryUser[dwRenderShard].DirtyGet()) {
      // BUGFIX - backup the previous version
      DWORD dwKey = (KeyGet ("UserBackup", (DWORD) 0) % 4);
      static BOOL fCopiedAlready = FALSE;
      char szTemp[256];
      if (!fCopiedAlready) {
         // BUGFIX - get library location from registry instead, so get from location of 3dob's exe
         char szLib[256];
         if (fAppDir)
            strcpy (szLib, gszAppDir);
         else
            M3DLibraryDir (szLib, sizeof(szLib));

         // BUGFIX - Only backup once per session
         KeySet ("UserBackup", dwKey+1);
         sprintf (szTemp, "%sLibraryUserBack%d." LIBRARYFILEEXTA, szLib, (int) dwKey);
         WideCharToMultiByte (CP_ACP, 0, gaLibraryUser[dwRenderShard].m_szFile, -1, szLib, sizeof(szLib), 0,0);
         CopyFile (szLib, szTemp, FALSE);

         fCopiedAlready = TRUE;
      }

      // BUGFIX - When put flag in for high compress or not temporarily turn it off
      // when saving user library to make it fast
      BOOL fCur;
      fCur = MMLCompressMaxGet();
      MMLCompressMaxSet (FALSE);
      gaLibraryUser[dwRenderShard].Commit(pProgress);
      MMLCompressMaxSet ( fCur);
   }

   if (pProgress) {
      pProgress->Pop();
      pProgress->Update (1);
   }
}


/********************************************************************************
LibraryCombineLists - The EnumXXX functions fill in a CListFixed with pointers
to strings. Since there will often be an installed and a user library, this will
be used to combine the two lists together.

inputs
   PCListFixed    lA - List A. Ultimately this one is modified.
   PCListFixed    lB - List B - combined into A.
returns
   none
*/
static int _cdecl LCLCompare (const void *elem1, const void *elem2)
{
   PWSTR *pdw1, *pdw2;
   pdw1 = (PWSTR*) elem1;
   pdw2 = (PWSTR*) elem2;

   // BUGFIX - Move non-alpha start to end
   BOOL fAlpha1, fAlpha2;
   fAlpha1 = iswalpha ((*pdw1)[0]);
   fAlpha2 = iswalpha ((*pdw2)[0]);
   if (fAlpha1 && !fAlpha2)
      return -1;
   else if (!fAlpha1 && fAlpha2)
      return 1;

   return _wcsicmp(*pdw1, *pdw2);
}

void LibraryCombineLists (PCListFixed lA, PCListFixed lB)
{
   // allocate temporary memory large enough for both
   CMem  mem;
   DWORD dwNum;
   dwNum = lA->Num() + lB->Num();
   if (!mem.Required(dwNum * sizeof(PWSTR)))
      return;
   PWSTR *ppsz;
   ppsz = (PWSTR*) mem.p;
   memcpy (ppsz, lA->Get(0), lA->Num() * sizeof(PWSTR));
   memcpy (ppsz + lA->Num(), lB->Get(0), lB->Num() * sizeof(PWSTR));

   // sort
   qsort (ppsz, dwNum, sizeof(PWSTR), LCLCompare);

   // eliminate duplicates
   DWORD dwSrc, dwCopy;
   for (dwSrc = 0, dwCopy = 0; dwSrc < dwNum; dwSrc++) {
      if (dwCopy && !_wcsicmp(ppsz[dwSrc], ppsz[dwCopy-1])) {
         // they're the same, so skip this
         continue;
      }

      // copy
      if (dwSrc != dwCopy)
         ppsz[dwCopy] = ppsz[dwSrc];
      dwCopy++;
   }

   // fill in
   lA->Init (sizeof(PWSTR), ppsz, dwCopy);
}

/********************************************************************************
LibraryUserClone - This creates a clone of the current user library with
ONLY its effects. Objects and textures are EXCLUDED. The clone can then be
passed into the M3DFileOpenRemoveUser() function.

returns
   PWSTR       pszFile - File name to save the clone as
   PCLibrary - New library. You must delete this when done
*/
PCLibrary LibraryUserClone (DWORD dwRenderShard, PWSTR pszFile)
{
   PWSTR apsz[1] = {gpszEffects};
   return gaLibraryUser[dwRenderShard].Clone (pszFile, apsz, 1);
}

// BUGBUG - when save away main (installed) library, want to compact it so that
// empty space is not wasted and installed only all users hard drives

// BUGBUG - may want to compact user library once in awhile
