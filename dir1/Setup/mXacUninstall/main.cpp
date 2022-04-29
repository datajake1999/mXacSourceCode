/*********************************************************************
main.cpp - main uninstall

uninstall reads the file in and looks at:
   file= lines to delete files
   dir= lines to delete directories
   regkey= lines to delete registry keys
*/

#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <stdio.h>


char  *gpszInf;
DWORD gdwInfSize;

/******************************************************************************
InfFind - Find a value in inf.

inputs
   char     *pszValue - such as "file="
   char     *pszFillIn - Fill in pszFillIn with the data, such as "c:\test.txt"
   DWORD    dwStart - start position in file
   DWORD    *pdwEnd - filled in with the end, which is the next start
returns
   BOOL - TRUE if succede
*/
BOOL InfFind (char *pszValue, char *pszFillIn, DWORD dwStart = 0, DWORD *pdwEnd = NULL)
{
   DWORD dwLen;
   dwLen = strlen(pszValue);

   char  *pMax;
   pMax = gpszInf + gdwInfSize;

   char  *pCur;
   for (pCur = gpszInf + dwStart; pCur < pMax; ) {
      // find the end of the line
      char  *pEOL;
      for (pEOL = pCur; pEOL < pMax; pEOL++) {
         if ((*pEOL == '\n') || (*pEOL == '\r'))
            break;
      }

      // see if find
      if (pCur + dwLen > pMax)
         break;   // beyond edge
      if (!strnicmp (pCur, pszValue, dwLen)) {
         // match
         DWORD i;
         for (i = 0; pCur + i + dwLen < pEOL; i++)
            pszFillIn[i] = pCur[i+dwLen];
         pszFillIn[i] = 0;
         if (pdwEnd)
            *pdwEnd = (DWORD)(pEOL - gpszInf) +1;
         return TRUE;
      }

      // go to next line
      pCur = pEOL + 1;
   }

   // cant find
   if (pdwEnd)
      *pdwEnd = (DWORD) (pMax - gpszInf) + 1;
   return FALSE;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nShowCmd)
{
#if 0 // BUGFIX - Take this out because too many people complain
   // bring up the E-mail app
   ShellExecute (NULL /*ghWndMain*/, NULL, "mailto:MikeRozak@bigpond.com", NULL, NULL, SW_SHOW);

   // ask the user to send E-mail
   MessageBox (NULL,
      "Uninstall my application? As you wish.\r\n"
      "\r\n"
      "But please tell me: Did it not work as you expected? "
      "Were important features missing? Was it too difficult to use? "
      "Please tell me why you didn't like it. Maybe I can fix the problem. "
      "Send me E-mail at MikeRozak@bigpond.com.\r\n"
      "\r\n"
      "Thanks, Mike Rozak, mXac"
      ,
      "Uninstall",
      MB_OK | MB_ICONINFORMATION);
#endif // 0

   // open the file in the command line
   char  szTemp[256];
   szTemp[0] = 0;
   if (lpCmdLine && *lpCmdLine) {
      if (lpCmdLine[0] != '\"')
         strcpy (szTemp, lpCmdLine);
      else {
         strcpy (szTemp, lpCmdLine+1);
         if (szTemp[strlen(szTemp)-1] == '\"')
            szTemp[strlen(szTemp)-1] = 0;
      }
   }

   FILE  *f;
   f = fopen (szTemp, "rb");
   if (!f)
      return -1;
   fseek (f, 0, SEEK_END);
   DWORD dwSize;
   dwSize = ftell (f);
   fseek (f, 0, SEEK_SET);

   VOID  *pMem;
   pMem = malloc (dwSize);
   if (!pMem) {
      fclose (f);
      return FALSE;
   }
   fread (pMem, 1, dwSize, f);
   fclose (f);

   gpszInf = (char*) pMem;
   gdwInfSize = dwSize;

   // delete all the files
   char  szShort[512];
   DWORD dwCur;
   dwCur = 0;
   while (InfFind ("file=", szShort, dwCur, &dwCur)) {
      DeleteFile (szShort);
   }

   // delete all the directories
   dwCur = 0;
   while (InfFind ("dir=", szShort, dwCur, &dwCur)) {
      RemoveDirectory (szShort);
   }

   // NOTE: Deleteking HKLM. May need other tags for other keys
   dwCur = 0;
   while (InfFind ("regkey=", szShort, dwCur, &dwCur)) {
      RegDeleteKey (HKEY_LOCAL_MACHINE, szShort);
   }

   dwCur = 0;
   while (InfFind ("uninstallexe=", szShort, dwCur, &dwCur)) {
      STARTUPINFO si;
      PROCESS_INFORMATION  pi;
      memset (&si, 0, sizeof(si));
      si.cb = sizeof(si);
      memset (&pi, 0, sizeof(pi));
      CreateProcess (
         NULL, szShort,
         NULL, NULL,
         FALSE,
         0,
         NULL, // environment
         NULL,
         &si,
         &pi
         );
   }

   if (pMem)
      free (pMem);

   MessageBox (NULL, "Uninstall complete.", "Uninstall", MB_OK);

   return 0;
}
