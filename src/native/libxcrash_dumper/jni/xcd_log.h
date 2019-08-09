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

#ifndef XCD_LOG_H
#define XCD_LOG_H 1

#include <stdarg.h>
#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XCD_LOG_PRIO ANDROID_LOG_WARN

#define XCD_LOG_TAG "xcrash_dumper"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define XCD_LOG_DEBUG(fmt, ...) do{if(XCD_LOG_PRIO <= ANDROID_LOG_DEBUG) __android_log_print(ANDROID_LOG_DEBUG, XCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define XCD_LOG_INFO(fmt, ...)  do{if(XCD_LOG_PRIO <= ANDROID_LOG_INFO)  __android_log_print(ANDROID_LOG_INFO,  XCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define XCD_LOG_WARN(fmt, ...)  do{if(XCD_LOG_PRIO <= ANDROID_LOG_WARN)  __android_log_print(ANDROID_LOG_WARN,  XCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#define XCD_LOG_ERROR(fmt, ...) do{if(XCD_LOG_PRIO <= ANDROID_LOG_ERROR) __android_log_print(ANDROID_LOG_ERROR, XCD_LOG_TAG, fmt, ##__VA_ARGS__);}while(0)
#pragma clang diagnostic pop

//debug-log flags for modules
#define XCD_CORE_DEBUG          0
#define XCD_THREAD_DEBUG        0
#define XCD_ELF_DEBUG           0
#define XCD_ELF_INTERFACE_DEBUG 0
#define XCD_FRAMES_DEBUG        0
#define XCD_DWARF_DEBUG         0
#define XCD_ARM_EXIDX_DEBUG     0

#ifdef __cplusplus
}
#endif

#endif
