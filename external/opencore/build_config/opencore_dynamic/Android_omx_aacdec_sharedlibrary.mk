LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := \
	libomx_aac_component_lib \
 	libpv_aac_dec

LOCAL_MODULE := libomx_aacdec_sharedlibrary

ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
-include $(PV_TOP)/Android_platform_extras.mk
endif

# To include lpthread to LOCAL_LDLIBS and Simulator related libs
-include $(PV_TOP)/Android_system_extras.mk
LOCAL_SHARED_LIBRARIES +=   libomx_sharedlibrary libopencore_common

include $(BUILD_SHARED_LIBRARY)
include   $(PV_TOP)/codecs_v2/omx/omx_aac/Android.mk
include   $(PV_TOP)/codecs_v2/audio/aac/dec/Android.mk

