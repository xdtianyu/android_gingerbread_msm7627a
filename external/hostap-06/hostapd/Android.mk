LOCAL_PATH := $(call my-dir)

# To force sizeof(enum) = 4
#ifeq ($(TARGET_ARCH),arm)
#HAPD_CFLAGS+= -mabi=aapcs-linux
#endif

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
HAPD_CFLAGS += -DHOSTAPD_DUMP_STATE -Dlinux -DANDROID

INCLUDES =  $(LOCAL_PATH)/../src
INCLUDES += $(LOCAL_PATH)/../src/crypto
INCLUDES += $(LOCAL_PATH)/../src/utils
INCLUDES += $(LOCAL_PATH)/../src/common
INCLUDES += $(LOCAL_PATH)/../../openssl/include

# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# HAPD_CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include

include $(LOCAL_PATH)/.config

ifndef CONFIG_OS
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_OS=win32
else
CONFIG_OS=unix
endif
endif

ifeq ($(CONFIG_OS), internal)
HAPD_CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

ifdef CONFIG_NATIVE_WINDOWS
HAPD_CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32
endif

HAPD_SRC =	hostapd.c ieee802_1x.c eapol_sm.c \
	ieee802_11.c config.c ieee802_11_auth.c accounting.c \
	sta_info.c wpa.c ctrl_iface.c \
	drivers.c preauth.c pmksa_cache.c beacon.c \
	hw_features.c wme.c ap_list.c \
	mlme.c vlan_init.c wpa_auth_ie.c

HAPD_SRC += ../src/utils/eloop.c
HAPD_SRC += ../src/utils/common.c
HAPD_SRC += ../src/utils/wpa_debug.c
HAPD_SRC += ../src/utils/wpabuf.c
HAPD_SRC += ../src/utils/os_$(CONFIG_OS).c
HAPD_SRC += ../src/utils/ip_addr.c

HAPD_SRC += ../src/common/ieee802_11_common.c
HAPD_SRC += ../src/common/wpa_common.c

HAPD_SRC += ../src/radius/radius.c
HAPD_SRC += ../src/radius/radius_client.c

HAPD_SRC += ../src/crypto/md5.c
HAPD_SRC += ../src/crypto/rc4.c
HAPD_SRC += ../src/crypto/md4.c
HAPD_SRC += ../src/crypto/sha1.c
HAPD_SRC += ../src/crypto/des.c
HAPD_SRC += ../src/crypto/aes_wrap.c
HAPD_SRC += ../src/crypto/aes.c

HLOCAL_SRC_FILES=../src/hlr_auc_gw/hlr_auc_gw.c ../src/utils/common.c ../src/utils/wpa_debug.c ../src/utils/os_$(CONFIG_OS).c ../src/hlr_auc_gw/milenage.c ../src/crypto/aes_wrap.c ../src/crypto/aes.c

HAPD_CFLAGS += -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX

ifdef CONFIG_IAPP
HAPD_CFLAGS += -DCONFIG_IAPP
HAPD_SRC += iapp.c
endif

ifdef CONFIG_RSN_PREAUTH
HAPD_CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_PEERKEY
HAPD_CFLAGS += -DCONFIG_PEERKEY
HAPD_SRC += peerkey.c
endif

ifdef CONFIG_IEEE80211W
HAPD_CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
endif

ifdef CONFIG_IEEE80211R
HAPD_CFLAGS += -DCONFIG_IEEE80211R
HAPD_SRC += wpa_ft.c
NEED_SHA256=y
endif

ifdef CONFIG_IEEE80211N
HAPD_CFLAGS += -DCONFIG_IEEE80211N
endif

ifdef CONFIG_DRIVER_HOSTAP
HAPD_CFLAGS += -DCONFIG_DRIVER_HOSTAP
HAPD_SRC += driver_hostap.c
endif

ifdef CONFIG_DRIVER_WIRED
HAPD_CFLAGS += -DCONFIG_DRIVER_WIRED
HAPD_SRC += driver_wired.c
endif

ifdef CONFIG_DRIVER_MADWIFI
HAPD_CFLAGS += -DCONFIG_DRIVER_MADWIFI
HAPD_SRC += driver_madwifi.c
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_DRIVER_ATHEROS
HAPD_CFLAGS += -DCONFIG_DRIVER_ATHEROS
HAPD_SRC += driver_atheros.c
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_DRIVER_PRISM54
HAPD_CFLAGS += -DCONFIG_DRIVER_PRISM54
HAPD_SRC += driver_prism54.c
endif

ifdef CONFIG_DRIVER_NL80211
HAPD_CFLAGS += -DCONFIG_DRIVER_NL80211
HAPD_SRC += driver_nl80211.c radiotap.c
LIBS += -lnl
ifdef CONFIG_LIBNL20
LIBS += -lnl-genl
HAPD_CFLAGS += -DCONFIG_LIBNL20
endif
endif

ifdef CONFIG_DRIVER_BSD
HAPD_CFLAGS += -DCONFIG_DRIVER_BSD
HAPD_SRC += driver_bsd.c
CONFIG_L2_PACKET=linux
CONFIG_DNET_PCAP=y
CONFIG_L2_FREEBSD=y
endif

ifdef CONFIG_DRIVER_QCHOSTAPD
HAPD_CFLAGS += -DCONFIG_DRIVER_QCHOSTAPD
HAPD_SRC += driver_qcsoftap.c
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_DRIVER_TEST
HAPD_CFLAGS += -DCONFIG_DRIVER_TEST
HAPD_SRC += driver_test.c
endif

ifdef CONFIG_DRIVER_NONE
HAPD_CFLAGS += -DCONFIG_DRIVER_NONE
HAPD_SRC += driver_none.c
endif

ifdef CONFIG_L2_PACKET
ifdef CONFIG_DNET_PCAP
ifdef CONFIG_L2_FREEBSD
LIBS += -lpcap
HAPD_SRC += ../src/l2_packet/l2_packet_freebsd.c
else
LIBS += -ldnet -lpcap
HAPD_SRC += ../src/l2_packet/l2_packet_pcap.c
endif
else
HAPD_SRC += ../src/l2_packet/l2_packet_linux.c
endif
else
HAPD_SRC += ../src/l2_packet/l2_packet_none.c
endif


ifdef CONFIG_EAP_MD5
HAPD_CFLAGS += -DEAP_MD5
HAPD_SRC += ../src/eap_server/eap_md5.c
CHAP=y
endif

ifdef CONFIG_EAP_TLS
HAPD_CFLAGS += -DEAP_TLS
HAPD_SRC += ../src/eap_server/eap_tls.c
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_PEAP
HAPD_CFLAGS += -DEAP_PEAP
HAPD_SRC += ../src/eap_server/eap_peap.c
HAPD_SRC += ../src/eap_common/eap_peap_common.c
TLS_FUNCS=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
HAPD_CFLAGS += -DEAP_TTLS
HAPD_SRC += ../src/eap_server/eap_ttls.c
TLS_FUNCS=y
CHAP=y
endif

ifdef CONFIG_EAP_MSCHAPV2
HAPD_CFLAGS += -DEAP_MSCHAPv2
HAPD_SRC += ../src/eap_server/eap_mschapv2.c
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
HAPD_CFLAGS += -DEAP_GTC
HAPD_SRC += ../src/eap_server/eap_gtc.c
endif

ifdef CONFIG_EAP_SIM
HAPD_CFLAGS += -DEAP_SIM
HAPD_SRC += ../src/eap_server/eap_sim.c
CONFIG_EAP_SIM_COMMON=y
endif

ifdef CONFIG_EAP_AKA
HAPD_CFLAGS += -DEAP_AKA
HAPD_SRC += ../src/eap_server/eap_aka.c
CONFIG_EAP_SIM_COMMON=y
NEED_SHA256=y
endif

ifdef CONFIG_EAP_AKA_PRIME
HAPD_CFLAGS += -DEAP_AKA_PRIME
endif

ifdef CONFIG_EAP_SIM_COMMON
HAPD_SRC += ../src/eap_common/eap_sim_common.c
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementating the interface specified in
# eap_sim_db.h.
HAPD_SRC += ../src/eap_server/eap_sim_db.c
NEED_FIPS186_2_PRF=y
endif

ifdef CONFIG_EAP_PAX
HAPD_CFLAGS += -DEAP_PAX
HAPD_SRC += ../src/eap_server/eap_pax.c ../src/eap_common/eap_pax_common.c
endif

ifdef CONFIG_EAP_PSK
HAPD_CFLAGS += -DEAP_PSK
HAPD_SRC += ../src/eap_server/eap_psk.c ../src/eap_common/eap_psk_common.c
endif

ifdef CONFIG_EAP_SAKE
HAPD_CFLAGS += -DEAP_SAKE
HAPD_SRC += ../src/eap_server/eap_sake.c ../src/eap_common/eap_sake_common.c
endif

ifdef CONFIG_EAP_GPSK
HAPD_CFLAGS += -DEAP_GPSK
HAPD_SRC += ../src/eap_server/eap_gpsk.c ../src/eap_common/eap_gpsk_common.c
ifdef CONFIG_EAP_GPSK_SHA256
HAPD_CFLAGS += -DEAP_GPSK_SHA256
endif
NEED_SHA256=y
endif

ifdef CONFIG_EAP_VENDOR_TEST
HAPD_CFLAGS += -DEAP_VENDOR_TEST
HAPD_SRC += ../src/eap_server/eap_vendor_test.c
endif

ifdef CONFIG_EAP_FAST
HAPD_CFLAGS += -DEAP_FAST
HAPD_SRC += ../src/eap_server/eap_fast.c
HAPD_SRC += ../src/eap_common/eap_fast_common.c
TLS_FUNCS=y
NEED_T_PRF=y
endif

ifdef CONFIG_WPS
HAPD_CFLAGS += -DCONFIG_WPS -DEAP_WSC
HAPD_SRC += ../src/utils/uuid.c
HAPD_SRC += wps_hostapd.c
HAPD_SRC += ../src/eap_server/eap_wsc.c ../src/eap_common/eap_wsc_common.c
HAPD_SRC += ../src/wps/wps.c
HAPD_SRC += ../src/wps/wps_common.c
HAPD_SRC += ../src/wps/wps_attr_parse.c
HAPD_SRC += ../src/wps/wps_attr_build.c
HAPD_SRC += ../src/wps/wps_attr_process.c
HAPD_SRC += ../src/wps/wps_dev_attr.c
HAPD_SRC += ../src/wps/wps_enrollee.c
HAPD_SRC += ../src/wps/wps_registrar.c
NEED_DH_GROUPS=y
NEED_SHA256=y
NEED_CRYPTO=y
NEED_BASE64=y

ifdef CONFIG_WPS_UPNP
HAPD_CFLAGS += -DCONFIG_WPS_UPNP
HAPD_SRC += ../src/wps/wps_upnp.c
HAPD_SRC += ../src/wps/wps_upnp_ssdp.c
HAPD_SRC += ../src/wps/wps_upnp_web.c
HAPD_SRC += ../src/wps/wps_upnp_event.c
HAPD_SRC += ../src/wps/httpread.c
endif

endif

ifdef CONFIG_EAP_IKEV2
HAPD_CFLAGS += -DEAP_IKEV2
HAPD_SRC += ../src/eap_server/eap_ikev2.c ../src/eap_server/ikev2.c
HAPD_SRC += ../src/eap_common/eap_ikev2_common.c ../src/eap_common/ikev2_common.c
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
endif

ifdef CONFIG_EAP_TNC
HAPD_CFLAGS += -DEAP_TNC
HAPD_SRC += ../src/eap_server/eap_tnc.c
HAPD_SRC += ../src/eap_server/tncs.c
NEED_BASE64=y
ifndef CONFIG_DRIVER_BSD
LIBS += -ldl
endif
endif

# Basic EAP functionality is needed for EAPOL
HAPD_SRC += ../src/eap_server/eap.c
HAPD_SRC += ../src/eap_common/eap_common.c
HAPD_SRC += ../src/eap_server/eap_methods.c
HAPD_SRC += ../src/eap_server/eap_identity.c

ifdef CONFIG_EAP
HAPD_CFLAGS += -DEAP_SERVER
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifeq ($(CONFIG_TLS), internal)
ifndef CONFIG_CRYPTO
CONFIG_CRYPTO=internal
endif
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
HAPD_CFLAGS += -DCONFIG_INTERNAL_X509
endif
ifeq ($(CONFIG_CRYPTO), internal)
HAPD_CFLAGS += -DCONFIG_INTERNAL_X509
endif


ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
HAPD_CFLAGS += -DEAP_TLS_FUNCS
HAPD_SRC += ../src/eap_server/eap_tls_common.c
NEED_TLS_PRF=y
ifeq ($(CONFIG_TLS), openssl)
HAPD_SRC += ../src/crypto/tls_openssl.c
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
LIBS_h += -lcrypto
endif
ifeq ($(CONFIG_TLS), gnutls)
HAPD_SRC += ../src/crypto/tls_gnutls.c
LIBS += -lgnutls -lgcrypt -lgpg-error
LIBS_p += -lgcrypt
LIBS_h += -lgcrypt
endif
ifdef CONFIG_GNUTLS_EXTRA
HAPD_CFLAGS += -DCONFIG_GNUTLS_EXTRA
LIBS += -lgnutls-extra
endif
ifeq ($(CONFIG_TLS), internal)
HAPD_SRC += ../src/crypto/tls_internal.c
HAPD_SRC += ../src/tls/tlsv1_common.c ../src/tls/tlsv1_record.c
HAPD_SRC += ../src/tls/tlsv1_cred.c ../src/tls/tlsv1_server.c
HAPD_SRC += ../src/tls/tlsv1_server_write.c ../src/tls/tlsv1_server_read.c
HAPD_SRC += ../src/tls/asn1.c ../src/tls/x509v3.c
LOCAL_SRC_FILES_p += ../src/tls/asn1.c
LOCAL_SRC_FILES_p += ../src/crypto/rc4.c ../src/crypto/aes_wrap.c ../src/crypto/aes.c
NEED_BASE64=y
HAPD_CFLAGS += -DCONFIG_TLS_INTERNAL
HAPD_CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
ifeq ($(CONFIG_CRYPTO), internal)
ifdef CONFIG_INTERNAL_LIBTOMMATH
HAPD_CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
else
LIBS += -ltommath
LIBS_p += -ltommath
endif
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
LIBS += -ltomcrypt -ltfm
LIBS_p += -ltomcrypt -ltfm
endif
endif
NEED_CRYPTO=y
else
HAPD_SRC += ../src/crypto/tls_none.c
endif

ifdef CONFIG_PKCS12
HAPD_CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
HAPD_SRC += ../src/crypto/ms_funcs.c
NEED_CRYPTO=y
endif

ifdef CHAP
HAPD_SRC += ../src/eap_common/chap.c
endif

ifdef NEED_CRYPTO
ifndef TLS_FUNCS
ifeq ($(CONFIG_TLS), openssl)
LIBS += -lcrypto
LIBS_p += -lcrypto
LIBS_h += -lcrypto
endif
ifeq ($(CONFIG_TLS), gnutls)
LIBS += -lgcrypt
LIBS_p += -lgcrypt
LIBS_h += -lgcrypt
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
LIBS += -ltomcrypt -ltfm
LIBS_p += -ltomcrypt -ltfm
endif
endif
endif
ifeq ($(CONFIG_TLS), openssl)
HAPD_SRC += ../src/crypto/crypto_openssl.c
LOCAL_SRC_FILES_p += ../src/crypto/crypto_openssl.c
HLOCAL_SRC_FILES += ../src/crypto/crypto_openssl.c
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_TLS), gnutls)
HAPD_SRC += ../src/crypto/crypto_gnutls.c
LOCAL_SRC_FILES_p += ../src/crypto/crypto_gnutls.c
HLOCAL_SRC_FILES += ../src/crypto/crypto_gnutls.c
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
HAPD_SRC += ../src/crypto/crypto_libtomcrypt.c
LOCAL_SRC_FILES_p += ../src/crypto/crypto_libtomcrypt.c
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_CRYPTO), internal)
HAPD_SRC += ../src/crypto/crypto_internal.c ../src/tls/rsa.c ../src/tls/bignum.c
LOCAL_SRC_FILES_p += ../src/crypto/crypto_internal.c ../src/tls/rsa.c ../src/tls/bignum.c
HAPD_CFLAGS += -DCONFIG_CRYPTO_INTERNAL
ifdef CONFIG_INTERNAL_LIBTOMMATH
HAPD_CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
ifdef CONFIG_INTERNAL_LIBTOMMATH_FAST
HAPD_CFLAGS += -DLTM_FAST
endif
else
LIBS += -ltommath
LIBS_p += -ltommath
endif
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_DES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD4=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_SHA256=y
endif
endif
else
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_SHA256=y
endif

ifdef CONFIG_INTERNAL_AES
HAPD_CFLAGS += -DINTERNAL_AES
endif
ifdef CONFIG_INTERNAL_SHA1
HAPD_CFLAGS += -DINTERNAL_SHA1
endif
ifdef CONFIG_INTERNAL_SHA256
HAPD_CFLAGS += -DINTERNAL_SHA256
endif
ifdef CONFIG_INTERNAL_MD5
HAPD_CFLAGS += -DINTERNAL_MD5
endif
ifdef CONFIG_INTERNAL_MD4
HAPD_CFLAGS += -DINTERNAL_MD4
endif
ifdef CONFIG_INTERNAL_DES
HAPD_CFLAGS += -DINTERNAL_DES
endif

ifdef NEED_SHA256
HAPD_SRC += ../src/crypto/sha256.c
endif

ifdef NEED_DH_GROUPS
HAPD_SRC += ../src/crypto/dh_groups.c
ifdef NEED_DH_GROUPS_ALL
HAPD_CFLAGS += -DALL_DH_GROUPS
endif
endif

ifndef NEED_FIPS186_2_PRF
HAPD_CFLAGS += -DCONFIG_NO_FIPS186_2_PRF
endif

ifndef NEED_T_PRF
HAPD_CFLAGS += -DCONFIG_NO_T_PRF
endif

ifndef NEED_TLS_PRF
HAPD_CFLAGS += -DCONFIG_NO_TLS_PRF
endif

ifdef CONFIG_RADIUS_SERVER
HAPD_CFLAGS += -DRADIUS_SERVER
HAPD_SRC += ../src/radius/radius_server.c
endif

ifdef CONFIG_IPV6
HAPD_CFLAGS += -DCONFIG_IPV6
endif

ifdef CONFIG_DRIVER_RADIUS_ACL
HAPD_CFLAGS += -DCONFIG_DRIVER_RADIUS_ACL
endif

ifdef CONFIG_FULL_DYNAMIC_VLAN
# define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and vlan interfaces for the vlan feature.
HAPD_CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
endif

ifdef NEED_BASE64
HAPD_SRC += ../src/utils/base64.c
endif

ifdef CONFIG_NO_STDOUT_DEBUG
HAPD_CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
endif

ifdef CONFIG_NO_AES_EXTRAS
HAPD_CFLAGS += -DCONFIG_NO_AES_UNWRAP
HAPD_CFLAGS += -DCONFIG_NO_AES_CTR -DCONFIG_NO_AES_OMAC1
HAPD_CFLAGS += -DCONFIG_NO_AES_EAX -DCONFIG_NO_AES_CBC
HAPD_CFLAGS += -DCONFIG_NO_AES_DECRYPT
HAPD_CFLAGS += -DCONFIG_NO_AES_ENCRYPT_BLOCK
endif

#################### BUILD HOSTAPD #########################
include $(CLEAR_VARS)
LOCAL_MODULE := hostapd
LOCAL_SHARED_LIBRARIES := libc libcutils libssl libcrypto libsysutils
LOCAL_CFLAGS := $(HAPD_CFLAGS)
LOCAL_SRC_FILES := $(HAPD_SRC)
LOCAL_C_INCLUDES := $(INCLUDES)
PRODUCT_COPY_FILES += $(LOCAL_PATH)/hostapd.conf:persist/qcom/softap/hostapd_default.conf
PRODUCT_COPY_FILES += $(LOCAL_PATH)/hostapd.conf:data/hostapd/hostapd.conf
PRODUCT_COPY_FILES += $(LOCAL_PATH)/hostapd.accept:data/hostapd/hostapd.accept
PRODUCT_COPY_FILES += $(LOCAL_PATH)/hostapd.deny:data/hostapd/hostapd.deny
include $(BUILD_EXECUTABLE)

##################### BUILD CLI ############################
include $(CLEAR_VARS)
LOCAL_MODULE := hostapd_cli
LOCAL_SHARED_LIBRARIES := libc libcutils libcrypto libssl
LOCAL_CFLAGS := $(HAPD_CFLAGS)
LOCAL_SRC_FILES := hostapd_cli.c ../src/common/wpa_ctrl.c ../src/utils/os_$(CONFIG_OS).c
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_EXECUTABLE)

#################### NT PASSWORD HASH #######################
include $(CLEAR_VARS)
LOCAL_MODULE := nt_password_hash
LOCAL_SHARED_LIBRARIES := libc libcutils libcrypto libssl
LOCAL_CFLAGS := $(HAPD_CFLAGS)
LOCAL_SRC_FILES := nt_password_hash.c ../src/crypto/ms_funcs.c ../src/crypto/sha1.c ../src/crypto/rc4.c ../src/crypto/md5.c ../src/crypto/crypto_openssl.c ../src/utils/os_$(CONFIG_OS).c
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_EXECUTABLE)

#################### NT PASSWORD HASH #######################
include $(CLEAR_VARS)
LOCAL_MODULE := hlr_auc_gw
LOCAL_SHARED_LIBRARIES := libc libcutils libcrypto libssl
LOCAL_CFLAGS := $(HAPD_CFLAGS)
LOCAL_SRC_FILES := $(HLOCAL_SRC_FILES)
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_EXECUTABLE)

ifdef CONFIG_TEST_MILINAGE
#################### TEST MILINAGE #######################
TEST_SRC_MILENAGE = ../src/hlr_auc_gw/milenage.c ../src/crypto/aes_wrap.c ../src/crypto/aes.c ../src/utils/common.c ../src/utils/wpa_debug.c ../src/utils/os_$(CONFIG_OS).c
include $(CLEAR_VARS)
LOCAL_MODULE := test-milenage
LOCAL_SHARED_LIBRARIES := libc libcutils libcrypto libssl
LOCAL_CFLAGS := -DTEST_MAIN_MILENAGE -I. -DINTERNAL_AES -I../src/crypto -I../src/utils  -Wall -Werror
LOCAL_SRC_FILES := $(TEST_SRC_MILENAGE)
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_EXECUTABLE)
endif

#%.eps: %.fig
#	fig2dev -L eps $*.fig $*.eps
#
#%.png: %.fig
#	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
#		> $*.png
#
#docs-pics: doc/hostapd.png doc/hostapd.eps
#
#docs: docs-pics
#	(cd ..; doxygen hostapd/doc/doxygen.full; cd hostapd)
#	$(MAKE) -C doc/latex
#	cp doc/latex/refman.pdf hostapd-devel.pdf
#
#docs-fast: docs-pics
#	(cd ..; doxygen hostapd/doc/doxygen.fast; cd hostapd)
#
#clean-docs:
#	rm -rf doc/latex doc/html
#	rm -f doc/hostapd.{eps,png} hostapd-devel.pdf
#
#-include $(LOCAL_SRC_FILES:%.o=%.d)
