LOCAL_PATH:= $(call my-dir)

# BTIO plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	btio.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.91\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \
	-DANDROID \

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../src \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libbluetoothd \
	libdbus


LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := libbtio_static

include $(BUILD_STATIC_LIBRARY)

