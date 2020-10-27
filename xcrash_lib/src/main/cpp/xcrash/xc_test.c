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
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <android/log.h>
#include "xcc_util.h"
#include "xc_test.h"
#include "xc_common.h"
#include "xc_dl.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#define XC_TEST_LOG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "xcrash", fmt, ##__VA_ARGS__)
#define XC_TEST_ABORT_MSG     "abort message for xCrash internal testing"

static void xc_test_set_abort_msg()
{
    xc_dl_t                           *libc          = NULL;
    xcc_util_libc_set_abort_message_t  set_abort_msg = NULL;

    if(xc_common_api_level >= 29) libc = xc_dl_create(XCC_UTIL_LIBC_APEX);
    if(NULL == libc && NULL == (libc = xc_dl_create(XCC_UTIL_LIBC))) goto end;
    if(NULL == (set_abort_msg = (xcc_util_libc_set_abort_message_t)xc_dl_sym(libc, XCC_UTIL_LIBC_SET_ABORT_MSG))) goto end;

    set_abort_msg(XC_TEST_ABORT_MSG);

 end:
    if(NULL != libc) xc_dl_destroy(&libc);
}

int xc_test_call_4(int v)
{
    int *a = NULL;

    xc_test_set_abort_msg();
    
    *a = v; // crash!
    (*a)++;
    v = *a;

    return v;
}

int xc_test_call_3(int v)
{
    int r = xc_test_call_4(v + 1);
    return r;
}

int xc_test_call_2(int v)
{
    int r = xc_test_call_3(v + 1);
    return r;
}

void xc_test_call_1(void)
{
    int r = xc_test_call_2(1);
    r = 0;
}

static void *xc_test_new_thread(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "xcrash_test_cal");

    xc_test_call_1();
    
    return NULL;
}

static void *xc_test_keep_logging(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), "xcrash_test_log");

    int i = 0;
    while(++i < 600)
    {
        XC_TEST_LOG("crashed APP's thread is running ...... %d", i);
        usleep(1000 * 100);
    }

    return NULL;
}

void xc_test_crash(int run_in_new_thread)
{
    pthread_t tid;
    
    pthread_create(&tid, NULL, &xc_test_keep_logging, NULL);
    usleep(1000 * 10);

    if(run_in_new_thread)
        pthread_create(&tid, NULL, &xc_test_new_thread, NULL);
    else
        xc_test_call_1();
}

#pragma clang diagnostic pop
