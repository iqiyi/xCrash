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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/syscall.h>
#include <android/log.h>
#include "xcc_signal.h"
#include "xcc_errno.h"
#include "xcc_libc_support.h"

#define XCC_SIGNAL_CRASH_STACK_SIZE (1024 * 128)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct
{
    int              signum;
    struct sigaction oldact;
} xcc_signal_crash_info_t;
#pragma clang diagnostic pop

static xcc_signal_crash_info_t xcc_signal_crash_info[] =
{
    {.signum = SIGABRT},
    {.signum = SIGBUS},
    {.signum = SIGFPE},
    {.signum = SIGILL},
    {.signum = SIGSEGV},
    {.signum = SIGTRAP},
    {.signum = SIGSYS},
    {.signum = SIGSTKFLT}
};

int xcc_signal_crash_register(void (*handler)(int, siginfo_t *, void *))
{
    stack_t ss;
    if(NULL == (ss.ss_sp = calloc(1, XCC_SIGNAL_CRASH_STACK_SIZE))) return XCC_ERRNO_NOMEM;
    ss.ss_size  = XCC_SIGNAL_CRASH_STACK_SIZE;
    ss.ss_flags = 0;
    if(0 != sigaltstack(&ss, NULL)) return XCC_ERRNO_SYS;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    
    size_t i;
    for(i = 0; i < sizeof(xcc_signal_crash_info) / sizeof(xcc_signal_crash_info[0]); i++)
        if(0 != sigaction(xcc_signal_crash_info[i].signum, &act, &(xcc_signal_crash_info[i].oldact)))
            return XCC_ERRNO_SYS;

    return 0;
}

int xcc_signal_crash_unregister(void)
{
    int r = 0;
    size_t i;
    for(i = 0; i < sizeof(xcc_signal_crash_info) / sizeof(xcc_signal_crash_info[0]); i++)
        if(0 != sigaction(xcc_signal_crash_info[i].signum, &(xcc_signal_crash_info[i].oldact), NULL))
            r = XCC_ERRNO_SYS;
    
    return r;
}

int xcc_signal_crash_ignore(void)
{
    struct sigaction act;
    xcc_libc_support_memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_DFL;
    act.sa_flags = SA_RESTART;
    
    int r = 0;
    size_t i;
    for(i = 0; i < sizeof(xcc_signal_crash_info) / sizeof(xcc_signal_crash_info[0]); i++)
        if(0 != sigaction(xcc_signal_crash_info[i].signum, &act, NULL))
            r = XCC_ERRNO_SYS;

    return r;
}

int xcc_signal_crash_queue(siginfo_t* si)
{
    if(SIGABRT == si->si_signo || SI_FROMUSER(si))
    {
        if(0 != syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si))
            return XCC_ERRNO_SYS;
    }

    return 0;
}

static sigset_t         xcc_signal_trace_oldset;
static struct sigaction xcc_signal_trace_oldact;

int xcc_signal_trace_register(void (*handler)(int, siginfo_t *, void *))
{
    int              r;
    sigset_t         set;
    struct sigaction act;

    //un-block the SIGQUIT mask for current thread, hope this is the main thread
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    if(0 != (r = pthread_sigmask(SIG_UNBLOCK, &set, &xcc_signal_trace_oldset))) return r;

    //register new signal handler for SIGQUIT
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    if(0 != sigaction(SIGQUIT, &act, &xcc_signal_trace_oldact))
    {
        pthread_sigmask(SIG_SETMASK, &xcc_signal_trace_oldset, NULL);
        return XCC_ERRNO_SYS;
    }

    return 0;
}

void xcc_signal_trace_unregister(void)
{
    pthread_sigmask(SIG_SETMASK, &xcc_signal_trace_oldset, NULL);
    sigaction(SIGQUIT, &xcc_signal_trace_oldact, NULL);
}
