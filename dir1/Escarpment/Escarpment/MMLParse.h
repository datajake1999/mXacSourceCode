/*****************************************************************************
MMLParse.h - parsing header
*/

#ifndef _MMLPARSE_H_
#define _MMLPARSE_H_

#include "tools.h"

// old MMLNOde
// content structure
typedef struct {
   DWORD    dwType;     // 0 for text, 1 for another node
   // what follows is either a null-treminated unicode string, or a pointer to the node
} MMCONTENT, *PMMCONTENT;



PWSTR DataToUnicode (PVOID pData, DWORD dwSize, BOOL *pfWasUnicode = NULL);
PWSTR ResourceToUnicode (HINSTANCE hInstance, DWORD dwID, char *pszResType = NULL);
PWSTR FileToUnicode (PWSTR pszFile, BOOL *pfWasUnicode = NULL);
WCHAR *StringToMMLString (WCHAR *pszString);


#endif // _MMLPARSE_H