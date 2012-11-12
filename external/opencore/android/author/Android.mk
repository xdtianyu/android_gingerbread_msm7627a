LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# don't export mio symbols
LOCAL_CFLAGS += -DHIDE_MIO_SYMBOLS

# Temporary workaround
ifeq ($(strip $(USE_SHOLES_PROPERTY)),true)
LOCAL_CFLAGS += -DSHOLES_PROPERTY_OVERRIDES
endif

LOCAL_SRC_FILES := \
    authordriver.cpp \
    PVMediaRecorder.cpp \
    android_camera_input.cpp \
    android_audio_input.cpp \
    android_audio_input_threadsafe_callbacks.cpp \
    ../thread_init.cpp \
    android_camera_input_threadsafe_callbacks.cpp \
    android_audio_inputFMA2DP.cpp \

LOCAL_CFLAGS := $(PV_CFLAGS)

# board-specific configuration
LOCAL_CFLAGS += $(BOARD_OPENCORE_FLAGS)

# Disable 720p/H264 recording for unsupported targets (except 7x30)
ifneq (,$(filter qsd8k msm7k, $(TARGET_BOARD_PLATFORM)))
    ifneq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF8K_7x27
    endif
endif

ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF7x30
    else
        LOCAL_CFLAGS += -DSURF
    endif
else
    ifeq ($(TARGET_BOARD_PLATFORM),qsd8k)
        LOCAL_CFLAGS += -DSURF8K
    endif
endif

ifeq ($(TARGET_BOARD_PLATFORM),msm8660)
   LOCAL_CFLAGS += -DNTENCODE_8660
endif

LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES := $(PV_INCLUDES) \
    $(PV_TOP)/engines/common/include \
    $(PV_TOP)/codecs_v2/omx/omx_common/include \
    $(PV_TOP)/fileformats/mp4/parser/include \
    $(PV_TOP)/pvmi/media_io/pvmiofileoutput/include \
    $(PV_TOP)/nodes/pvmediaoutputnode/include \
    $(PV_TOP)/nodes/pvmediainputnode/include \
    $(PV_TOP)/nodes/pvmp4ffcomposernode/include \
    $(PV_TOP)/engines/player/include \
    $(PV_TOP)/nodes/common/include \
    libs/drm/mobile1/include \
    $(call include-path-for, graphics corecg) \
    external/tremolo/Tremolo

LOCAL_SHARED_LIBRARIES := libmedia libbinder

LOCAL_MODULE := libandroidpvauthor

LOCAL_LDLIBS += 

include $(BUILD_STATIC_LIBRARY)

