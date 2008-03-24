
// this file is needed for mingw compiling as mingw does not use ctime_r 
#ifdef __MINGW32__

#include <cstdlib>
#include <ctime>
#include <cstdio>

#define SECSPERMIN  60L
#define MINSPERHOUR 60L
#define HOURSPERDAY 24L
#define SECSPERHOUR (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY  (SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK 7
#define MONSPERYEAR 12

#define YEAR_BASE   1900
#define EPOCH_YEAR      1970
#define EPOCH_WDAY      4

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

static const int mon_lengths[2][MONSPERYEAR] = {
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
} ;

static const int year_lengths[2] = {
  365,
  366
} ;

struct tm *localtime_r( const time_t *tim_p, struct tm *res )
{
  long days, rem;
  int y;
  int yleap;
  const int *ip;

  days = ((long) *tim_p) / SECSPERDAY;
  rem = ((long) *tim_p) % SECSPERDAY;
  while (rem < 0) 
    {
      rem += SECSPERDAY;
      --days;
    }
  while (rem >= SECSPERDAY)
    {
      rem -= SECSPERDAY;
      ++days;
    }
 
  /* compute hour, min, and sec */  
  res->tm_hour = (int) (rem / SECSPERHOUR);
  rem %= SECSPERHOUR;
  res->tm_min = (int) (rem / SECSPERMIN);
  res->tm_sec = (int) (rem % SECSPERMIN);

  /* compute day of week */
  if ((res->tm_wday = ((EPOCH_WDAY + days) % DAYSPERWEEK)) < 0)
    res->tm_wday += DAYSPERWEEK;

  /* compute year & day of year */
  y = EPOCH_YEAR;
  if (days >= 0)
    {
      for (;;)
    {
      yleap = isleap(y);
      if (days < year_lengths[yleap])
        break;
      y++;
      days -= year_lengths[yleap];
    }
    }
  else
    {
      do
    {
      --y;
      yleap = isleap(y);
      days += year_lengths[yleap];
    } while (days < 0);
    }

  res->tm_year = y - YEAR_BASE;
  res->tm_yday = days;
  ip = mon_lengths[yleap];
  for (res->tm_mon = 0; days >= ip[res->tm_mon]; ++res->tm_mon)
    days -= ip[res->tm_mon];
  res->tm_mday = days + 1;

  /* set daylight saving time flag */
  res->tm_isdst = -1;
  
  return (res);
}

char *asctime_r( const struct tm *tim_p, char *result )
{
  static char day_name[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  static char mon_name[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  sprintf (result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
       day_name[tim_p->tm_wday], 
       mon_name[tim_p->tm_mon],
       tim_p->tm_mday, tim_p->tm_hour, tim_p->tm_min,
       tim_p->tm_sec, 1900 + tim_p->tm_year);
  return result;
}
char *ctime_r( const time_t *clock, char * buf)
{
    struct tm tm;
    return asctime_r (localtime_r (clock, &tm), buf);
}
#endif // __MINGW32__

