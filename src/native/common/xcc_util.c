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
#include <time.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/system_properties.h>
#include "xcc_util.h"
#include "xcc_errno.h"
#include "xcc_fmt.h"
#include "xcc_version.h"
#include "xcc_libc_support.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wformat-nonliteral"

#define XCC_UTIL_TIME_FORMAT "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld%02ld"

const char* xcc_util_get_signame(const siginfo_t* si)
{
    switch (si->si_signo)
    {
    case SIGABRT:   return "SIGABRT";
    case SIGBUS:    return "SIGBUS";
    case SIGFPE:    return "SIGFPE";
    case SIGILL:    return "SIGILL";
    case SIGSEGV:   return "SIGSEGV";
    case SIGTRAP:   return "SIGTRAP";
    case SIGSYS:    return "SIGSYS";
    case SIGSTKFLT: return "SIGSTKFLT";
    default:        return "?";
    }
}

const char* xcc_util_get_sigcodename(const siginfo_t* si)
{
    // Try the signal-specific codes...
    switch (si->si_signo) {
    case SIGBUS:
        switch(si->si_code)
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
        switch(si->si_code)
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
        switch(si->si_code)
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
        switch(si->si_code)
        {
        case SEGV_MAPERR:  return "SEGV_MAPERR";
        case SEGV_ACCERR:  return "SEGV_ACCERR";
        case SEGV_BNDERR:  return "SEGV_BNDERR";
        case SEGV_PKUERR:  return "SEGV_PKUERR";
        default:           break;
        }
        break;
    case SIGTRAP:
        switch(si->si_code)
        {
        case TRAP_BRKPT:   return "TRAP_BRKPT";
        case TRAP_TRACE:   return "TRAP_TRACE";
        case TRAP_BRANCH:  return "TRAP_BRANCH";
        case TRAP_HWBKPT:  return "TRAP_HWBKPT";
        default:           break;
        }
        if((si->si_code & 0xff) == SIGTRAP)
        {
            switch((si->si_code >> 8) & 0xff)
            {
            case PTRACE_EVENT_FORK:       return "PTRACE_EVENT_FORK";
            case PTRACE_EVENT_VFORK:      return "PTRACE_EVENT_VFORK";
            case PTRACE_EVENT_CLONE:      return "PTRACE_EVENT_CLONE";
            case PTRACE_EVENT_EXEC:       return "PTRACE_EVENT_EXEC";
            case PTRACE_EVENT_VFORK_DONE: return "PTRACE_EVENT_VFORK_DONE";
            case PTRACE_EVENT_EXIT:       return "PTRACE_EVENT_EXIT";
            case PTRACE_EVENT_SECCOMP:    return "PTRACE_EVENT_SECCOMP";
            case PTRACE_EVENT_STOP:       return "PTRACE_EVENT_STOP";
            default:                      break;
            }
        }
        break;
    case SIGSYS:
        switch(si->si_code)
        {
        case SYS_SECCOMP: return "SYS_SECCOMP";
        default:          break;
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
    if(si->si_code == SI_USER || si->si_code == SI_QUEUE || si->si_code == SI_TKILL) return 0;

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

        nleft -= (size_t)nwritten;
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
    len = (size_t)(tmp - str);
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
    
    return xcc_util_write(fd, buf, (size_t)len);
}

int xcc_util_write_format_safe(int fd, const char *format, ...)
{
    va_list ap;
    char    buf[1024];
    size_t  len;

    if(fd < 0) return XCC_ERRNO_INVAL;
    
    va_start(ap, format);
    len = xcc_fmt_vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if(0 == len) return 0;
    
    return xcc_util_write(fd, buf, len);
}

char *xcc_util_gets(char *s, size_t size, int fd)
{
    ssize_t i, nread;
    char    c, *p;

    if(fd < 0 || NULL == s || size < 2) return NULL;
    
    s[0] = '\0';
    p = s;
    
    for(i = 0; i < (ssize_t)(size - 1); i++)
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
    
    if(0 > (fd = XCC_UTIL_TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC))))
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

static int xcc_util_get_process_thread_name(const char *path, char *buf, size_t len)
{
    char    tmp[256], *data;
    size_t  data_len, cpy_len;
    int     r;

    //read a line
    if(0 != (r = xcc_util_read_file_line(path, tmp, sizeof(tmp)))) return r;

    //trim
    data = xcc_util_trim(tmp);

    //return data
    if(0 == (data_len = strlen(data))) return XCC_ERRNO_MISSING;
    cpy_len = XCC_UTIL_MIN(len - 1, data_len);
    memcpy(buf, data, cpy_len);
    buf[cpy_len] = '\0';

    return 0;
}

void xcc_util_get_process_name(pid_t pid, char *buf, size_t len)
{
    char path[128];

    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    if(0 != xcc_util_get_process_thread_name(path, buf, len))
        strncpy(buf, "unknown", len);
}

void xcc_util_get_thread_name(pid_t tid, char *buf, size_t len)
{
    char path[128];
    
    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/comm", tid);
    
    if(0 != xcc_util_get_process_thread_name(path, buf, len))
        strncpy(buf, "unknown", len);
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
static int xcc_util_is_root_saved = -1;

int xcc_util_is_root(void)
{
    size_t i;

    if(xcc_util_is_root_saved >= 0) return xcc_util_is_root_saved;
    
    for(i = 0; i < sizeof(xcc_util_su_pathnames) / sizeof(xcc_util_su_pathnames[0]); i++)
    {
        if(0 == access(xcc_util_su_pathnames[i], F_OK))
        {
            xcc_util_is_root_saved = 1;
            return 1;
        }
    }

    xcc_util_is_root_saved = 0;
    return 0;
}

size_t xcc_util_get_dump_header(char *buf,
                                size_t buf_len,
                                const char *crash_type,
                                long time_zone,
                                uint64_t start_time,
                                uint64_t crash_time,
                                const char *app_id,
                                const char *app_version,
                                int api_level,
                                const char *os_version,
                                const char *kernel_version,
                                const char *abi_list,
                                const char *manufacturer,
                                const char *brand,
                                const char *model,
                                const char *build_fingerprint)
{
    time_t       start_sec  = (time_t)(start_time / 1000000);
    suseconds_t  start_usec = (time_t)(start_time % 1000000);
    struct tm    start_tm;
    time_t       crash_sec   = (time_t)(crash_time / 1000000);
    suseconds_t  crash_usec = (time_t)(crash_time % 1000000);
    struct tm    crash_tm;

    //convert times
    xcc_libc_support_memset(&start_tm, 0, sizeof(start_tm));
    xcc_libc_support_memset(&crash_tm, 0, sizeof(crash_tm));
    xcc_libc_support_localtime_r(&start_sec, time_zone, &start_tm);
    xcc_libc_support_localtime_r(&crash_sec, time_zone, &crash_tm);

    return xcc_fmt_snprintf(buf, buf_len,
                            XCC_UTIL_TOMB_HEAD
                            "Tombstone maker: '"XCC_VERSION_STR"'\n"
                            "Crash type: '%s'\n"
                            "Start time: '"XCC_UTIL_TIME_FORMAT"'\n"
                            "Crash time: '"XCC_UTIL_TIME_FORMAT"'\n"
                            "App ID: '%s'\n"
                            "App version: '%s'\n"
                            "Rooted: '%s'\n"
                            "API level: '%d'\n"
                            "OS version: '%s'\n"
                            "Kernel version: '%s'\n"
                            "ABI list: '%s'\n"
                            "Manufacturer: '%s'\n"
                            "Brand: '%s'\n"
                            "Model: '%s'\n"
                            "Build fingerprint: '%s'\n"
                            "ABI: '"XCC_UTIL_ABI_STRING"'\n",
                            crash_type,
                            start_tm.tm_year + 1900, start_tm.tm_mon + 1, start_tm.tm_mday,
                            start_tm.tm_hour, start_tm.tm_min, start_tm.tm_sec, start_usec / 1000,
                            time_zone < 0 ? '-' : '+', labs(time_zone / 3600), labs(time_zone % 3600),
                            crash_tm.tm_year + 1900, crash_tm.tm_mon + 1, crash_tm.tm_mday,
                            crash_tm.tm_hour, crash_tm.tm_min, crash_tm.tm_sec, crash_usec / 1000,
                            time_zone < 0 ? '-' : '+', labs(time_zone / 3600), labs(time_zone % 3600),
                            app_id,
                            app_version,
                            xcc_util_is_root() ? "Yes" : "No",
                            api_level,
                            os_version,
                            kernel_version,
                            abi_list,
                            manufacturer,
                            brand,
                            model,
                            build_fingerprint);
}

static int xcc_util_record_logcat_buffer(int fd, pid_t pid, int api_level,
                                         const char *buffer, unsigned int lines, char priority)
{
    FILE *fp;
    char  cmd[128];
    char  buf[1025];
    int   with_pid;
    char  pid_filter[64] = "";
    char  pid_label[32] = "";
    int   r = 0;

    //Since Android 7.0 Nougat (API level 24), logcat has --pid filter option.
    with_pid = (api_level >= 24 ? 1 : 0);

    if(with_pid)
    {
        //API level >= 24, filtered by --pid option
        xcc_fmt_snprintf(pid_filter, sizeof(pid_filter), "--pid %d ", pid);
    }
    else
    {
        //API level < 24, filtered by ourself, so we need to read more lines
        lines = (unsigned int)(lines * 1.2);
        xcc_fmt_snprintf(pid_label, sizeof(pid_label), " %d ", pid);
    }
    
    xcc_fmt_snprintf(cmd, sizeof(cmd), "/system/bin/logcat -b %s -d -v threadtime -t %u %s*:%c",
                     buffer, lines, pid_filter, priority);

    if(0 != (r = xcc_util_write_format_safe(fd, "--------- tail end of log %s (%s)\n", buffer, cmd))) return r;

    if(NULL != (fp = popen(cmd, "r")))
    {
        buf[sizeof(buf) - 1] = '\0';
        while(NULL != fgets(buf, sizeof(buf) - 1, fp))
            if(with_pid || NULL != strstr(buf, pid_label))
                if(0 != (r = xcc_util_write_str(fd, buf))) break;
        pclose(fp);
    }
    
    return r;
}

int xcc_util_record_logcat(int fd,
                           pid_t pid,
                           int api_level,
                           unsigned int logcat_system_lines,
                           unsigned int logcat_events_lines,
                           unsigned int logcat_main_lines)
{
    int r;
    
    if(0 == logcat_system_lines && 0 == logcat_events_lines && 0 == logcat_main_lines) return 0;
    
    if(0 != (r = xcc_util_write_str(fd, "logcat:\n"))) return r;

    if(logcat_main_lines > 0)
        if(0 != (r = xcc_util_record_logcat_buffer(fd, pid, api_level, "main", logcat_main_lines, 'D'))) return r;
    
    if(logcat_system_lines > 0)
        if(0 != (r = xcc_util_record_logcat_buffer(fd, pid, api_level, "system", logcat_system_lines, 'W'))) return r;

    if(logcat_events_lines > 0)
        if(0 != (r = xcc_util_record_logcat_buffer(fd, pid, api_level, "events", logcat_events_lines, 'I'))) return r;

    if(0 != (r = xcc_util_write_str(fd, "\n"))) return r;

    return 0;
}

int xcc_util_record_fds(int fd, pid_t pid)
{
    int                fd2 = -1;
    char               path[128];
    char               fd_path[512];
    char               buf[512];
    long               n, i;
    int                fd_num;
    size_t             total = 0;
    xcc_util_dirent_t *ent;
    ssize_t            len;
    int                r = 0;

    if(0 != (r = xcc_util_write_str(fd, "open files:\n"))) return r;

    xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    if((fd2 = XCC_UTIL_TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC))) < 0) goto end;
    
    while((n = syscall(XCC_UTIL_SYSCALL_GETDENTS, fd2, buf, sizeof(buf))) > 0)
    {
        for(i = 0; i < n;)
        {
            ent = (xcc_util_dirent_t *)(buf + i);

            //get the fd
            if('\0' == ent->d_name[0]) goto next;
            if(0 == memcmp(ent->d_name, ".", 1)) goto next;
            if(0 == memcmp(ent->d_name, "..", 2)) goto next;
            if(0 != xcc_util_atoi(ent->d_name, &fd_num)) goto next;
            if(fd_num < 0) goto next;

            //count
            total++;
            if(total > 1024) goto next;

            //read link of the path
            xcc_fmt_snprintf(path, sizeof(path), "/proc/%d/fd/%d", pid, fd_num);
            len = readlink(path, fd_path, sizeof(fd_path) - 1);
            if(len <= 0 || len > (ssize_t)(sizeof(fd_path) - 1))
                strncpy(path, "???", sizeof(path));
            else
                fd_path[len] = '\0';
            
            //dump
            if(0 != (r = xcc_util_write_format_safe(fd, "    fd %d: %s\n", fd_num, fd_path))) goto clean;
            
        next:
            i += ent->d_reclen;
        }
    }

 end:
    if(total > 1024)
        if(0 != (r = xcc_util_write_str(fd, "    ......\n"))) goto clean;
    if(0 != (r = xcc_util_write_format_safe(fd, "    (number of FDs: %zu)\n", total))) goto clean;
    r = xcc_util_write_str(fd, "\n");

 clean:
    if(fd2 >= 0) close(fd2);
    return r;
}

#pragma clang diagnostic pop
