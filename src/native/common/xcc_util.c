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

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/system_properties.h>
#include "xcc_errno.h"
#include "xcc_fmt.h"
#include "xcc_util.h"

const char* xcc_util_get_signame(const siginfo_t* si)
{
    switch (si->si_signo)
    {
    case SIGABRT:   return "SIGABRT";
    case SIGBUS:    return "SIGBUS";
    case SIGFPE:    return "SIGFPE";
    case SIGILL:    return "SIGILL";
    case SIGSEGV:   return "SIGSEGV";
    case SIGSTKFLT: return "SIGSTKFLT";
    default:        return "?";
    }
}

const char* xcc_util_get_sigcodename(const siginfo_t* si)
{
    // Try the signal-specific codes...
    switch (si->si_signo) {
    case SIGBUS:
        switch (si->si_code)
        {
        case BUS_ADRALN:    return "BUS_ADRALN";
        case BUS_ADRERR:    return "BUS_ADRERR";
        case BUS_OBJERR:    return "BUS_OBJERR";
        case BUS_MCEERR_AR: return "BUS_MCEERR_AR";
        case BUS_MCEERR_AO: return "BUS_MCEERR_AO";
        default:            break;
        }
        break;
    case SIGFPE:
        switch (si->si_code)
        {
        case FPE_INTDIV:   return "FPE_INTDIV";
        case FPE_INTOVF:   return "FPE_INTOVF";
        case FPE_FLTDIV:   return "FPE_FLTDIV";
        case FPE_FLTOVF:   return "FPE_FLTOVF";
        case FPE_FLTUND:   return "FPE_FLTUND";
        case FPE_FLTRES:   return "FPE_FLTRES";
        case FPE_FLTINV:   return "FPE_FLTINV";
        case FPE_FLTSUB:   return "FPE_FLTSUB";
        default:           break;
        }
        break;
    case SIGILL:
        switch (si->si_code)
        {
        case ILL_ILLOPC:   return "ILL_ILLOPC";
        case ILL_ILLOPN:   return "ILL_ILLOPN";
        case ILL_ILLADR:   return "ILL_ILLADR";
        case ILL_ILLTRP:   return "ILL_ILLTRP";
        case ILL_PRVOPC:   return "ILL_PRVOPC";
        case ILL_PRVREG:   return "ILL_PRVREG";
        case ILL_COPROC:   return "ILL_COPROC";
        case ILL_BADSTK:   return "ILL_BADSTK";
        default:           break;
        }
        break;
    case SIGSEGV:
        switch (si->si_code)
        {
        case SEGV_MAPERR:  return "SEGV_MAPERR";
        case SEGV_ACCERR:  return "SEGV_ACCERR";
        case SEGV_BNDERR:  return "SEGV_BNDERR";
        case SEGV_PKUERR:  return "SEGV_PKUERR";
        default:           break;
        }
        break;
    default:
        break;
    }
    
    // Then the other codes...
    switch (si->si_code) {
    case SI_USER:     return "SI_USER";
    case SI_KERNEL:   return "SI_KERNEL";
    case SI_QUEUE:    return "SI_QUEUE";
    case SI_TIMER:    return "SI_TIMER";
    case SI_MESGQ:    return "SI_MESGQ";
    case SI_ASYNCIO:  return "SI_ASYNCIO";
    case SI_SIGIO:    return "SI_SIGIO";
    case SI_TKILL:    return "SI_TKILL";
    case SI_DETHREAD: return "SI_DETHREAD";
    }
    
    // Then give up...
    return "?";
}

int xcc_util_signal_has_si_addr(const siginfo_t* si)
{
    //manually sent signals won't have si_addr
    if (si->si_code == SI_USER || si->si_code == SI_QUEUE || si->si_code == SI_TKILL) return 0;

    switch (si->si_signo)
    {
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
    case SIGSEGV:
    case SIGTRAP:
        return 1;
    default:
        return 0;
    }
}

int xcc_util_signal_has_sender(const siginfo_t* si, pid_t caller_pid)
{
    return (SI_FROMUSER(si) && (si->si_pid != 0) && (si->si_pid != caller_pid)) ? 1 : 0;
}

int xcc_util_atoi(const char *str, int *i)
{
    //We have to do this job very carefully for some unusual version of stdlib.
    
    long  val = 0;
    char *endptr = NULL;
    const char *p = str;

    //check
    if(NULL == str || NULL == i) return XCC_ERRNO_INVAL;
    if((*p < '0' || *p > '9') && *p != '-') return XCC_ERRNO_INVAL;
    p++;
    while(*p)
    {
        if(*p < '0' || *p > '9') return XCC_ERRNO_INVAL;
        p++;
    }

    //convert
    errno = 0;
    val = strtol(str, &endptr, 10);

    //check
    if((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0))
        return XCC_ERRNO_INVAL;
    if(endptr == str)
        return XCC_ERRNO_INVAL;
    if(val > INT_MAX || val < INT_MIN)
        return XCC_ERRNO_INVAL;

    //OK
    *i = (int)val;
    return 0;
}

char *xcc_util_trim(char *start)
{
    char *end;

    if(NULL == start) return NULL;
    
    end = start + strlen(start);
    if(start == end) return start;
    
    while(start < end && isspace((int)(*start))) start++;
    if(start == end) return start;

    while(start < end && isspace((int)(*(end - 1)))) end--;
    *end = '\0';
    return start;
}

int xcc_util_write(int fd, const char *buf, size_t len)
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *ptr;

    if(fd < 0) return XCC_ERRNO_INVAL;

    ptr   = buf;
    nleft = len;

    while(nleft > 0)
    {
        errno = 0;
        if((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if(nwritten < 0 && errno == EINTR)
                nwritten = 0; /* call write() again */
            else
                return XCC_ERRNO_SYS;    /* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }

    return 0;
}

int xcc_util_write_str(int fd, const char *str)
{
    const char *tmp = str;
    size_t      len = 0;

    if(fd < 0) return XCC_ERRNO_INVAL;
    
    while(*tmp) tmp++;
    len = tmp - str;
    if(0 == len) return 0;

    return xcc_util_write(fd, str, len);
}

int xcc_util_write_format(int fd, const char *format, ...)
{
    va_list ap;
    char    buf[1024];
    ssize_t len;

    if(fd < 0) return XCC_ERRNO_INVAL;
    
    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if(len <= 0) return 0;
    
    return xcc_util_write(fd, buf, len);
}

int xcc_util_write_format_safe(int fd, const char *format, ...)
{
    va_list ap;
    char    buf[1024];
    ssize_t len;

    if(fd < 0) return XCC_ERRNO_INVAL;
    
    va_start(ap, format);
    len = xcc_fmt_vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if(len <= 0) return 0;
    
    return xcc_util_write(fd, buf, len);
}

char *xcc_util_gets(char *s, int size, int fd)
{
    ssize_t i, nread;
    char    c, *p;

    if(fd < 0 || NULL == s || size < 2) return NULL;
    
    s[0] = '\0';
    p = s;
    
    for(i = 0; i < size - 1; i++)
    {
        if(1 == (nread = read(fd, &c, 1)))
        {
            *p++ = c;
            if('\n' == c) break;
        }
        else if(0 == nread) //EOF
        {
            break;
        }
        else
        {
            if (errno != EINTR) return NULL;
        }
    }

    *p = '\0';
    return ('\0' == s[0] ? NULL : s);
}

int xcc_util_read_file_line(const char *path, char *buf, size_t len)
{
    int fd = -1;
    int r = 0;
    
    if(0 > (fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC))))
    {
        r = XCC_ERRNO_SYS;
        goto end;
    }
    if(NULL == xcc_util_gets(buf, len, fd))
    {
        r = XCC_ERRNO_SYS;
        goto end;
    }

 end:
    if(fd >= 0) close(fd);
    return r;
}

int xcc_util_get_process_name(pid_t pid, char *buf, size_t len)
{
    char path[128];
    
    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    return xcc_util_read_file_line(path, buf, len);
}
    
int xcc_util_get_thread_name(pid_t tid, char *buf, size_t len)
{
    char path[128];
    
    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/comm", tid);
    
    return xcc_util_read_file_line(path, buf, len);
}

static const char *xcc_util_su_pathnames[] =
{
    "/data/local/su",
    "/data/local/bin/su",
    "/data/local/xbin/su",
    "/system/xbin/su",
    "/system/bin/su",
    "/system/bin/.ext/su",
    "/system/bin/failsafe/su",
    "/system/sd/xbin/su",
    "/system/usr/we-need-root/su",
    "/sbin/su",
    "/su/bin/su"
};

int xcc_util_is_root()
{
    size_t i;
    for(i = 0; i < sizeof(xcc_util_su_pathnames) / sizeof(xcc_util_su_pathnames[0]); i++)
        if(0 == access(xcc_util_su_pathnames[i], F_OK))
            return 1;

    return 0;
}

static char *xcc_util_parse_prop(char *buf, const char *key)
{
    size_t  buf_len = strlen(buf);
    size_t  key_len = strlen(key);
    char   *val;

    if(buf_len <= key_len + 1) return NULL;
    if(0 != memcmp(buf, key, key_len)) return NULL;
    if('=' != buf[key_len]) return NULL;

    val = xcc_util_trim(buf + (key_len + 1));
    if(NULL == val || 0 == strlen(val)) return NULL;
    return val;
}

static char *xcc_util_parse_prop_str(char *buf, const char *key)
{
    char *val = xcc_util_parse_prop(buf, key);

    return NULL == val ? NULL : strdup(val);
}

static int xcc_util_parse_prop_int(char *buf, const char *key)
{
    char *val = xcc_util_parse_prop(buf, key);
    int   val_int = 0;

    if(NULL == val) return 0; //failed
    if(0 != xcc_util_atoi(val, &val_int)) return 0; //failed
    return val_int;
}

static char *xcc_util_get_prop_str(const char *key)
{
    char buf[PROP_VALUE_MAX];
    memset(buf, 0, PROP_VALUE_MAX);

    __system_property_get(key, buf);
    if('\0' == buf[0]) return NULL;

    return strdup(buf);
}

static int xcc_util_get_prop_int(const char *key)
{
    char buf[PROP_VALUE_MAX];
    memset(buf, 0, PROP_VALUE_MAX);

    __system_property_get(key, buf);
    if('\0' == buf[0]) return 0;

    int val = 0;
    if(0 != xcc_util_atoi(buf, &val)) return 0;
    return val;
}

void xcc_util_load_build_prop(xcc_util_build_prop_t *prop)
{
    FILE   *fp;
    char    buf[256];
    char   *abi = NULL;
    char   *abi2 = NULL;
    size_t  len;

    memset(prop, 0, sizeof(xcc_util_build_prop_t));

    //read props from build.prop
    if(NULL != (fp = fopen("/system/build.prop", "r")))
    {
        while(fgets(buf, sizeof(buf), fp))
        {
            if(0 == prop->api_level)                   prop->api_level         = xcc_util_parse_prop_int(buf, "ro.build.version.sdk");
            if(NULL == prop->os_version)               prop->os_version        = xcc_util_parse_prop_str(buf, "ro.build.version.release");
            if(NULL == prop->manufacturer)             prop->manufacturer      = xcc_util_parse_prop_str(buf, "ro.product.manufacturer");
            if(NULL == prop->brand)                    prop->brand             = xcc_util_parse_prop_str(buf, "ro.product.brand");
            if(NULL == prop->model)                    prop->model             = xcc_util_parse_prop_str(buf, "ro.product.model");
            if(NULL == prop->build_fingerprint)        prop->build_fingerprint = xcc_util_parse_prop_str(buf, "ro.build.fingerprint");
            if(NULL == prop->revision)                 prop->revision          = xcc_util_parse_prop_str(buf, "ro.revision");
            if(NULL == prop->abi_list)                 prop->abi_list          = xcc_util_parse_prop_str(buf, "ro.product.cpu.abilist");
            if(NULL == prop->abi_list && NULL == abi)  abi                     = xcc_util_parse_prop_str(buf, "ro.product.cpu.abi");
            if(NULL == prop->abi_list && NULL == abi2) abi2                    = xcc_util_parse_prop_str(buf, "ro.product.cpu.abi2");
        }
        fclose(fp);
    }

    //read props from __system_property_get
    if(0 == prop->api_level)                   prop->api_level         = xcc_util_get_prop_int("ro.build.version.sdk");
    if(NULL == prop->os_version)               prop->os_version        = xcc_util_get_prop_str("ro.build.version.release");
    if(NULL == prop->manufacturer)             prop->manufacturer      = xcc_util_get_prop_str("ro.product.manufacturer");
    if(NULL == prop->brand)                    prop->brand             = xcc_util_get_prop_str("ro.product.brand");
    if(NULL == prop->model)                    prop->model             = xcc_util_get_prop_str("ro.product.model");
    if(NULL == prop->build_fingerprint)        prop->build_fingerprint = xcc_util_get_prop_str("ro.build.fingerprint");
    if(NULL == prop->revision)                 prop->revision          = xcc_util_get_prop_str("ro.revision");
    if(NULL == prop->abi_list)                 prop->abi_list          = xcc_util_get_prop_str("ro.product.cpu.abilist");
    if(NULL == prop->abi_list && NULL == abi)  abi                     = xcc_util_get_prop_str("ro.product.cpu.abi");
    if(NULL == prop->abi_list && NULL == abi2) abi2                    = xcc_util_get_prop_str("ro.product.cpu.abi2");

    //build abi_list from abi and abi2
    if(NULL == prop->abi_list && (NULL != abi || NULL != abi2))
    {
        if(NULL != abi)  len = snprintf(buf, sizeof(buf), "%s", abi);
        if(NULL != abi2) snprintf(buf + len, sizeof(buf) - len, ",%s", abi2);
        prop->abi_list = strdup(buf);
    }

    //default value
    if(NULL == prop->os_version)        prop->os_version = "";
    if(NULL == prop->manufacturer)      prop->manufacturer = "";
    if(NULL == prop->brand)             prop->brand = "";
    if(NULL == prop->model)             prop->model = "";
    if(NULL == prop->build_fingerprint) prop->build_fingerprint = "";
    if(NULL == prop->revision)          prop->revision = "";
    if(NULL == prop->abi_list)          prop->abi_list = "";

    if(NULL != abi)  free(abi);
    if(NULL != abi2) free(abi2);
}

void xcc_util_get_kernel_version(char *buf, size_t len)
{
    struct utsname uts;

    if(0 != uname(&uts))
    {
        strncpy(buf, "unknown", len);
        buf[len - 1] = '\0';
        return;
    }

    snprintf(buf, len, "%s version %s %s (%s)", uts.sysname, uts.release, uts.version, uts.machine);
}
