CONN_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)
include $(call first-makefiles-under, $(CONN_PATH))
endif

