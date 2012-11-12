LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
       CRefCne.cpp\
       CRefCneRadio.cpp\
       CneSvc.cpp

LOCAL_MODULE:= librefcne

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_C_INCLUDES := \
	external/connectivity/reference-cne/inc \
	external/connectivity/include/cne

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

