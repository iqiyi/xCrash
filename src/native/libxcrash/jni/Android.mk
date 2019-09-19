LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE           := test
LOCAL_CFLAGS           := -std=c11 -Weverything -Werror -O0
LOCAL_C_INCLUDES       := $(LOCAL_PATH)
LOCAL_SRC_FILES        := xc_test.c
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE           := xcrash
LOCAL_CFLAGS           := -std=c11 -Weverything -Werror -fvisibility=hidden
LOCAL_LDLIBS           := -ldl -llog
LOCAL_STATIC_LIBRARIES := test
LOCAL_C_INCLUDES       := $(LOCAL_PATH) $(LOCAL_PATH)/../../common
LOCAL_SRC_FILES        := xc_jni.c      \
                          xc_common.c   \
                          xc_crash.c    \
                          xc_trace.c    \
                          xc_dl.c       \
                          xc_fallback.c \
                          xc_util.c     \
                          $(wildcard $(LOCAL_PATH)/../../common/*.c)
include $(BUILD_SHARED_LIBRARY)
