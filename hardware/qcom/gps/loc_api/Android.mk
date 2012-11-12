ifneq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE),)
LOCAL_PATH := $(call my-dir)
#set the name of the symlink to be used by symlink.in
SYMLINK := $(TARGET_OUT_SHARED_LIBRARIES)/hw/gps.$(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE).so
GPS_DIR_LIST :=
# add RPC dirs if RPC is available
ifneq ($(TARGET_NO_RPC),true)
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api-rpc-50001/
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api-rpc/
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api/
GPS_DIR_LIST += $(LOCAL_PATH)/libloc_api_50001/
endif #TARGET_NO_RPC

#add QMI libraries
QMI_PRODUCT_LIST := msm8960
ifneq (, $(filter $(QMI_PRODUCT_LIST), $(QCOM_TARGET_PRODUCT)))
GPS_DIR_LIST += $(LOCAL_PATH)/loc_api_v02/
GPS_DIR_LIST += $(LOCAL_PATH)/loc_eng_v02/
endif #filter $(PRODUCT_LIST), $(QCOM_TARGET_PRODUCT)

#call the subfolders
include $(addsuffix Android.mk, $(GPS_DIR_LIST))

endif#BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE
