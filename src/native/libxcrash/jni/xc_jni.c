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
#include "xc_core.h"
#include "xc_util.h"
#include "xc_test.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#define XC_JNI_CLASS_NAME "xcrash/NativeCrashHandler"

#define XC_JNI_CHECK_PENDING_EXCEPTION(label)             \
    do {                                                  \
        if((*env)->ExceptionCheck(env))                   \
        {                                                 \
            (*env)->ExceptionClear(env);                  \
            goto label;                                   \
        }                                                 \
    } while(0)

#define XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(v, label) \
    do {                                                  \
        XC_JNI_CHECK_PENDING_EXCEPTION(label);            \
        if(NULL == (v)) goto label;                       \
    } while(0)

#define XC_JNI_CALLBACK_CLASS_NAME  "xcrash/NativeCrashHandler"
#define XC_JNI_CALLBACK_METHOD_NAME "callback"
#define XC_JNI_CALLBACK_METHOD_SIG  "(Ljava/lang/String;Ljava/lang/String;ZZLjava/lang/String;)V"

//java class/methods cache
static JavaVM     *xc_jni_vm           = NULL;
static jclass      xc_jni_cb_class     = NULL;
static jmethodID   xc_jni_cb_method    = NULL;

static pthread_t   xc_jni_cb_thd;
static int         xc_jni_evfd         = -1;

static pid_t       xc_jni_crash_tid    = -1;
static const char *xc_jni_log_pathname = NULL;
static char       *xc_jni_emergency    = NULL;

static void *xc_jni_callback_thread(void *arg)
{
    JNIEnv   *env = NULL;
    uint64_t  data = 0;
    jstring   j_pathname  = NULL;
    jstring   j_emergency = NULL;
    jboolean  j_is_java_thread = JNI_FALSE;
    jboolean  j_is_main_thread = JNI_FALSE;
    jstring   j_thread_name = NULL;
    char      c_thread_name[16] = "\0";
    
    (void)arg;
    
    pthread_setname_np(pthread_self(), "xcrash_callback");

    if(JNI_OK != (*xc_jni_vm)->AttachCurrentThread(xc_jni_vm, &env, NULL)) return NULL;

    //block until native crashed
    if(sizeof(data) != XCC_UTIL_TEMP_FAILURE_RETRY(read(xc_jni_evfd, &data, sizeof(data)))) return NULL;

    //prepare callback parameters
    if(NULL != xc_jni_log_pathname)
    {
        if(NULL == (j_pathname = (*env)->NewStringUTF(env, xc_jni_log_pathname))) goto clean;
    }
    if(NULL != xc_jni_emergency)
    {
        if(NULL == (j_emergency = (*env)->NewStringUTF(env, xc_jni_emergency))) goto clean;
    }
    j_is_java_thread = (xc_jni_crash_tid >= 0 ? JNI_TRUE : JNI_FALSE);
    if(j_is_java_thread)
    {
        j_is_main_thread = (getpid() == xc_jni_crash_tid ? JNI_TRUE : JNI_FALSE);
        if(!j_is_main_thread)
        {
            if(0 != xcc_util_get_thread_name(xc_jni_crash_tid, c_thread_name, sizeof(c_thread_name))) goto clean;
            if(NULL == (j_thread_name = (*env)->NewStringUTF(env, c_thread_name))) goto clean;
        }
    }

    //do callback
    (*env)->CallStaticVoidMethod(env, xc_jni_cb_class, xc_jni_cb_method, j_pathname, j_emergency, j_is_java_thread, j_is_main_thread, j_thread_name);
    XC_JNI_CHECK_PENDING_EXCEPTION(clean);

 clean:
    (*env)->DeleteGlobalRef(env, xc_jni_cb_class);
    XC_JNI_CHECK_PENDING_EXCEPTION(end);

 end:
    return NULL;
}

void xc_jni_callback(const char *log_pathname, char *emergency)
{
    uint64_t  data = 1;
    JNIEnv   *env  = NULL;

    //if this is a java thread?
    if(JNI_OK == (*xc_jni_vm)->GetEnv(xc_jni_vm, (void**)&env, JNI_VERSION_1_6))
    {
        XC_JNI_CHECK_PENDING_EXCEPTION(skip);
        xc_jni_crash_tid = gettid();
    }

 skip:
    xc_jni_log_pathname = log_pathname;
    xc_jni_emergency    = emergency;
    
    //wake up the callback thread
    if(sizeof(data) != XCC_UTIL_TEMP_FAILURE_RETRY(write(xc_jni_evfd, &data, sizeof(data)))) return;
    
    pthread_join(xc_jni_cb_thd, NULL);
}

static jint xc_jni_init(JNIEnv *env,
                        jobject thiz,
                        jboolean restore_signal_handler,
                        jstring app_id,
                        jstring app_version,
                        jstring app_lib_dir,
                        jstring log_dir,
                        jint logcat_system_lines,
                        jint logcat_events_lines,
                        jint logcat_main_lines,
                        jboolean dump_elf_hash,
                        jboolean dump_map,
                        jboolean dump_fds,
                        jboolean dump_all_threads,
                        jint dump_all_threads_count_max,
                        jobjectArray dump_all_threads_whitelist)
{
    const char  *c_app_id                         = NULL;
    const char  *c_app_version                    = NULL;
    const char  *c_app_lib_dir                    = NULL;
    const char  *c_log_dir                        = NULL;
    const char **c_dump_all_threads_whitelist     = NULL;
    size_t       c_dump_all_threads_whitelist_len = 0;
    int          r                                = XCC_ERRNO_JNI;
    size_t       len, i;
    jclass       tmp_class;
    jstring      tmp_str;
    const char  *tmp_c_str;

    (void)thiz;

    if(!env || !(*env) || !app_id || !app_version || !app_lib_dir || !log_dir ||
       logcat_system_lines < 0 || logcat_events_lines < 0 || logcat_main_lines < 0) return XCC_ERRNO_INVAL;

    if(NULL == (c_app_id      = (*env)->GetStringUTFChars(env, app_id,      0))) goto clean;
    if(NULL == (c_app_version = (*env)->GetStringUTFChars(env, app_version, 0))) goto clean;
    if(NULL == (c_app_lib_dir = (*env)->GetStringUTFChars(env, app_lib_dir, 0))) goto clean;
    if(NULL == (c_log_dir     = (*env)->GetStringUTFChars(env, log_dir,     0))) goto clean;

    //threads whitelist
    if(dump_all_threads_whitelist)
    {
        len = (size_t)(*env)->GetArrayLength(env, dump_all_threads_whitelist);
        if(len > 0)
        {
            if(NULL != (c_dump_all_threads_whitelist = calloc(len, sizeof(char *))))
            {
                c_dump_all_threads_whitelist_len = len;
                for(i = 0; i < len; i++)
                {
                    tmp_str = (jstring)((*env)->GetObjectArrayElement(env, dump_all_threads_whitelist, (jsize)i));
                    c_dump_all_threads_whitelist[i] = (tmp_str ? (*env)->GetStringUTFChars(env, tmp_str, 0) : NULL);
                }
            }
        }
    }

    //cache the callback class and method
    tmp_class = (*env)->FindClass(env, XC_JNI_CALLBACK_CLASS_NAME);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(tmp_class, clean);
    xc_jni_cb_class = (*env)->NewGlobalRef(env, tmp_class);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_jni_cb_class, clean);
    xc_jni_cb_method = (*env)->GetStaticMethodID(env, xc_jni_cb_class, XC_JNI_CALLBACK_METHOD_NAME, XC_JNI_CALLBACK_METHOD_SIG);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_jni_cb_method, clean);

    //eventfd and a new thread for callback
    if(0 > (xc_jni_evfd = eventfd(0, EFD_CLOEXEC))) goto clean;
    if(0 != pthread_create(&xc_jni_cb_thd, NULL, xc_jni_callback_thread, NULL)) goto clean;
    
    //init native core
    r = xc_core_init((int)restore_signal_handler,
                     c_app_id,
                     c_app_version,
                     c_app_lib_dir,
                     c_log_dir,
                     (unsigned int)logcat_system_lines,
                     (unsigned int)logcat_events_lines,
                     (unsigned int)logcat_main_lines,
                     (int)dump_elf_hash,
                     (int)dump_map,
                     (int)dump_fds,
                     (int)dump_all_threads,
                     (int)dump_all_threads_count_max,
                     c_dump_all_threads_whitelist,
                     c_dump_all_threads_whitelist_len);

 clean:
    if(app_id      && c_app_id)      (*env)->ReleaseStringUTFChars(env, app_id,      c_app_id);
    if(app_version && c_app_version) (*env)->ReleaseStringUTFChars(env, app_version, c_app_version);
    if(app_lib_dir && c_app_lib_dir) (*env)->ReleaseStringUTFChars(env, app_lib_dir, c_app_lib_dir);
    if(log_dir     && c_log_dir)     (*env)->ReleaseStringUTFChars(env, log_dir,     c_log_dir);

    if(dump_all_threads_whitelist && NULL != c_dump_all_threads_whitelist)
    {
        for(i = 0; i < c_dump_all_threads_whitelist_len; i++)
        {
            tmp_str = (jstring)((*env)->GetObjectArrayElement(env, dump_all_threads_whitelist, (jsize)i));
            tmp_c_str = c_dump_all_threads_whitelist[i];
            if(tmp_str && NULL != tmp_c_str) (*env)->ReleaseStringUTFChars(env, tmp_str, tmp_c_str);
        }
        free(c_dump_all_threads_whitelist);
    }
    
    return r;
}

static void xc_jni_test(JNIEnv *env, jobject thiz, jint run_in_new_thread)
{
    (void)env;
    (void)thiz;

    xc_test_crash(run_in_new_thread);
}

static JNINativeMethod xc_jni_methods[] = {
    {
        "init",
        "("
        "Z"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "I"
        "I"
        "I"
        "Z"
        "Z"
        "Z"
        "Z"
        "I"
        "[Ljava/lang/String;"
        ")"
        "I",
        (void *)xc_jni_init
    },
    {
        "test",
        "("
        "I"
        ")"
        "V",
        (void *)xc_jni_test
    }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass clazz;

    (void)reserved;

    //save the VM handler
    xc_jni_vm = vm;

    //get env
    if(JNI_OK != (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6)) return -1;
    
    //get class
    if(NULL == (clazz = (*env)->FindClass(env, XC_JNI_CLASS_NAME))) return -1;

    //register native methods
    if((*env)->RegisterNatives(env, clazz, xc_jni_methods, sizeof(xc_jni_methods) / sizeof(xc_jni_methods[0]))) return -1;
    
    return JNI_VERSION_1_6;
}

#pragma clang diagnostic pop
