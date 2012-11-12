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
LOCAL_PATH := $(call my-dir)

#
# This library depends on the AllJoyn daemon library.  It must be separately
# built before building the JNI code in this directory.  We have the linker
# look for this library in the top level build directory (not in the subdir
# alljoyn_core/build.  To build the JNI library here, do "ndk-build -B V=1".
#
#BUS_LIB_DIR := ../../../../../build/android/arm/$(APP_OPTIM)/dist/lib/
BUS_LIB_DIR := ../../lib/

include $(CLEAR_VARS)

LOCAL_MODULE    := bus-daemon-jni
LOCAL_SRC_FILES := bus-daemon-jni.c
LOCAL_LDLIBS := -L$(BUS_LIB_DIR) -lalljoyn-daemon -lalljoyn -lcrypto -llog -lgcc

include $(BUILD_SHARED_LIBRARY)
