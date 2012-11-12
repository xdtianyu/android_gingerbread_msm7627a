LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_mp4ffparser_node.cpp \
 	src/pvmf_mp4ffparser_factory.cpp \
 	src/pvmf_mp4ffparser_outport.cpp \
 	src/pvmf_mp4ffparser_node_metadata.cpp \
 	src/pvmf_mp4ffparser_node_cap_config.cpp


LOCAL_MODULE := libpvmp4ffparsernode

LOCAL_CFLAGS :=  $(PV_CFLAGS)

ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
   ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF7x30
        endif
endif

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/pvmp4ffparsernode/src \
 	$(PV_TOP)/nodes/pvmp4ffparsernode/include \
 	$(PV_TOP)/nodes/pvmp4ffparsernode/src/default \
 	$(PV_TOP)/fileformats/mp4/parser/utils/mp4recognizer/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_mp4ffparser_factory.h \
 	include/pvmf_mp4ffparser_events.h

include $(BUILD_STATIC_LIBRARY)
