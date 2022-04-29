/************************************************************************8
MakeBuild - generate buildnum.h
*/

#include <windows.h>
#include <stdio.h>

int main (int argc, char **argv)
{
   if (argc != 2)
      return -1;

   SYSTEMTIME  st;
   GetSystemTime (&st);

   int   iNum;
   iNum = st.wDay + st.wMonth * 100 + (st.wYear-2002) * 1200;
   
   char  sz[] =
   "/* buildnum.h*/\n"
   "\n"
   "\n"
   "#ifndef BUILDNUM_H\n"
   "#define BUILDNUM_H\n"
   "/* NOTE: To change build number, change the definitions\n"
   " * of both BN_BN and BUILD_NUMBER. The rest will use\n"
   " * the BN_BN definition.\n"
   " *\n"
   " * Change log:\n"
   " */\n"
   "#define BN_BN   %d\n"
   "#define BN_MAJOR   2\n"
   "#define BN_STR  \"%d\"\n"
   "#ifndef BUILD_NUMBER\n"
   "#define BUILD_NUMBER \"2.1.\" BN_STR \"\\0\"\n"
   "#endif\n"
   "\n"
   "#ifndef JUSTDEFINES\n"
   "// char gszBuildNumber[] = \"@(#) Dragonfly Build \" BUILD_NUMBER;\n"
   "#endif\n"
   "\n"
   "#ifndef BN_PRODUCT_VERSION\n"
   "#define BN_PRODUCT_VERSION 2,1,0,BN_BN\n"
   "#endif\n"
   "\n"
   "#ifndef BN_FILE_VER\n"
   "#define BN_FILE_VERSION BN_PRODUCT_VERSION\n"
   "#else\n"
   "#define BN_FILE_VERSION BN_FILE_VER,BN_BN\n"
   "#endif//BN_FILE_VER\n"
   "\n"
   "#ifndef BN_FILE_STR\n"
   "#define BN_FILE_VERSION_STR BUILD_NUMBER\n"
   "#else\n"
   "#define BN_FILE_VERSION_STR BN_FILE_STR \" \" BN_STR \"\\0\"\n"
   "#endif\n"
   "\n"
   "#ifndef BN_FLAGS\n"
   "    #ifdef _DEBUG\n"
   "        #define BN_FLAGS VS_FF_DEBUG\n"
   "    #else\n"
   "        #ifdef TEST\n"
   "            #define BN_FLAGS VS_FF_PRERELEASE\n"
   "        #else\n"
   "            #define BN_FLAGS 0\n"
   "        #endif\n"
   "    #endif\n"
   "#endif\n"
   "\n"
   "#ifndef BN_FLAGSMASK\n"
   "#define BN_FLAGSMASK VS_FF_PRERELEASE | VS_FF_DEBUG\n"
   "#endif//BN_FLAGSMASK\n"
   "\n"
   "#ifndef BN_PRODUCTNAME\n"
   "#define BN_PRODUCTNAME \"Dragonfly\\0\"\n"
   "#endif//BN_PRODUCTNAME\n"
   "\n"
   "#ifndef BN_COPYRIGHT\n"
   "#define BN_COPYRIGHT \"Copyright © 2000-2004 mXac\\0\"\n"
   "#endif//BN_COPYRIGHT\n"
   "\n"
   "#endif // BUILDNUM_H\n";

   FILE  *f;
   f = fopen (argv[1], "wt");
   fprintf (f, sz, iNum, iNum);
   fclose (f);

   return 0;
}


