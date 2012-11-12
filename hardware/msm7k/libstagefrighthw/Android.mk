# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ifneq ($(BUILD_TINY_ANDROID),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifneq (, $(filter msm7201a_ffa msm7201a_surf msm7627a msm7627_ffa msm7627_surf msm7627_7x_ffa msm7627_7x_surf qsd8250_ffa qsd8250_surf qsd8650a_st1x, $(QCOM_TARGET_PRODUCT)))
LOCAL_SRC_FILES := \
    stagefright_surface_output_msm72xx.cpp \
    QComHardwareRenderer.cpp \
    omx_drmplay_renderer.cpp \
    QComOMXPlugin.cpp
endif

ifneq (, $(filter msm7630_surf msm7630_1x msm8660_surf msm8660_csfb msm7630_fusion msm8960, $(QCOM_TARGET_PRODUCT)))
LOCAL_SRC_FILES := \
    stagefright_surface_output_msm7x30.cpp \
    QComHardwareOverlayRenderer.cpp \
    QComHardwareRenderer.cpp \
    omx_drmplay_renderer.cpp \
    QComOMXPlugin.cpp
endif

LOCAL_CFLAGS := $(PV_CFLAGS_MINUS_VISIBILITY)

ifeq ($(TARGET_BOARD_PLATFORM),msm7k)
    ifeq ($(BOARD_USES_QCOM_AUDIO_V2), true)
        LOCAL_CFLAGS += -DSURF7x30
    endif
endif

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/base/include/media/stagefright/openmax

LOCAL_C_INCLUDES += hardware/msm7k/libgralloc-qsd8k

LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libdl                   \
        libui                   \
        libsurfaceflinger_client \
        libhardware

LOCAL_MODULE := libstagefrighthw

include $(BUILD_SHARED_LIBRARY)

endif # not BUILD_TINY_ANDROID
