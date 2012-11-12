LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := src/pv_audio_config_parser.cpp

ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
LOCAL_SRC_FILES += src/pv_video_config_parser.cpp
endif

LOCAL_MODULE := libpv_config_parser

LOCAL_CFLAGS :=  $(PV_CFLAGS)

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/codecs_v2/utilities/pv_config_parser/src \
 	$(PV_TOP)/codecs_v2/utilities/pv_config_parser/include \
 	$(PV_TOP)/pvmi/pvmf/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := include/pv_audio_config_parser.h

ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
LOCAL_COPY_HEADERS += include/pv_video_config_parser.h
endif

include $(BUILD_STATIC_LIBRARY)