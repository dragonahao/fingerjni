
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES  := finger.c integration.c 

LOCAL_MODULE     := libfinger
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE    := false
LOCAL_LDLIBS    := -llog
LOCAL_SHARED_LIBRARIES := libc  liblog  libcutils

include $(BUILD_SHARED_LIBRARY)

