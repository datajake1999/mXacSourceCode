/* M3DKey.cpp -


inputs
   M3DKey.exe <EmailName>

outputs
   Registration key through printf


Sample usage:

  M3DKey.exe MikeRozak@bigpond.com

The output is:
   ???
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef unsigned int DWORD;   // int=32 bits

#define  RANDNUM1    0x8a3498af     // change for every app
#define  RANDNUM2    0x1bf8c924     // change for every app
#define  RANDNUM3    0x5d388cc1     // change for every app

/**********************************************************************************
MySRand, MyRand - Personal random functions.
*/
static DWORD   gdwRandSeed;

void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

DWORD MyRand (void)
{
   gdwRandSeed = (gdwRandSeed ^ RANDNUM1) * (gdwRandSeed ^ RANDNUM2) +
      (gdwRandSeed ^ RANDNUM3);

   return gdwRandSeed;
}



/**********************************************************************************
HashString - Hash an E-mail (or other string) to a DWORD number. Use this as the
registration key.

inputs
   char  *psz
returns
   DWORD
*/
DWORD HashString (char *psz)
{
   DWORD dwSum;

   DWORD i;
   dwSum = 324233;
   for (i = 0; psz[i]; i++) {
      MySRand ((DWORD) _tolower (psz[i]));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}



int main (int argc, char **argv)
{
   if (argc != 2) {
      printf ("M3DKey.exe <EmailName>");
      return -1;
   }

   DWORD dw;
   dw = HashString (argv[1]);
   printf ("%u\n", dw);

   return 0;

}