# For cnd binary
# =======================
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        cnd.c \
        cnd_event.cpp \
        cnd_process.cpp \
        cnd_iproute2.cpp

LOCAL_MODULE:= cnd

LOCAL_SHARED_LIBRARIES := \
        libutils \
        libbinder \
        libcutils \
        libdl \
        libhardware_legacy \

LOCAL_C_INCLUDES := \
        external/connectivity/cnd/inc  \
        external/connectivity/include/cne

include external/connectivity/stlport/libstlport.mk

include $(BUILD_EXECUTABLE)
# vim: et
