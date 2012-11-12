LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	src/aacfileparser.cpp


LOCAL_MODULE := libpvaacparser

LOCAL_CFLAGS :=  $(PV_CFLAGS)

#Use HW AAC decoder for all 7K targets except 7x30
ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifneq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DUSE_HW_AAC_DEC
    endif
endif

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/fileformats/rawaac/parser/src \
 	$(PV_TOP)/fileformats/rawaac/parser/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/aacfileparser.h

include $(BUILD_STATIC_LIBRARY)
