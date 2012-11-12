LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_MODULE := scmst_test
LOCAL_MODULE_TAGS := eng
include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)
ifeq ($(TARGET_BUILD_VARIANT),eng)
ALL_PREBUILT += $(TARGET_OUT)/bin/scmst_test
$(TARGET_OUT)/bin/scmst_test : $(LOCAL_PATH)/scmst_test | $(ACP)
	$(transform-prebuilt-to-target)
endif

