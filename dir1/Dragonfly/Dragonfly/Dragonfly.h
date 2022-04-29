/* dragoonfly header */

#ifndef _DRAGONFLY_H_
#define _DRAGONFLY_H_

#include <escarpment.h>

// #define HANGDEBUG // BUGBUG - for testing
#ifdef HANGDEBUG

#define HANGFUNCIN {WCHAR szLine[256], szWFile[256], szWFunc[256]; MultiByteToWideChar (CP_ACP, 0, __FILE__, -1, szWFile, sizeof(szWFile)/sizeof(WCHAR)); MultiByteToWideChar (CP_ACP, 0, __FUNCTION__, -1, szWFunc, sizeof(szWFunc)/sizeof(WCHAR)); swprintf (szLine, L"\r\nFuncIn %s %s %d", szWFunc, szWFile, (int)__LINE__); EscOutputDebugString (szLine); }
#define HANGFUNCOUT {WCHAR szLine[256], szWFile[256], szWFunc[256]; MultiByteToWideChar (CP_ACP, 0, __FILE__, -1, szWFile, sizeof(szWFile)/sizeof(WCHAR)); MultiByteToWideChar (CP_ACP, 0, __FUNCTION__, -1, szWFunc, sizeof(szWFunc)/sizeof(WCHAR)); swprintf (szLine, L"\r\nFuncOut %s %s %d", szWFunc, szWFile, (int)__LINE__); EscOutputDebugString (szLine); }

#else

#define HANGFUNCIN   // do nothing
#define HANGFUNCOUT  // do nothing

#endif // HANGDEBUG

typedef DWORD DFDATE;
typedef DWORD DFTIME;

// reoccurance structure - for storing reoccurance info
typedef struct {
   DFDATE      m_dwLastDate;  // last date this occurred
   DFDATE      m_dwEndDate;     // if not 0, then indicates that reocurring task ends
   DWORD       m_dwPeriod;    // perdiod of reoccuranc.
                              // 0 => no reoccurance
#define REOCCUR_EVERYNDAYS    1
#define REOCCUR_EVERYWEEKDAY  2
#define REOCCUR_WEEKLY        3
#define REOCCUR_MONTHDAY      4
#define REOCCUR_MONTHRELATIVE 5
#define REOCCUR_YEARDAY       6
#define REOCCUR_YEARRELATIVE  7
#define REOCCUR_NWEEKLY       8
   DWORD       m_dwParam1;    // meaning depends on m_dwPeriod
   DWORD       m_dwParam2;    // meaning depends on m_dwPeriod
   DWORD       m_dwParam3;    // meaning depends on m_dwPeriod
} REOCCURANCE, *PREOCCURANCE;


// Planner item
typedef struct {
   PCMem       pMemMML;       // pointer to a memocy object containing MML to display. Must be freed
   DWORD       dwType;        // 0=meeting,1=phonecall,2=task,3=projec task,4=break
   DWORD       dwMajor, dwMinor, dwSplit; // identification numbers
   BOOL        fFixedTime;    // TRUE if it's a fixed duration
   DFTIME      dwTime;        // if fFixedTime then the start time, else earliest time (0 for any time)
   DWORD       dwDuration;    // # of minutes
   DWORD       dwDuration2;   // used if split.
   DWORD       dwPriority;    // priority for order of appearance. Not done for fixed times.
   DFDATE      dwNextDate;    // if split task, this is the date/time when the next chunk happens
   DFDATE      dwPreviousDate;// if split task, this is the date/time when the previous chunk happens, -1 => continued from before but not sure when
} PLANNERITEM, *PPLANNERITEM;

/****************************************************************************
Misc */
#define TODFDATE(day,month,year) ((DFDATE) ((DWORD)(day) + ((DWORD)(month)<<8) + ((DWORD)(year)<<16) ))
#define YEARFROMDFDATE(df) ((DWORD)(df) >> 16)
#define MONTHFROMDFDATE(df) (((DWORD)(df) >> 8) & 0xff)
#define DAYFROMDFDATE(df) ((DWORD)(df) & 0xff)

#define TODFTIME(hour,minute) ( ((DWORD)((hour) & 0xff)<<8) + (DWORD)((minute)&0xff) )
#define HOURFROMDFTIME(df) ( ((df) >= 0x1800) ? (DWORD)-1 : ((DWORD)(df) >> 8) )
#define MINUTEFROMDFTIME(df) ( ((df) >= 0x1800) ? (DWORD)-1 : ((DWORD)(df) & 0xff) )

/****************************************************************************
Strings
*/
#define  MAXPEOPLE      4

extern WCHAR gszMainDBFile[];
extern WCHAR gszRootName[];
extern WCHAR gszNumber[];
extern WCHAR gszZero[];
extern WCHAR gszDataSubDir[];
extern WCHAR gszFileName[];
extern WCHAR gszNext[];
extern WCHAR gszBack[];
extern WCHAR gszRedoSamePage[];
extern WCHAR gszName[];
extern WCHAR gszText[];
extern WCHAR gszDirectory[];
extern WCHAR gszDatabasePrefix[];
extern WCHAR gszNodeZero[];
extern WCHAR gszOutOfMemory[];
extern WCHAR gszWriteError[];
extern WCHAR gszWriteErrorSmall[];
extern WCHAR gszRegUsers[];
extern char  gszRegUserDir[];
extern WCHAR gszUser[];
extern WCHAR gszCurSel[];
extern WCHAR gszStartingURL[];
extern WCHAR gszPrint[];
extern WCHAR gszNotYetImplimented[];
extern WCHAR gszPeopleNode[];
extern WCHAR gszProjectListNode[];
extern WCHAR gszReminderListNode[];
extern WCHAR gszProjectNode[];
extern WCHAR gszPersonNode[];
extern WCHAR gszBusinessNode[];
extern WCHAR gszPersonBusinessNode[];
extern WCHAR gszPercentD[];
extern WCHAR gszDescription[];
extern WCHAR gszSummary[];
extern WCHAR gszDaysPerWeek[];
extern WCHAR gszSubProject[];
extern WCHAR gszDuration[];
extern WCHAR gszID[];
extern WCHAR gszDateCompleted[];
extern WCHAR gszDateBefore[];
extern WCHAR gszDateAfter[];
extern WCHAR gszIDBefore[];
extern WCHAR gszIDAfter[];
extern WCHAR gszDaysBefore[];
extern WCHAR gszDaysAfter[];
extern WCHAR gszOK[];
extern WCHAR gszCancel[];
extern WCHAR gszProjectTask[];
extern WCHAR gszSubProject[];
extern WCHAR gszNextTaskID[];
extern WCHAR gszTask[];
extern WCHAR gszReminderNode[];
extern WCHAR gszWorkListNode[];
extern WCHAR gszEventListNode[];
extern WCHAR gszSchedListNode[];
extern WCHAR gszDateShow[];
extern WCHAR gszPriority[];
extern WCHAR gszEstimated[];
extern WCHAR gszReoccurPeriod[];
extern WCHAR gszReoccurLastDate[];
extern WCHAR gszReoccurEndDate[];
extern WCHAR gszReoccurParam1[];
extern WCHAR gszReoccurParam2[];
extern WCHAR gszReoccurParam3[];
extern WCHAR gszEveryNDays[];
extern WCHAR gszChecked[];
extern WCHAR gszEveryNDaysEdit[];
extern WCHAR gszEveryWeekday[];
extern WCHAR gszStartOn[];
extern WCHAR gszEndOn[];
extern WCHAR gszWeekly[];  
extern WCHAR gszWeeklySun[];
extern WCHAR gszNWeekly[];
extern WCHAR gszNWeeklyOther[];
extern WCHAR gszNWeeklyDOW[];
extern WCHAR gszWeeklyMon[];
extern WCHAR gszWeeklyTues[];
extern WCHAR gszWeeklyWed[];
extern WCHAR gszWeeklyThurs[];
extern WCHAR gszWeeklyFri[];
extern WCHAR gszWeeklySat[];
extern WCHAR gszMonthDay[];
extern WCHAR gszMonthDayEdit[];
extern WCHAR gszMonthRelative[];
extern WCHAR gszMonthRelativeWeek[];
extern WCHAR gszMonthRelativeOf[];
extern WCHAR gszYearDay[];
extern WCHAR gszYearMonth[];
extern WCHAR gszYearDayEdit[];
extern WCHAR gszYearRelative[];
extern WCHAR gszYearRelativeWeek[];
extern WCHAR gszYearRelativeOf[];
extern WCHAR gszYearRelativeMonth[];
extern WCHAR gszPerson[];
extern WCHAR gszBusiness[];
extern WCHAR gszPersonBusiness[];
extern WCHAR gszAdd[];
extern WCHAR gszLastName[];
extern WCHAR gszFirstName[];
extern WCHAR gszNickName[];
extern WCHAR gszRelationship[];
extern WCHAR gszGender[];
extern WCHAR gszHomePhone[];
extern WCHAR gszWorkPhone[];
extern WCHAR gszMobilePhone[];
extern WCHAR gszFAXPhone[];
extern WCHAR gszPersonalEmail[];
extern WCHAR gszBusinessEMail[];
extern WCHAR gszBusinessEmail[];
extern WCHAR gszPersonalWeb[];
extern WCHAR gszHomeAddress[];
extern WCHAR gszWordAddress[];
extern WCHAR gszCompany[];
extern WCHAR gszJobTitle[];
extern WCHAR gszDepartment[];
extern WCHAR gszOffice[];
extern WCHAR gszManager[];
extern WCHAR gszAssistant[];
extern WCHAR gszSpouse[];
extern WCHAR gszChild1[];
extern WCHAR gszChild2[];
extern WCHAR gszChild3[];
extern WCHAR gszChild4[];
extern WCHAR gszChild5[];
extern WCHAR gszChild6[];
extern WCHAR gszChild7[];
extern WCHAR gszChild8[];
extern WCHAR gszMother[];
extern WCHAR gszFather[];
extern WCHAR gszBirthday[];
extern WCHAR gszRemindBDay[];
extern WCHAR gszShowBDay[];
extern WCHAR gszDeathDay[];
extern WCHAR gszMiscNotes[];
extern WCHAR gszCompleted[];
extern WCHAR gszDelete[];
extern WCHAR gszMeetingNotesNode[];
extern WCHAR gszPhoneNotesNode[];
extern WCHAR gszDate[];
extern WCHAR gszEnd[];
extern WCHAR gszStart[];
extern WCHAR gszPhoneNode[];
extern WCHAR gszInteraction[];
extern WCHAR gszCalendarNode[];
extern WCHAR gszCalendarLogDay[];
extern WCHAR gszNoteListNode[];
extern WCHAR gszJournalNode[];
extern WCHAR gszJournalCategoryNode[];
extern WCHAR gszJournalEntryNode[];
extern WCHAR gszMemoryEntryNode[];
extern WCHAR gszMemoryListNode[];
extern WCHAR gszMemoryNode[];
extern PWSTR gaszPerson[MAXPEOPLE];
extern WCHAR gszMeetingDate[];
extern WCHAR gszMemory[];
extern WCHAR gszDeepThoughtsNode[];
extern WCHAR gszWrapUpNode[];
extern WCHAR gszArchiveNode[];
extern WCHAR gszArchiveEntryNode[];
extern WCHAR gszRegisterURL[];
extern WCHAR gszExamples[];
extern char    gszRegBase[];
extern char gszFullColor[];
extern char gszMicroHelp[];
extern char gszAskedAboutMicroHelp[];
extern char gszMinimizeDragonflyToTaskbar[];
extern char gszChangeWallpaper[];
extern char gszSmallFont[];
extern char gszDisableSounds[];
extern char gszBugAboutWrap[];
extern char gszDisableCursor[];
extern char gszLastLogin[];
extern char gszAutoLogon[];
extern char gszTemplateR[];
extern char gszPrintTwoColumns[];
extern char gszFirstTime[];
extern WCHAR gszParent[];
extern WCHAR gszJournal[];
extern WCHAR gszNotes[];
extern WCHAR gszTimer[];
extern PWSTR gpszMonth[12];
extern WCHAR gszPhotosNode[];
extern WCHAR gszPhotosEntryNode[];
extern WCHAR gszCategory[];
extern WCHAR gszPhotos[];
extern WCHAR gszPlannerNode[];
extern WCHAR gszAlarm[];
extern WCHAR gszAlarmDate[];
extern WCHAR gszAlarmTime[];
extern WCHAR gszTimeZoneNode[];
extern WCHAR gszMiscNode[];
extern WCHAR gszPP[];
extern WCHAR gszFirstDate[];

/****************************************************************************
Encrypt object */
#define  ENCRYPTSIZE    4096

class CRandSequence {
public:
   CRandSequence (void);
   ~CRandSequence (void);

   void  Init (DWORD dwKey);
   DWORD GetValue (DWORD dwPosn);

private:
   DWORD    m_adwEncrypt[ENCRYPTSIZE];
};

typedef CRandSequence * PCRandSequence;


/* CEncrypt */
#define  MAXENCRYPTPW      64
class CEncrypt {
public:
   CEncrypt (void);
   ~CEncrypt (void);

   BOOL  Init (PWSTR pszPassword);
   DWORD Encrypt (PBYTE pData, DWORD dwSize);
   BOOL  Decrypt (PBYTE pData, DWORD dwSize, DWORD dwCheckSum);

private:
   WCHAR    m_szPassword[MAXENCRYPTPW]; // max length
   DWORD    m_dwChars;        // # chars in m_szPassword
   PCRandSequence m_apRS[MAXENCRYPTPW];   // random sequence based on the password

   DWORD CheckSum (PBYTE pData, DWORD dwSize);
   void  XOREnDecrypt (PBYTE pData, DWORD dwSize);
   void  ShuffleEncrypt (PBYTE pData, DWORD dwSize);
   void  ShuffleDecrypt (PBYTE pData, DWORD dwSize);
};

/****************************************************************************
CDatabase.cpp
*/

#define  NODESPERFILE      100   // number of nodes per file
#define  FILESPERDIR       100   // number of files that will put in each subdirectory

typedef struct {
   DWORD       dwFile;  // file number
   PCMMLNode   pRoot;   // root node
   DWORD       dwRefCount; // reference count. 0 => can free up
   DWORD       dwTimeRelease; // GetTickCount() when dwRefCount went to 0
   PCMMLNode   apNodes[NODESPERFILE];  // pointer to each of the nodes in the file
   DWORD       adwNodeCount[NODESPERFILE];   // reference count for each. included in dwRefCount
} DBCACHE, *PDBCACHE;

class CDatabase {
public:
   CDatabase (void);
   ~CDatabase (void);
   BOOL InitOpen (PWSTR pszDir, PWSTR pszPassword, PWSTR pszPrefix, int *piError);
   BOOL InitCreate (PWSTR pszDir, PWSTR pszPassword, PWSTR pszPrefix,
                          BOOL fFailIfExists = TRUE);
   BOOL Flush (BOOL fForce = FALSE);
   BOOL FileFlush (PCMMLNode pNode);
   PCMMLNode FileCache (DWORD dwNum, BOOL fCreate = TRUE);
   BOOL FileRelease (PCMMLNode pNode);
   BOOL VerifyPassword (void);
   BOOL EmptyUnused (void);
   DWORD Num (void);
   PWSTR Dir (void);
   void DirParent (PWSTR psz);
   void DirSub (PWSTR psz);
   void DirNode (DWORD dwNode, PWSTR pszFile);
   PCMMLNode NodeGet (DWORD dwNode);
   BOOL NodeRelease (PCMMLNode pNode);
   PCMMLNode NodeAdd (PWSTR pszType, DWORD *pdwNode = NULL);
   BOOL NodeDelete (DWORD dwNode);
   BOOL ChangePassword (PWSTR pszNew);
   BOOL Backup (PWSTR pszBackupTo);

   WCHAR    m_szPassword[256];     // password. Only CDatabase should change this


private:
   WCHAR    m_szDir[256];          // database directory
   WCHAR    m_szPrefix[32];        // prefix for files
   DWORD    m_dwNum;               // number of elements total
   DWORD    m_dwNumFlushed;        // value of m_dwNum when last flushed
   CListFixed m_Cache;             // list of DBCACHE
   CEncrypt m_Encrypt;             // encryption opject
   DWORD    m_dwTimer;             // timer ID (for waiting to flush) or 0 if none
};
typedef CDatabase *PCDatabase;


/***************************************************************************
Util */
extern CMem     gMemTemp;         // temporary memory for whatever
extern PWSTR gaszMonth[12];

typedef struct {
   DFDATE      date;    // date planned for
   DWORD       dwPriority; // priority, lower is higher
   DWORD       dwTimeAllotted;   // time allotted this go around, in minutes
   DFTIME      start;   // minimum start time before start working
} PLANTASKITEM, *PPLANTASKITEM;

class CPlanTask {
public:
   CPlanTask (void);
   ~CPlanTask (void);
   DWORD Num (void);
   PPLANTASKITEM Get (DWORD dwElem);
   BOOL Remove (DWORD dwElem);
   DWORD Add (PPLANTASKITEM pItem);
   BOOL Write (PCMMLNode pNode);
   BOOL Read (PCMMLNode pNode);
   BOOL Verify (DWORD dwTotalTime, DFDATE today, DFDATE addto, DWORD dwPriority=100, DWORD dwMinPerDay=60*8);

   DWORD       m_dwTimeCompleted;   // # minutes completed so far in previous items

private:
   CListFixed  m_list;     // list to store data of plantaskitem
};
typedef CPlanTask *PCPlanTask;


typedef struct {
   PWSTR       psz;  // string. Valid only until node that called NodeListGet() is released
   int         iNumber; // number for element
   DFDATE      date; // date
   DFTIME      start;   // start time
   DFTIME      end;  // end time
   int         iExtra;  // extra value
} NLG, *PNLG;

#define  R_FIRST     0
#define  R_SECOND    1
#define  R_THIRD     2
#define  R_FOURTH    3
#define  R_LAST      4
#define  R_DAYOFWEEK 0
#define  R_WEEKDAY   1
#define  R_WEEKENDDAY   2
#define  R_SUNDAY    3
#define  R_MONDAY    4
#define  R_TUESDAY   5
#define  R_WEDNESDAY 6
#define  R_THURSDAY  7
#define  R_FRIDAY    8
#define  R_SATURDAY  9

PWSTR NodeValueGet (PCMMLNode pNode, PWSTR pszName, int*piNumber = NULL);
int NodeValueGetInt (PCMMLNode pNode, PWSTR pszName, int iDefault = 0);
BOOL NodeValueSet (PCMMLNode pNode, PWSTR pszName, PWSTR pszValue, int iNumber = 0);
BOOL NodeValueSet (PCMMLNode pNode, PWSTR pszName, int dwValue);
DWORD FilteredStringToIndex (PWSTR pszName,
                               PCListVariable pNames, PCListFixed pLoc);
DWORD FilteredDatabaseToIndex (DWORD dwLoc,
                               PCListVariable pNames, PCListFixed pLoc);
DWORD FilteredIndexToDatabase (DWORD dwIndex,
                               PCListVariable pNames, PCListFixed pLoc);
BOOL RenameFilteredList (DWORD dwDelLoc, PWSTR pszNewName,
                         PCMMLNode pNode, PWSTR pszKeyword,
                         PCListVariable pNames, PCListFixed pLoc);
BOOL RemoveFilteredList (DWORD dwDelLoc, PCMMLNode pNode, PWSTR pszKeyword,
                         PCListVariable pNames, PCListFixed pLoc);
BOOL AddFilteredList (PWSTR pszAddName, DWORD dwAddLoc,
                      PCMMLNode pNode, PWSTR pszKeyword,
                      PCListVariable pNames, PCListFixed pLoc);
BOOL FillFilteredList (PCMMLNode pNode, PWSTR pszKeyword,
                       PCListVariable pNames, PCListFixed pLoc);
PCMMLNode FindMajorSection (PWSTR pszSection, DWORD *pdwID = NULL);
void MemCat (PCMem pMem, PWSTR psz);
void MemCatSanitize (PCMem pMem, PWSTR psz);
void MemZero (PCMem pMem);
void MemCat (PCMem pMem, int iNum);
DFDATE DateControlGet (PCEscPage pPage, PWSTR pszControl);
BOOL DateControlSet (PCEscPage pPage, PWSTR pszControl, DFDATE date);
void DFDATEToString (DFDATE date, PWSTR psz);
__int64 DFDATEToMinutes (DFDATE date);
DFDATE MinutesToDFDATE (__int64 iMinutes);
DFDATE Today (void);
void DialogRect (RECT *pRect);
void ReoccurFromControls (PCEscPage pPage, PREOCCURANCE pr);
void ReoccurToControls (PCEscPage pPage, PREOCCURANCE pr);
void ReoccurToString (PREOCCURANCE pr, PWSTR psz);
DWORD ReoccurFindNth (DWORD dwMonth, DWORD dwYear, DWORD dwNth, DWORD dwElem);
DFDATE ReoccurNext (PREOCCURANCE pr);
DWORD DFDATEToDayOfWeek (DFDATE date);
DFTIME TimeControlGet (PCEscPage pPage, PWSTR pszControl);
BOOL TimeControlSet (PCEscPage pPage, PWSTR pszControl, DFTIME time);
void DFTIMEToString (DFTIME time, PWSTR psz);
DFTIME Now (void);
BOOL NodeElemSet (PCMMLNode pNode, PWSTR pszName, PWSTR pszValue, int iNumber = 0,
                  BOOL fRemove = FALSE, DFDATE date = 0, DFTIME startTime = (DWORD)-1, DFTIME endTime = (DWORD)-1,
                  int iExtra = 0);
PWSTR NodeElemGet (PCMMLNode pNode, PWSTR pszName, DWORD *pdwIndex,
                   int*piNumber = NULL, DFDATE *pdate = NULL, DFTIME *pstartTime = NULL, DFTIME *pendTime = NULL,
                   int*piExtra = NULL);
BOOL NodeElemRemove (PCMMLNode pNode, PWSTR pszName, int iNumber);
PCListFixed NodeListGet (PCMMLNode pNode, PWSTR pszName, BOOL fAscending);
PCMMLNode MonthYearTree (PCMMLNode pRoot, DFDATE date, PWSTR pszPrefix,
                         BOOL fCreateIfNotExist, DWORD *pdwNode);
PCMMLNode DayMonthYearTree (PCMMLNode pRoot, DFDATE date, PWSTR pszPrefix,
                         BOOL fCreateIfNotExist, DWORD *pdwNode);
int DFTIMEToMinutes (DFTIME time);
PCListFixed EnumMonthYearTree (PCMMLNode pRoot, PWSTR pszPrefix);
int RepeatableRandom (int iSeed, DWORD dwIndex);
int GetCBValue (PCEscPage pPage, PWSTR pszName, int iDefault);
__int64 DiskFreeSpace (PWSTR psz);
DWORD KeyGet (char *pszKey, DWORD dwDefault);
void KeySet (char *pszKey, DWORD dwValue);
double HowLong (PCEscPage pPage, double fDefault);
DWORD AlarmUI (PWSTR pszMain, PWSTR pszSub);
BOOL AlarmIsVisible (void);
void DateToDayOfYear (DFDATE date, DWORD *pdwDayIndex, DWORD *pdwDaysInYear, DWORD *pdwWeekIndex);
void KeySetString (char *pszKey, char *pszValue);
BOOL KeyGetString (char *pszKey, char *pszValue, DWORD dwSize);

/***************************************************************************
Search */
BOOL SearchPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);

/****************************************************************************
main.cpp
*/
typedef struct {
   WCHAR    szLink[256];   // link name. controls all
   BOOL     fResolved;     // set to TRUE if resolved, FALSE if cant
   int      iVScroll;      // vertical scroll to use. -1 if use szLink
   int      iSection;      // index into szLink for character just after #. -1 if none
   DWORD    dwResource;    // MML resource using for this.
   DWORD    dwData;        // Database number using for this. -1 if none
} HISTORY, *PHISTORY;

extern BOOL        gfPrinting;  // set to true
extern HISTORY     gCurHistory;         // current location
extern HINSTANCE   ghInstance;
extern char        gszAppPath[256];     // application path
extern char        gszAppDir[256];      // application directory
extern PCEscWindow gpWindow;            // main window object
extern PCDatabase  gpData;              // database
extern WCHAR       gszUserName[128];    // current user name
extern CEscSearch  gSearch;             // search
extern BOOL        gfFullColor;  // set to true to use lots of color, FALSE to tend towards BnW
extern BOOL        gfMicroHelp;  // if TRUE then show small help tips at top of page, else terse
extern BOOL        gfAskedAboutMicroHelp;  // if TRUE then already asked user if wanted microhelp
extern DWORD       gdwSmallFont;  // 0 normal size, 1 is slightly smaller, 2 is much smaller
extern BOOL        gfDisableSounds; // if TRUE then dont play beeps
extern BOOL        gfBugAboutWrap;  // if checked, will bug about not completing daily wrapup
extern DWORD       gdwLastLogin;    // index of the last login
extern BOOL        gfAutoLogon;     // if TRUE automatically log on
extern BOOL        gfDisableCursor; // if TRUE then use boring windows cursor
extern BOOL        gfTemplateR;  // if TRUE then use the right menu template
extern BOOL        gfPrintTwoColumns;  // if true print with two columns
extern BOOL        gfMinimizeDragonflyToTaskbar;   // if true miniize dragonfly to a taskbar icon
extern BOOL        gfChangeWallpaper;  // if true change the wallpaper from time to time
extern BOOL        gfFirstTime;     // first time using dragonfly
extern WCHAR       gszPowerPassword[64];   // password that must type to get access after power down
#define JPEGREMAP 15
extern char        gaszJPEGRemap[JPEGREMAP][256]; // remap to file names
extern DWORD       gadwJPEGRemap[JPEGREMAP];

BOOL DefPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/*****************************************************************************
Logon */
BOOL LogOnMain (void);
void LogOff (void);

#endif // _DRAGONFLY_H_


/******************************************************************************
Project */
void ProjectShutDown (void);
BOOL ProjectListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ProjectAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ProjectRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ProjectSetView (DWORD dwNode);
BOOL ProjectViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PCListVariable ProjectGetTaskList (void);
PCListVariable ProjectGetSubProjectList (void);
DWORD ProjectAddSubProject (PCEscPage pPage);
PWSTR ProjectSummary (DFDATE endDate = 0, DFDATE startDate = 0);
void ProjectMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks = FALSE);
BOOL IsProjectDisabled (void);
PWSTR ProjectTimerSubst (void);
BOOL ProjectTimerEndUI (PCEscPage pPage, DWORD dwHash);
BOOL ProjectParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, BOOL fJumpToEdit);
void ProjectEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays);
void ProjectChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time);
void ProjectChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date);
void ProjectAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority);
void ProjectSplitPlan (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwMin);


/******************************************************************************
Today */
BOOL TodayPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL TomorrowPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/*******************************************************************************
Reminders */
BOOL RemindersPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ReminderParseLink (PWSTR pszLink, PCEscPage pPage);
PWSTR RemindersSummary (DFDATE dat, DFDATE start = 0, BOOL fShowIfEmpty = FALSE);
BOOL ReminderAdd (PWSTR psz, DFDATE due);
void ReminderMonthEnumerate (DFDATE date, PCMem *papMem, DFDATE bunchup = 0, BOOL fWithLinks = FALSE);
BOOL ReminderQuickAdd (PCEscPage pPage, DFDATE dAddDate = 0);
void RemindersCheckAlarm (DFDATE today, DFTIME now);


/*******************************************************************************
Works */
BOOL WorksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL WorkParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, BOOL fJumpToEdit);
PWSTR WorkSummary (BOOL fAll, DFDATE date, DFDATE start = 0, BOOL fShowIfEmpty = FALSE);
BOOL WorkTaskShowAddUI (PCEscPage pPage, DFDATE dAddDate = 0);
BOOL WorkTasksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void TaskBirthday (DWORD dwNode, PWSTR pszFirst, PWSTR pszLast,
                   DFDATE bday, BOOL fEnable);
PWSTR WorkActionItemAdd (PCEscPage pPage);
void WorkMonthEnumerate (DFDATE date, PCMem *papMem, DFDATE bunchup = 0, BOOL fWithLinks = FALSE);
void WorkEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays);
void WorkChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time);
void WorkChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date);
void WorkAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority);
void WorkSplitPlan (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwMin);


/*******************************************************************************
People */
BOOL PeopleNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL BusinessNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PCListVariable PeopleFilteredList (void);
PCListVariable BusinessFilteredList (void);
PCListVariable PeopleBusinessFilteredList (void);
BOOL PeopleLookUpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PeopleSetView (DWORD dwNode);
BOOL PeoplePersonViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PeopleBusinessViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PeoplePersonEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PeopleBusinessEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PeopleListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DWORD PeopleQuickAdd (PCEscPage pPage);
DWORD BusinessQuickAdd (PCEscPage pPage);
DWORD PeopleBusinessQuickAdd (PCEscPage pPage);
BOOL PeoplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DWORD PeopleIndexToDatabase (DWORD dwIndex);
DWORD BusinessIndexToDatabase (DWORD dwIndex);
DWORD PeopleBusinessIndexToDatabase (DWORD dwIndex);
PWSTR PeopleIndexToName (DWORD dwIndex);
PWSTR BusinessIndexToName (DWORD dwIndex);
PWSTR PeopleBusinessIndexToName (DWORD dwIndex);
DWORD PeopleDatabaseToIndex (DWORD dwIndex);
DWORD BusinessDatabaseToIndex (DWORD dwIndex);
DWORD PeopleBusinessDatabaseToIndex (DWORD dwIndex);
BOOL PhoneNumGet (PCEscPage pPage, DWORD dwNode, PWSTR psz, DWORD dwSize);
BOOL WriteCombo (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName);
BOOL ReadCombo (PCMMLNode pNode, PWSTR pszName, PCEscPage pPage, PWSTR pszControlName);
BOOL PeopleFullName (PWSTR pszLast, PWSTR pszFirst, PWSTR pszNick,
                      PWSTR pszFull, DWORD dwSize);
BOOL PeoplePrintPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PersonLinkToJournal (DWORD dwPerson, PWSTR pszName, DWORD dwEntry, DFDATE dfDate,
                  DFTIME dfStart, DFTIME dfEnd);
BOOL PeopleExportPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/********************************************************************************
Meetings */
BOOL SchedMeetingsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL SchedParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, DWORD *pdwNext);
PWSTR SchedSummary (BOOL fAll, DFDATE date);
BOOL SchedSetView (DWORD dwNode);
BOOL SchedMeetingNotesViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL SchedMeetingNotesEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PWSTR ActionItemOther (PCEscPage pPage, DWORD dwPersonID);
BOOL SchedMeetingShowAddUI (PCEscPage pPage, DFDATE dAddDate = 0);
void SchedMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks = FALSE);
// PWSTR SchedSummaryScale (DFDATE date, BOOL fShowIfEmpty = FALSE);
BOOL LookForConflict (DFDATE date, DFTIME start, DFTIME end, DWORD dwExclude, BOOL fMeeting = TRUE);
void SchedEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays);
void CalcAlarmTime (DFDATE m_dwDate, DFTIME m_dwStart, DWORD m_dwAlarm,
                    DFDATE *pdwAlarmDate, DFTIME *pdwAlarmTime);
BOOL AlarmControlSet (PCEscPage pPage, PWSTR pszName, DWORD dwAlarm);
DWORD AlarmControlGet (PCEscPage pPage, PWSTR pszName);
void SchedCheckAlarm (DFDATE today, DFTIME now);


/*********************************************************************************
Phone */
class CPhoneCall;
typedef CPhoneCall * PCPhoneCall;

// Phone
class CPhone {
public:
   CPhone (void);
   ~CPhone (void);

   BOOL Write (void);
   BOOL Flush (void);
   BOOL Init (void);
   BOOL Init (DWORD dwDatabaseNode);
   PCPhoneCall CallAdd (PWSTR pszName, PWSTR pszDescription, DWORD dwBefore);
   PCPhoneCall CallGetByIndex (DWORD dwIndex);
   PCPhoneCall CallGetByID (DWORD dwID);
   DWORD CallIndexByID (DWORD dwID);
   void Sort (void);
   void BringUpToDate (DFDATE today);
   void DeleteAllChildren (DWORD dwID);


   CListFixed     m_listCalls;   // list of PCPhoneCalls, ordered according order of doing

   BOOL           m_fDirty;      // set to TRUE if the Phone is dirty and should be flushed
   DWORD          m_dwDatabaseNode; // node in the database to save to. 0 if not defined
   DWORD          m_dwNextCallID;   // next Call ID created

private:

};

typedef CPhone * PCPhone;

// Call
class CPhoneCall {
public:
   CPhoneCall (void);
   ~CPhoneCall (void);

   PCPhone         m_pPhone; // Phone that this belongs to. If change sets dirty
   CMem           m_memDescription; // description
   DWORD          m_dwID;     // unique ID for the Call.
   DWORD          m_dwParent;    // non-zero if created by a reoccurring task
   REOCCURANCE    m_reoccur;     // how often the Call reoccurs.

   DFDATE         m_dwDate;   // date of the Call
   DFTIME         m_dwStart;  // start time
   DFTIME         m_dwEnd;    // end time
   DWORD          m_dwAlarm;  // sound an alarm this many minutes before
   DFDATE         m_dwAlarmDate; // if follow m_dwAlarm, then this is the date of the alarm
   DFTIME         m_dwAlarmTime; // if follow m_dwAlarm, then this is the time of the alarm
   DWORD          m_dwAttend;    // attendee node IDs. -1 if empty
   CPlanTask      m_PlanTask; // planner task

   BOOL Write (PCMMLNode pParent);
   BOOL Read (PCMMLNode pFrom);
   void SetDirty (void);
   void BringUpToDate (DFDATE today);
private:
};

extern CPhone       gCurPhone;      // current Phone that Phoneing with

BOOL PhoneInit (void);
DWORD PhoneCall (PCEscPage pPage, BOOL fReallyCall, BOOL fOutgoing,
                 DWORD dwPerson, PWSTR pszNumber);
BOOL PhoneSetView (DWORD dwNode);
BOOL PhoneNotesViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneNotesEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneMakeCallPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneSpeedNewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhonePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneSpeedRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneCallLogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhoneCallShowAddUI (PCEscPage pPage, DFDATE dAddDate = 0);
BOOL PhoneCallShowAddReoccurUI (PCEscPage pPage);
BOOL PhoneParseLink (PWSTR pszLink, PCEscPage pPage, BOOL *pfRefresh, DWORD *pdwNext);
void PhoneMonthEnumerate (DFDATE date, PCMem *papMem, BOOL fWithLinks = FALSE);
void PhoneEnumForPlanner (DFDATE date, DWORD dwDays, PCListFixed palDays, BOOL fForCombo = FALSE);
void PhoneAdjustPriority (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DWORD dwPriority);
void PhoneChangeDate (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFDATE date);
void PhoneChangeTime (DWORD dwMajor, DWORD dwMinor, DWORD dwSplit, DFTIME time);
void PhoneCheckAlarm (DFDATE today, DFTIME now);

/********************************************************************************
Calendar */
extern DFDATE gdateCalendar;    // month that the calendar displays
extern DFDATE gdateCalendarCombo;  // to show on the combo
BOOL CalendarPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL CalendarLogAdd (DFDATE date, DFTIME start, DFTIME end, PWSTR psz,
                     DWORD dwLink = (DWORD)-1, BOOL fReplaceOld = TRUE);
BOOL CalendarLogRemove (DFDATE date, DWORD dwLink);
BOOL CalendarLogPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL CalendarSetView (DWORD dwNode);
PCMMLNode GetCalendarLogNode (DFDATE date, BOOL fCreateIfNotExist, DWORD *pdwNode);
BOOL CalendarHolidaysPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL CalendarListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL CalendarSummaryPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PWSTR CalendarLogSub (DFDATE date, BOOL fCat = FALSE, BOOL *pfFound = NULL);
BOOL CalendarComboPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL CalendarYearlyPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void Holiday (PCMem paMem, DWORD dwDate, PWSTR psz);
void HolidaysFromMonth (DWORD dwMonth, DWORD dwYear, PCMem paMem);
DFDATE CalendarLogRandom (void);


/**********************************************************************************
Notes */
BOOL NotesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PCListVariable NotesFilteredList (void);
DWORD NotesQuickAdd (PCEscPage pPage);
BOOL NotesQuickAddNote (PCEscPage pPage);

/********************************************************************************
Journal */
BOOL JournalPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL JournalCatAddPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL JournalCatRemovePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL JournalCatPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL JournalEntryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL JournalEntryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void JournalCatSet (DWORD dwNode);
void JournalEntrySet (DWORD dwNode);
PCListVariable JournalListVariable (void);
DWORD JournalQuickAdd (PCEscPage pPage);
BOOL JournalLink (DWORD dwCategory, PWSTR pszName, DWORD dwEntry, DFDATE dfDate,
                  DFTIME dfStart, DFTIME dfEnd, double fOverride = -1,
                  BOOL fKeepOld = FALSE);
DWORD JournalDatabaseToIndex (DWORD dwIndex);
DWORD JournalIndexToDatabase (DWORD dwIndex);
PWSTR JournalTimerSubst (void);
BOOL JournalTimerEndUI (PCEscPage pPage, DWORD dwIndex);
extern CListVariable glistJournalNames;

/******************************************************************************
Memory */
BOOL MemoryLanePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL MemoryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL MemoryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL MemoryListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL MemorySuggestPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void MemoryListSet (DWORD dwNode);
void MemoryEntrySet (DWORD dwNode);


/*******************************************************************************
Deep thoughts */
#define TOPN      5
typedef struct {
   PWSTR    apszShort[TOPN];
   PWSTR    apszLong[TOPN];
   PWSTR    apszChange[TOPN];
   PWSTR    apszWorld[TOPN];
   DWORD    adwPerson[TOPN];  // Node ID. -1 if empty
   PWSTR    apszPerson[TOPN]; // name string
} DEEPTHOUGHTS, *PDEEPTHOUGHTS;

PCMMLNode DeepFillStruct (DEEPTHOUGHTS *pdt);
BOOL DeepPeoplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL DeepShortPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL DeepLongPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL DeepChangePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL DeepWorldPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PWSTR DeepThoughtGenerate (void);
BOOL QuotesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/*****************************************************************************
Wrapup*/
void WrapUpSet (DWORD dwNode);
BOOL WrapUpEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL WrapUpViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL WrapUpPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
DWORD WrapUpCreateOrGet (DFDATE date, BOOL fCreate);


/***************************************************************************
Archive */
void ArchiveShutDown (void);
void ArchiveOnOff (BOOL fOn);
void ArchiveInit (void);
BOOL ArchiveIsOn (void);
BOOL ArchiveOptionsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ArchivePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ArchiveViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ArchiveEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void ArchiveEntrySet (DWORD dwNode);
BOOL ArchiveListPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/****************************************************************************8
Register */
BOOL RegisterIsRegistered (void);
DWORD RegisterKeyGet (WCHAR *pszEmail, DWORD dwSize);
void RegisterKeySet (PWSTR pszEmail, DWORD dwKey);

/******************************************************************************
Misc */
BOOL RegisterPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL BackupPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL ChangePasswordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL MiscUIPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void BackupBugUser (PCEscPage pPage);


/********************************************************************************
Favorites */
void FavoritesInit (void);
void FavoritesSave (void);
PWSTR FavoritesMenuSub (BOOL fRight = FALSE);
BOOL FavoritesMenuAdd (PCEscPage pPage);
BOOL FavoritesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PWSTR FavoritesGetStartup (void);

/*********************************************************************************
Password */
BOOL PasswordPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PasswordSuspendPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PasswordSuspendVerify (PCEscWindow pWindow);


/**********************************************************************************
Time */
BOOL TimeZonesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL TimeMoonPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL TimeSunlightPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/********************************************************************************
Event */
void EventEnumMonth (DWORD dwMonth, DWORD dwYear, PCMem paMem);
void EventBirthday (DWORD dwNode, PWSTR pszFirst, PWSTR pszLast,
                   DFDATE bday, BOOL fEnable);
BOOL EventDaysPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);


/**********************************************************************************
Photos */
BOOL PhotosPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void PhotosEntrySet (DWORD dwNode);
BOOL PhotosEntryViewPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PhotosEntryEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
void PhotosUpdateDesktop (BOOL fForce);
DWORD PhotoRandom (void);
BOOL PhotosFileName (DWORD dwNode, PWSTR pszFile);
void ImportPhotos (PCEscPage pPage, DWORD dwJournal = (DWORD)-1);



/**********************************************************************************
Planner */
BOOL PlannerPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
BOOL PlannerBreaksPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam);
PWSTR PlannerSummaryScale (DFDATE date, BOOL fShowIfEmpty = FALSE);
