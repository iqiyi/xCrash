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
#include <android/log.h>
#include "xcc_errno.h"
#include "xcc_util.h"
#include "xc_jni.h"
#include "xc_core.h"
#include "xc_util.h"
#include "xc_test.h"

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

static JavaVM     *xc_jni_jvm          = NULL;
static jclass      xc_jni_class_cb     = NULL;
static jmethodID   xc_jni_method_cb    = NULL;

static pid_t       xc_jni_crash_tid    = 0;
static char       *xc_jni_emergency    = NULL;
static int         xc_jni_log_fd       = 0;
static const char *xc_jni_log_pathname = NULL;

static int xc_jni_get_app_info(JNIEnv *env, jobject context, jstring *app_id, jstring *app_version, jstring *app_lib_dir, jstring *files_dir)
{
    jclass context_clazz = (*env)->GetObjectClass(env, context);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(context_clazz, err);
    
    //get app_lib_dir
    jmethodID getApplicationInfo = (*env)->GetMethodID(env, context_clazz, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getApplicationInfo, err);
    jobject applicationInfo = (*env)->CallObjectMethod(env, context, getApplicationInfo);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(applicationInfo, err);
    jclass applicationInfo_clazz = (*env)->GetObjectClass(env, applicationInfo);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(applicationInfo_clazz, err);
    jfieldID nativeLibraryDir = (*env)->GetFieldID(env, applicationInfo_clazz, "nativeLibraryDir", "Ljava/lang/String;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(nativeLibraryDir, err);
    *app_lib_dir = (jstring)(*env)->GetObjectField(env, applicationInfo, nativeLibraryDir);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(*app_lib_dir, err);

    //get app_id
    jmethodID getPackageName = (*env)->GetMethodID(env, context_clazz, "getPackageName", "()Ljava/lang/String;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getPackageName, skip);
    *app_id = (jstring)(*env)->CallObjectMethod(env, context, getPackageName);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(*app_id, skip);

    //get app_version
    if(app_version)
    {
        jmethodID getPackageManager = (*env)->GetMethodID(env, context_clazz, "getPackageManager", "()Landroid/content/pm/PackageManager;");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getPackageManager, skip);
        jobject packageManager = (*env)->CallObjectMethod(env, context, getPackageManager);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(packageManager, skip);
        jclass packageManager_clazz = (*env)->GetObjectClass(env, packageManager);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(packageManager_clazz, skip);
        jmethodID getPackageInfo = (*env)->GetMethodID(env, packageManager_clazz, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getPackageInfo, skip);
        jobject packageInfo = (*env)->CallObjectMethod(env, packageManager, getPackageInfo, *app_id, 0);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(packageInfo, skip);
        jclass packageInfo_clazz = (*env)->GetObjectClass(env, packageInfo);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(packageInfo_clazz, skip);
        jfieldID versionName = (*env)->GetFieldID(env, packageInfo_clazz, "versionName", "Ljava/lang/String;");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(versionName, skip);
        *app_version = (jstring)(*env)->GetObjectField(env, packageInfo, versionName);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(*app_version, skip);
    }

 skip:

    //get files_dir
    if(files_dir)
    {
        jmethodID getFilesDir = (*env)->GetMethodID(env, context_clazz, "getFilesDir", "()Ljava/io/File;");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getFilesDir, err);
        jobject file = (*env)->CallObjectMethod(env, context, getFilesDir);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(file, err);
        jclass file_clazz = (*env)->GetObjectClass(env, file);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(file_clazz, err);
        jmethodID getCanonicalPath = (*env)->GetMethodID(env, file_clazz, "getCanonicalPath", "()Ljava/lang/String;");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(getCanonicalPath, err);
        *files_dir = (jstring)(*env)->CallObjectMethod(env, file, getCanonicalPath);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(*files_dir, err);
    }

    return 0;

 err:    
    return XCC_ERRNO_JNI;
}

static void xc_jni_record_stack_trace(JNIEnv *env, int log_fd, pid_t crash_tid)
{
    int   is_main_thread = (getpid() == crash_tid ? 1 : 0);
    char  tname[64] = "\0";
    
    //get thread name
    if(!is_main_thread)
    {
        if(0 != xcc_util_get_thread_name(crash_tid, tname, sizeof(tname))) return;
        if('\0' == tname[0]) return;
    }

    //java.lang.Thread
    jclass class_Thread = (*env)->FindClass(env, "java/lang/Thread");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(class_Thread, err);
    jmethodID method_getAllStackTraces = (*env)->GetStaticMethodID(env, class_Thread, "getAllStackTraces", "()Ljava/util/Map;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_getAllStackTraces, err);
    jmethodID method_getStackTrace = (*env)->GetMethodID(env, class_Thread, "getStackTrace", "()[Ljava/lang/StackTraceElement;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_getStackTrace, err);
    jmethodID method_getName = (*env)->GetMethodID(env, class_Thread, "getName", "()Ljava/lang/String;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_getName, err);

    //java.lang.StackTraceElement
    jclass class_StackTraceElement = (*env)->FindClass(env, "java/lang/StackTraceElement");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(class_StackTraceElement, err);
    jmethodID method_toString = (*env)->GetMethodID(env, class_StackTraceElement, "toString", "()Ljava/lang/String;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_toString, err);

    //java.util.Map
    jclass class_Map = (*env)->FindClass(env, "java/util/Map");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(class_Map, err);
    jmethodID method_keySet = (*env)->GetMethodID(env, class_Map, "keySet", "()Ljava/util/Set;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_keySet, err);

    //java.util.Set
    jclass class_Set = (*env)->FindClass(env, "java/util/Set");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(class_Set, err);
    jmethodID method_toArray = (*env)->GetMethodID(env, class_Set, "toArray", "()[Ljava/lang/Object;");
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(method_toArray, err);
    
    jobject allStackTracesMap = (*env)->CallStaticObjectMethod(env, class_Thread, method_getAllStackTraces);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(allStackTracesMap, err);

    jobject allStackTracesSet = (*env)->CallObjectMethod(env, allStackTracesMap, method_keySet);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(allStackTracesSet, err);
    
    jobjectArray allStackTracesArray = (jobjectArray)(*env)->CallObjectMethod(env, allStackTracesSet, method_toArray);
    XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(allStackTracesArray, err);
    
    jsize allStackTracesArrayLen = (*env)->GetArrayLength(env, allStackTracesArray);
    XC_JNI_CHECK_PENDING_EXCEPTION(err);
    
    jsize i;
    for(i = 0; i < allStackTracesArrayLen; i++)
    {
        jobject thread = (*env)->GetObjectArrayElement(env, allStackTracesArray, i);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(thread, err);

        jstring name = (*env)->CallObjectMethod(env, thread, method_getName);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(name, err);

        const char *c_name = (*env)->GetStringUTFChars(env, name, 0);

        //Notice:
        //TID from linux kernel and JVM are different things.
        //Here we match thread by thread name is not very accurate.
        if((is_main_thread && 0 == strcmp(c_name, "main")) ||
           (!is_main_thread && strstr(c_name, tname)))
        {
            if(0 != xcc_util_write_str(log_fd, "java stacktrace:\n")) goto err;

            jobjectArray stackTrace = (jobjectArray)(*env)->CallObjectMethod(env, thread, method_getStackTrace);
            XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(stackTrace, err);
            
            jsize stackTraceLen = (*env)->GetArrayLength(env, stackTrace);
            XC_JNI_CHECK_PENDING_EXCEPTION(err);

            jsize j;
            for(j = 0; j < stackTraceLen; j++)
            {
                jobject stackTraceElement = (*env)->GetObjectArrayElement(env, stackTrace, j);
                XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(stackTraceElement, err);

                jstring stackTraceElementStr = (*env)->CallObjectMethod(env, stackTraceElement, method_toString);
                XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(stackTraceElementStr, err);

                const char *c_stackTraceElementStr = (*env)->GetStringUTFChars(env, stackTraceElementStr, 0);
                if(0 != xcc_util_write_str(log_fd, "    at ")) goto err;
                if(0 != xcc_util_write_str(log_fd, c_stackTraceElementStr)) goto err;
                if(0 != xcc_util_write_str(log_fd, "\n")) goto err;
                (*env)->ReleaseStringUTFChars(env, stackTraceElementStr, c_stackTraceElementStr);
            }
            if(0 != xcc_util_write_str(log_fd, "\n")) goto err;
            break;
        }
        
        (*env)->ReleaseStringUTFChars(env, name, c_name);
    }

 err:
    return;
}

static void xc_jni_callback_java(JNIEnv *env, int log_fd, const char *log_pathname, char *emergency)
{
    const char *c_pathname = NULL;
    jstring     j_pathname = NULL;
    jstring     j_emergency  = NULL;
    
    if(NULL == xc_jni_class_cb || NULL == xc_jni_method_cb) return;

    //get log pathname
    if(log_fd >= 0)
    {
        c_pathname = log_pathname;
    }
    if(NULL != c_pathname)
    {
        j_pathname = (*env)->NewStringUTF(env, c_pathname);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(j_pathname, err);
    }

    //get emergency
    if(NULL != emergency)
    {
        j_emergency = (*env)->NewStringUTF(env, emergency);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(j_emergency, err);
    }
    
    (*env)->CallStaticVoidMethod(env, xc_jni_class_cb, xc_jni_method_cb, j_pathname, j_emergency);
    XC_JNI_CHECK_PENDING_EXCEPTION(err);

 err:
    (*env)->DeleteGlobalRef(env, xc_jni_class_cb);
    XC_JNI_CHECK_PENDING_EXCEPTION(end);

 end:
    return;
}

static void *xc_jni_callback_do(void *arg)
{
    JNIEnv *env      = NULL;
    int     attached = 0;
    jint    r, r2;

    (void)arg;
    
    pthread_setname_np(pthread_self(), "xcrash_callback");
    
    //get env
    r = (*xc_jni_jvm)->GetEnv(xc_jni_jvm, (void **)&env, JNI_VERSION_1_6);
    if(JNI_OK == r)
    {
        //OK
    }
    else if(JNI_EDETACHED == r)
    {
        //try attach current thread to JVM
        r2 = (*xc_jni_jvm)->AttachCurrentThread(xc_jni_jvm, &env, NULL);
        XC_JNI_CHECK_PENDING_EXCEPTION(err);
        if(JNI_OK != r2) goto err;
        attached = 1;
    }
    else
    {
        //error
        goto err;
    }
    if(NULL == env) goto err;

    //record java stack trace
    if(xc_jni_log_fd >= 0) xc_jni_record_stack_trace(env, xc_jni_log_fd, xc_jni_crash_tid);

    //callback to jvm
    xc_jni_callback_java(env, xc_jni_log_fd, xc_jni_log_pathname, xc_jni_emergency);

 err:
    if(attached) (*xc_jni_jvm)->DetachCurrentThread(xc_jni_jvm);
    return NULL;
}

void xc_jni_callback(int log_fd, const char *log_pathname, char *emergency)
{
    pthread_t thd;

    if(NULL == xc_jni_jvm) return;

    xc_jni_crash_tid    = gettid();
    xc_jni_emergency    = emergency;
    xc_jni_log_fd       = log_fd;
    xc_jni_log_pathname = log_pathname;
    
    if(0 != pthread_create(&thd, NULL, xc_jni_callback_do, NULL)) return;
    pthread_join(thd, NULL);
}

static jint xc_jni_init_ex(JNIEnv *env, jobject thiz, jobject context, jboolean restore_signal_handler,
                           jstring app_version, jstring log_dir, jstring log_prefix, jstring log_suffix,
                           jint logcat_system_lines, jint logcat_events_lines, jint logcat_main_lines,
                           jboolean dump_map, jboolean dump_fds, jboolean dump_all_threads,
                           jint dump_all_threads_count_max, jobjectArray dump_all_threads_whitelist,
                           jstring callback_class, jstring callback_method)
{
    const char  *c_app_id                         = NULL;
    const char  *c_app_version                    = NULL;
    const char  *c_log_dir                        = NULL;
    const char  *c_log_prefix                     = NULL;
    const char  *c_log_suffix                     = NULL;
    const char  *c_callback_class                 = NULL;
    const char  *c_callback_method                = NULL;
    const char  *c_app_lib_dir                    = NULL;
    const char  *c_files_dir                      = NULL;
    
    const char **c_dump_all_threads_whitelist     = NULL;
    size_t       c_dump_all_threads_whitelist_len = 0;
    size_t       len, i;

    jstring      app_id                           = NULL;
    jstring      app_lib_dir                      = NULL;
    jstring      files_dir                        = NULL;
    
    const char  *p_log_dir                        = NULL;
    char        *p_log_dir_tmp                    = NULL;
    int          r                                = 0;

    (void)thiz;

    if(!env || !(*env) || !context || logcat_system_lines < 0 || logcat_events_lines < 0 || logcat_main_lines < 0) return XCC_ERRNO_INVAL;

    //get app_id, app_version, app_lib_dir, files_dir from context
    if(0 != xc_jni_get_app_info(env, context, &app_id, (app_version ? NULL : &app_version), &app_lib_dir,
                                (log_dir ? NULL : &files_dir))) return XCC_ERRNO_INVAL;

    c_app_id = (app_id ? (*env)->GetStringUTFChars(env, app_id, 0) : NULL);
    c_app_version = (app_version ? (*env)->GetStringUTFChars(env, app_version, 0) : NULL);
    c_log_dir = (log_dir ? (*env)->GetStringUTFChars(env, log_dir, 0) : NULL);
    c_log_prefix = (log_prefix ? (*env)->GetStringUTFChars(env, log_prefix, 0) : NULL);
    c_log_suffix = (log_suffix ? (*env)->GetStringUTFChars(env, log_suffix, 0) : NULL);
    c_callback_class = (callback_class ? (*env)->GetStringUTFChars(env, callback_class, 0) : NULL);
    c_callback_method = (callback_method ? (*env)->GetStringUTFChars(env, callback_method, 0) : NULL);
    c_app_lib_dir = (app_lib_dir ? (*env)->GetStringUTFChars(env, app_lib_dir, 0) : NULL);
    c_files_dir = (files_dir ? (*env)->GetStringUTFChars(env, files_dir, 0) : NULL);

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
                    jstring wl = (jstring)((*env)->GetObjectArrayElement(env, dump_all_threads_whitelist, (jsize)i));
                    c_dump_all_threads_whitelist[i] = (wl ? (*env)->GetStringUTFChars(env, wl, 0) : NULL);
                }
            }
        }
    }
    
    //check c_app_lib_dir
    if(NULL == c_app_lib_dir)
    {
        r = XCC_ERRNO_INVAL;
        goto end;
    }
    
    //check & build log_dir
    if(NULL == c_log_dir)
    {
        if(NULL == c_files_dir)
        {
            r = XCC_ERRNO_INVAL;
            goto end;
        }
        if(NULL == (p_log_dir_tmp = xc_util_strdupcat(c_files_dir, "/tombstones")))
        {
            r = XCC_ERRNO_NOMEM;
            goto end;
        }
        p_log_dir = (const char *)p_log_dir_tmp;
    }
    else
    {
        p_log_dir = c_log_dir;
    }

    //save callback class & method
    if(NULL != c_callback_class && NULL != c_callback_method)
    {
        jclass class_cb = (*env)->FindClass(env, c_callback_class);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(class_cb, skip);

        xc_jni_class_cb = (*env)->NewGlobalRef(env, class_cb);
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_jni_class_cb, skip);

        xc_jni_method_cb = (*env)->GetStaticMethodID(env, xc_jni_class_cb, c_callback_method, "(Ljava/lang/String;Ljava/lang/String;)V");
        XC_JNI_CHECK_NULL_AND_PENDING_EXCEPTION(xc_jni_method_cb, skip);
    }
    
 skip:

    //init native core
    r = xc_core_init((int)restore_signal_handler, c_app_id, c_app_version, c_app_lib_dir,
                     p_log_dir, c_log_prefix, c_log_suffix,
                     (unsigned int)logcat_system_lines, (unsigned int)logcat_events_lines, (unsigned int)logcat_main_lines,
                     (int)dump_map, (int)dump_fds, (int)dump_all_threads,
                     (int)dump_all_threads_count_max, c_dump_all_threads_whitelist,
                     c_dump_all_threads_whitelist_len);

    //free log_dir
    if(NULL != p_log_dir_tmp)
        free(p_log_dir_tmp);

 end:
    if(app_id) (*env)->ReleaseStringUTFChars(env, app_id, c_app_id);
    if(app_version) (*env)->ReleaseStringUTFChars(env, app_version, c_app_version);
    if(log_dir) (*env)->ReleaseStringUTFChars(env, log_dir, c_log_dir);
    if(log_prefix) (*env)->ReleaseStringUTFChars(env, log_prefix, c_log_prefix);
    if(log_suffix) (*env)->ReleaseStringUTFChars(env, log_suffix, c_log_suffix);
    if(callback_class) (*env)->ReleaseStringUTFChars(env, callback_class, c_callback_class);
    if(callback_method) (*env)->ReleaseStringUTFChars(env, callback_method, c_callback_method);
    if(app_lib_dir) (*env)->ReleaseStringUTFChars(env, app_lib_dir, c_app_lib_dir);
    if(files_dir) (*env)->ReleaseStringUTFChars(env, files_dir, c_files_dir);

    if(dump_all_threads_whitelist && NULL != c_dump_all_threads_whitelist)
    {
        for(i = 0; i < c_dump_all_threads_whitelist_len; i++)
        {
            jstring wl = (jstring)((*env)->GetObjectArrayElement(env, dump_all_threads_whitelist, (jsize)i));
            const char *c_wl = c_dump_all_threads_whitelist[i];
            if(wl && NULL != c_wl) (*env)->ReleaseStringUTFChars(env, wl, c_wl);
        }
    }
    
    return r;
}

static jint xc_jni_init(JNIEnv *env, jobject thiz, jobject context)
{
    //use default values
    return xc_jni_init_ex(env, thiz, context, 1, NULL, NULL, NULL, NULL, 50, 50, 200, 1, 1, 1, 0, NULL, NULL, NULL);
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
        "Landroid/content/Context;"
        ")"
        "I",
        (void *)xc_jni_init
    },
    {
        "initEx",
        "("
        "Landroid/content/Context;"
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
        "I"
        "[Ljava/lang/String;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        ")"
        "I",
        (void *)xc_jni_init_ex
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

    //save the JVM handler
    xc_jni_jvm = vm;

    //get env
    if(JNI_OK != (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6)) return -1;
    
    //get class
    if(NULL == (clazz = (*env)->FindClass(env, XC_JNI_CLASS_NAME))) return -1;

    //register native methods
    if((*env)->RegisterNatives(env, clazz, xc_jni_methods, sizeof(xc_jni_methods) / sizeof(xc_jni_methods[0]))) return -1;
    
    return JNI_VERSION_1_6;
}
