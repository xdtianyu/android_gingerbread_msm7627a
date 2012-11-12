LOCAL_PATH := $(my-dir)

########################
include $(CLEAR_VARS)

ifeq "$(findstring msm8660,$(QCOM_TARGET_PRODUCT))" "msm8660"

LOCAL_MODULE := media_profiles.xml

LOCAL_MODULE_TAGS := user

# This will install the file in /system/etc
#
LOCAL_MODULE_CLASS := ETC

LOCAL_SRC_FILES := $(LOCAL_MODULE)

include $(BUILD_PREBUILT)
endif


