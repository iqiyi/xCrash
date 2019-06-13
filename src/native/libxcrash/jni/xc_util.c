// Copyright (c) 2019-present, iQIYI, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by caikelun on 2019-03-07.

#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xc_util.h"

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define XCA_UTIL_ISLEAP(year)         ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define XCA_UTIL_SECS_PER_HOUR        (60 * 60)
#define XCA_UTIL_SECS_PER_DAY         (XCA_UTIL_SECS_PER_HOUR * 24)
#define XCA_UTIL_DIV(a, b)            ((a) / (b) - ((a) % (b) < 0))
#define XCA_UTIL_LEAPS_THRU_END_OF(y) (XCA_UTIL_DIV(y, 4) - XCA_UTIL_DIV(y, 100) + XCA_UTIL_DIV(y, 400))

static const unsigned short int xca_util_mon_yday[2][13] =
{
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/* Compute the `struct tm' representation of *T,
   offset GMTOFF seconds east of UTC,
   and store year, yday, mon, mday, wday, hour, min, sec into *RESULT.
   Return RESULT if successful.  */
struct tm *xca_util_time2tm(const time_t timev, long gmtoff, struct tm *result)
{
    time_t days, rem, y;
    const unsigned short int *ip;

    if(NULL == result) return NULL;

    result->tm_gmtoff = gmtoff;

    days = timev / XCA_UTIL_SECS_PER_DAY;
    rem = timev % XCA_UTIL_SECS_PER_DAY;
    rem += gmtoff;
    while (rem < 0)
    {
        rem += XCA_UTIL_SECS_PER_DAY;
        --days;
    }
    while (rem >= XCA_UTIL_SECS_PER_DAY)
    {
        rem -= XCA_UTIL_SECS_PER_DAY;
        ++days;
    }
    result->tm_hour = (int)(rem / XCA_UTIL_SECS_PER_HOUR);
    rem %= XCA_UTIL_SECS_PER_HOUR;
    result->tm_min = (int)(rem / 60);
    result->tm_sec = rem % 60;
    /* January 1, 1970 was a Thursday.  */
    result->tm_wday = (4 + days) % 7;
    if (result->tm_wday < 0)
        result->tm_wday += 7;
    y = 1970;

    while (days < 0 || days >= (XCA_UTIL_ISLEAP(y) ? 366 : 365))
    {
        /* Guess a corrected year, assuming 365 days per year.  */
        time_t yg = y + days / 365 - (days % 365 < 0);

        /* Adjust DAYS and Y to match the guessed year.  */
        days -= ((yg - y) * 365
                 + XCA_UTIL_LEAPS_THRU_END_OF (yg - 1)
                 - XCA_UTIL_LEAPS_THRU_END_OF (y - 1));

        y = yg;
    }
    result->tm_year = (int)(y - 1900);
    if (result->tm_year != y - 1900)
    {
        /* The year cannot be represented due to overflow.  */
        errno = EOVERFLOW;
        return NULL;
    }
    result->tm_yday = (int)days;
    ip = xca_util_mon_yday[XCA_UTIL_ISLEAP(y)];
    for (y = 11; days < (long int) ip[y]; --y)
        continue;
    days -= ip[y];
    result->tm_mon = (int)y;
    result->tm_mday = (int)(days + 1);
    return result;
}

char *xc_util_strdupcat(const char *s1, const char *s2)
{
    size_t s1_len, s2_len;
    char *s;

    if(NULL == s1 || NULL == s2) return NULL;

    s1_len = strlen(s1);
    s2_len = strlen(s2);

    if(NULL == (s = malloc(s1_len + s2_len + 1))) return NULL;
    memcpy(s, s1, s1_len);
    memcpy(s + s1_len, s2, s2_len + 1);

    return s;
}

int xc_util_mkdirs(const char *dir)
{
    size_t  len;
    char    buf[PATH_MAX];
    char   *p;

    //check or try create dir directly
    errno = 0;
    if(0 == mkdir(dir, S_IRWXU) || EEXIST == errno) return 0;

    //trycreate dir recursively...

    len = strlen(dir);
    if(0 == len) return XCC_ERRNO_INVAL;
    if('/' != dir[0]) return XCC_ERRNO_INVAL;
    
    memcpy(buf, dir, len + 1);
    if(buf[len - 1] == '/') buf[len - 1] = '\0';
    
    for(p = buf + 1; *p; p++)
    {
        if(*p == '/')
        {
            *p = '\0';
            errno = 0;
            if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
            *p = '/';
        }
    }
    errno = 0;
    if(0 != mkdir(buf, S_IRWXU) && EEXIST != errno) return errno;
    return 0;
}
