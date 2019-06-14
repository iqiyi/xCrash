LOCAL_PATH := $(call my-dir)
NATIVE_PATH := $(LOCAL_PATH)/../../../../../../native
include $(NATIVE_PATH)/libxcrash/jni/Android.mk
include $(NATIVE_PATH)/libxcrash_dumper/jni/Android.mk
