#ifndef RTC_H_
#define RTC_H_

#include <TimeLib.h>
#include <Timezone.h>

extern time_t utc;
extern time_t local;
extern bool SystemTimeUpdated;
extern void printDateTime(time_t t, const char *tz);
  
//RTC
void startSystemTime();
void setLocalTime();

#endif