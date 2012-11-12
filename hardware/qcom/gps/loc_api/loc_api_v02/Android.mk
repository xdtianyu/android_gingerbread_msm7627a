ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libloc_api_v02

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libqmi_cci \
    libqmi_csi \
    libqmi_common_so

LOCAL_SRC_FILES += \
    loc_api_v02_client.c \
    loc_api_sync_req.c \
    qmi_loc_v02.c

LOCAL_CFLAGS += \
     -fno-short-enums \
     -D_ANDROID_

## Includes
LOCAL_C_INCLUDES += \
        $(QC_PROP_ROOT)/qmi-framework/inc \
        $(QC_PROP_ROOT)/qmi-framework/qcci/inc \
        $(QC_PROP_ROOT)/qmi-framework/qcsi/inc \
        $(QC_PROP_ROOT)/qmi-framework/common/inc

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

endif # not BUILD_TINY_ANDROID


