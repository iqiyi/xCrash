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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <jni.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <android/log.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xc_jni.h"
#include "xc_common.h"
#include "xc_crash.h"
#include "xc_trace.h"
#include "xc_util.h"
#include "xc_test.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static int xc_jni_inited = 0;

static jint xc_jni_init(JNIEnv       *env,
                        jobject       thiz,
                        jint          api_level,
                        jstring       os_version,
                        jstring       abi_list,
                        jstring       manufacturer,
                        jstring       brand,
                        jstring       model,
                        jstring       build_fingerprint,
                        jstring       app_id,
                        jstring       app_version,
                        jstring       app_lib_dir,
                        jstring       log_dir,
                        jboolean      crash_enable,
                        jboolean      crash_rethrow,
                        jint          crash_logcat_system_lines,
                        jint          crash_logcat_events_lines,
                        jint          crash_logcat_main_lines,
                        jboolean      crash_dump_elf_hash,
                        jboolean      crash_dump_map,
                        jboolean      crash_dump_fds,
                        jboolean      crash_dump_all_threads,
                        jint          crash_dump_all_threads_count_max,
                        jobjectArray  crash_dump_all_threads_whitelist,
                        jboolean      trace_enable,
                        jboolean      trace_rethrow,
                        jint          trace_logcat_system_lines,
                        jint          trace_logcat_events_lines,
                        jint          trace_logcat_main_lines,
                        jboolean      trace_dump_fds)
{
    int              r_crash                                = XCC_ERRNO_JNI;
    int              r_trace                                  = XCC_ERRNO_JNI;
    
    const char      *c_os_version                           = NULL;
    const char      *c_abi_list                             = NULL;
    const char      *c_manufacturer                         = NULL;
    const char      *c_brand                                = NULL;
    const char      *c_model                                = NULL;
    const char      *c_build_fingerprint                    = NULL;
    const char      *c_app_id                               = NULL;
    const char      *c_app_version                          = NULL;
    const char      *c_app_lib_dir                          = NULL;
    const char      *c_log_dir                              = NULL;
    
    const char     **c_crash_dump_all_threads_whitelist     = NULL;
    size_t           c_crash_dump_all_threads_whitelist_len = 0;
    
    size_t           len, i;
    jstring          tmp_str;
    const char      *tmp_c_str;

    (void)thiz;

    //only once
    if(xc_jni_inited) return XCC_ERRNO_JNI;
    xc_jni_inited = 1;

    if(!env || !(*env) || (!crash_enable && ! trace_enable) || api_level < 0 ||
       !os_version || !abi_list || !manufacturer || !brand || !model || !build_fingerprint ||
       !app_id || !app_version || !app_lib_dir || !log_dir ||
       crash_logcat_system_lines < 0 || crash_logcat_events_lines < 0 || crash_logcat_main_lines < 0 ||
       crash_dump_all_threads_count_max < 0 ||
       trace_logcat_system_lines < 0 || trace_logcat_events_lines < 0 || trace_logcat_main_lines < 0)
        return XCC_ERRNO_INVAL;

    if(NULL == (c_os_version        = (*env)->GetStringUTFChars(env, os_version,        0))) goto clean;
    if(NULL == (c_abi_list          = (*env)->GetStringUTFChars(env, abi_list,          0))) goto clean;
    if(NULL == (c_manufacturer      = (*env)->GetStringUTFChars(env, manufacturer,      0))) goto clean;
    if(NULL == (c_brand             = (*env)->GetStringUTFChars(env, brand,             0))) goto clean;
    if(NULL == (c_model             = (*env)->GetStringUTFChars(env, model,             0))) goto clean;
    if(NULL == (c_build_fingerprint = (*env)->GetStringUTFChars(env, build_fingerprint, 0))) goto clean;
    if(NULL == (c_app_id            = (*env)->GetStringUTFChars(env, app_id,            0))) goto clean;
    if(NULL == (c_app_version       = (*env)->GetStringUTFChars(env, app_version,       0))) goto clean;
    if(NULL == (c_app_lib_dir       = (*env)->GetStringUTFChars(env, app_lib_dir,       0))) goto clean;
    if(NULL == (c_log_dir           = (*env)->GetStringUTFChars(env, log_dir,           0))) goto clean;

    //common init
    if(0 != xc_common_init((int)api_level, 
                           c_os_version,
                           c_abi_list,
                           c_manufacturer,
                           c_brand,
                           c_model,
                           c_build_fingerprint,
                           c_app_id,
                           c_app_version,
                           c_app_lib_dir,
                           c_log_dir)) goto clean;
    
    r_crash = 0;
    r_trace = 0;
    
    if(crash_enable)
    {
        r_crash = XCC_ERRNO_JNI;
        
        if(crash_dump_all_threads_whitelist)
        {
            len = (size_t)(*env)->GetArrayLength(env, crash_dump_all_threads_whitelist);
            if(len > 0)
            {
                if(NULL != (c_crash_dump_all_threads_whitelist = calloc(len, sizeof(char *))))
                {
                    c_crash_dump_all_threads_whitelist_len = len;
                    for(i = 0; i < len; i++)
                    {
                        tmp_str = (jstring)((*env)->GetObjectArrayElement(env, crash_dump_all_threads_whitelist, (jsize)i));
                        c_crash_dump_all_threads_whitelist[i] = (tmp_str ? (*env)->GetStringUTFChars(env, tmp_str, 0) : NULL);
                    }
                }
            }
        }

        //crash init
        r_crash = xc_crash_init(env,
                                crash_rethrow ? 1 : 0,
                                (unsigned int)crash_logcat_system_lines,
                                (unsigned int)crash_logcat_events_lines,
                                (unsigned int)crash_logcat_main_lines,
                                crash_dump_elf_hash ? 1 : 0,
                                crash_dump_map ? 1 : 0,
                                crash_dump_fds ? 1 : 0,
                                crash_dump_all_threads ? 1 : 0,
                                (unsigned int)crash_dump_all_threads_count_max,
                                c_crash_dump_all_threads_whitelist,
                                c_crash_dump_all_threads_whitelist_len);
    }
    
    if(trace_enable)
    {
        //trace init
        r_trace = xc_trace_init(env,
                            trace_rethrow ? 1 : 0,
                            (unsigned int)trace_logcat_system_lines,
                            (unsigned int)trace_logcat_events_lines,
                            (unsigned int)trace_logcat_main_lines,
                            trace_dump_fds ? 1 : 0);
    }
    
 clean:
    if(os_version        && c_os_version)        (*env)->ReleaseStringUTFChars(env, os_version,        c_os_version);
    if(abi_list          && c_abi_list)          (*env)->ReleaseStringUTFChars(env, abi_list,          c_abi_list);
    if(manufacturer      && c_manufacturer)      (*env)->ReleaseStringUTFChars(env, manufacturer,      c_manufacturer);
    if(brand             && c_brand)             (*env)->ReleaseStringUTFChars(env, brand,             c_brand);
    if(model             && c_model)             (*env)->ReleaseStringUTFChars(env, model,             c_model);
    if(build_fingerprint && c_build_fingerprint) (*env)->ReleaseStringUTFChars(env, build_fingerprint, c_build_fingerprint);
    if(app_id            && c_app_id)            (*env)->ReleaseStringUTFChars(env, app_id,            c_app_id);
    if(app_version       && c_app_version)       (*env)->ReleaseStringUTFChars(env, app_version,       c_app_version);
    if(app_lib_dir       && c_app_lib_dir)       (*env)->ReleaseStringUTFChars(env, app_lib_dir,       c_app_lib_dir);
    if(log_dir           && c_log_dir)           (*env)->ReleaseStringUTFChars(env, log_dir,           c_log_dir);

    if(crash_dump_all_threads_whitelist && NULL != c_crash_dump_all_threads_whitelist)
    {
        for(i = 0; i < c_crash_dump_all_threads_whitelist_len; i++)
        {
            tmp_str = (jstring)((*env)->GetObjectArrayElement(env, crash_dump_all_threads_whitelist, (jsize)i));
            tmp_c_str = c_crash_dump_all_threads_whitelist[i];
            if(tmp_str && NULL != tmp_c_str) (*env)->ReleaseStringUTFChars(env, tmp_str, tmp_c_str);
        }
        free(c_crash_dump_all_threads_whitelist);
    }
    
    return (0 == r_crash && 0 == r_trace) ? 0 : XCC_ERRNO_JNI;
}

static void xc_jni_notify_java_crashed(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;

    xc_common_java_crashed = 1;
}

static void xc_jni_test_crash(JNIEnv *env, jobject thiz, jint run_in_new_thread)
{
    (void)env;
    (void)thiz;

    xc_test_crash(run_in_new_thread);
}

static JNINativeMethod xc_jni_methods[] = {
    {
        "nativeInit",
        "("
        "I"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Z"
        "Z"
        "I"
        "I"
        "I"
        "Z"
        "Z"
        "Z"
        "Z"
        "I"
        "[Ljava/lang/String;"
        "Z"
        "Z"
        "I"
        "I"
        "I"
        "Z"
        ")"
        "I",
        (void *)xc_jni_init
    },
    {
        "nativeNotifyJavaCrashed",
        "("
        ")"
        "V",
        (void *)xc_jni_notify_java_crashed
    },
    {
        "nativeTestCrash",
        "("
        "I"
        ")"
        "V",
        (void *)xc_jni_test_crash
    }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass  cls;

    (void)reserved;

    if(NULL == vm) return -1;
    
    //register JNI methods
    if(JNI_OK != (*vm)->GetEnv(vm, (void**)&env, XC_JNI_VERSION)) return -1;
    if(NULL == env || NULL == *env) return -1;
    if(NULL == (cls = (*env)->FindClass(env, XC_JNI_CLASS_NAME))) return -1;
    if((*env)->RegisterNatives(env, cls, xc_jni_methods, sizeof(xc_jni_methods) / sizeof(xc_jni_methods[0]))) return -1;

    xc_common_set_vm(vm, env, cls);

    return XC_JNI_VERSION;
}

#pragma clang diagnostic pop
