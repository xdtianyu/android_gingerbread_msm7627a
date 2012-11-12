LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    metadatadriver.cpp \
    playerdriver.cpp \
    thread_init.cpp \
    pvmediascanner.cpp \
    android_surface_output.cpp \
    android_audio_output.cpp \
    android_audio_stream.cpp \
    android_audio_mio.cpp \
    android_audio_output_threadsafe_callbacks.cpp \
    android_audio_lpadecode.cpp \
    android_audio_lpadecode_threadsafe_callbacks.cpp

LOCAL_CFLAGS := $(PV_CFLAGS)

# board-specific configuration
LOCAL_CFLAGS += $(BOARD_OPENCORE_FLAGS)

ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF7x30
    endif
endif


LOCAL_C_INCLUDES := $(PV_INCLUDES) \
    $(PV_TOP)/engines/common/include \
    $(PV_TOP)/fileformats/mp4/parser/include \
    $(PV_TOP)/fileformats/qcp/parser/include \
    $(PV_TOP)/pvmi/media_io/pvmiofileoutput/include \
    $(PV_TOP)/nodes/pvmediaoutputnode/include \
    $(PV_TOP)/nodes/pvmediainputnode/include \
    $(PV_TOP)/nodes/pvmp4ffcomposernode/include \
    $(PV_TOP)/engines/player/include \
    $(PV_TOP)/nodes/common/include \
    $(PV_TOP)/fileformats/pvx/parser/include \
	$(PV_TOP)/nodes/pvprotocolenginenode/download_protocols/common/src \
    libs/drm/mobile1/include \
    include/graphics \
    external/skia/include/corecg \
    $(call include-path-for, graphics corecg) \
    external/tremolo/Tremolo

LOCAL_MODULE := libandroidpv

LOCAL_SHARED_LIBRARIES := libui libutils libbinder libsurfaceflinger_client libcamera_client
LOCAL_STATIC_LIBRARIES := libosclbase libosclerror libosclmemory libosclutil

LOCAL_LDLIBS +=

include $(BUILD_STATIC_LIBRARY)

