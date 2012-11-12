LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_omx_audiodec_factory.cpp \
 	src/pvmf_omx_audiodec_node.cpp


LOCAL_MODULE := libpvomxaudiodecnode

LOCAL_CFLAGS :=  $(PV_CFLAGS)

#Use HW AAC decoder for all 7K targets except 7x30
ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF7x30
    else
        LOCAL_CFLAGS += -DUSE_HW_AAC_DEC
    endif
endif
LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/pvomxaudiodecnode/src \
 	$(PV_TOP)/nodes/pvomxaudiodecnode/include \
 	$(PV_TOP)/extern_libs_v2/khronos/openmax/include \
 	$(PV_TOP)/extern_libs_v2/khronos/openmax/include \
 	$(PV_TOP)/baselibs/threadsafe_callback_ao/src \
 	$(PV_TOP)/nodes/pvomxbasedecnode/include \
 	$(PV_TOP)/nodes/pvomxbasedecnode/src \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/pvmf_omx_audiodec_factory.h

include $(BUILD_STATIC_LIBRARY)
