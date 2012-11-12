LOCAL_PATH:= $(call my-dir)

#
# libbluetoothd
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	android_bluez.c \
	adapter.c \
	agent.c \
	attrib-server.c \
	dbus-common.c \
	device.c \
	error.c \
	event.c \
	glib-helper.c \
	log.c \
	main.c \
	manager.c \
	oob.c \
	oui.c \
	plugin.c \
	rfkill.c \
	sdpd-request.c \
	sdpd-service.c \
	sdpd-server.c \
	sdpd-database.c \
	sdp-xml.c \
	storage.c \
	textfile.c

LOCAL_CFLAGS:= \
	-DVERSION=\"4.91\" \
	-DSTORAGEDIR=\"/data/misc/bluetoothd\" \
	-DCONFIGDIR=\"/etc/bluetooth\" \
	-DSERVICEDIR=\"/system/bin\" \
	-DPLUGINDIR=\"/system/lib/bluez-plugin\" \
	-DOUIFILE=\"/var/lib/misc/oui.txt\" \
	-DANDROID_SET_AID_AND_CAP \
	-DANDROID_EXPAND_NAME

ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)
LOCAL_CFLAGS += \
	-DBOARD_HAVE_BLUETOOTH_BCM
endif

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../gdbus \
	$(LOCAL_PATH)/../plugins \
	$(LOCAL_PATH)/../btio \
	$(LOCAL_PATH)/../attrib \
	$(LOCAL_PATH)/.. \
	$(call include-path-for, glib) \
	$(call include-path-for, glib)/glib \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libdl \
	libbluetooth \
	libdbus \
	libutils \
	libcutils

LOCAL_STATIC_LIBRARIES := \
	libbuiltinplugin \
	libgdbus_static \
	libattrib_static \
	libbtio_static \
	libglib_static

LOCAL_MODULE:=libbluetoothd

include $(BUILD_SHARED_LIBRARY)

#
# bluetoothd
#

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libbluetoothd

LOCAL_MODULE:=bluetoothd

include $(BUILD_EXECUTABLE)
