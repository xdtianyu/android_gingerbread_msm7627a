LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := \
	libosclbase \
	libosclerror \
	libosclmemory \
	libosclutil \
	libosclproc \
	libosclio \
	libosclregserv \
	libosclregcli \
	liboscllib \
	libpv_amr_nb_common_lib \
	libgetactualaacconfig \
	libpv_config_parser \
	libthreadsafe_callback_ao \
	libpvmediadatastruct \
	libpvmimeutils \
	libpvgendatastruct \
	libpvmf \
	libpvthreadmessaging


ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
LOCAL_WHOLE_STATIC_LIBRARIES += \
	libomx_mastercore_lib \
	libpv_avc_common_lib \
	libpvgsmamrparser \
	libm4v_config \
	libcolorconvert \
	libpvfileoutputnode \
	libpvmediainputnode \
	libpvomxencnode \
	libpvmiofileinput \
	libpvmioaviwavfileinput \
	libpvavifileparser \
	libpvmiofileoutput \
	libpvmediaoutputnode \
	libpvomxvideodecnode \
	libpvomxaudiodecnode \
	libpvomxbasedecnode \
	libpvlatmpayloadparser \
	libpvwav \
	libpvfileparserutils
endif


# to solve circular dependency among the static libraries.
LOCAL_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES) $(LOCAL_WHOLE_STATIC_LIBRARIES)

LOCAL_MODULE := libopencore_common

ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
-include $(PV_TOP)/Android_platform_extras.mk
endif

# To include lpthread to LOCAL_LDLIBS and Simulator related libs
-include $(PV_TOP)/Android_system_extras.mk

LOCAL_SHARED_LIBRARIES +=   

include $(BUILD_SHARED_LIBRARY)
include   $(PV_TOP)/oscl/oscl/osclbase/Android.mk
include   $(PV_TOP)/oscl/oscl/osclerror/Android.mk
include   $(PV_TOP)/oscl/oscl/osclmemory/Android.mk
include   $(PV_TOP)/oscl/oscl/osclutil/Android.mk
include   $(PV_TOP)/oscl/oscl/osclproc/Android.mk
include   $(PV_TOP)/oscl/oscl/osclio/Android.mk
include   $(PV_TOP)/oscl/oscl/osclregserv/Android.mk
include   $(PV_TOP)/oscl/oscl/osclregcli/Android.mk
include   $(PV_TOP)/oscl/pvlogger/Android.mk
include   $(PV_TOP)/oscl/oscl/oscllib/Android.mk
include   $(PV_TOP)/codecs_v2/audio/gsm_amr/common/dec/Android.mk
include   $(PV_TOP)/codecs_v2/audio/gsm_amr/amr_nb/common/Android.mk
include   $(PV_TOP)/codecs_v2/audio/aac/dec/util/getactualaacconfig/Android.mk
include   $(PV_TOP)/codecs_v2/utilities/pv_config_parser/Android.mk
include   $(PV_TOP)/baselibs/threadsafe_callback_ao/Android.mk
include   $(PV_TOP)/baselibs/media_data_structures/Android.mk
include   $(PV_TOP)/baselibs/pv_mime_utils/Android.mk
include   $(PV_TOP)/baselibs/gen_data_structures/Android.mk
include   $(PV_TOP)/baselibs/thread_messaging/Android.mk
include   $(PV_TOP)/pvmi/pvmf/Android.mk

ifneq ($(BUILD_PV_AUDIO_DEC_ONLY),1)
include   $(PV_TOP)/codecs_v2/omx/omx_mastercore/Android.mk
include   $(PV_TOP)/codecs_v2/video/avc_h264/common/Android.mk
include   $(PV_TOP)/fileformats/rawgsmamr/parser/Android.mk
include   $(PV_TOP)/codecs_v2/utilities/m4v_config_parser/Android.mk
include   $(PV_TOP)/codecs_v2/utilities/colorconvert/Android.mk
include   $(PV_TOP)/nodes/pvfileoutputnode/Android.mk
include   $(PV_TOP)/nodes/pvmediainputnode/Android.mk
include   $(PV_TOP)/nodes/pvomxencnode/Android.mk
include   $(PV_TOP)/pvmi/media_io/pvmi_mio_fileinput/Android.mk
include   $(PV_TOP)/pvmi/media_io/pvmi_mio_avi_wav_fileinput/Android.mk
include   $(PV_TOP)/fileformats/avi/parser/Android.mk
include   $(PV_TOP)/pvmi/media_io/pvmiofileoutput/Android.mk
include   $(PV_TOP)/nodes/pvmediaoutputnode/Android.mk
include   $(PV_TOP)/nodes/pvomxvideodecnode/Android.mk
include   $(PV_TOP)/nodes/pvomxaudiodecnode/Android.mk
include   $(PV_TOP)/nodes/pvomxbasedecnode/Android.mk
include   $(PV_TOP)/protocols/rtp_payload_parser/util/build/Android.mk
include   $(PV_TOP)/fileformats/wav/parser/Android.mk
include   $(PV_TOP)/fileformats/common/parser/Android.mk
include   $(PV_TOP)/nodes/common/Android.mk
include   $(PV_TOP)/engines/common/Android.mk
include   $(PV_TOP)/pvmi/content_policy_manager/plugins/common/Android.mk
endif
