LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# -------------------------- Corgi-Raw ---------------------------
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src

LOCAL_SRC_FILES :=  corgi-raw.c

LOCAL_MODULE := corgi
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

# -------------------------- Dejitter ---------------------------
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src

LOCAL_SRC_FILES := dejitter.c  ../src/ts_parse_vars.c

LOCAL_MODULE := dejitter

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

# -------------------------- Input-Raw ---------------------------
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src/

LOCAL_SRC_FILES := input-raw.c

LOCAL_MODULE := inputraw

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

# -------------------------- Linear ---------------------------
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src

LOCAL_SRC_FILES := linear.c  ../src/ts_parse_vars.c ../android/CalibrateTouchScreen.c

LOCAL_MODULE := linear

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

# -------------------------- Variance ---------------------------
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src

LOCAL_SRC_FILES := variance.c ../src/ts_parse_vars.c

LOCAL_MODULE := variance

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

# -------------------------- Pthres ---------------------------
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src

LOCAL_SRC_FILES := pthres.c ../src/ts_parse_vars.c

LOCAL_MODULE := pthres

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
