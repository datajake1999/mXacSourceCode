/* OilPaintKey.cpp -


inputs
   OilPaintKey.exe <EmailName>

outputs
   Registration key through printf


Sample usage:

  OilPaintKey.exe MikeRozak@bigpond.com

The output is:
   895316199
*/

#include <stdio.h>
#include <stdlib.h>

typedef unsigned int DWORD;   // int=32 bits

#define  RANDNUM1    0x3498af93     // change for every app
#define  RANDNUM2    0x14f8c924     // change for every app
#define  RANDNUM3    0x5d3819c1     // change for every app

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
      MySRand ((DWORD) tolower (psz[i]));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}



int main (int argc, char **argv)
{
   if (argc != 2) {
      printf ("OilPaintKey.exe <EmailName>");
      return -1;
   }

   DWORD dw;
   dw = HashString (argv[1]);
   printf ("%u\n", dw);

   return 0;

}