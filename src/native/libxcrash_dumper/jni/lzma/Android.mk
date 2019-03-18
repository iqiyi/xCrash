LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE            := lzma
LOCAL_CFLAGS            := -std=c11 -Wall -Wextra -Werror -Wno-enum-conversion -fPIE -D_7ZIP_ST
LOCAL_C_INCLUDES        := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_SRC_FILES         := 7zCrc.c      \
                           7zCrcOpt.c   \
                           CpuArch.c    \
                           Bra.c        \
                           Bra86.c      \
                           BraIA64.c    \
                           Delta.c      \
                           Lzma2Dec.c   \
                           LzmaDec.c    \
                           Sha256.c     \
                           Xz.c         \
                           XzCrc64.c    \
                           XzCrc64Opt.c \
                           XzDec.c
include $(BUILD_STATIC_LIBRARY)
