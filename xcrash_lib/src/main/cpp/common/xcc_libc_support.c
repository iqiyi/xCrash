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

// Created by Sim Sun on 2019-09-17.

#include <stddef.h>
#include "xcc_libc_support.h"
#include "xcc_errno.h"

void *xcc_libc_support_memset(void *s, int c, size_t n)
{
    char *p = (char *)s;
    
    while(n--)
        *p++ = (char)c;
    
    return s;
}

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define XCC_LIC_SUPPORT_ISLEAP(year)         ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define XCC_LIC_SUPPORT_SECS_PER_HOUR        (60 * 60)
#define XCC_LIC_SUPPORT_SECS_PER_DAY         (XCC_LIC_SUPPORT_SECS_PER_HOUR * 24)
#define XCC_LIC_SUPPORT_DIV(a, b)            ((a) / (b) - ((a) % (b) < 0))
#define XCC_LIC_SUPPORT_LEAPS_THRU_END_OF(y) (XCC_LIC_SUPPORT_DIV(y, 4) - XCC_LIC_SUPPORT_DIV(y, 100) + XCC_LIC_SUPPORT_DIV(y, 400))

static const unsigned short int xcc_libc_support_mon_yday[2][13] =
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
struct tm *xcc_libc_support_localtime_r(const time_t *timep, long gmtoff, struct tm *result)
{
    time_t days, rem, y;
    const unsigned short int *ip;

    if(NULL == result) return NULL;

    result->tm_gmtoff = gmtoff;

    days = ((*timep) / XCC_LIC_SUPPORT_SECS_PER_DAY);
    rem = ((*timep) % XCC_LIC_SUPPORT_SECS_PER_DAY);
    rem += gmtoff;
    while (rem < 0)
    {
        rem += XCC_LIC_SUPPORT_SECS_PER_DAY;
        --days;
    }
    while (rem >= XCC_LIC_SUPPORT_SECS_PER_DAY)
    {
        rem -= XCC_LIC_SUPPORT_SECS_PER_DAY;
        ++days;
    }
    result->tm_hour = (int)(rem / XCC_LIC_SUPPORT_SECS_PER_HOUR);
    rem %= XCC_LIC_SUPPORT_SECS_PER_HOUR;
    result->tm_min = (int)(rem / 60);
    result->tm_sec = rem % 60;
    /* January 1, 1970 was a Thursday.  */
    result->tm_wday = (4 + days) % 7;
    if (result->tm_wday < 0)
        result->tm_wday += 7;
    y = 1970;

    while (days < 0 || days >= (XCC_LIC_SUPPORT_ISLEAP(y) ? 366 : 365))
    {
        /* Guess a corrected year, assuming 365 days per year.  */
        time_t yg = y + days / 365 - (days % 365 < 0);

        /* Adjust DAYS and Y to match the guessed year.  */
        days -= ((yg - y) * 365
                 + XCC_LIC_SUPPORT_LEAPS_THRU_END_OF (yg - 1)
                 - XCC_LIC_SUPPORT_LEAPS_THRU_END_OF (y - 1));

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
    ip = xcc_libc_support_mon_yday[XCC_LIC_SUPPORT_ISLEAP(y)];
    for (y = 11; days < (long int) ip[y]; --y)
        continue;
    days -= ip[y];
    result->tm_mon = (int)y;
    result->tm_mday = (int)(days + 1);
    return result;
}

