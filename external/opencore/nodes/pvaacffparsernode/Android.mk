LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_aacffparser_node.cpp \
 	src/pvmf_aacffparser_factory.cpp \
 	src/pvmf_aacffparser_outport.cpp


LOCAL_MODULE := libpvaacffparsernode

LOCAL_CFLAGS :=  $(PV_CFLAGS)

ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
       LOCAL_CFLAGS += -DSURF7x30
    endif
endif

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/pvaacffparsernode/src \
 	$(PV_TOP)/nodes/pvaacffparsernode/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_aacffparser_factory.h \
 	include/pvmf_aacffparser_defs.h

include $(BUILD_STATIC_LIBRARY)
