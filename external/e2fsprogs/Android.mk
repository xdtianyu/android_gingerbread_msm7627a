E2FSPROGS_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),x86)
  include $(call all-subdir-makefiles)
else
  ifeq ($(BUILD_E2FSCK),true)
    include $(E2FSPROGS_PATH)/e2fsck/Android.mk
    include $(E2FSPROGS_PATH)/lib/ext2fs/Android.mk
    include $(E2FSPROGS_PATH)/lib/blkid/Android.mk
    include $(E2FSPROGS_PATH)/lib/e2p/Android.mk
    include $(E2FSPROGS_PATH)/lib/et/Android.mk
    include $(E2FSPROGS_PATH)/lib/uuid/Android.mk
  endif
endif
endif
