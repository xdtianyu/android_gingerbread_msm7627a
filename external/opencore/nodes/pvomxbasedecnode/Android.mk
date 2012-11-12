LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_omx_basedec_node.cpp \
 	src/pvmf_omx_basedec_port.cpp \
 	src/pvmf_omx_basedec_callbacks.cpp


LOCAL_MODULE := libpvomxbasedecnode

LOCAL_CFLAGS :=  $(PV_CFLAGS)

# board-specific configuration
LOCAL_CFLAGS += $(BOARD_OPENCORE_FLAGS)

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
	$(PV_TOP)/nodes/pvomxbasedecnode/src \
 	$(PV_TOP)/nodes/pvomxbasedecnode/include \
 	$(PV_TOP)/extern_libs_v2/khronos/openmax/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_omx_basedec_defs.h \
 	include/pvmf_omx_basedec_port.h \
 	include/pvmf_omx_basedec_node.h \
 	include/pvmf_omx_basedec_callbacks.h \
 	include/pvmf_omx_basedec_node_extension_interface.h

include $(BUILD_STATIC_LIBRARY)
