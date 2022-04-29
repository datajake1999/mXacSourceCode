/**********************************************************************
CDatabase.cpp - Code for MML database used in Dragonfly.

begun 6/2/2000 by Mike Rozak
Copyright 2000 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <escarpment.h>
#include "dragonfly.h"

#define  MFH         0x3512f89
#define  DFH         0x13512f89
typedef struct {
   DWORD       dwID;    // always MFH
   DWORD       dwNum;   // number of elements
} MAINFILEHEADER, *PMAINFILEHEADER;

typedef struct {
   DWORD       dwID;    // always DFH
   DWORD       dwCheckSum; // checksum
} DATAFILEHEADER, *PDATAFILEHEADER;

/**********************************************************************
Constructor & destructor
*/
CDatabase::CDatabase (void)
{
   HANGFUNCIN;
   m_szDir[0] = 0;
   m_szPassword[0] = 0;
   m_szPrefix[0] = 0;
   m_dwNum = m_dwNumFlushed = 0;
   m_Cache.Init (sizeof(DBCACHE));
   m_dwTimer = 0;
}

CDatabase::~CDatabase (void)
{
   HANGFUNCIN;
   // make sure everything is written
   Flush(TRUE);   // will also kill the timer

   // free all the memory
   DWORD i;
   for (i = 0; i < m_Cache.Num(); i++) {
      PDBCACHE p = (PDBCACHE) m_Cache.Get(i);

#ifdef _DEBUG
      if (p->dwRefCount) {
         OutputDebugString ("CDatabase::~CDatabase - RefCount is not 0!\r\n");

         DWORD j;
         for (j = 0; j < NODESPERFILE; j++) {
            if (!p->adwNodeCount[j])
               continue;

            // note type
            PWSTR psz;
            psz = p->apNodes[j]->NameGet();
            char szTemp[256];
            szTemp[0] = 0;
            if (psz)
               WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp),0,0);
            OutputDebugString ("\tGuilty party:");
            OutputDebugString (szTemp);
            OutputDebugString ("\r\n");
         }
      }
#endif

      if (p->pRoot)
         delete p->pRoot;
   }
}


/**********************************************************************
InitOpen - Opens an existing database. This fails if the database
doesn't exist.

inputs
   PWSTR    pszDir - database directory
   PWSTR    pszPassword - password to use
   PWSTR    pszPrefix - Prefix for the main file, directories, and data files.
   int      *piError - Filled in eith error if failed. Can be NULL.
returns
   BOOL - TRUE if succede
*/
BOOL CDatabase::InitOpen (PWSTR pszDir, PWSTR pszPassword, PWSTR pszPrefix, int *piError)
{
   HANGFUNCIN;

   if (piError)
      *piError = 0;

   // fail if already have something
   if (m_szDir[0]) {
      if (piError)
         *piError = -1;
      return FALSE;
   }

   // try opening it
   wcscpy (m_szDir, pszDir);
   wcscpy (m_szPassword, pszPassword);
   wcscpy (m_szPrefix, pszPrefix);

   FILE  *f;
   WCHAR szTemp[256];
   char  szaTemp[256];
   swprintf (szTemp, gszMainDBFile, pszDir, pszPrefix);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   f = fopen(szaTemp, "rb");
   if (!f) {
      if (piError)
         _get_errno (piError);

      m_szDir[0] = 0;
      return FALSE;
   }

   MAINFILEHEADER mfh;
   memset (&mfh, 0, sizeof(mfh));
   fread (&mfh, 1, sizeof(mfh), f);
   fclose (f);

   // verify it's real
   if (mfh.dwID != MFH) {
      if (piError)
         *piError = -2;
      m_szDir[0] = 0;
      return FALSE;
   }

   // else, it's real
   m_dwNum = m_dwNumFlushed = mfh.dwNum;

   // init encryption
   if (!m_Encrypt.Init(m_szPassword)) {
      if (piError)
         *piError = -3;
      return FALSE;
   }

   return TRUE;
}


/**********************************************************************
InitCreate - Creates a database.

This also creates the first file (containing no nodes).

inputs
   PWSTR    pszDir - database directory. If the directory doesn't
            exist then it's created.
   PWSTR    pszPassword - password to use
   PWSTR    pszPrefix - Prefix for the main file, directories, and data files.
   BOOL     fFailIfExists - if TRUE and a database already exists in the
            directory then FAIL
retursn
   BOOL - fails on FALSE
*/
BOOL CDatabase::InitCreate (PWSTR pszDir, PWSTR pszPassword, PWSTR pszPrefix,
                          BOOL fFailIfExists)
{
   HANGFUNCIN;
   // fail if already have something
   if (m_szDir[0])
      return FALSE;

   // try opening it
   wcscpy (m_szDir, pszDir);
   wcscpy (m_szPassword, pszPassword);
   wcscpy (m_szPrefix, pszPrefix);

   // create the directory
   char  szaTemp[256];
   WideCharToMultiByte (CP_ACP, 0, pszDir, -1, szaTemp, sizeof(szaTemp), 0, 0);
   CreateDirectory(szaTemp, NULL);

   FILE  *f;
   WCHAR szTemp[256];
   swprintf (szTemp, gszMainDBFile, pszDir, pszPrefix);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   f = fopen(szaTemp, "rb");
   if (f) {
      // it exists already
      m_szDir[0] = 0;
      fclose (f);
      if (fFailIfExists)
         return FALSE;
   }

   f = fopen (szaTemp, "wb");
   if (!f) {
      m_szDir[0] = 0;
      return FALSE;
   }

   MAINFILEHEADER mfh;
   memset (&mfh, 0, sizeof(mfh));
   mfh.dwID = MFH;
   mfh.dwNum = 0;
   fwrite (&mfh, sizeof(mfh), 1, f);
   fclose (f);

   m_dwNum = m_dwNumFlushed = 0;
   swprintf (szTemp, gszDataSubDir, m_szDir, m_szPrefix, 0);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   CreateDirectory (szaTemp, NULL);

   // create the directory where the first file goes

   // init encryption
   if (!m_Encrypt.Init(m_szPassword))
      return FALSE;

   // create the first file
   DBCACHE  c;
   memset (&c, 0, sizeof(c));
   c.dwFile = 0;
   c.dwRefCount = 0;
   c.dwTimeRelease = GetTickCount();
   c.pRoot = new CMMLNode;
   if (!c.pRoot) {
      m_szDir[0] = 0;
      return FALSE;
   }
   c.pRoot->NameSet (gszRootName);
   c.pRoot->AttribSet (gszNumber, gszZero);
   c.pRoot->DirtySet();
   m_Cache.Add (&c);

   // flush it
   Flush(TRUE);

   return TRUE;
}


/**********************************************************************
DBTimerProc - Calls flush
*/
static PCDatabase gpDBFlushTimer = NULL;  // so know what to flush to
static void CALLBACK DBTimerProc (
  HWND hwnd,     // handle of window for timer messages
  UINT uMsg,     // WM_TIMER message
  UINT idEvent,  // timer identifier
  DWORD dwTime   // current system time
)
{
   HANGFUNCIN;
   gpDBFlushTimer->Flush(TRUE);
}

/**********************************************************************
Flush - Writes all dirty data in the database to disk. That way if the
system crashes it will be written out.

This also writes the main database file containing the number of elements.

inputs
   BOOL  fForce - If TRUE forces an immediate flush. If FALSE will flush shortly
            Defaults to FALSE.
returns
   BOOL - TRUE if succede. Might fail if not enough disk space
*/
BOOL CDatabase::Flush (BOOL fForce)
{
   HANGFUNCIN;
   // make sure all timers are killed
   if (m_dwTimer) {
      KillTimer (0, m_dwTimer);
      m_dwTimer = 0;
      gpDBFlushTimer = NULL;
   }

   // BUGFIX - If not doing a foced flush then just wait a second or so
   if (!fForce && !gpDBFlushTimer) {
      gpDBFlushTimer = this;
      m_dwTimer = SetTimer (0, 0, 1000, DBTimerProc);
#ifdef _DEBUG
      OutputDebugString ("Flush - fake\r\n");
#endif
      return TRUE;
   }

   // else, actually write it
#ifdef _DEBUG
      OutputDebugString ("Flush - WRITE\r\n");
#endif

   // if main database file ditry then write it
   if (m_dwNum != m_dwNumFlushed) {
      FILE  *f;
      char szaTemp[256];
      WCHAR szTemp[256];
      swprintf (szTemp, gszMainDBFile,m_szDir, m_szPrefix);
      WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
      f = fopen (szaTemp, "wb");
      if (!f) {
         m_szDir[0] = 0;
         return FALSE;
      }

      MAINFILEHEADER mfh;
      memset (&mfh, 0, sizeof(mfh));
      mfh.dwID = MFH;
      mfh.dwNum = m_dwNum;
      fwrite (&mfh, sizeof(mfh), 1, f);
      fclose (f);

      m_dwNumFlushed = m_dwNum;
   }

   // loop through everything cached
   DWORD i;
   for (i = 0; i < m_Cache.Num(); i++) {
      PDBCACHE p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot) {
         if (!FileFlush (p->pRoot))
            return FALSE;
      }
   }

   return TRUE;
}

/**********************************************************************
VerifyPassword - Verify that the password works by reading the file 0
and seeing if can decompress

returns
   BOOL - TRUE if OK. FALSE if fail
*/
BOOL CDatabase::VerifyPassword (void)
{
   HANGFUNCIN;
   // cache file 0
   PCMMLNode   pNode;
   if (!(pNode = FileCache (0, FALSE)))
      return FALSE;

   return FileRelease (pNode);
}


/**********************************************************************
EmptyUnused - Find any files that have been completely released for
more than 10 minutes and close them.

returns
   BOOL - TRUE if succede
*/
BOOL CDatabase::EmptyUnused (void)
{
   HANGFUNCIN;
   DWORD i;
   DWORD dwTime = GetTickCount();

   for (i = m_Cache.Num()-1; i < m_Cache.Num(); i--) {
      PDBCACHE p = (PDBCACHE) m_Cache.Get(i);

      // if there's a refcount skip
      if (p->dwRefCount)
         continue;

      // if it hasn't been 10 minutes then skip
      if (p->dwTimeRelease + (1000*60*10) >= dwTime)
         continue;

      // else, free it up

      // first make sure it's saved to disk
      if (!FileFlush (p->pRoot))
         return FALSE;

      // then, delete it from the list
      if (p->pRoot)
         delete p->pRoot;
      m_Cache.Remove(i);
   }

   return TRUE;
}


/**********************************************************************
Num - Returns the number of elements in the database

returns
   DWORD - number
*/
DWORD CDatabase::Num (void)
{
   return m_dwNum;
}


/**********************************************************************
Dir - Returns a pointer to the database directory. This is the full
path.

returns
   PWSTR - Directory. Valid while the database is valid.
*/
PWSTR CDatabase::Dir (void)
{
   return m_szDir;
}


/**********************************************************************
DirParent - Fills a string (make sure it's fairly large) in with the
database's parent directory.

inputs
   PWSTR - Filled in with the directory
*/
void CDatabase::DirParent (PWSTR psz)
{
   HANGFUNCIN;
   wcscpy (psz, m_szDir);

   // find the last '\'
   WCHAR *pszLast, *pCur;
   pszLast = NULL;
   for (pCur = psz; *pCur; pCur++)
      if (*pCur == L'\\')
         pszLast = pCur;

   if (pszLast)
      *pszLast = 0;
}

/**********************************************************************
DirSub - Fills a string (make sure it's fairly large) in with the
database's subdirector (within DirParent)

inputs
   PWSTR - Filled in with the directory
*/
void CDatabase::DirSub (PWSTR psz)
{
   HANGFUNCIN;
   // find the last '\'
   WCHAR *pszLast, *pCur;
   pszLast = NULL;
   for (pCur = m_szDir; *pCur; pCur++)
      if (*pCur == L'\\')
         pszLast = pCur;

   if (pszLast)
      wcscpy (psz, pszLast + 1);
   else
      wcscpy (psz, m_szDir);
}

/**********************************************************************
NodeGet - Gets an element from the database.

inputs
   DWORD    dwNode - Node number, from 0 and up. Node 0 is the main
            node that points to other nodes
returns
   PCMMLNode - Node. You must call NodeRelease() on this.
*/
PCMMLNode CDatabase::NodeGet (DWORD dwNode)
{
   HANGFUNCIN;
   if (dwNode >= m_dwNum)
      return NULL;

   // first, get the file
   PCMMLNode   pRoot;
   pRoot = FileCache (dwNode / NODESPERFILE, FALSE);
   if (!pRoot)
      return NULL;

   // find it in the list of cached items
   DWORD i;
   for (i = 0; i < m_Cache.Num(); i++) {
      PDBCACHE p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot != pRoot)
         continue;

      // see if it's in the list
      DWORD dwIndex;
      dwIndex = dwNode % NODESPERFILE;
      if (!p->apNodes[dwIndex]) {
         // not there
         FileRelease (pRoot);
         return NULL;
      }

      // else found it, so incrememnt that count so don't delete
      p->adwNodeCount[dwIndex]++;
      return p->apNodes[dwIndex];
   }

   // not found
   FileRelease (pRoot);
   return NULL;
}


/**********************************************************************
NodeRelease - Release a node from NodeGet. This works because each
pCMMLNode returned by NodeGet contains an attribute, "number=XXX" (do not
touch this). From the node number can determine what file it's in.

inputs
   PCMMLNode - node
returns
   BOOL - TRUE if ok
*/
BOOL CDatabase::NodeRelease (PCMMLNode pNode)
{
   HANGFUNCIN;
   // find its parent
   PCMMLNode   pParent;
   pParent = pNode;
   while (pParent->m_pParent)
      pParent = pParent->m_pParent;

   // look for that to release
   DWORD i;
   for (i = 0; i < m_Cache.Num(); i++) {
      PDBCACHE p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot != pParent)
         continue;

      // find this node in the list
      int   iNum;
      iNum = 0;
      AttribToDecimal (pNode->AttribGet(gszNumber), &iNum);
      DWORD dwIndex;
      dwIndex = (DWORD) iNum % NODESPERFILE;
      if ((p->apNodes[dwIndex] != pNode) || !p->adwNodeCount[dwIndex])
         return FALSE;  // error

      // decrease the count
      p->adwNodeCount[dwIndex]--;
      return FileRelease (pParent);
   }

   // if get here couldn't find. problem
   return FALSE;
}

/**********************************************************************
NodeAdd - Adds a node.

inputs
   PWSTR pszType - Used as the node name
   DWORD *pdwNode - Node number. Use for nodeGet.
returns
   PCMMLNode - Node. You must call NodeRelease() on this.
*/
PCMMLNode CDatabase::NodeAdd (PWSTR pszType, DWORD *pdwNode)
{
   HANGFUNCIN;
   // what number will we want to add?
   DWORD dwAdd = m_dwNum;

   // get the root file
   PCMMLNode   pRoot;
   pRoot = FileCache (dwAdd / NODESPERFILE, TRUE);
   if (!pRoot)
      return NULL;

   // find the database
   DWORD i;
   PDBCACHE p;
   for (i = 0; i < m_Cache.Num(); i++) {
      p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot == pRoot)
         break;
   }
   if (i >= m_Cache.Num()) {
      FileRelease (pRoot);
      return NULL;   // unexpected error
   }

   // create the new node
   DWORD dwIndex;
   dwIndex = dwAdd % NODESPERFILE;
   if (!p->apNodes[dwIndex]) {
      p->apNodes[dwIndex] = pRoot->ContentAddNewNode ();
   }
   if (!p->apNodes[dwIndex]) {
      FileRelease(pRoot);
      return NULL;   // couldn't add
   }
   p->adwNodeCount[dwIndex]++;

   // wipe out all the data in there. There shouldn't be any but if
   // the file was somehow corrupt there might be
   PCMMLNode   pNew;
   pNew = p->apNodes[dwIndex];
   while (pNew->ContentNum())
      pNew->ContentRemove (0);

   // set the info
   pNew->NameSet (pszType);
   WCHAR szTemp[16];
   _itow (dwAdd, szTemp, 10);
   pNew->AttribSet (gszNumber, szTemp);

   // remember that have incremented
   m_dwNum++;

   // if node then set
   if (pdwNode)
      *pdwNode = dwAdd;

   // done
   return pNew;
}


/**********************************************************************
NodeDelete - Deletes a node.

inputs
   DWORD dwNode - node number
returns  
   BOOL - TRUE if OK. FALSE if fail. Fails if the node is addrefed and
      in use elsewhere.
*/
BOOL CDatabase::NodeDelete (DWORD dwNode)
{
   HANGFUNCIN;
   // if node is greater than # nodes then error
   if (dwNode >= m_dwNum)
      return FALSE;

   // cache the file
   PCMMLNode   pRoot;
   pRoot = FileCache (dwNode / NODESPERFILE, FALSE);
   if (!pRoot)
      return FALSE;

   // find the node and make sure its not in use elsewhere
   DWORD i;
   PDBCACHE p;
   for (i = 0; i < m_Cache.Num(); i++) {
      p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot == pRoot)
         break;
   }
   if (i >= m_Cache.Num()) {
      FileRelease (pRoot);
      return NULL;   // unexpected error
   }

   DWORD dwIndex;
   dwIndex = dwNode % NODESPERFILE;
   if (p->adwNodeCount[dwIndex]) {
      FileRelease (pRoot);
      return FALSE;  // cached elswhere
   }

   // if it doesn't exist then error
   if (!p->apNodes[dwIndex]) {
      FileRelease (pRoot);
      return FALSE;  // cached elswhere
   }

   // else delete it
   p->apNodes[dwIndex] = 0;
   WCHAR szTemp[16];
   _itow (dwNode, szTemp, 10);

   DWORD dwFind;
   dwFind = pRoot->ContentFind (NULL, gszNumber, szTemp);
   if (dwFind == (DWORD) -1) {
      FileRelease (pRoot);
      return FALSE;  // error
   }

   // dlete
   pRoot->ContentRemove (dwFind);

   return FileRelease (pRoot);
}


/**********************************************************************
DirNode - Fills in a string with and UNICODE version of the directory
where the given node appears.

inputs
   DWORD       dwNode - Node number
   PWSTR       pszFile - Filled in with the file name. Should be about 256 chars
returns
   none
*/
void CDatabase::DirNode (DWORD dwNode, PWSTR pszFile)
{
   HANGFUNCIN;
   DWORD dwNum = dwNode / NODESPERFILE;
   swprintf (pszFile, L"%s\\%s%d", m_szDir, m_szPrefix, dwNum / FILESPERDIR);
}

/**********************************************************************
FileCache - Internal function. Caches a specific file number.

inputs
   DWORD    dwNum - file number. 100 nodes per file
   BOOL     fCreate - if TRUE and it doesn't exist, then creates it.
            Ele error if doesn't exist. If the directory that the file
            is to be in doesn't exist then it's created.
returns
   PCMMLNode - Root for the file. Must release with FileRelease()
*/
PCMMLNode CDatabase::FileCache (DWORD dwNum, BOOL fCreate)
{
   HANGFUNCIN;
   // see if the file is alreday loaded
   DWORD i;
   PDBCACHE p;
   for (i = 0; i < m_Cache.Num(); i++) {
      p = (PDBCACHE) m_Cache.Get(i);

      if (p->dwFile == dwNum) {
         // found it
         p->dwRefCount++;
         return p->pRoot;
      }
   }

   // not cached. Empty unsused cache items
   EmptyUnused ();

   // else, open it
   WCHAR szTemp[256];
   char  szaTemp[256];
   swprintf (szTemp, gszFileName, m_szDir, m_szPrefix, dwNum / FILESPERDIR,
      m_szPrefix, dwNum);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   FILE  *f;
   f = fopen(szaTemp, "rb");
   if (!f) {
      // if not create then return null
      if (!fCreate)
         return NULL;

      // make something up
      DBCACHE  c;
      memset (&c, 0, sizeof(c));
      c.dwFile = dwNum;
      c.dwRefCount = 1;
      c.pRoot = new CMMLNode;
      if (!c.pRoot) {
         return NULL;
      }
      c.pRoot->NameSet (gszRootName);
      WCHAR szTemp[16];
      _itow (dwNum, szTemp, 10);
      c.pRoot->AttribSet (gszNumber, szTemp);
      c.pRoot->DirtySet();
      m_Cache.Add (&c);

      // flush??? DOn't know.
      return c.pRoot;
   }

   // allocate enough memory for the entire file
   DWORD dwSize;
   fseek (f, 0, SEEK_END);
   dwSize = (DWORD) ftell(f);
   PBYTE pb;
   pb = (PBYTE) malloc (dwSize);
   if (!pb) {
      fclose (f);
      return FALSE;
   }

   // read it in
   fseek (f, 0, 0);
   if (dwSize != fread (pb, 1, dwSize, f)) {
      fclose (f);
      return FALSE;
   }
   // done
   fclose (f);

   // verify its what it says it is
   PDATAFILEHEADER ph;
   ph = (PDATAFILEHEADER) pb;
   if ((dwSize < sizeof(DATAFILEHEADER)) || (ph->dwID != DFH)) {
      free (pb);
      return NULL;
   }

   // decrypt it
   if (!m_Encrypt.Decrypt (pb + sizeof(*ph), dwSize - sizeof(*ph), ph->dwCheckSum)) {
      // couldn't decrypt
      free (pb);
      return NULL;
   }

   // load as a node
   PCMMLNode   pNew;
   CEscError   err;
   pNew = ParseMML ((PWSTR) (pb + sizeof(*ph)), ghInstance, NULL, NULL, &err, FALSE);
   if (!pNew) {
      // eror parsing
      free (pb);
      return NULL;
   }

   // it's not dirty though
   pNew->m_fDirty = FALSE;

   // else add
   DBCACHE  c;
   memset (&c, 0, sizeof(c));
   c.dwFile = dwNum;
   c.dwRefCount = 1;
   c.pRoot = pNew;
   // WCHAR szTemp[16];
   _itow (dwNum, szTemp, 10);
   pNew->AttribSet (gszNumber, szTemp);

   // loop through all the contents and figure out which nodes have data
   //DWORD i;
   PCMMLNode   pSub;
   PWSTR psz;
   for (i = 0; i < pNew->ContentNum(); i++) {
      pSub = NULL;
      pNew->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;

      // get the number
      int   iNum;
      if (!AttribToDecimal (pSub->AttribGet(gszNumber), &iNum))
         continue;   // this shouldn't happen
      if ((DWORD)iNum / NODESPERFILE !=dwNum)
         continue;   // shouldn't happen
      DWORD dwIndex;
      dwIndex = (DWORD)iNum % NODESPERFILE;

      c.apNodes[dwIndex] = pSub;
   }

   m_Cache.Add (&c);

   // free memory
   free (pb);


   // not dirty
   pNew->m_fDirty = FALSE;

   return pNew;
}

/**********************************************************************
FileRelease - Internal function. Release the specific file number.
   The root attribute of the file node contains "number=XXX" with file
   number.

inputs
   PCMMLNode - Root for the file.
returns
   BOOL - TRUE if successful
*/
BOOL CDatabase::FileRelease (PCMMLNode pNode)
{
   HANGFUNCIN;
   // find the file in the cache
   DWORD i;
   PDBCACHE p;
   for (i = 0; i < m_Cache.Num(); i++) {
      p = (PDBCACHE) m_Cache.Get(i);

      if (p->pRoot == pNode)
         break;
   }
   if (i >= m_Cache.Num())
      return FALSE;  // can't find

   // decrease the reference count
   if (p->dwRefCount)
      p->dwRefCount--;
   if (!p->dwRefCount) {
      p->dwTimeRelease = GetTickCount();

      // flush this also
      //FileFlush (pNode);

      // BUGFIX - Set a timer to flush eventually
      Flush();
   }

   return TRUE;
}


/**********************************************************************
MMLToMem - Writes the current MML node to memory. Recursive.

inputs
   PCMMLNode      pNode - node
   PCMem          pMem - memory object. Written to at the current location
   BOOL           fSkipTag - If TRUE don't write tag start/end (only do this for main node)
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL MMLToMem (PCMMLNode pNode, PCMem pMem, BOOL fSkipTag = FALSE)
{
   HANGFUNCIN;
   DWORD i;
   DWORD dwRequired;
   size_t dwNeeded;

   // BUGFIX - If we're close to filling up the memory then add more.
   // Use large chunks because these files are big
   if (pMem->m_dwCurPosn + 1024 > pMem->m_dwAllocated)
      pMem->Required (pMem->m_dwAllocated + 10240);

   if (!fSkipTag) {
      // write the intro tag
      pMem->StrCat (L"<");
      pMem->StrCat (pNode->NameGet());

      // attributes?
      for (i = 0; i < pNode->AttribNum(); i++) {
         PWSTR pszName, pszValue;
         if (!pNode->AttribEnum(i, &pszName, &pszValue))
            continue;   // not supposed to happen

         // write it out
         pMem->StrCat (L" ");
         pMem->StrCat (pszName);
         pMem->StrCat (L"=\"");

         // IMPORTANT: This doesnt do macros <!xMacro> or <?Questions?>

         // make sure large enough for string
         dwRequired = wcslen(pszValue)*sizeof(WCHAR) * 2;   // just make sure large enough
   tryagain:
         if (!pMem->Required (dwRequired + pMem->m_dwCurPosn))
            return FALSE;  // error
         dwNeeded = 0;
         if (!StringToMMLString (pszValue, (WCHAR*) ((PBYTE)pMem->p + pMem->m_dwCurPosn),
            pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

            // need more
            dwRequired = dwNeeded;
            if (dwNeeded)
               goto tryagain;
         }
         pMem->m_dwCurPosn += wcslen((PWSTR) ((PBYTE) pMem->p + pMem->m_dwCurPosn))*2;

         // ending quote
         pMem->StrCat (L"\"");
      }

      // if there are no contents then end it here
      if (!pNode->ContentNum()) {
         pMem->StrCat (L"/>");
         return TRUE;
      }

      pMem->StrCat (L">");

   }  // fskiptag

   // loop through the contents
   for (i = 0; i < pNode->ContentNum(); i++) {
      PWSTR psz;
      PCMMLNode   pSub;

      if (!pNode->ContentEnum(i, &psz, &pSub))
         continue;   // shouldnt happen

      if (pSub) {
         if (!MMLToMem (pSub, pMem))
            return FALSE;
         continue;
      }

      // else string
      if (!psz[0])
         continue;   // empty

      // convert
      // make sure large enough for string
      dwRequired = wcslen(psz)*sizeof(WCHAR) * 2;   // just make sure large enough
tryagain2:
      if (!pMem->Required (dwRequired + pMem->m_dwCurPosn))
         return FALSE;  // error
      dwNeeded = 0;
      if (!StringToMMLString (psz, (WCHAR*) ((PBYTE)pMem->p + pMem->m_dwCurPosn),
         pMem->m_dwAllocated - pMem->m_dwCurPosn, &dwNeeded)) {

         // need more
         dwRequired = dwNeeded;
         if (dwNeeded)
            goto tryagain2;
      }
      pMem->m_dwCurPosn += wcslen((PWSTR) ((PBYTE) pMem->p + pMem->m_dwCurPosn))*2;

   }

   if (!fSkipTag) {
      // end it
      pMem->StrCat (L"</");
      pMem->StrCat (pNode->NameGet());
      pMem->StrCat (L">");
   }

   return TRUE;
}

/**********************************************************************
FileFlush - Writes an individual file to disk (if it's dirty).

inputs
   PCMMLNode - Root for the file.
returns
   BOOL - TRUE if successful
*/
BOOL CDatabase::FileFlush (PCMMLNode pNode)
{
   HANGFUNCIN;
   // if it's not dirty then ignore
   if (!pNode->m_fDirty)
      return TRUE;

   // get the file number from the header
   DWORD dwNum;
   if (!AttribToDecimal (pNode->AttribGet(gszNumber), (int*) &dwNum))
      return FALSE; // no number

   // write to memory
   CMem  mem;
   if (!MMLToMem (pNode, &mem, TRUE))
      return FALSE;
   mem.CharCat (0);

   // encode it
   DATAFILEHEADER df;
   memset (&df, 0, sizeof(df));
   df.dwID = DFH;
   df.dwCheckSum = m_Encrypt.Encrypt ((PBYTE) mem.p, mem.m_dwCurPosn);

   // write it out
   FILE  *f;
   WCHAR szTemp[256];
   char  szaTemp[256];
   swprintf (szTemp, gszFileName, m_szDir, m_szPrefix, dwNum / FILESPERDIR,
      m_szPrefix, dwNum);
   WideCharToMultiByte (CP_ACP, 0, szTemp, -1, szaTemp, sizeof(szaTemp), 0, 0);
   f = fopen(szaTemp, "wb");
   if (!f) {
      // maybe create the directory?
      WCHAR szTemp2[256];
      char  szaTemp2[256];
      swprintf (szTemp2, gszDataSubDir, m_szDir, m_szPrefix, dwNum / FILESPERDIR);
      WideCharToMultiByte (CP_ACP, 0, szTemp2, -1, szaTemp2, sizeof(szaTemp2), 0, 0); // BUGFIX - Was create directory szaTemp
      CreateDirectory (szaTemp2, NULL);

      f = fopen(szaTemp, "wb");
   }
   if (!f)
      return FALSE;  // error

   // write it out
   if (sizeof(df) != fwrite (&df, 1, sizeof(df), f)) {
      fclose (f);
      return FALSE;
   }
   if (mem.m_dwCurPosn != fwrite(mem.p, 1, mem.m_dwCurPosn, f)) {
      fclose (f);
      return FALSE;
   }

   fclose (f);

   // dirty flag to false
   pNode->m_fDirty = FALSE;

   return TRUE;
}


/**********************************************************************8
ChangePassword - Change the password of all the files.

inputs
   PWSTR    pszNew - New password
returns
   BOOL - TRUE if OK
*/
BOOL CDatabase::ChangePassword (PWSTR pszNew)
{
   HANGFUNCIN;
   // store the old passowrd away
   WCHAR    szOld[256];
   wcscpy (szOld, m_szPassword);

   // go through all the files
   DWORD i;
   for (i = 0; i < m_dwNum; i += NODESPERFILE) {
      // stick in the old password
      wcscpy (m_szPassword, szOld);
      m_Encrypt.Init(m_szPassword);

      // load it in
      PCMMLNode pNode;
      pNode = FileCache (i / NODESPERFILE, FALSE);
      if (!pNode)
         continue;

      // change password
      wcscpy (m_szPassword, pszNew);
      m_Encrypt.Init(m_szPassword);

      // set dirty
      pNode->m_fDirty = TRUE;

      // write it out with new password
      FileFlush (pNode);

      // uncache
      FileRelease (pNode);
   }

   // done.

   // just to make sure, set the new passowrd
   wcscpy (m_szPassword, pszNew);
   m_Encrypt.Init(m_szPassword);

   return TRUE;
}



/********************************************************************************
BackupDirectory -  Internal function used to backup all the contents of one
directory to another, including subdirectories

inputs
   char     *pszFrom - source
   char     *pszTo - Destination
returns
   BOOL - FALSE if error
*/
BOOL BackupDirectory (char *pszFrom, char *pszTo)
{
   HANGFUNCIN;
   char  szFromAll[256];
   strcpy (szFromAll, pszFrom);
   strcat (szFromAll, "\\*.*");

   HANDLE hFind;
   WIN32_FIND_DATA fd;
   memset (&fd,0,sizeof(fd));
   hFind = FindFirstFile (szFromAll, &fd);
   if (hFind == INVALID_HANDLE_VALUE)
      return TRUE;
   while (TRUE) {
      // if . or .. skip
      if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
         goto nextfile;

      // see what it is
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
         // subdirectorys
         char  szFromSub[256], szToSub[256];
         sprintf (szFromSub, "%s\\%s", pszFrom, fd.cFileName);
         sprintf (szToSub, "%s\\%s", pszTo, fd.cFileName);

         CreateDirectory (szToSub, NULL);

         // go on
         if (!BackupDirectory (szFromSub, szToSub)) {
            FindClose (hFind);
            return FALSE;
         }
         // continue
      }
      else {
         // it's a file, copy
         char  szFromSub[256], szToSub[256];
         sprintf (szFromSub, "%s\\%s", pszFrom, fd.cFileName);
         sprintf (szToSub, "%s\\%s", pszTo, fd.cFileName);
         
         // get stats on the one to copy to
         HFILE hFile;
         OFSTRUCT of;
         memset (&of, 0, sizeof(of));
         of.cBytes = sizeof(of);
         hFile = OpenFile (szToSub, &of, OF_READ | OF_SHARE_DENY_NONE);
         if (hFile != HFILE_ERROR) {
            FILETIME write;
            DWORD dwHigh, dwLow;
            GetFileTime ((HANDLE) hFile, NULL, NULL, &write);
            dwLow = GetFileSize ((HANDLE)hFile, &dwHigh);
            CloseHandle ((HANDLE) hFile);

            // if they're the same time/size then continue
            if (!CompareFileTime (&fd.ftLastWriteTime, &write) &&
               (dwHigh == fd.nFileSizeHigh) && (dwLow == fd.nFileSizeLow))
               goto nextfile;

         }
         // else, just copy it

#ifdef _DEBUG
         OutputDebugString ("Copy ");
         OutputDebugString (szFromSub);
         OutputDebugString ("\r\n");
#endif

         if (!CopyFile (szFromSub, szToSub, FALSE)) {
            FindClose (hFind);
            return FALSE;
         }
      }
   
nextfile:
      // go ontot he next one
      memset (&fd, 0, sizeof(fd));
      if (!FindNextFile (hFind, &fd)) {
         // cant find
         break;
      }
   }
   FindClose (hFind);
   return TRUE;
}


/********************************************************************************
Backup - Copies the entire database to a new directory, including any
   images stored there. Important: Before copying a file it checks to see if
   one exists already; if so, and if the size and times are the same, it doesn't
   update them.

inputs
   PWSTR    pszBackupTo - Directy to backup to, such as "c:\mike"
returns
   BOOL - TRUE if succssful, FALSE if error (like HD full)
*/
BOOL CDatabase::Backup (PWSTR pszBackupTo)
{
   HANGFUNCIN;
   // flish
   Flush(TRUE);

   char szBackupTo[256], szFrom[256];
   WideCharToMultiByte (CP_ACP, 0, pszBackupTo, -1, szBackupTo, sizeof(szBackupTo), 0, 0);
   WideCharToMultiByte (CP_ACP, 0, m_szDir, -1, szFrom, sizeof(szFrom), 0, 0);

   CreateDirectory(szBackupTo, NULL);

   return BackupDirectory (szFrom, szBackupTo);
}




// BUGBUG - 2.0 - need to monitor free disk space and warn user, because otherwise
// flushing won't quite work

