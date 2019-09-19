LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE           := xcrash_dumper
LOCAL_CFLAGS           := -std=c11 -Weverything -Werror -fvisibility=hidden -fPIE
LOCAL_LDFLAGS          := -pie
LOCAL_LDLIBS           := -ldl -llog
LOCAL_STATIC_LIBRARIES := lzma
LOCAL_C_INCLUDES       := $(LOCAL_PATH) $(LOCAL_PATH)/../../common
LOCAL_SRC_FILES        := $(wildcard $(LOCAL_PATH)/*.c) $(wildcard $(LOCAL_PATH)/../../common/*.c)
include $(BUILD_EXECUTABLE)
include $(LOCAL_PATH)/lzma/Android.mk
