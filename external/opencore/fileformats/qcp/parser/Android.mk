LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	src/qcpfileparser.cpp \
 	src/iqcpff.cpp \
 	src/qcpparser.cpp \
 	src/qcputils.cpp


LOCAL_MODULE := libpvqcpparser

LOCAL_CFLAGS :=  $(PV_CFLAGS)



LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/fileformats/qcp/parser/src \
 	$(PV_TOP)/fileformats/qcp/parser/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/qcpfileparser.h \
 	include/iqcpff.h \
 	include/qcpparser.h \
 	include/qcputils.h

include $(BUILD_STATIC_LIBRARY)
