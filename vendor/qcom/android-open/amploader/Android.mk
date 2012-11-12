ifeq ($(BOARD_HAVE_BLUETOOTH), true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_BUILD_TYPE),debug)
LOCAL_CFLAGS += -DAMP_DEBUG
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES += liblog libcutils

ifeq ($(BOARD_HAS_QCOM_WLAN), true)
LOCAL_CFLAGS += -DBOARD_HAS_QCOM_WLAN
LOCAL_C_INCLUDES += $(call include-path-for, libhardware_legacy)/hardware_legacy
LOCAL_SHARED_LIBRARIES += libhardware_legacy
endif # BOARD_HAS_QCOM_WLAN

LOCAL_SRC_FILES := amploader.c

LOCAL_MODULE := amploader

LOCAL_PRELINK_MODULE := false
include $(BUILD_EXECUTABLE)

endif # BOARD_HAVE_BLUETOOTH

