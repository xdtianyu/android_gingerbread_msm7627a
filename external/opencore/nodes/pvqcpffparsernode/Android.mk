LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/pvmf_qcpffparser_node.cpp \
 	src/pvmf_qcpffparser_port.cpp \
 	src/pvmf_qcpffparser_factory.cpp


LOCAL_MODULE := libpvqcpffparsernode

LOCAL_CFLAGS :=  $(PV_CFLAGS)



LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/nodes/pvqcpffparsernode/src \
 	$(PV_TOP)/nodes/pvqcpffparsernode/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/pvmf_qcpffparser_factory.h \
 	include/pvmf_qcpffparser_registry_factory.h \
 	include/pvmf_qcpffparser_events.h

include $(BUILD_STATIC_LIBRARY)
