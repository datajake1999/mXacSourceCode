/************************************************************************
Measure.cpp - Handles english and metric units. And shows/converts
   strings to/from units.

begun 15/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define  MK_NOSYMBOL                0  // got to the end and didn't find any symbol
#define  MK_BADSYMBOL               1  // found a symbol but it wasn't what was expected
#define  MK_METERS                  10
#define  MK_CENTIMETERS             11
#define  MK_MILLIMETERS             12
#define  MK_FEET                    13
#define  MK_INCHES                  14
#define  MK_KM                      15
#define  MK_MILES                   16

typedef struct {
   char     *pszKeyword;
   DWORD    dwID;
} KWINFO, *PKWINFO;


static KWINFO gaKWInfo[] = {
   "meters", MK_METERS,
   "metres", MK_METERS,
   "meter", MK_METERS,
   "metre", MK_METERS,
   "km", MK_KM,
   "km.", MK_KM,
   "centimeters", MK_CENTIMETERS,
   "centimetres", MK_CENTIMETERS,
   "centimeter", MK_CENTIMETERS,
   "centimetre", MK_CENTIMETERS,
   "cm.", MK_CENTIMETERS,
   "cm", MK_CENTIMETERS,
   "millimeters", MK_MILLIMETERS,
   "millimetres", MK_MILLIMETERS,
   "millimeter", MK_MILLIMETERS,
   "millimetre", MK_MILLIMETERS,
   "mm.", MK_MILLIMETERS,
   "mm", MK_MILLIMETERS,
   "mil.", MK_MILLIMETERS,
   "mil", MK_MILLIMETERS,
   "feet", MK_FEET,
   "foot", MK_FEET,
   "ft.", MK_FEET,
   "ft", MK_FEET,
   "'", MK_FEET,
   "miles", MK_MILES,
   "mi.", MK_MILES,
   "inches", MK_INCHES,
   "inch", MK_INCHES,
   "in.", MK_INCHES,
   "in", MK_INCHES,
   "\"", MK_INCHES,
   "m.", MK_METERS,
   "m", MK_METERS
};

static BOOL gfKnowDefUnits = FALSE;   // set to TRUE if we've decided default units
static BOOL gfMetric = TRUE;          // set to TRUE if metric, FALSE if english
static DWORD gdwUnitsMetric = MUNIT_METRIC_METERS;          // units, see MUNIT_XXX]
static DWORD gdwUnitsEnglish = MUNIT_ENGLISH_FEET;          // units to display in english


/*******************************************************************************
MeasureDefaultUnits - Returns the default units to use. If the units to
use are unknown checkes the local ID and uses that.

returns
   DWORD - MUNIT_XXX
*/
DWORD MeasureDefaultUnits (void)
{
   if (!gfKnowDefUnits) {
      char szTemp[64];
      szTemp[0] = 0;

      GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_IMEASURE, (LPSTR)szTemp, sizeof(szTemp));
      gfMetric = !(szTemp[0] == '1');
      gfKnowDefUnits = TRUE;
   }

   return gfMetric ? gdwUnitsMetric : gdwUnitsEnglish;
}

/*******************************************************************************
MeasureDefaultUnitsSet - Set the default units.

inputs
   DWORD dwUnits - MUNIT_XXX
retursn
   none
*/
void MeasureDefaultUnitsSet (DWORD dwUnits)
{
   gfKnowDefUnits = TRUE;
   if (dwUnits & MUNIT_ENGLISH) {
      gfMetric = FALSE;
      gdwUnitsEnglish = dwUnits;
   }
   else {
      gfMetric = TRUE;
      gdwUnitsMetric = dwUnits;
   }
}



/*******************************************************************************
MeasureParseDouble - Skips whitespace until it gets to a number and parses it
as a fp. Sees how many character it eats up.

NOTE: This doesn't deal with the sign, '-'. That's assumed to be elsewhere

inputs
   char     *psz - Where to start looking
   DWORD    *pdwUsed - Filled with the number of characters used
   fp   *pf - Filled with the fp value pulled out
returns
   BOOL - TRUE if got something that could be a fp, FALSE if completely failed
*/
BOOL MeasureParseDouble (char *psz, DWORD *pdwUsed, fp *pf)
{
   DWORD i;
   for (i = 0; psz[i] == ' '; i++);

   fp fValue, fFraction;
   BOOL fFoundDigit;

   fValue = 0;
   fFraction = 0.1;

   // looking for numbers followed by period followed by numbers
   fFoundDigit = FALSE;
   for (; (psz[i] >= '0') && (psz[i] <= '9'); i++) {
      fValue = fValue * 10 + (psz[i] - '0');
      fFoundDigit = TRUE;
   }

   // if the next character is not period then done
   if (psz[i] != '.') {
      *pf = fValue;
      *pdwUsed = i;
      return fFoundDigit;  // at least got an integer out of it
   }
   i++;  // eat up dot

   // get more digits
   fFoundDigit = FALSE;
   for (; (psz[i] >= '0') && (psz[i] <= '9'); i++) {
      fValue = fValue + (psz[i] - '0') * fFraction;
      fFraction /= 10;
      fFoundDigit = TRUE;
   }

   // if had a period and no digits then error, else success
   *pf = fValue;
   *pdwUsed = i;
   return fFoundDigit;

}


/*******************************************************************************
MeasureParseInt - Skips whitespace until it gets to a number and parses it
as an integer. Sees how many character it eats up.

NOTE: This doesn't deal with the sign, '-'. That's assumed to be elsewhere

inputs
   char     *psz - Where to start looking
   DWORD    *pdwUsed - Filled with the number of characters used
   int       *pi - Filled with the fp value pulled out
returns
   BOOL - TRUE if got something that could be a fp, FALSE if completely failed
*/
BOOL MeasureParseInt (char *psz, DWORD *pdwUsed, int *pi)
{
   DWORD i;
   for (i = 0; psz[i] == ' '; i++);

   int iValue;
   BOOL fFoundDigit;

   iValue = 0;

   // looking for numbers
   fFoundDigit = FALSE;
   for (; (psz[i] >= '0') && (psz[i] <= '9'); i++) {
      iValue = iValue * 10 + (psz[i] - '0');
      fFoundDigit = TRUE;
   }

   *pi = iValue;
   *pdwUsed = i;
   return fFoundDigit;
}

/*******************************************************************************
MeasureParseFraction - Skips whitespace until it gets to a number and parses it
as an integer. It looks for a fraction, which is either an integer by itself,
or an integer followed by a space, integer, '/', and integer.

NOTE: This doesn't deal with the sign, '-'. That's assumed to be elsewhere

inputs
   char     *psz - Where to start looking
   DWORD    *pdwUsed - Filled with the number of characters used
   fp   *pf - Filled with the fp value pulled out
returns
   BOOL - TRUE if got something that could be a fraction, FALSE if completely failed
*/
BOOL MeasureParseFraction (char *psz, DWORD *pdwUsed, fp *pf)
{
   // get an integer
   DWORD dwUsedInt;
   int   iValInt;
   if (!MeasureParseInt (psz, &dwUsedInt, &iValInt))
      return FALSE;
   DWORD dwUsedDenom;
   DWORD i;
   int iValDenom;

   // if have a '/' already then that was just the denominator
   for (; psz[dwUsedInt] == ' '; dwUsedInt++);
   if (psz[dwUsedInt] == '/') {
      i = dwUsedInt+1;
      iValDenom = iValInt;
      iValInt = 0;
      goto foundslash;
   }

   // look for the denominator
   if (!MeasureParseInt (psz + dwUsedInt, &dwUsedDenom, &iValDenom) || !iValDenom) {
      // at least have integer
      *pdwUsed = dwUsedInt;
      *pf = iValInt;
      return TRUE;
   }

   // look for a slash
   for (i = dwUsedInt + dwUsedDenom; psz[i] == ' '; i++);
   if (psz[i] != '/')
      return FALSE;  // if no slash no good
   i++;

foundslash:
   // numerator
   DWORD dwUsedNum;
   int iValNum;
   if (!MeasureParseInt (psz + i, &dwUsedNum, &iValNum) || !iValNum)
      return FALSE;

   // got it
   *pf = (fp) iValInt + (fp)iValDenom / (fp)iValNum;
   *pdwUsed = i + dwUsedNum;
   return TRUE;
}

/*******************************************************************************
MeasureIsRestBlank - Looks at the string and returns TRUE if it's all blank to the end.

inputs
   char        *psz
returns
   BOOL - TRUE if it's all blank to the end, FALSE if there's text
*/
BOOL MeasureIsRestBlank (char *psz)
{
   DWORD i;
   for (i = 0; psz[i] == ' '; i++);
   return (psz[i] == 0);
}



/*******************************************************************************
MeasureParseLongestKeyword - Looks for a keyword and parses the longest one.

inputs
   char        *psz - string to parse. may have spaces before
   DWORD       *pdwUsed - filled in with the number used
returns
   DWORD - keyword, MK_XXX
*/
DWORD MeasureParseLongestKeyword (char *psz, DWORD *pdwUsed)
{
   DWORD i;
   for (i = 0; psz[i] == ' '; i++);
   if (psz[i] == 0) {
      *pdwUsed = i;
      return MK_NOSYMBOL;
   }

   // try to find
   DWORD j;
   for (j = 0; j < sizeof(gaKWInfo) / sizeof(KWINFO); j++)
      if (!_strnicmp (psz + i, gaKWInfo[j].pszKeyword, strlen(gaKWInfo[j].pszKeyword))) {
         // found symbol
         *pdwUsed = i + (DWORD)strlen(gaKWInfo[j].pszKeyword);
         return gaKWInfo[j].dwID;
      }
   
   // else, didn't match anything
   return MK_BADSYMBOL;
}


/*******************************************************************************
MeasureParseString - Given an ANSI string, this parses the string
and returns the distance in meters.

inputs
   char        *psz - String to parse
   fp      *pf - Filled the the value in meters
returns
   BOOL - if parsed successfuly, FALSE if couldn't parse it all
*/
BOOL MeasureParseString (char *psz, fp *pf)
{
   // skip spaces
   DWORD i;
   DWORD dwKeyword, dwUsedKey;
   for (i = 0; psz[i] == ' '; i++);

   // look for negative
   BOOL fNegative;
   fNegative = FALSE;
   if (psz[i] == '-') {
      i++;
      fNegative = TRUE;
   }
   psz += i;

   // look for a fp followed by an identifier
   DWORD dwUsed;
   if (MeasureParseDouble (psz, &dwUsed, pf)) {
      // found the fp, now find the identifier
      dwKeyword = MeasureParseLongestKeyword (psz + dwUsed, &dwUsedKey);
     
      // if the keyword is unidentified then skip
      if (dwKeyword == MK_BADSYMBOL)
         goto try2;

      // make sure there's nothing after they keyword
      if (!MeasureIsRestBlank (psz + dwUsed + dwUsedKey))
         goto try2;

      // if keyword is unknown then use defaults
      if (dwKeyword == MK_NOSYMBOL) switch (MeasureDefaultUnits()) {
         case MUNIT_METRIC_CENTIMETERS:
            dwKeyword = MK_CENTIMETERS;
            break;
         case MUNIT_METRIC_MILLIMETERS:
            dwKeyword = MK_MILLIMETERS;
            break;
         case MUNIT_ENGLISH_FEET:
            dwKeyword = MK_FEET;
            break;
         case MUNIT_ENGLISH_INCHES:
            dwKeyword = MK_INCHES;
            break;
         case MUNIT_ENGLISH_FEETINCHES:
            // can't have defaults using this
            goto try2;
            break;
         default:
         case MUNIT_METRIC_METERS:
            dwKeyword = MK_METERS;
            break;
      }

      // right units
      if (fNegative)
         *pf *= -1;
      switch (dwKeyword) {
      case MK_CENTIMETERS:
         *pf /= 100;
         break;
      case MK_MILLIMETERS:
         *pf /= 1000;
         break;
      case MK_MILES:
         *pf *= METERSPERFOOT * FEETPERMETER;
         break;
      case MK_FEET:
         *pf *= METERSPERFOOT;
         break;
      case MK_INCHES:
         *pf *= METERSPERINCH;
         break;
      case MK_KM:
         *pf *= 1000;
         break;
      default:
      case MK_METERS:
         // do nothing
         break;
      }

      // done
      return TRUE;
   }

try2:
   // if we got here we know it's either feet or inches AND it's not a decimal number
   // followed by feet/inches

   // try a fractional number by itself, or followed by feet/inches
   if (MeasureParseFraction (psz, &dwUsed, pf)) {
      // found the fp, now find the identifier
      dwKeyword = MeasureParseLongestKeyword (psz + dwUsed, &dwUsedKey);
     
      // if the keyword is unidentified then skip
      if (dwKeyword == MK_BADSYMBOL)
         goto try3;

      // make sure there's nothing after they keyword
      if (!MeasureIsRestBlank (psz + dwUsed + dwUsedKey))
         goto try3;

      // if keyword is unknown then use defaults
      if (dwKeyword == MK_NOSYMBOL) switch (MeasureDefaultUnits()) {
         case MUNIT_ENGLISH_FEET:
            dwKeyword = MK_FEET;
            break;
         case MUNIT_ENGLISH_INCHES:
            dwKeyword = MK_INCHES;
            break;
         default:
         case MUNIT_METRIC_CENTIMETERS:
         case MUNIT_METRIC_MILLIMETERS:
         case MUNIT_METRIC_METERS:
         case MUNIT_ENGLISH_FEETINCHES:
            // metric units are not typically given as fractions, so a fraction is illegal
            goto try3;
            break;
      }

      // right units
      if (fNegative)
         *pf *= -1;
      switch (dwKeyword) {
      case MK_FEET:
         *pf *= METERSPERFOOT;
         break;
      case MK_INCHES:
         *pf *= METERSPERINCH;
         break;
      default:
         // metric units are not typically given as fractions, so a fraction is illegal
         goto try3;
      }

      // done
      return TRUE;
   }

try3:
   // try an integer followed by feet. followed by a fractional number followed by inches
   int   iVal;
   if (!MeasureParseInt (psz, &dwUsed, &iVal))
      return FALSE;
   psz += dwUsed;

   // get the units. if not feed then error
   dwKeyword = MeasureParseLongestKeyword (psz, &dwUsedKey);
   if (dwKeyword != MK_FEET)
      return FALSE;
   psz += dwUsedKey;

   // now should have a fractional amount in inches
   if (!MeasureParseFraction (psz, &dwUsed, pf))
      return FALSE;
   psz += dwUsed;

   // get the units. should be inches
   dwKeyword = MeasureParseLongestKeyword (psz, &dwUsedKey);
   if (dwKeyword != MK_INCHES)
      return FALSE;
   psz += dwUsedKey;

   // shouldn't be anything after this
   if (!MeasureIsRestBlank(psz))
      return FALSE;

   // have value
   *pf = *pf + iVal * 12;
   *pf *= METERSPERINCH;

   if (fNegative)
      *pf *= -1;

   return TRUE;
};

/*******************************************************************************
MeasureParseString - Given an UNICODE string, this parses the string
and returns the distance in meters.

inputs
   WCHAR        *psz - String to parse
   fp      *pf - Filled the the value in meters
returns
   BOOL - if parsed successfuly, FALSE if couldn't parse it all
*/
BOOL MeasureParseString (WCHAR *psz, fp *pf)
{
   char  szTemp[256];
   WideCharToMultiByte (CP_ACP, 0, psz, -1, szTemp, sizeof(szTemp), 0, 0);
   return MeasureParseString (szTemp, pf);
}

/*******************************************************************************
MeasureParseString - Given an text control in a page, this parses the string
and returns the distance in meters.

inputs
   PCEscPage      pPage - page
   WCHAR        *pszControl - Control name
   fp      *pf - Filled the the value in meters
returns
   BOOL - if parsed successfuly, FALSE if couldn't parse it all
*/
BOOL MeasureParseString (PCEscPage pPage, WCHAR *pszControl, fp *pf)
{
   *pf = 0;

   PCEscControl pc;
   pc = pPage->ControlFind (pszControl);
   if (!pc)
      return FALSE;

   WCHAR szTemp[256];
   DWORD dwNeeded;
   szTemp[0] = 0;
   pc->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);

   return MeasureParseString (szTemp, pf);
}

/*******************************************************************************
ApplyGrid - Applies a grid to the number, rounding up/down to the nearest grid.

inputs
   fp      fValue - value
   fp      fGrid - Grid size
returns
   fp - rounded value
*/
fp ApplyGrid (fp fValue, fp fGrid)
{
   if (fGrid <= EPSILON)
      return fValue;

   if (fValue >= 0) {
      fValue += fGrid / 2;
      fValue -= fmod(fValue, fGrid);
      return fValue;
   }
   else {
      fValue *= -1;
      fValue += fGrid / 2;
      fValue -= fmod(fValue, fGrid);
      fValue *= -1;
      return fValue;
   }
}


/*******************************************************************************
MeasureToString - Converts a measurement to a string.

inputs
   fp   fValue - value
   char     *psz - Fill this in
   BOOL  fAccurate - Displays numbers to 10x as accuate as before
returns
   none
*/
void MeasureToString (fp fValue, char *psz, BOOL fAccurate)
{
   // show minus sign
   if (fValue < 0) {
      psz[0] = '-';
      psz++;
      fValue *= -1;
   }

   // BUGFIX - If have units like mm and end up with really large number then
   // automagically use larger

   // BUGFIX - If default units in meters and get a small value then go to mm
   DWORD dwUnits;
   dwUnits = MeasureDefaultUnits();
   switch (dwUnits) {
   case MUNIT_METRIC_CENTIMETERS:
   case MUNIT_METRIC_MILLIMETERS:
      if (fValue >= 1000)
         dwUnits = MUNIT_METRIC_KM;
      else if (fValue >= 5)
         dwUnits = MUNIT_METRIC_METERS;
      break;

   case MUNIT_METRIC_METERS:
      if (fValue >= 1000)
         dwUnits = MUNIT_METRIC_KM;
      else if (fValue < .1)
         dwUnits = MUNIT_METRIC_MILLIMETERS;
      break;

   case MUNIT_ENGLISH_FEETINCHES:
   case MUNIT_ENGLISH_FEET:
      if (fValue >= 1000)
         dwUnits = MUNIT_ENGLISH_MILES;
      else if (fValue < .1)
         dwUnits = MUNIT_ENGLISH_INCHES;
      break;

   case MUNIT_ENGLISH_INCHES:
      if (fValue >= 1000)
         dwUnits = MUNIT_ENGLISH_MILES;
      else if (fValue >= 5)
         dwUnits = MUNIT_ENGLISH_FEETINCHES;
      break;
   }

   // BUGFIX - Make accyrate go to .01 mm
   switch (dwUnits) {
   case MUNIT_METRIC_CENTIMETERS:
      sprintf (psz, "%g cm", (double)ApplyGrid (fValue*100, 0.1 * (fAccurate ? .01 : 1)));  // show to millimeter accuracy
      break;

   case MUNIT_METRIC_MILLIMETERS:
      sprintf (psz, "%g mm", (double)ApplyGrid (fValue*1000, 1 * (fAccurate ? .01 : 1)));  // show to millimeter accuracy
      break;

   case MUNIT_ENGLISH_FEETINCHES:
      {
         int iFeet, iInches, iSixteenth;
         fValue = ApplyGrid (fValue * INCHESPERMETER, 1.0 / 16.0 * (fAccurate ? .01 : 1));
         iFeet = (int) (fValue / 12);
         fValue -= (iFeet * 12);
         iInches = (int) fValue;
         fValue -= iInches;
         iSixteenth = (int) (fValue * 16);

         // if they're all 0 then show 0
         if (!iFeet && !iInches && !iSixteenth) {
            strcpy (psz, "0 in.");
            break;
         }

         // if feet show feet
         if (iFeet) {
            sprintf (psz, (iInches || iSixteenth) ? "%d ft. " : "%d ft.", iFeet);
            psz += strlen(psz);
         }

         // if inches show inches
         if (iInches || iSixteenth) {
            if (iInches) {
               sprintf (psz, iSixteenth ? "%d " : "%d", iInches);
               psz += strlen(psz);
            }

            if (iSixteenth) {
               int iFrac;
               iFrac = 16;
               while (!(iSixteenth % 2)) {
                  iSixteenth /= 2;
                  iFrac /= 2;
               }
               sprintf (psz, "%d/%d", iSixteenth, iFrac);
            }

            strcat (psz, " in.");
         }
      }
      break;

   case MUNIT_ENGLISH_INCHES:
      {
         int iInches, iSixteenth;
         fValue = ApplyGrid (fValue * INCHESPERMETER, 1.0 / 16.0 * (fAccurate ? .01 : 1));
         iInches = (int) fValue;
         fValue -= iInches;
         iSixteenth = (int) (fValue * 16);

         // if they're all 0 then show 0
         if (!iInches && !iSixteenth) {
            strcpy (psz, "0 in.");
            break;
         }

         if (iInches) {
            sprintf (psz, iSixteenth ? "%d " : "%d", iInches);
            psz += strlen(psz);
         }

         if (iSixteenth) {
            int iFrac;
            iFrac = 16;
            while (!(iSixteenth % 2)) {
               iSixteenth /= 2;
               iFrac /= 2;
            }
            sprintf (psz, "%d/%d", iSixteenth, iFrac);
         }

         strcat (psz, " in.");
      }
      break;

   case MUNIT_ENGLISH_FEET:
      sprintf (psz, "%g ft.", (double)ApplyGrid (fValue * FEETPERMETER, 0.01 * (fAccurate ? .01 : 1)));  // show to millimeter accuracy
      break;

   case MUNIT_METRIC_KM:
      sprintf (psz, "%g km", (double)ApplyGrid (fValue / 1000, 0.001 * (fAccurate ? .01 : 1)));  // show to km accuracy
      break;

   case MUNIT_ENGLISH_MILES:
      sprintf (psz, "%g mi.", (double)ApplyGrid (fValue * FEETPERMETER / FEETPERMILE, 0.001 * (fAccurate ? .01 : 1)));  // show to miles accuracy
      break;

   case MUNIT_METRIC_METERS:
   default:
      sprintf (psz, "%g m", (double)ApplyGrid (fValue, 0.001 * (fAccurate ? .01 : 1)));  // show to millimeter accuracy
      break;
   }
}


/*******************************************************************************
MeasureToString - Converts a measurement to a string.

inputs
   fp   fValue - value
   WCHAR     *psz - Fill this in
   BOOL  fAccurate - Displays numbers to 10x as accuate as before
returns
   none
*/
void MeasureToString (fp fValue, WCHAR *psz, BOOL fAccurate)
{
   char szTemp[256];
   MeasureToString (fValue, szTemp, fAccurate);
   MultiByteToWideChar (CP_ACP, 0, szTemp, -1, psz, 256);
}

/*******************************************************************************
MeasureToString - Given an text control in a page, this generates the string
for the measurement and sets the control.

inputs
   PCEscPage      pPage - page
   WCHAR        *pszControl - Control name
   fp      fValue - Value
   BOOL  fAccurate - Displays numbers to 10x as accuate as before
returns
   none
*/
void MeasureToString (PCEscPage pPage, WCHAR *pszControl, fp fValue, BOOL fAccurate)
{
   WCHAR szTemp[256];
   MeasureToString (fValue, szTemp, fAccurate);

   PCEscControl pc;
   pc = pPage->ControlFind (pszControl);
   if (pc)
      pc->AttribSet (gszText, szTemp);
}


/*******************************************************************************
MeasureFindScale - Given a distance, this finds the next largest (round) distance
to use.

inputs
   fp          fScale - Scale to look for
returns
   fp - Next largest (rounded) distance to use
*/
fp MeasureFindScale (fp fScale)
{
   fp fUnits;
   if (MeasureDefaultUnits() & MUNIT_ENGLISH) {
      if (fScale <= METERSPERFOOT)
         fUnits = METERSPERINCH;
      else if (fScale <= 10.0 * METERSPERFOOT)
         fUnits = METERSPERFOOT;
      else if (fScale <= 100.0 * METERSPERFOOT)
         fUnits = 10.0 * METERSPERFOOT;
      else if (fScale <= 1000.0 * METERSPERFOOT)
         fUnits = 100.0 * METERSPERFOOT;
      else if (fScale <= METERSPERMILE)
         fUnits = 1000.0 * METERSPERFOOT;
      else if (fScale <= 10.0 * METERSPERMILE)
         fUnits = METERSPERMILE;
      else if (fScale <= 100.0 * METERSPERMILE)
         fUnits = 10.0 * METERSPERMILE;
   }
   else {
      fUnits = log10(max(fScale,CLOSE));
      fUnits = floor(fUnits);
      fUnits = pow(10, fUnits);
   }

   fScale = fScale / fUnits;
   fScale = ceil(fScale);
   fScale *= fUnits;
   return fScale;
}
