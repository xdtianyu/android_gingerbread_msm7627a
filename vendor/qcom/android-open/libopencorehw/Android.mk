ifneq ($(BUILD_WITHOUT_PV), true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Set up the OpenCore variables.
include external/opencore/Config.mk
LOCAL_C_INCLUDES := $(PV_INCLUDES)\
                    hardware/msm7k/libgralloc-qsd8k

ifneq (, $(filter msm7201a_ffa msm7201a_surf msm7627a msm7627_ffa msm7627_surf qsd8250_ffa qsd8250_surf qsd8650a_st1x, $(QCOM_TARGET_PRODUCT)))
  LOCAL_SRC_FILES := android_surface_output_msm72xx.cpp
endif
ifneq (, $(filter msm7630_surf msm7630_1x msm7630_fusion msm8660_surf msm8660_csfb, $(QCOM_TARGET_PRODUCT)))
  LOCAL_SRC_FILES := android_surface_output_msm7x30.cpp
endif


LOCAL_CFLAGS := $(PV_CFLAGS_MINUS_VISIBILITY)

LOCAL_C_INCLUDES += hardware/msm7k/libgralloc-qsd8k

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libui \
    libhardware\
    libandroid_runtime \
    libmedia \
    libskia \
    libopencore_common \
    libicuuc \
    libsurfaceflinger_client \
    libopencore_player

LOCAL_MODULE := libopencorehw

LOCAL_MODULE_TAGS := optional

LOCAL_LDLIBS += 

include $(BUILD_SHARED_LIBRARY)
endif
