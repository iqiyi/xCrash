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

#ifndef XC_JNI_H
#define XC_JNI_H 1

#include <stdint.h>
#include <sys/types.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XC_JNI_IGNORE_PENDING_EXCEPTION()                 \
    do {                                                  \
        if((*env)->ExceptionCheck(env))                   \
        {                                                 \
            (*env)->ExceptionClear(env);                  \
        }                                                 \
    } while(0)

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

#define XC_JNI_VERSION    JNI_VERSION_1_6
#define XC_JNI_CLASS_NAME "xcrash/NativeHandler"

#ifdef __cplusplus
}
#endif

#endif
