#include "EpochTime.h"

uint32_t getEpochTime(void){  
  Time tm;
  tm = rtc.getTime();
  int i;
  uint32_t seconds;
  int year = tm.year - 1970;
 
  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds= year*(SECS_PER_DAY * 365);
  for (i = 0; i < year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }
 
  // add days for this year, months start from 1
  for (i = 1; i < tm.mon; i++) {
    if ( (i == 2) && LEAP_YEAR(year)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  seconds+= (tm.date-1) * SECS_PER_DAY;
  seconds+= tm.hour * SECS_PER_HOUR;
  seconds+= tm.min * SECS_PER_MIN;
  seconds+= tm.sec;
  return seconds;
}