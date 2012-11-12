AUDIO_NATIVE := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_BOARD_PLATFORM), msm7k)
    include $(AUDIO_NATIVE)/qdsp5/Android.mk
else ifeq "$(findstring qsd8250,$(QCOM_TARGET_PRODUCT))" "qsd8250"
    include $(AUDIO_NATIVE)/qdsp6/Android.mk
else ifeq "$(findstring qsd8650a,$(QCOM_TARGET_PRODUCT))" "qsd8650a"
    include $(AUDIO_NATIVE)/qdsp6/Android.mk
else ifeq "$(findstring msm8660,$(QCOM_TARGET_PRODUCT))" "msm8660"
    include $(AUDIO_NATIVE)/qdsp5/Android.mk
endif
