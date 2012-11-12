ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libloc_eng_v02.$(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE)

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libloc_api_v02

LOCAL_SRC_FILES += \
    loc_eng.cpp \
    loc_eng_xtra.cpp \
    loc_eng_cfg.cpp \
    loc_eng_ni.cpp \
    ../libloc_api_50001/gps.c


LOCAL_CFLAGS += \
     -fno-short-enums \
     -D_ANDROID_

## Includes
LOCAL_C_INCLUDES += \
     $(TOP)/hardware/qcom/gps/loc_api/loc_api_v02 \
     $(QC_PROP_ROOT)/qmi-framework/inc \
     $(QC_PROP_ROOT)/qmi-framework/qcci/inc \
     $(QC_PROP_ROOT)/qmi-framework/qcsi/inc \
     $(QC_PROP_ROOT)/qmi-framework/common/inc

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
include $(BUILD_SHARED_LIBRARY)
include $(LOCAL_PATH)/../symlink.in
endif # not BUILD_TINY_ANDROID


