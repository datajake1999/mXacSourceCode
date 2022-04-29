/* MMLInterpret header */

#ifndef _MMLINTERPRET_H_
#define _MMLINTERPRET_H_

#include "tools.h"
#include "fontcache.h"
#include "textwrap.h"
#include "mmlparse.h"


#define  MMLOBJECT_TABLE         10
#define  MMLOBJECT_CELL          11
#define  MMLOBJECT_CONTROL       13
#define  MMLOBJECT_STRETCH       14
#define  MMLOBJECT_SECTION       15

typedef struct {
   DWORD          dwID;    // always = MMLOBJECT_TABLE
   PCListVariable  plistTWTEXTELEM; // text info list
   PCListFixed     plistTWOBJECTPOSN;   // object position
} TABLEOBJECT, *PTABLEOBJECT;

typedef struct {
   DWORD          dwID;    // always = MMLOBJECT_CELL
   COLORREF       cBack;   // background color. -1 for transparent
   COLORREF       cEdge;   // edge color
   int            iLeft, iRight, iTop, iBottom; // width of the edges on the side
} CELLOBJECT, *PCELLOBJECT;

typedef struct {
   DWORD          dwID;    // always MMLOBJECT_CONTROL
   PVOID          pControl;   // PCEscControl, but easier this way because of typecasting
} CONTROLOBJECT, *PCONTROLOBJECT;

typedef struct {
   DWORD          dwID;    // always MMLOBJECT_STRETCH
   BOOL           fStart;  // true if indicates the start
} STRETCHOBJECT, *PSTRETCHOBJECT;

typedef struct {
   DWORD          dwID;    // always MMLOBJECT_SECTION
   PWSTR          pszName; // section name. must be freed()
} SECTIONOBJECT, *PSECTIONOBJECT;

void EscTextFromNode (PCMMLNode pNode, PCMem pMem);

HMIDIOUT MIDIClaim (void);
MMRESULT MIDIShortMsg (HMIDIOUT hMidi, DWORD dw);
void MIDIRelease (void);
void MIDIShutdown (void);

PCMMLNode FindPageInfo (PCMMLNode pNode);


#endif // _MMLINTERPRET_H_
