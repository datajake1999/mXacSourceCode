/***************************************************************************
main.cpp - main routines for the buildsetup code. To use the app pass in
the setup inf data

begun 1/14/2000 by Mike Rozak
copyright Mike Rozak. All rigths reserved
*/

#include <windows.h>
#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <stdio.h>
#include "common.h"


/* globals */
CChunk      gChunk;
char        gszSubDir[256] = "";

/***************************************************************************8
Help - display help
*/

#define PROGRAMFILES    "c:\\program files (x86)"
#define ZDRIVE          "z:" // "c:\\mXac"

void Help (void)
{
   printf ("BuildSetup <setupinf>\n");
   printf ("\n");
   printf ("Setupinf is a text file with multiples lines. Each line can contain\n");
   printf ("any of the following, in no particular order. Note: Some fields are\n");
   printf ("multiple entry, and some can only be used once.\n");
   printf ("\n");

   printf ("subdir=<dir> - Subdirectory off of the main install directory where\n");
   printf ("\tto put subsequent files. For example 'subdir=testapp' causes all\n");
   printf ("\tsubsquent files to be installed into " PROGRAMFILES "\\mxac\\app\\testapp\\.\n");
   printf ("\tYou can use 'subdir=$system' to install into the windows\\system dierctory.\n");

   printf ("file=<file> - Causes a file specified by <file> to be installed in\n");
   printf ("\tthe destination directory. <file> is the full path to the source\n");
   printf ("\tof the file, such as d:\\files\\test.txt. The files are installed\n");
   printf ("\tinto the last setting of subdir=\n");

   printf ("license=<licence file> - Includes a license agreement. If none is specified\n");
   printf ("\tthen a default one is generated.\n");

   printf ("compress=<0 or 1> - (Defaults to 1) If this is set to 0 then no compression\n");
   printf ("\twill be done on the data.\n");

   printf ("appname=<name> - Application name, such as 'Flying Fox'. Used for the\n");
   printf ("\tUI, license, and install directory.\n");

   printf ("mfg=<name> - Manufracurer name, such as 'mXac'. Used for the\n");
   printf ("\tUI, license, and install directory.\n");

   printf ("title=<name> - If not set, uses the application name. However, if title");
   printf ("\tis used the it's displayed in the UI instead of the application name.\n");

   printf ("icon=<exe>,<text> - Creates an icon in the start menu. Using the exe,\n");
   printf ("such as 'flyfox.exe' and name 'Flying Fox'.\n");

   printf ("selfexe=<file> - Where the self extracting setup exe should be saved to.\n");
   printf ("\t<file> is the full path.\n");

   printf ("selfexeauto=1 - If set to 1 the selfexe will automatically be run\n"
      "without waiting for the player to press OK.\n");

   printf ("del=<file> - File created in the install directory that should be deleted\n");
   printf ("\ton uninstall. For example, 'help.gid', since help creates files automatically.\n");

   printf ("installexe=<file> - Run this application during the install process.\n");

   printf ("uninstallexe=<file> - Run this application during the uninstall process.\n");
}


/***************************************************************************8
AddToChunk - Adds a file to the chunks. A chunk of type dwType is added under
   the gChunk, containing all the data from pszFile. In that chunk is added
   a such chunk of type "100" containing the name of the file.

   Displays an error in printf if can't find file.

inputs
   char     *pszFile - file name.
   DWORD    dwType - chunk type to add it as.
returns
   BOOL - TRUE if OK, FALSE if cant add
*/
BOOL AddToChunk (char *pszFile, void *pMem, DWORD dwSize, DWORD dwType)
{
   // add it
   CChunk   *p1, *p2;
   p1 = gChunk.SubChunkAdd ();
   p1->DataSet (pMem, dwSize);
   p1->m_dwID = dwType;

   // add the name
   char  szTemp[256];
   if (gszSubDir[0]) {
      strcpy (szTemp, gszSubDir);
      strcat (szTemp, "\\");
   }
   else {
      szTemp[0] = 0;
   }
   char  *psz;
   for (psz = pszFile; strchr(psz, '\\'); psz = strchr(psz, '\\')+1);
   strcat (szTemp, psz);

   // add the name
   p2 = p1->SubChunkAdd ();
   p2->DataSet (szTemp, strlen(szTemp)+1);
   p2->m_dwID = 100;

   printf ("Added %s into %s\n", pszFile, szTemp);

   return TRUE;
}

BOOL AddToChunk (char *pszFile, DWORD dwType)
{
   FILE  *f;
   DWORD dwSize;
   f = fopen (pszFile, "rb");
   if (!f) {
      printf ("Can't find the file %s.\n", pszFile);
      return FALSE;
   }
   fseek (f, 0, SEEK_END);
   dwSize = ftell (f);
   fseek (f, 0, SEEK_SET);

   VOID  *pMem;
   pMem = malloc (dwSize);
   if (!pMem) {
      fclose (f);
      printf ("%s is too large.\n", pszFile);
      return FALSE;
   }
   fread (pMem, 1, dwSize, f);
   fclose (f);

   AddToChunk (pszFile, pMem, dwSize, dwType);
   printf ("\tSize=%d\n", (int)dwSize);

   free (pMem);

   return TRUE;
}



/***************************************************************************8
AddLicense - Generates a license agreement and adds it

inputs
   char     *pszAppName - application name
*/
BOOL AddLicense (char *pszAppName)
{
   char  szLicenseBase[] =
      "\r\n"
      "\r\n"
      "END-USER LICENSE AGREEMENT\r\n"
      "\r\n"
      "Please read this license agreement (called the \"EULA\") carefully.\r\n"
      "By using the software or any related documentation (called the \"SOFTWARE\r\n"
      "PRODUCT\"), you agree to be bound by the terms of the EULA. If you do not\r\n"
      "accept this EULA, do not install or use this product.\r\n"
      "\r\n"
      "\r\n"
      "\r\n"
      "SOFTWARE PRODUCT LICENSE\r\n"
      "\r\n"
      "The SOFTWARE PRODUCT is owned by mXac. It is copyrighted by copyright\r\n"
      "laws and international copyright treaties. The SOFTWARE PRODUCT is\r\n"
      "licensed, not sold.\r\n"
      "\r\n"
      "1. GRANT OF LICENSE\r\n"
      "mXac grants you a non-exclusive, non-transferable license to download\r\n"
      "the SOFTWARE PRODUCT and install, use, access, display, run, or\r\n"
      "otherwise interact with (\"RUN\") the SOFTWARE PRODUCT on one\r\n"
      "computer.\r\n"
      "\r\n"
      "You may redistribute the self-extracting executable (which you are running\r\n"
      "now) of the SOFTWARE PRODUCT on the Internet provided\r\n"
      "you do not charge for it. However, mXac recommends that you point to\r\n"
      "the appropriate location on www.mXac.com.au instead of distributing your\r\n"
      "own version. This ensures that users are get the latest version.\r\n"
      "You may redistribute the self-extracting executable on CD-ROM or DVD\r\n"
      "provided it is part of a collection of shareware software.\r\n"
      "\r\n"
      "Without prejudice to any other rights, mXac may terminate this\r\n"
      "EULA if you fail to comply with the terms and conditions of this EULA.\r\n"
      "In such event, you must destroy all copies of the SOFTWARE PRODUCT and\r\n"
      "all of its component parts.\r\n"
      "\r\n"
      "\r\n"
      "2. DESCRIPTION OF OTHER RIGHTS AND LIMITATIONS\r\n"
      "You agree not to reverse engineer any component of the SOFTWARE\r\n"
      "PRODUCT. This includes disassembling or examining the code\r\n"
      "and file formats.\r\n"
      "\r\n"
      "There is no WARRANTY. The SOFTWARE PRODOUCT is provided \"AS IS.\" mXac\r\n"
      "makes no warranty about its use or performance.  USE IT AT YOUR OWN\r\n"
      "RISK. mXac makes no guarantees that the SOFTWARE PRODUCT will meet your\r\n"
      "requirements, or that operation of the SOFTWARE PRODUCT will be\r\n"
      "error-free or that defects in the software will be corrected.\r\n"
      "\r\n"
      "mXac will not be liable for any direct, consequential, incidental, or\r\n"
      "special damages, including lost profits or lost savings, even if mXac\r\n"
      "has been advised of the possibility of such damages. mXac will not be\r\n"
      "liable for the loss of, or damage to, your records or data, or any\r\n"
      "claims by any third parties.\r\n"
      "\r\n"
      "mXac will not provide technical support for the SOFTWARE PRODUCT.\r\n"
      "\r\n"
      "\r\n"
      "\r\n"
      "\r\n"
      "PRESS THE \"YES\" BUTTON TO ACCEPT ALL THE TERMS OF THIS AGREEMENT\r\n"
      "AND TO PROCEED WITH THE INSTALLATION OF THE SOFTWARE.  PRESS THE\r\n"
      "\"NO\" BUTTON IF  YOU WISH TO EXIT THE PROGRAM.\r\n";

   char  szLicense[5000];

   strcpy (szLicense, pszAppName);
   strcat (szLicense, szLicenseBase);

   return AddToChunk ("license.txt", szLicense, strlen(szLicense), 3);  // licenses are tyoe 3
}



/***************************************************************************8
main
*/
int main (int argc, char **argv)
{
   printf ("You must call " ZDRIVE "\\bin\\vcvarsall.bat before running this.\n");

   if (argc != 2) {
      Help();
      return -1;
   }

   // store the .inf in itself. tag it with type 1
   if (!AddToChunk (argv[1], 1))
      return -1;

   // store in the uninstall applet
   if (!AddToChunk (ZDRIVE "\\setup\\mxacuninstall\\release\\mxacuninstall.exe", 2))  // uninstall is type 2
      return -1;

   // read the file and fill in info
   FILE  *f;
   f = fopen (argv[1], "rt");
   if (!f) {
      printf ("Can't read %s.\n", argv[1]);
      return -1;
   }
   char  szTemp[1024];
   char  szSelfExe[256];
   char  szLicense[256];
   char  szAppName[256];
   char  szTitle[256];
   strcpy (szSelfExe, "setup.exe");
   szLicense[0] = 0;
   szAppName[0] = 0;
   szTitle[0] = 0;
   while (fgets(szTemp, sizeof(szTemp), f)) {
      // take off end of line
      if (strchr(szTemp, '\n'))
         *(strchr(szTemp, '\n')) = 0;
      if (strchr(szTemp, '\r'))
         *(strchr(szTemp, '\r')) = 0;

      // find the equals
      char  *p;
      p = strchr(szTemp, '=');
      if (!p)
         continue;
      *p = 0;
      p++;

      if (!_stricmp(szTemp, "file")) {
         // add files as type 100
         if (!AddToChunk (p, 100))
            break;
      }
      else if (!_stricmp(szTemp, "selfexe"))
         strcpy (szSelfExe, p);
      else if (!_stricmp (szTemp, "license"))
         strcpy (szLicense, p);
      else if (!_stricmp (szTemp, "appname"))
         strcpy (szAppName, p);
      else if (!_stricmp (szTemp, "title"))
         strcpy (szTitle, p);
      else if (!_stricmp (szTemp, "subdir"))
         strcpy (gszSubDir, p);
      else if (!_stricmp (szTemp, "compress")) {
         if (p[0] == '1')
            gDontCompress = FALSE;
         else
            gDontCompress = TRUE;
      }
   }

   fclose (f);

   if (!szAppName[0]) {
      printf ("The appname must be specified\n");
      return -1;
   }

   // license
   if (szLicense[0])
      AddToChunk (szLicense, 3); // license are type 3
   else
      AddLicense (szTitle[0] ? szTitle : szAppName);

   // write out the chunk into the setup directory so the setup
   // app can be rebuilt
   CEncFile cFile;
   if (!cFile.Create (ZDRIVE "\\setup\\setup\\data.dat")) {
      printf ("Can't create the temporary setup file.\n");
      return -1;
   }

   printf ("Writing file...\n");

   DWORD dwTime;
   dwTime= GetTickCount ();
   if (!gChunk.Write (&cFile)) {
      printf ("Error writing the temporary setup file.\n");
      return -1;
   }
   printf ("Time to compress = %d ms\n",GetTickCount() - dwTime);

   cFile.Close ();

   // run the makefile and wait for it to complete
   STARTUPINFO si;
   PROCESS_INFORMATION  pi;

   // vc8
#define VSROOT    PROGRAMFILES "\\Microsoft Visual Studio 8"
#define VCINSTALL VSROOT "\\Vc7"
#define VCFRAMEWORK VSROOT "\\FrameworkSDK"

   char  szEnv[] =
#define VSINSTALLDIR VSROOT "\\Common7\\IDE"
#define VCINSTALLDIR VSROOT "\\VC"
#define FRAMEWORKSDKDIR VSROOT "\\SDK\v2.0"
#define FRAMEWORKDIR "C:\\WINDOWS\\Microsoft.NET\\Framework"
#define FRAMEWORKVERSION "v2.0.50727"

      "FrameworkDir=" FRAMEWORKDIR "\0"
      "FrameworkVersion=" FRAMEWORKVERSION "\0"
      "FrameworkSDKDir=" FRAMEWORKSDKDIR "\0"

      "DevEnvDir=" VSINSTALLDIR "\0"
      "MSVCDir=" VCINSTALLDIR "\\VC7"
      "PATH=" VSINSTALLDIR ";" VCINSTALLDIR "\\BIN;" VCINSTALLDIR "\\Common7\\Tools;" VCINSTALLDIR "\\Common7\\Tools\\bin\\prerelease;" VCINSTALLDIR "\\Common7\\Tools\\bin;"
         FRAMEWORKSDKDIR "\\bin;" FRAMEWORKDIR "\\" FRAMEWORKVERSION
         ";" VCINSTALLDIR "\\VCPackages"
         ";C:\\WINDOWS\\system32;C:\\WINDOWS;C:\\WINDOWS\\System32\\Wbem;\0"
      "INCLUDE=" VCINSTALLDIR "\\ATLMFC\\INCLUDE;"
         VCINSTALLDIR "\\INCLUDE;"
         VCINSTALLDIR "\\PlatformSDK\\include\\prerelease;"
         VCINSTALLDIR "\\PlatformSDK\\include;"
         FRAMEWORKSDKDIR "\\include;"
         VSROOT "\\FrameworkSDK\\include\0"
      "LIB="
         VCINSTALLDIR "\\ATLMFC\\LIB;"
         VCINSTALLDIR "\\LIB;"
         VCINSTALLDIR "\\PlatformSDK\\lib\\prerelease;"
         VCINSTALLDIR "\\PlatformSDK\\lib;"
         FRAMEWORKSDKDIR "\\lib;"
         VSROOT "\\FrameworkSDK\\Lib\0"
      "LIBPATH="
         FRAMEWORKDIR "\\" FRAMEWORKVERSION ";"
         VCINSTALLDIR "\\ATLMFC\\LIB\0"

#if 0    // slightly less old code, for vc7
#define VCINSTALL "C:\\Program Files\\Microsoft Visual Studio .NET\\Vc7"
#define VCFRAMEWORK "C:\\Program Files\\Microsoft Visual Studio .NET\\FrameworkSDK"

   char  szEnv[] =
#define VSINSTALLDIR "C:\\Program Files\\Microsoft Visual Studio .NET\\Common7\\IDE"
#define VCINSTALLDIR "C:\\Program Files\\Microsoft Visual Studio .NET"
#define FRAMEWORKSDKDIR "C:\\Program Files\\Microsoft Visual Studio .NET\\FrameworkSDK"
#define FRAMEWORKDIR "C:\\WINDOWS\\Microsoft.NET\\Framework"
#define FRAMEWORKVERSION "v1.0.3705"

      "FrameworkDir=" FRAMEWORKDIR "\0"
      "FrameworkVersion=" FRAMEWORKVERSION "\0"
      "FrameworkSDKDir=" FRAMEWORKSDKDIR "\0"

      "DevEnvDir=" VSINSTALLDIR "\0"
      "MSVCDir=" VCINSTALLDIR "\\VC7"
      "PATH=" VSINSTALLDIR ";" VCINSTALLDIR "\\BIN;" VCINSTALLDIR "\\Common7\\Tools;" VCINSTALLDIR "\\Common7\\Tools\\bin\\prerelease;" VCINSTALLDIR "\\Common7\\Tools\\bin;"
         FRAMEWORKSDKDIR "\bin;" FRAMEWORKDIR "\\" FRAMEWORKVERSION
         ";C:\\WINDOWS\\system32;C:\\WINDOWS;C:\\WINDOWS\\System32\\Wbem;\0"
      "INCLUDE=" VCINSTALLDIR "\\ATLMFC\\INCLUDE;"
         VCINSTALLDIR "\\INCLUDE;"
         VCINSTALLDIR "\\PlatformSDK\\include\\prerelease;"
         VCINSTALLDIR "\\PlatformSDK\\include;"
         FRAMEWORKSDKDIR "\\include;"
         "C:\\Program Files\\Microsoft Visual Studio .NET\\FrameworkSDK\\include\0"
      "LIB="
         VCINSTALLDIR "\\ATLMFC\\LIB;"
         VCINSTALLDIR "\\LIB;"
         VCINSTALLDIR "\\PlatformSDK\\lib\\prerelease;"
         VCINSTALLDIR "\\PlatformSDK\\lib;"
         FRAMEWORKSDKDIR "\\lib;"
         "C:\\Program Files\\Microsoft Visual Studio .NET\\FrameworkSDK\\Lib\0"
#endif // 0

#if 0 // old code
      "TMP=C:\\WINDOWS\\TEMP\0"
      "TEMP=C:\\WINDOWS\\TEMP\0"
      "PROMPT=$p$g\0"
      "winbootdir=C:\\WINDOWS\0"
      "COMSPEC=C:\\WINDOWS\\COMMAND.COM\0"
      "MSINPUT=C:\\MSINPUT\0"
      "windir=C:\\WINDOWS\0"
      "SNDSCAPE=C:\\WINDOWS\0"
      "BLASTER=A220 I7 D1 T2 \0"
      "VSCOMMONDIR=C:\\Program Files\\Microsoft Visual Studio\\Common\0"
      "MSDEVDIR=C:\\Program Files\\Microsoft Visual Studio\\Common\\msdev98\0"
      "MSVCDIR=C:\\Program Files\\Microsoft Visual Studio\\VC98\0"
      "VCOSDIR=WIN95\0"
      "INCLUDE=C:\\Program Files\\Microsoft Visual Studio\\VC98\\ATL\\INCLUDE;C:\\Program Files\\Microsoft Visual Studio\\VC98\\INCLUDE;C:\\Program Files\\Microsoft Visual Studio\\VC98\\MFC\\INCLUDE;\0"
      "LIB=C:\\Program Files\\Microsoft Visual Studio\\VC98\\LIB;C:\\Program Files\\Microsoft Visual Studio\\VC98\\MFC\\LIB;\0"
      "PATH=C:\\Program Files\\Microsoft Visual Studio\\COMMON\\MSDEV98\\BIN;C:\\Program Files\\Microsoft Visual Studio\\VC98\\BIN;C:\\Program Files\\Microsoft Visual Studio\\COMMON\\TOOLS\\WIN95;C:\\Program Files\\Microsoft Visual Studio\\COMMON\\TOOLS;C:\\WINDOWS\\SYSTEM;C:\\WINDOWS;C:\\WINDOWS\\COMMAND\0"
#endif // 0
      "\0"
      ;
   memset (&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset (&pi, 0, sizeof(pi));
#ifdef USENMAKE
   if (!CreateProcess (
      VCINSTALLDIR "\\bin\\nmake.exe",
      "-f " ZDRIVE "\\setup\\setup\\setup.mak CFG=\"Setup - Win32 Release\"",
      NULL, NULL,
      FALSE,
      0,
      NULL, // BUGFIX - So use setvars... szEnv, // environment
      ZDRIVE "\\setup\\setup",
      &si,
      &pi
      )) {
      printf ("Error: Can't create the makefile\n");
      return -1;
   }
#else
   PSTR pszFile = VSINSTALLDIR "\\devenv.exe";
   PSTR pszParam = "\"" VSINSTALLDIR "\\devenv.exe" "\" " ZDRIVE "\\setup\\setup\\setup.sln /build Release";
   if (!CreateProcess (
      NULL, // BUGFIX - Dont use  pszFile,
      pszParam,
      NULL, NULL,
      FALSE,
      0,
      NULL, // BUGFIX - So use setvars... szEnv, // environment
      ZDRIVE "\\setup\\setup",
      &si,
      &pi
      )) {

      DWORD dwErr = GetLastError ();
      printf ("Error: Can't create run buildenv\n");
      return -1;
   }
#endif
   WaitForSingleObject (pi.hProcess, INFINITE);

   // copy the file over
   printf ("Copying self extracting file\n");
   if (!CopyFile (
      ZDRIVE "\\setup\\setup\\release\\setup.exe",
      szSelfExe,
      FALSE))
      printf ("Error: Couldn't copy the file!\n");
   printf ("Done\n");


   return 0;
}
