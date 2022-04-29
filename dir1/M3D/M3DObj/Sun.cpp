/************************************************************************
Sun.cpp - Functions for determining the location of the sun, the moon,
and brightness.

begun 15/9/01 by Mike Rozak
Copyright 2001 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"


#define     SMALLNUMBER    0.01  // BUGFIX - Was .001, but increased because of roundoff error when fp==float

/***************************************************************************
DFDATEToTimeInYear - Takes the month and the date and returns a number from
0..11.9999 indicating the time of the year. 0 = january 1, 11.9999 = dec 31
*/
fp DFDATEToTimeInYear (DFDATE dwDate)
{
   DWORD dwMonth, dwYear, dwDay;
   dwMonth = MONTHFROMDFDATE(dwDate);
   dwDay = DAYFROMDFDATE(dwDate);
   dwYear = YEARFROMDFDATE(dwDate);

   fp f;
   f = ((fp)dwDay-1) / EscDaysInMonth ((int)dwMonth, (int)dwYear, NULL);
   f += (fp) dwMonth - 1;
   return f;
}

/***************************************************************************
DFTIMEToTimeInDay - Takes a DFTIME settings and returns a number from 0..23.999
for the time of the day. 0 = 12:00 midnitht, 23.9999 = 11:59 PM
*/
fp DFTIMEToTimeInDay (DFTIME dwTime)
{
   return (fp)HOURFROMDFTIME(dwTime) + MINUTEFROMDFTIME(dwTime) / 60.0;
}

/***************************************************************************
TimeInYearDayToDFDATETIME - Converts a year, time in the year (from 0..1 for
Janury 1 to Dec 31) and time of day (from 0..1 for 12:00 midnight to 11:59 PM)
and fills in DFDATE and DFTIME. This also does modulo in case the date and
time have wrapped around.

inputs
   fp       fTimeInDay - Time of day from 0..23.9999
   fp       fTimeInYear - Time in the year from 0..11.9999
   DWORD    dwYear - Year
   DFTIME   *pdwTime - Filled with the time
   DFDATE   *pdwDate - Filled with the date
returns
   none
*/
void TimeInYearDayToDFDATETIME (fp fTimeInDay, fp fTimeInYear, DWORD dwYear,
                                DFTIME *pdwTime, DFDATE *pdwDate)
{
   fp fMinute, fHour, fDayOfMonth, fMonth;

   // modulo
   fTimeInDay = myfmod (fTimeInDay, 24);
   fTimeInYear = myfmod (fTimeInYear, 12);

   // calc
   fHour = floor(fTimeInDay);
   fMinute = (fTimeInDay - fHour) * 60.0;
   fMonth = floor(fTimeInYear) + 1;
   fDayOfMonth = (fTimeInYear - fMonth+1) * EscDaysInMonth ((int)fMonth, (int)dwYear, NULL) + 1;

   *pdwTime = TODFTIME ((int)fHour, (int) fMinute);
   *pdwDate = TODFDATE ((int)fDayOfMonth, (int) fMonth, (int) dwYear);
}


/***************************************************************************
PositionInSolar - Given a time, date, and latitude, this will return an XYZ
position in the solar system, with the sun at 0,0,0.

inputs
   fp         fTime - 0 to 23.999. 0 = 12:00 am, 23.999 = 11:59 pm, etc.
   fp         fDay - 0 to 11.999. 0 = Jan 1. 11.9999 = Dec 31
   fp         fLatitude - Latitude in degrees. Positive values are north
   PCPoint        pSolar - Filled in the the location relative to solar system
   BOOL       fOriginEarth - if TRUE, want the origin to be the center of earth, FALSE want to be sun.
                  Do this for numberical accuracy reasons
returns
   none
*/
void PositionInSolar (fp fTime, fp fDay, fp fLatitude, PCPoint pSolar, BOOL fOriginEarth)
{
   fTime /= 24.0;
   fDay /= 12.0;

   CRenderMatrix rm;

   // december 22nd is actually the shortest day, not jan1
   fDay += 9 / 31.0 / 12.0;

#define SUNDIST      93000000
   rm.Translate (cos(fDay * 2 * PI) * SUNDIST, sin(fDay * 2 * PI) * SUNDIST,0);  // Earth is 93 million miles away
   rm.Rotate (22.5 / 360.0 * 2 * PI, 2);  // earth is tilted at 22.5 degrees, around Y
   rm.Rotate ((fTime + fDay) * 2 * PI, 3);   // rotates around once a day. Take into account that rotates
                                             // a bit futher every day
   rm.Rotate (-fLatitude / 360.0 * 2 * PI, 2);  // Latitude is a Y rotation

   // BUGFIX - Translate, removing the center of the earth to take care of float precision problems...
   // But, easist way is to get the matrix and then remove the terms oneself
   CMatrix m;
   rm.CTMGet (&m);
   if (fOriginEarth) {
      m.p[3][0] = 0;
      m.p[3][1] = 0;
      m.p[3][2] = 0;
   }

   // center of earch
   //CPoint pEarthCent;
   //pEarthCent.Zero();
   //pEarthCent.p[3] = 1;
   //rm.Transform (1, &pEarthCent);
   //rm.Translate (-pEarthCent.p[0], -pEarthCent.p[1], -pEarthCent.p[2]);
   // BUGFIX - Removing by the center of the earth takes care of precision problems
   
   CPoint src;
   src.Zero();
   if (fOriginEarth)
      src.p[0] = 4000;  // earth is 4000 miles in radius
   src.p[3] = 1;
   //rm.Transform (&src, pSolar);
   m.Multiply (&src, pSolar);

   // BUGFIX - If use float then don't have enough precision, so do doulbe-precision
   // matrix multiply here
   //CMatrix m;
   //rm.CTMGet (&m);
   //#define  MMP(i)   (fp)(pSolar->p[i] = (double)m.p[0][i] * (double)src.p[0] + (double)m.p[1][i] * (double)src.p[1] + (double)m.p[2][i] * (double)src.p[2] + (double)m.p[3][i] * (double)src.p[3])
   //MMP(0);
   //MMP(1);
   //MMP(2);
   //MMP(3);
   //#undef MMP
   // done
}

/***************************************************************************
FindUDNSEWAtLatitude - Given a time, date, and Latitude, this returns
normalized vectors for north, east, and up.

inputs
   fp         fTime - 0 to 23.99. 0 = 12:00 am, 23.99 = 11:59 pm, etc.
   fp         fDay - 0 to 12.0. 0 = Jan 1. 11.9999 = Dec 31
   fp         fLatitude - Latitude in degrees. Positive values are north
   PCPoint        pEast, pNorth, pUp - Filled in the the normalized dirctions
returns
   none
*/
void FindUDNSEWAtLatitude (fp fTime, fp fDay, fp fLatitude,
                            PCPoint pEast, PCPoint pNorth, PCPoint pUp)
{
   // if Latitude is at the poles then move it just off the poles or we
   // wont calculate this right
   if (fLatitude + 2 * SMALLNUMBER > 90)
      fLatitude = 90 - 2 * SMALLNUMBER;
   if (fLatitude - 2 * SMALLNUMBER < -90)
      fLatitude = -90 + 2 * SMALLNUMBER;

   // get locations for current, east, and north
   CPoint pCur, pToEast, pToNorth;
   PositionInSolar (fTime-SMALLNUMBER*10, fDay, fLatitude, &pCur, TRUE);
   PositionInSolar (fTime+SMALLNUMBER*10, fDay, fLatitude, &pToEast, TRUE);  // changed from -smallnumber to +smallnumber
   pEast->Subtract (&pToEast, &pCur);
   pEast->Normalize();

   PositionInSolar (fTime, fDay, fLatitude - SMALLNUMBER, &pCur, TRUE);
   PositionInSolar (fTime, fDay, fLatitude + SMALLNUMBER, &pToNorth, TRUE);
   pNorth->Subtract (&pToNorth, &pCur);
   pNorth->Normalize();

   // calculate up
   pUp->CrossProd (pEast, pNorth);
   pUp->Normalize();

   // recalculate east so it's perpendicular
   pEast->CrossProd (pNorth, pUp);
   pEast->Normalize();

}

/***************************************************************************
SunVector - Given a time, date, and Latitude, this returns a vector to
the sun. X=east, Y=north, Z=up.

inputs
   DWORD          dwYear - Year
   fp             fTimeInYear - From 0 to 12, for Jan1 to 11.9999=Dec31
   fp             fTimeInDay - From 0 to 24, for 12:00 midnight to 23.999=11:59 PM
   fp         fLatitude - In degrees. Negative for south
   PCPoint        pSun - Vector to the sun. This is normalized
returns
   none
*/
void SunVector (DWORD dwYear, fp fTimeInYear, fp fTimeInDay, fp fLatitude, PCPoint pSun)
{
   // convert the time and date into numbers from 0 to 1
   fp fTime, fDate;
   fTime = myfmod(fTimeInDay,24);
   fDate = myfmod(fTimeInYear,12);

   // get the point in space and the normals
   CPoint pLoc, pEast, pNorth, pUp;
   PositionInSolar (fTime, fDate, fLatitude, &pLoc, FALSE);
   FindUDNSEWAtLatitude (fTime, fDate, fLatitude, &pEast,&pNorth, &pUp);

   // flip the sign since the sun is at 0,0,0, and the vector pointint to the sun
   // is 0,0,0 - pLoc
   pLoc.Scale (-1);

   // dot-product
   pSun->p[0] = pEast.DotProd (&pLoc);
   pSun->p[1] = pNorth.DotProd (&pLoc);
   pSun->p[2] = pUp.DotProd (&pLoc);
   pSun->p[3] = 1;
   pSun->Normalize();

   // done
}



/***************************************************************************
SunVector - Given a time, date, and Latitude, this returns a vector to
the sun. X=east, Y=north, Z=up.

inputs
   DFTIME         dwTime - time
   DFDATE         dwDate - date
   fp         fLatitude - In degrees. Negative for south
   PCPoint        pSun - Vector to the sun. This is normalized
returns
   none
*/
void SunVector (DFTIME dwTime, DFDATE dwDate, fp fLatitude, PCPoint pSun)
{
   // convert the time and date into numbers from 0 to 1
   fp fTime, fDate;
   fTime = DFTIMEToTimeInDay(dwTime);
   fDate = DFDATEToTimeInYear (dwDate);

   SunVector (YEARFROMDFDATE(dwDate), fDate, fTime, fLatitude, pSun);
}


/**********************************************************************
PhaseOfMoon - Given a date and time, this returns the phase of the mooon.

inputs
   DWORD          dwYear - Year
   fp             fTimeInYear - From 0 to 12, for Jan1 to 11.999=Dec31
   fp             fTimeInDay - From 0 to 24, for 12:00 midnight to 23.999=11:59 PM
returns
   double - Phase. 0 = no light. 0.5 = Full light, 1.0 = no light.
*/
double PhaseOfMoon (DWORD dwYear, fp fTimeInYear, fp fTimeInDay)
{
   __int64 iRef, iCur;

   // get the date and time
   DFDATE date;
   DFTIME time;
   TimeInYearDayToDFDATETIME (fTimeInDay, fTimeInYear, dwYear, &time, &date);

   // adjust the date and time for the time zone, GMT
   iCur = DFDATEToMinutes (date) + HOURFROMDFTIME(time) * 60 + MINUTEFROMDFTIME(time);
   TIME_ZONE_INFORMATION tzi;
   memset (&tzi, 0, sizeof(tzi));
   GetTimeZoneInformation (&tzi);
   iCur -= tzi.Bias;

   // reference is new moon sunday, Mar 25, 20001, 00:46 GMT.
   iRef = DFDATEToMinutes (TODFDATE(25,3,2001)) + 46;//DFTIMEToMinutes(TODFTIME(0,46));

   // how many minutes in a moon cycle
   int   iMoon;
   iMoon = (29 * 24 + 12) * 60 + 44;

   // so that means
   iRef = iCur - iRef;
   // fix broken mod
   if (iRef < 0)
      iRef += ((int)(-iRef) / iMoon + 1) * iMoon;
   iRef = iRef % iMoon;

   return (double) (int) iRef / (double) iMoon;
}


/***************************************************************************
MoonVector - Given a time, date, and Latitude, this returns a vector to
the moon. X=east, Y=north, Z=up.

inputs
   DWORD          dwYear - Year
   fp             fTimeInYear - From 0 to 11.999, for Jan1 to 11.999=Dec31
   fp             fTimeInDay - From 0 to 23.999, for 12:00 midnight to 23.999=11:59 PM
   fp             fLatitude - In degrees. Negative for south
   PCPoint        pMoon - Vector to the sun. This is normalized
returns
   fp - The phase of the moon
*/
fp MoonVector (DWORD dwYear, fp fTimeInYear, fp fTimeInDay, fp fLatitude, PCPoint pMoon)
{
   // find the phase of the moon
   fp fPhase = PhaseOfMoon(dwYear, fTimeInYear, fTimeInDay);

   // basically, location of the moon is the location of the sun MINUS
   // the phase x 24 hours
   fTimeInDay += (1.0 - fPhase) * 24.0;
   //int   iTime;
   //iTime = HOURFROMDFTIME(dwTime) * 60 + MINUTEFROMDFTIME(dwTime);
   //iTime = (iTime + (int) ((1.0 - fPhase) * 24 * 60)) % (24 * 60);

   SunVector (dwYear, fTimeInYear, fTimeInDay, fLatitude, pMoon);

   return fPhase;
}

/***************************************************************************
MoonVector - Given a time, date, and Latitude, this returns a vector to
the moon. X=east, Y=north, Z=up.

inputs
   DFTIME         dwTime - time
   DFDATE         dwDate - date
   fp             fLatitude - In degrees. Negative for south
   PCPoint        pMoon - Vector to the sun. This is normalized
returns
   fp - The phase of the moon
*/
fp MoonVector (DFTIME dwTime, DFDATE dwDate, fp fLatitude, PCPoint pMoon)
{
   fp fTime, fDate;
   fTime = DFTIMEToTimeInDay(dwTime);
   fDate = DFDATEToTimeInYear (dwDate);

   return MoonVector (YEARFROMDFDATE(dwDate), fDate, fTime, fLatitude, pMoon);
}