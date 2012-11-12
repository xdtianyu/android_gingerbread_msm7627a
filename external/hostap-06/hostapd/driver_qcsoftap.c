/*

 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.


 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <linux/wireless.h>
#include "qc_sap_ioctl.h"
#include "common.h"
#include "eapol_sm.h"
#include "wpa.h"
#include "wpa_auth_i.h"
#include "wireless_copy.h"
#include "../src/wps/wps_defs.h"
#include "../src/wps/wps.h"

#ifdef WME_NUM_AC
/* Assume this is built against BSD branch of QcHostapd driver. */
#define MADWIFI_BSD
//#include <net80211/_ieee80211.h>
#endif /* WME_NUM_AC */
//#include <net80211/ieee80211_crypto.h>
//#include <net80211/ieee80211_ioctl.h>

#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
#include <netpacket/packet.h>

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW 0x0019
#endif
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */

/*
 * Avoid conflicts with hostapd definitions by undefining couple of defines
 * from QcHostapd header files.
 */
#undef RSN_VERSION
#undef WPA_VERSION
#undef WPA_OUI_TYPE
#undef WME_OUI_TYPE


#ifdef IEEE80211_IOCTL_SETWMMPARAMS
/* Assume this is built against QcHostapd-ng */
#define MADWIFI_NG
#endif /* IEEE80211_IOCTL_SETWMMPARAMS */

#ifdef ANDROID
#include <sys/ioctl.h>                                         
#include <stdlib.h>                                            
#include <fcntl.h>                                             
#include <errno.h>                                             
#include <string.h>                                            
#include <stdio.h>
/* Because this is defined in wireless_copy.h which has conflict with the android builtin wireless_copy.h */
#define IW_ENCODE_ALG_PMK 4
extern int delete_module(char * , int);
#else
#include "wireless_copy.h"
#endif

#include "hostapd.h"
#include "driver.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "priv_netlink.h"
#include "sta_info.h"
#include "l2_packet/l2_packet.h"

#include "wpa.h"
#include "radius/radius.h"
#include "ieee802_11.h"
#include "accounting.h"
#include "common.h"
#include "wps_hostapd.h"

/*HostApd configuration state*/
#define MAX_KEY_CONFIGURED 4

typedef enum
{
    eHapdState_Notconfigured,
    eHapdState_configured
}eHapdConnState;

struct key_information{
    size_t key_len;
    int  set_tx;
    wpa_alg alg;
    u8	addr[ETH_ALEN]; /* ff:ff:ff:ff:ff:ff for broadcast/multicast
			               * (group) keys or unicast address for
			               * individual keys */
    u8   *key;

};

struct QcHostapd_driver_data {
	struct hostapd_data *hapd;		/* back pointer */

    /*HostApd Configuration State*/
    eHapdConnState hapdConnState;

	char	iface[IFNAMSIZ + 1];
	int     ifindex;
	struct l2_packet_data *sock_xmit;	/* raw packet xmit socket */
	struct l2_packet_data *sock_recv;	/* raw packet recv socket */
	int	ioctl_sock;			/* socket for ioctl() use */
	int	wext_sock;			/* socket for wireless events */
	int	we_version;
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;

	struct l2_packet_data *sock_raw; /* raw 802.11 management frames */
	
	char privacy;

        u8 *assoc_req_ies;
        size_t assoc_req_ies_len;

    struct key_information key_info[MAX_KEY_CONFIGURED];  

};

static int QcHostapd_sta_deauth(void *priv, const u8 *addr, int reason_code);

static int QcHostapd_wps_pbc_probe_req_ind(struct hostapd_data *hapd);

static int
setQcSappriv(struct QcHostapd_driver_data *drv, int op, void *data, int len)
{
	struct iwreq iwr;
	int do_inline = len < IFNAMSIZ;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	if (do_inline) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->ioctl_sock, op, &iwr) < 0) {
		int first = QCSAP_IOCTL_SETPARAM;
		static const char *opnames[] = {
			"ioctl[QCSAP_IOCTL_SETPARAM]",
			"ioctl[QCSAP_IOCTL_GETPARAM]",
			"ioctl[QCSAP_IOCTL_COMMIT]",
			"ioctl[QCSAP_IOCTL_SETMLME]",
			"ioctl[QCSAP_IOCTL_GET_STAWPAIE]"
		};
		int idx = op - first;
		if (first <= op &&
		    idx < (int) (sizeof(opnames) / sizeof(opnames[0])) &&
		    opnames[idx])
			perror(opnames[idx]);
		else
			perror("ioctl[unknown???]");
		return -1;
	}
	return 0;
}
static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}

static int
QcHostapd_set_iface_flags(void *priv, int dev_up)
{
	struct QcHostapd_driver_data *drv = priv;
	struct ifreq ifr;

	wpa_printf(MSG_DEBUG, "%s: dev_up=%d", __func__, dev_up);

	if (drv->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->iface, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}

	if (dev_up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		return -1;
	}

	if (dev_up) {
		memset(&ifr, 0, sizeof(ifr));
		os_strlcpy(ifr.ifr_name, drv->iface, IFNAMSIZ);
		ifr.ifr_mtu = HOSTAPD_MTU;
		if (ioctl(drv->ioctl_sock, SIOCSIFMTU, &ifr) != 0) {
			perror("ioctl[SIOCSIFMTU]");
			printf("Setting MTU failed - trying to survive with "
			       "current value\n");
		}
	}

	return 0;
}

static int
QcHostapd_set_ieee8021x(const char *ifname, void *priv, int enabled)
{

        wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
        return 0;
}

static int
QcHostapd_set_privacy(const char *ifname, void *priv, int enabled)
{

        wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
        struct QcHostapd_driver_data *drv = priv;
        drv->privacy = enabled;
        return 0;
}

static int
QcHostapd_set_sta_authorized(void *priv, const u8 *addr, int authorized)
{
	struct QcHostapd_driver_data *drv = priv;
	struct sQcSapreq_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s authorized=%d",
		   __func__, ether_sprintf(addr), authorized);

	if (authorized)
		mlme.im_op = QCSAP_MLME_AUTHORIZE;
	else
		mlme.im_op = QCSAP_MLME_UNAUTHORIZE;
	mlme.im_reason = 0;
	memcpy(mlme.im_macaddr, addr, QCSAP_ADDR_LEN);
	ret = setQcSappriv(drv, QCSAP_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to %sauthorize STA " MACSTR,
			   __func__, authorized ? "" : "un", MAC2STR(addr));
	}

	return ret;
}

static int
QcHostapd_sta_set_flags(void *priv, const u8 *addr, int total_flags,
		      int flags_or, int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WLAN_STA_AUTHORIZED)
		return QcHostapd_set_sta_authorized(priv, addr, 1);
	if (!(flags_and & WLAN_STA_AUTHORIZED))
		return QcHostapd_set_sta_authorized(priv, addr, 0);
	return 0;
}

static int QcHostapd_set_key (const char *ifname, void *priv, 
	const char *alg_str, const u8 *addr, int key_idx, const u8 *key, 
	size_t key_len, int set_tx)
{
    struct QcHostapd_driver_data *drv = priv;
    struct iwreq iwr;
    int ret = 0;
    struct iw_encode_ext *ext;
    wpa_alg alg;

    wpa_printf(MSG_DEBUG, "%s: alg=%s, addr=%s, key_idx=%d, key_len=%d, set_tx=%d", 
                   __func__, alg_str, ether_sprintf(addr), key_idx, (int)key_len, set_tx);
 
    if (strcmp(alg_str, "none") == 0)
        alg = WPA_ALG_NONE;
    else if (strcmp(alg_str, "WEP") == 0){
        alg = WPA_ALG_WEP;
        if((drv->hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211N ) ||
           (drv->hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211N_ONLY))
            drv->hapd->iconf->hw_mode = HOSTAPD_MODE_IEEE80211G;
    }
    else if (strcmp(alg_str, "TKIP") == 0){
        alg = WPA_ALG_TKIP;
        if((drv->hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211N ) ||
           (drv->hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211N_ONLY))
            drv->hapd->iconf->hw_mode = HOSTAPD_MODE_IEEE80211G;
    }
    else if (strcmp(alg_str, "CCMP") == 0)
        alg = WPA_ALG_CCMP;
    else {
          printf("%s: unknown/unsupported algorithm %s\n",
                    __func__, alg_str);
                return -1;
    }


    if(eHapdState_configured != drv->hapdConnState){
        //TODO: copy the key information 
        drv->key_info[key_idx].alg = alg;
        
        if (addr)
            os_memcpy(drv->key_info[key_idx].addr, addr, ETH_ALEN);
        else
            os_memset(drv->key_info[key_idx].addr, 0xff, ETH_ALEN);
        
        drv->key_info[key_idx].key_len = key_len;
	if (key && key_len) {
            drv->key_info[key_idx].key = os_zalloc(key_len); 
            os_memcpy(drv->key_info[key_idx].key, key, key_len);
	}	
        drv->key_info[key_idx].set_tx = set_tx;
        return 0;
    }
	
    ext = os_zalloc(sizeof(*ext) + key_len);
    if (ext == NULL)
        return -1;
    os_memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
    iwr.u.encoding.flags = key_idx + 1;
    iwr.u.encoding.flags |= IW_ENCODE_TEMP;
    if (alg == WPA_ALG_NONE)
        iwr.u.encoding.flags |= IW_ENCODE_DISABLED;
    iwr.u.encoding.pointer = (caddr_t) ext;
    iwr.u.encoding.length = sizeof(*ext) + key_len;

    if (addr == NULL ||
        os_memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
        ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;
    if (set_tx)
        ext->ext_flags |= IW_ENCODE_EXT_SET_TX_KEY;

    ext->addr.sa_family = ARPHRD_ETHER;
    if (addr)
        os_memcpy(ext->addr.sa_data, addr, ETH_ALEN);
    else
        os_memset(ext->addr.sa_data, 0xff, ETH_ALEN);
    if (key && key_len) {
        os_memcpy(ext + 1, key, key_len);
//        ext->key_len = key_len;
    }
    
    ext->key_len = key_len;

    switch (alg) {
    case WPA_ALG_NONE:
        ext->alg = IW_ENCODE_ALG_NONE;
        break;
    case WPA_ALG_WEP:
        ext->alg = IW_ENCODE_ALG_WEP;
        break;
    case WPA_ALG_TKIP:
        ext->alg = IW_ENCODE_ALG_TKIP;
        break;
    case WPA_ALG_CCMP:
        ext->alg = IW_ENCODE_ALG_CCMP;
        break;
    case WPA_ALG_PMK:
        ext->alg = IW_ENCODE_ALG_PMK;
        break;
#ifdef CONFIG_IEEE80211W
    case WPA_ALG_IGTK:
        ext->alg = IW_ENCODE_ALG_AES_CMAC;
        break;
#endif /* CONFIG_IEEE80211W */
    default:
        wpa_printf(MSG_DEBUG, "%s: Unknown algorithm %d",
               __FUNCTION__, alg);
        os_free(ext);
        return -1;
    }

    if (ioctl(drv->ioctl_sock, SIOCSIWENCODEEXT, &iwr) < 0) {
        ret = errno == EOPNOTSUPP ? -2 : -1;
        if (errno == ENODEV) {
            /*
             * ndiswrapper seems to be returning incorrect error
             * code.. */
            ret = -2;
        }

        perror("ioctl[SIOCSIWENCODEEXT]");
    }

    os_free(ext);
   
    return ret;
}


static int
QcHostapd_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
		   u8 *seq)
{
	return 0;
}


static int 
QcHostapd_flush(void *priv)
{
#ifdef MADWIFI_BSD
	u8 allsta[IEEE80211_ADDR_LEN];
	memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return QcHostapd_sta_deauth(priv, allsta, IEEE80211_REASON_AUTH_LEAVE);
#else /* MADWIFI_BSD */
	return 0;		/* XXX */
#endif /* MADWIFI_BSD */
}


static int
QcHostapd_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
			     const u8 *addr)
{
	struct QcHostapd_driver_data *drv = priv;

#ifdef MADWIFI_BSD
	struct ieee80211req_sta_stats stats;

	memset(data, 0, sizeof(*data));

	/*
	 * Fetch statistics for station from the system.
	 */
	memset(&stats, 0, sizeof(stats));
	memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (setQcSappriv(drv,
#ifdef MADWIFI_NG
			 IEEE80211_IOCTL_STA_STATS,
#else /* MADWIFI_NG */
			 IEEE80211_IOCTL_GETSTASTATS,
#endif /* MADWIFI_NG */
			 &stats, sizeof(stats))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to fetch STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
			memcpy(data, &drv->acct_data, sizeof(*data));
			return 0;
		}

		printf("Failed to get station stats information element.\n");
		return -1;
	}

	data->rx_packets = stats.is_stats.ns_rx_data;
	data->rx_bytes = stats.is_stats.ns_rx_bytes;
	data->tx_packets = stats.is_stats.ns_tx_data;
	data->tx_bytes = stats.is_stats.ns_tx_bytes;
	return 0;

#else /* MADWIFI_BSD */

	char buf[1024], line[128], *pos;
	FILE *f;
	unsigned long val;

	memset(data, 0, sizeof(*data));
	snprintf(buf, sizeof(buf), "/proc/net/QcHostapd/%s/" MACSTR,
		 drv->iface, MAC2STR(addr));

	f = fopen(buf, "r");
	if (!f) {
		if (memcmp(addr, drv->acct_mac, ETH_ALEN) != 0)
			return -1;
		memcpy(data, &drv->acct_data, sizeof(*data));
		return 0;
	}
	/* Need to read proc file with in one piece, so use large enough
	 * buffer. */
	setbuffer(f, buf, sizeof(buf));

	while (fgets(line, sizeof(line), f)) {
		pos = strchr(line, '=');
		if (!pos)
			continue;
		*pos++ = '\0';
		val = strtoul(pos, NULL, 10);
		if (strcmp(line, "rx_packets") == 0)
			data->rx_packets = val;
		else if (strcmp(line, "tx_packets") == 0)
			data->tx_packets = val;
		else if (strcmp(line, "rx_bytes") == 0)
			data->rx_bytes = val;
		else if (strcmp(line, "tx_bytes") == 0)
			data->tx_bytes = val;
	}

	fclose(f);

	return 0;
#endif /* MADWIFI_BSD */
}


static int
QcHostapd_sta_clear_stats(void *priv, const u8 *addr)
{
#if defined(MADWIFI_BSD) && defined(IEEE80211_MLME_CLEAR_STATS)
	struct QcHostapd_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s", __func__, ether_sprintf(addr));

	mlme.im_op = IEEE80211_MLME_CLEAR_STATS;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = setQcSappriv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to clear STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
	}

	return ret;
#else /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
	return 0; /* FIX */
#endif /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
}

static int QcHostapd_set_generic_ie(void *priv, const u8 *ie,
                                      size_t ie_len)
{
        struct QcHostapd_driver_data *drv = priv;
        struct iwreq iwr;
        int ret = 0;

        wpa_printf(MSG_DEBUG, "%s", __func__);

        os_memset(&iwr, 0, sizeof(iwr));
        os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
        iwr.u.data.pointer = (caddr_t) ie;
        iwr.u.data.length = ie_len;

        if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
                perror("ioctl[SIOCSIWGENIE]");
                ret = -1;
        }

        return ret;
}
 
static int QcHostapd_set_wps_ie(void *priv, const u8 *ie,
                                      size_t ie_len)
{
        struct QcHostapd_driver_data *drv = priv;
        struct iwreq iwr;
        int ret = 0;

        os_memset(&iwr, 0, sizeof(iwr));
        os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
        iwr.u.data.pointer = (caddr_t) ie;
        iwr.u.data.length = ie_len;

        if (ioctl(drv->ioctl_sock, QCSAP_IOCTL_SETWPAIE, &iwr) < 0) {
            perror("ioctl[QCSAP_IOCTL_SETWPAIE]");
            ret = -1;
        }

        return ret;
}

static int
QcHostapd_set_opt_ie(const char *ifname, void *priv, const u8 *ie, size_t ie_len)
{
    struct QcHostapd_driver_data *drv = priv;
    
    if(eHapdState_configured == drv->hapdConnState)    
    {        
        return QcHostapd_set_generic_ie(priv, ie, ie_len);
    }
    return 0;
}

static int
QcHostapd_sta_deauth(void *priv, const u8 *addr, int reason_code)
{
	struct QcHostapd_driver_data *drv = priv;
	struct sQcSapreq_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = QCSAP_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, QCSAP_ADDR_LEN);
	ret = setQcSappriv(drv, QCSAP_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int
QcHostapd_sta_disassoc(void *priv, const u8 *addr, int reason_code)
{
	struct QcHostapd_driver_data *drv = priv;
	struct sQcSapreq_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = QCSAP_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, QCSAP_ADDR_LEN);
	ret = setQcSappriv(drv, QCSAP_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
static void QcHostapd_raw_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct QcHostapd_driver_data *drv = ctx;
	const struct ieee80211_mgmt *mgmt;
	const u8 *end, *ie;
	u16 fc;
	size_t ie_len;

	/* Send Probe Request information to WPS processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req))
		return;
	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_PROBE_REQ)
		return;

	end = buf + len;
	ie = mgmt->u.probe_req.variable;
	ie_len = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));

	hostapd_wps_probe_req_rx(drv->hapd, mgmt->sa, ie, ie_len);
}
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */

static int QcHostapd_receive_probe_req(struct QcHostapd_driver_data *drv)
{
	int ret = 0;
#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
	struct ieee80211req_set_filter filt;

	wpa_printf(MSG_DEBUG, "%s Enter", __func__);
	filt.app_filterype = IEEE80211_FILTER_TYPE_PROBE_REQ;

	ret = setQcSappriv(drv, IEEE80211_IOCTL_FILTERFRAME, &filt,
			   sizeof(struct ieee80211req_set_filter));
	if (ret)
		return ret;

	drv->sock_raw = l2_packet_init(drv->iface, NULL, ETH_P_80211_RAW,
				       QcHostapd_raw_receive, drv, 1);
	if (drv->sock_raw == NULL)
		return -1;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */
	return ret;
}

static int
QcHostapd_del_sta(struct QcHostapd_driver_data *drv, u8 addr[QCSAP_ADDR_LEN])
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
	if (sta != NULL) {
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
		wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
		sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
		ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
		ap_free_sta(hapd, sta);
	}
	return 0;
}

#ifdef CONFIG_WPS

static int
QcHostapd_set_wps_beacon_ie(const char *ifname, void *priv, const u8 *ie,
			  size_t len)
{
        u8 *wps_ie_buf;
        int ret;

        wps_ie_buf = malloc( len + 1);
        wps_ie_buf[0] = eQC_WPS_BEACON_IE;  
     
         
        memcpy(&wps_ie_buf[1], ie, len);
        ret = QcHostapd_set_wps_ie(priv, wps_ie_buf, len + 1);
        
        free(wps_ie_buf);
        return ret;
}

static int
QcHostapd_set_wps_probe_resp_ie(const char *ifname, void *priv, const u8 *ie,
			      size_t len)
{
        struct QcHostapd_driver_data *drv = priv;
        struct hostapd_data *hapd = drv->hapd;
        u8 *wps_ie_buf;
        u8 *wps_genie;
        u8 *pos;
        u16 wpsElemlen;
        int ret;
        u16 config_methods = 0;

        wps_ie_buf = malloc( len + 1);
        wps_ie_buf[0] = eQC_WPS_PROBE_RSP_IE;

        memcpy(&wps_ie_buf[1], ie, len);
 
 // Walkaround WPS config methods is not assigned issue               
        wps_genie = &wps_ie_buf[1];
        switch ( wps_genie[0] ) 
        {
         case WLAN_EID_VENDOR_SPECIFIC: 
            if (wps_genie[1] < 2 + 4)
               return -EINVAL;
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0) 
            {
                pos = &wps_genie[6];
                
                while (((size_t)pos - (size_t)&wps_genie[6])  < (size_t)(wps_genie[1] - 4) )
                {
                    switch((u16)(*pos<<8) | *(pos+1))
                    {
                        case ATTR_CONFIG_METHODS:
                            pos +=4;
                            if (hapd->conf->config_methods) {
                                char *m = hapd->conf->config_methods;
                                if (os_strstr(m, "label"))
                                    config_methods |= WPS_CONFIG_LABEL;
                                if (os_strstr(m, "display"))
                                    config_methods |= WPS_CONFIG_DISPLAY;
                                if (os_strstr(m, "push_button"))
                                    config_methods |= WPS_CONFIG_PUSHBUTTON;
                                if (os_strstr(m, "keypad"))
                                    config_methods |= WPS_CONFIG_KEYPAD;
                            }

                            if(config_methods)
                            {
                                *pos = (u8) (config_methods >> 8); 
                                *(pos+1) = (u8) (config_methods & 0xff);
                            }
                            pos += 2; 
                        break;
                      
                        default:
                            pos += 2; //Skip other WPS type field
                            wpsElemlen =  ((*pos) << 8) + *(pos+1);
                            pos += wpsElemlen + 2;                            
                        break;                      
                    }  // switch
                }
            } 
            else
            {
                wpa_printf (MSG_DEBUG, "WPS IE Mismatch");
            }        
        }        
        
        ret = QcHostapd_set_wps_ie(priv, wps_ie_buf, len + 1);

        free(wps_ie_buf);
        return ret;

}
#else /* CONFIG_WPS */
#define QcHostapd_set_wps_beacon_ie NULL
#define QcHostapd_set_wps_probe_resp_ie NULL
#endif /* CONFIG_WPS */

static int
QcHostapd_process_wpa_ie(struct QcHostapd_driver_data *drv, 
                         struct sta_info *sta) 
{
	struct hostapd_data *hapd = drv->hapd;
	struct sQcSapreq_wpaie ie;
	int ielen, res;
	u8 *iebuf;

	/*
	 * Fetch negotiated WPA/RSN parameters from the system.
	 */
	memset(&ie, 0, sizeof(ie));
	os_memcpy(ie.wpa_macaddr, sta->addr, QCSAP_ADDR_LEN);
	if (setQcSappriv(drv, QCSAP_IOCTL_GET_STAWPAIE, &ie, sizeof(ie))) 
        {
		wpa_printf(MSG_ERROR, "%s: Failed to get WPA/RSN IE",
			   __func__);
		printf("Failed to get WPA/RSN information element.\n");
		return -1;		/* XXX not right */
	}
	wpa_hexdump(MSG_MSGDUMP, "QcHostapd req WPA IE IELEN ",
		    ie.wpa_ie, QCSAP_MAX_OPT_IE);
	iebuf = ie.wpa_ie;
	/* QcHostapd seems to return some random data if WPA/RSN IE is not set.
	 * Assume the IE was not included if the IE type is unknown. */
	if ((iebuf[0] != WLAN_EID_VENDOR_SPECIFIC) && (iebuf[0] != WLAN_EID_RSN))
		iebuf[1] = 0;
	ielen = iebuf[1];
	if (ielen == 0) {
#ifdef CONFIG_WPS
		if (hapd->conf->wps_state) {
			wpa_printf(MSG_DEBUG, "STA did not include WPA/RSN IE "
				   "in (Re)Association Request - possible WPS "
				   "use");
			sta->flags |= WLAN_STA_MAYBE_WPS;
			return 0;
		}
#endif /* CONFIG_WPS */
		printf("No WPA/RSN information element for station!?\n");
		return -1;		/* XXX not right */
	}
	ielen += 2;
	if (sta->wpa_sm == NULL)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr);
	if (sta->wpa_sm == NULL) {
		printf("Failed to initialize WPA state machine\n");
		return -1;
	}
	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  iebuf, ielen, NULL, 0);
	if (res != WPA_IE_OK) {
		printf("WPA/RSN information element rejected? (res %u)\n", res);
		return -1;
	}
	return 0;
}

static int
QcHostapd_new_sta(struct QcHostapd_driver_data *drv,
	          u8 addr[QCSAP_ADDR_LEN])
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;
	int new_assoc;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "associated");

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		accounting_sta_stop(hapd, sta);
	} else {
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL)
			return -1;
	}

	if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(drv->acct_mac, 0, ETH_ALEN);
		memset(&drv->acct_data, 0, sizeof(drv->acct_data));
	}

	if (hapd->conf->wpa
#ifdef CONFIG_WPS
            || hapd->conf->wps_state
#endif
    ) {
        if (QcHostapd_process_wpa_ie(drv, sta))
            return -1;
	}

	/*
	 * Now that the internal station state is setup
	 * kick the authenticator into action.
	 */
	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
	return 0;
}

static void
QcHostapd_wireless_event_wireless_custom(struct QcHostapd_driver_data *drv,
				       char *custom)
{
    wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'", custom);

    if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
        char *pos;
        u8 addr[ETH_ALEN];
        pos = strstr(custom, "addr=");
        if (pos == NULL) {
            wpa_printf(MSG_DEBUG,
                       "MLME-MICHAELMICFAILURE.indication "
                        "without sender address ignored");
            return;
        }
        pos += 5;
        if (hwaddr_aton(pos, addr) == 0) {
            ieee80211_michael_mic_failure(drv->hapd, addr, 1);
        } else {
            wpa_printf(MSG_DEBUG,
                      "MLME-MICHAELMICFAILURE.indication "
                      "with invalid MAC address");
        }
    } else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
        char *key, *value;
        u32 val;
        key = custom;
        while ((key = strchr(key, '\n')) != NULL) {
            key++;
            value = strchr(key, '=');
            if (value == NULL)
                continue;
            *value++ = '\0';
            val = strtoul(value, NULL, 10);
            if (strcmp(key, "mac") == 0)
                hwaddr_aton(value, drv->acct_mac);
            else if (strcmp(key, "rx_packets") == 0)
                drv->acct_data.rx_packets = val;
            else if (strcmp(key, "tx_packets") == 0)
                drv->acct_data.tx_packets = val;
            else if (strcmp(key, "rx_bytes") == 0)
                drv->acct_data.rx_bytes = val;
            else if (strcmp(key, "tx_bytes") == 0)
                drv->acct_data.tx_bytes = val;
            key = value;
        }
	
    }
    else if (strncmp(custom, "STOP-BSS.response", 17) == 0) {
        /* Event received to stop Soft AP bss */
        wpa_printf(MSG_DEBUG, "Terminate hostapd\n");
        drv->hapdConnState = eHapdState_Notconfigured;
        eloop_terminate();
    }
    else if (strncmp(custom, "MLMEWPSPBCPROBEREQ.indication", 29) == 0)
    {
        QcHostapd_wps_pbc_probe_req_ind(drv->hapd);
    }
    else if (strncmp(custom, "AUTO-SHUT.indication ", strlen("AUTO-SHUT.indication ")) == 0)
    {
        int ret;
        char str[64], *drv_name;
        memset(&str, 0, sizeof(str));

#ifndef ANDROID
#define RMMOD_PATH "/sbin/rmmod "
        //for linux PC
        strncpy(str ,RMMOD_PATH, sizeof(str));
        drv_name = custom + strlen("AUTO-SHUT.indication ");

        strncat(str, drv_name, sizeof(str)-strlen(RMMOD_PATH));
        str[sizeof(str)-1] = '\0';
        ret = system(str);
#else
          drv_name = custom + strlen("AUTO-SHUT.indication ");

          if (0 == (ret = delete_module(drv_name, (int)(O_NONBLOCK | O_EXCL))))
              ret = delete_module("librasdioif", (int)O_NONBLOCK | O_EXCL);

#endif
        if (ret != 0)
          wpa_printf(MSG_DEBUG, "Error in unloading %s\n", drv_name);
        else
          wpa_printf(MSG_DEBUG, "%s unloaded successfully\n", drv_name);

        drv->hapdConnState = eHapdState_Notconfigured;
        eloop_terminate();
    }
}

static void
QcHostapd_wireless_event_wireless(struct QcHostapd_driver_data *drv,
					    char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

    //wpa_hexdump(MSG_MSGDUMP, "RX MIC FAIL",(unsigned char *) data, len);
	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
        wpa_printf(MSG_MSGDUMP, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;
		custom = pos + IW_EV_POINT_LEN;
		if (/*drv->we_version > 18 &&*/
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM )){
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			QcHostapd_del_sta(drv, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			QcHostapd_new_sta(drv, (u8 *)iwe->u.addr.sa_data);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			QcHostapd_wireless_event_wireless_custom(drv, buf);
			free(buf);
			break;
		case IWEVMICHAELMICFAILURE: {
                u8 addr[ETH_ALEN];
                if (custom + iwe->u.data.length > end)
                    return;
                memcpy(addr, &custom[6],6);
                ieee80211_michael_mic_failure(drv->hapd, addr, 1);
            }
            break;
		}

		pos += iwe->len;
	}
}


static void
QcHostapd_wireless_event_rtm_newlink(struct QcHostapd_driver_data *drv,
					       struct nlmsghdr *h, int len)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr * attr;

	if (len < (int) sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	if (ifi->ifi_index != drv->ifindex)
		return;

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			QcHostapd_wireless_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void
QcHostapd_wireless_event_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	char buf[256];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	struct QcHostapd_driver_data *drv = eloop_ctx;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			printf("Malformed netlink message: "
			       "len=%d left=%d plen=%d\n",
			       len, left, plen);
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			QcHostapd_wireless_event_rtm_newlink(drv, h, plen);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		printf("%d extra bytes in the end of netlink message\n", left);
	}
}


static int
QcHostapd_get_we_version(struct QcHostapd_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int
QcHostapd_wireless_event_init(void *priv)
{
	struct QcHostapd_driver_data *drv = priv;
	int s;
	struct sockaddr_nl local;

	QcHostapd_get_we_version(drv);

	drv->wext_sock = -1;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		return -1;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(s);
		return -1;
	}

	eloop_register_read_sock(s, QcHostapd_wireless_event_receive, drv, NULL);
	drv->wext_sock = s;

	return 0;
}


static void
QcHostapd_wireless_event_deinit(void *priv)
{
	struct QcHostapd_driver_data *drv = priv;

	if (drv != NULL) {
		if (drv->wext_sock < 0)
			return;
		eloop_unregister_read_sock(drv->wext_sock);
		close(drv->wext_sock);
	}
}


static int
QcHostapd_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
		   int encrypt, const u8 *own_addr)
{
	struct QcHostapd_driver_data *drv = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = htons(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", bp, len);

	status = l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, bp, len);

	if (bp != buf)
		free(bp);
	return status;
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct QcHostapd_driver_data *drv = ctx;
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, src_addr);
	if (!sta || !(sta->flags & WLAN_STA_ASSOC)) {
		printf("Data frame from not associated STA %s\n",
		       ether_sprintf(src_addr));
		/* XXX cannot happen */
		return;
	}
	ieee802_1x_receive(hapd, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}

static void *
QcHostapd_init(struct hostapd_data *hapd)
{
	struct QcHostapd_driver_data *drv;
	struct ifreq ifr;
	struct iwreq iwr;

	drv = os_zalloc(sizeof(struct QcHostapd_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for QcHostapd driver data\n");
		return NULL;
	}

	drv->hapd = hapd;
        //initialize the hostapd state to not configured state
        drv->hapdConnState = eHapdState_Notconfigured;

	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		goto bad;
	}
	memcpy(drv->iface, hapd->conf->iface, sizeof(drv->iface));

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->iface, sizeof(ifr.ifr_name));
	if (ioctl(drv->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		goto bad;
	}
	drv->ifindex = ifr.ifr_ifindex;

	drv->sock_xmit = l2_packet_init(drv->iface, NULL, ETH_P_EAPOL,
					handle_read, drv, 1);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, hapd->own_addr))
		goto bad;
	if (hapd->conf->bridge[0] != '\0') {
		wpa_printf(MSG_DEBUG, "Configure bridge %s for EAPOL traffic.",
			   hapd->conf->bridge);
		drv->sock_recv = l2_packet_init(hapd->conf->bridge, NULL,
						ETH_P_EAPOL, handle_read, drv,
						1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else
		drv->sock_recv = drv->sock_xmit;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.mode = IW_MODE_MASTER;
#if 0
	if (ioctl(drv->ioctl_sock, SIOCSIWMODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWMODE]");
		printf("Could not set interface to master mode!\n");
		goto bad;
	}
#endif
	QcHostapd_set_iface_flags(drv, 0);	/* mark down during setup */
	QcHostapd_set_privacy(drv->iface, drv, 0); /* default to no privacy */

	QcHostapd_receive_probe_req(drv);

	return drv;
bad:
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv != NULL)
		free(drv);
	return NULL;
}


static void
QcHostapd_deinit(void *priv)
{
	struct QcHostapd_driver_data *drv = priv;

	(void) QcHostapd_set_iface_flags(drv, 0);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->sock_raw)
		l2_packet_deinit(drv->sock_raw);
	free(drv);
}

static int
QcHostapd_set_ssid(const char *ifname, void *priv, const u8 *buf, int len)
{
	struct QcHostapd_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
	return 0;
}

static int
QcHostapd_get_ssid(const char *ifname, void *priv, u8 *buf, int len)
{
	struct QcHostapd_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
QcHostapd_set_countermeasures(void *priv, int enabled)
{
	struct QcHostapd_driver_data *drv = priv;
	struct sQcSapreq_mlme mlme;
	int ret;

	mlme.im_op = QCSAP_MLME_MICFAILURE;
	mlme.im_reason = enabled;

	ret = setQcSappriv(drv, QCSAP_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: TKIP failed STA ",
			   __func__);
	}

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);

	return ret;
}

static int
QcHostapd_set_key_info(void *data){
    struct QcHostapd_driver_data *drv = (struct QcHostapd_driver_data *)data;
    int key_idx = 0;
    int wep_init = 0; 
    char *alg_name;
    for (key_idx = 0; key_idx < NUM_WEP_KEYS; key_idx++) {
        switch (drv->key_info[key_idx].alg) {
            case WPA_ALG_NONE:
                alg_name = "none";
                break;
            case WPA_ALG_WEP:
                alg_name = "WEP";
                if( !wep_init ){
                    for (wep_init = 0; wep_init < NUM_WEP_KEYS; wep_init++) {
	                    if( drv->hapd->driver->set_encryption(drv->hapd->conf->iface, 
                                                          drv, "none", NULL, wep_init,
                                                          NULL, 0, wep_init == 0 ? 1 : 0 )){ 
                            wpa_printf(MSG_DEBUG, "Failed to clear default "
                                       "encryption keys (ifname=%s keyidx=%d)",
                                       drv->hapd->conf->iface, wep_init);
                        }
                    }       
                } 
                break;
                
            case WPA_ALG_TKIP:
                alg_name = "TKIP";
                break;
            case WPA_ALG_CCMP:
                alg_name = "CCMP";
                break;
            default:
                return -1;
        }
              
	if( 0 == drv->hapd->driver->set_encryption(drv->hapd->conf->iface, 
                                      drv, alg_name,
                                      (const u8 *)&drv->key_info[key_idx].addr,
                                      key_idx,
                                      drv->key_info[key_idx].key,
                                      drv->key_info[key_idx].key_len,
                                      drv->key_info[key_idx].set_tx))
            //if success free the key
            os_free(drv->key_info[key_idx].key);
		    
        if (drv->hapd->conf->ssid.wep.key[key_idx] &&
		    key_idx == drv->hapd->conf->ssid.wep.idx)
	        drv->hapd->driver->set_privacy(drv->hapd->conf->iface, 
                                           drv->hapd->drv_priv,
		                           1);
    }
    return 0;
 
}

static int
QcHostapd_commit(void *priv)
{
    struct QcHostapd_driver_data *drv = priv;
    struct iwreq iwr;
    s_CommitConfig_t *pQcCommitConfig;

    wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

    pQcCommitConfig = os_zalloc(sizeof(struct s_CommitConfig));

    if(pQcCommitConfig == NULL) {
        wpa_printf(MSG_DEBUG, "%s: Could not allocate memory ", __func__);
        return -1;
    }

    pQcCommitConfig->beacon_int = drv->hapd->iconf->beacon_int;
    pQcCommitConfig->channel = drv->hapd->iconf->channel;

    switch (drv->hapd->iconf->hw_mode ){
        case HOSTAPD_MODE_IEEE80211A:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11A;
            break;
        case HOSTAPD_MODE_IEEE80211B:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11B;
            break;
        case HOSTAPD_MODE_IEEE80211G:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11G;
            break;
        case HOSTAPD_MODE_IEEE80211N:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11N;
            break;
        case HOSTAPD_MODE_IEEE80211G_ONLY:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11G_ONLY;
            break;
        case HOSTAPD_MODE_IEEE80211N_ONLY:
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11N_ONLY;
            break;
        default :
        pQcCommitConfig->hw_mode = eQC_DOT11_MODE_11N;
            break;
    }

    pQcCommitConfig->qc_macaddr_acl = drv->hapd->conf->macaddr_acl;
    pQcCommitConfig->accept_mac = (struct qc_mac_acl_entry *)drv->hapd->conf->accept_mac;
    pQcCommitConfig->num_accept_mac = drv->hapd->conf->num_accept_mac;
    pQcCommitConfig->deny_mac = (struct qc_mac_acl_entry *)drv->hapd->conf->deny_mac;
    pQcCommitConfig->num_deny_mac = drv->hapd->conf->num_deny_mac;
    pQcCommitConfig->dtim_period = drv->hapd->conf->dtim_period;
    pQcCommitConfig->wps_state = drv->hapd->conf->wps_state;

    pQcCommitConfig->qcsap80211d = drv->hapd->iconf->ieee80211d;

    os_memcpy(pQcCommitConfig->countryCode, drv->hapd->iconf->country, 3);

    if((drv->hapd->conf->auth_algs & WPA_AUTH_ALG_OPEN) && (drv->hapd->conf->auth_algs & WPA_AUTH_ALG_SHARED))
    {
        pQcCommitConfig->authType = eQC_AUTH_TYPE_AUTO_SWITCH;
    }	
    else if(drv->hapd->conf->auth_algs & WPA_AUTH_ALG_OPEN) 
    {
        pQcCommitConfig->authType = eQC_AUTH_TYPE_OPEN_SYSTEM;
    }
    else if(drv->hapd->conf->auth_algs & WPA_AUTH_ALG_SHARED) 
    {
	pQcCommitConfig->authType = eQC_AUTH_TYPE_SHARED_KEY;
    }

    int i;

    pQcCommitConfig->privacy = drv->privacy;
	 
    for (i = 0; i < 4; i++) {
       if (drv->hapd->conf->ssid.wep.key[i] &&
           i == drv->hapd->conf->ssid.wep.idx)
          pQcCommitConfig->privacy = 1;
    }
	  
    if( (drv->hapd->wpa_auth != NULL) && (drv->hapd->wpa_auth->wpa_ie_len)){
        pQcCommitConfig->RSNWPAReqIELength = drv->hapd->wpa_auth->wpa_ie_len;
        os_memcpy ( pQcCommitConfig->RSNWPAReqIE, 
                    drv->hapd->wpa_auth->wpa_ie, 
                    drv->hapd->wpa_auth->wpa_ie_len);
    }else{
        pQcCommitConfig->RSNWPAReqIELength = 0;
    }

    
#ifdef CONFIG_IEEE80211N
    if(drv->hapd->iconf->ieee80211n)
        pQcCommitConfig->ht_capab = drv->hapd->iconf->ht_capab;
    else 
       pQcCommitConfig->ht_capab = 0xffff;
#endif /* CONFIG_IEEE80211N */

    pQcCommitConfig->SSIDinfo.ssidHidden = drv->hapd->conf->ignore_broadcast_ssid; // There is no configuration item for it
    os_memcpy(pQcCommitConfig->SSIDinfo.ssid.ssId, drv->hapd->conf->ssid.ssid, drv->hapd->conf->ssid.ssid_len);
    pQcCommitConfig->SSIDinfo.ssid.length = drv->hapd->conf->ssid.ssid_len;

    os_memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
    iwr.u.data.pointer = (caddr_t) pQcCommitConfig;
    iwr.u.data.length = sizeof(struct s_CommitConfig);

    if (ioctl(drv->ioctl_sock, QCSAP_IOCTL_COMMIT, &iwr) < 0) {
        drv->hapdConnState = eHapdState_Notconfigured;
        perror("ioctl[SIOCGIWESSID]");
        os_free(pQcCommitConfig);
        drv->hapdConnState = eHapdState_Notconfigured;
        return -1;
    }
    
    os_free(pQcCommitConfig);
    drv->hapdConnState = eHapdState_configured;

    //Configuring the keys
    QcHostapd_set_key_info((void *)drv);

    return QcHostapd_set_iface_flags(priv, 1);

}

int
QcHostapd_wps_pbc_probe_req_ind(struct hostapd_data *hapd)
{
    struct QcHostapd_driver_data *drv = hapd->drv_priv;
    sQcSapreq_WPSPBCProbeReqIES_t *pWPSProbeReq;
    struct iwreq iwr;
    int ret = 0;
     
    pWPSProbeReq = os_zalloc(sizeof(sQcSapreq_WPSPBCProbeReqIES_t));

    os_memset(&iwr, 0, sizeof(iwr));
    os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
    
    iwr.u.data.pointer = (caddr_t) pWPSProbeReq;
    iwr.u.data.length = sizeof(sQcSapreq_WPSPBCProbeReqIES_t);

    if (ioctl(drv->ioctl_sock, QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES, &iwr) < 0) {
        perror("ioctl[QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES]");
        os_free(pWPSProbeReq);
        return -1;
    }    
    
    hostapd_wps_probe_req_rx(drv->hapd, pWPSProbeReq->macaddr, pWPSProbeReq->probeReqIE, 
        pWPSProbeReq->probeReqIELen);
    
    os_free(pWPSProbeReq);

	return ret;
}



const struct wpa_driver_ops wpa_driver_QcHostapd_ops = {
	.name			= "QcHostapd",
	.init			= QcHostapd_init,
	.deinit			= QcHostapd_deinit,
	.set_ieee8021x		= QcHostapd_set_ieee8021x,
	.set_privacy		= QcHostapd_set_privacy,
	.set_encryption		= QcHostapd_set_key,
	.get_seqnum		= QcHostapd_get_seqnum,
	.flush			= QcHostapd_flush,
	.set_generic_elem	= QcHostapd_set_opt_ie,
	.wireless_event_init	= QcHostapd_wireless_event_init,
	.wireless_event_deinit	= QcHostapd_wireless_event_deinit,
	.sta_set_flags		= QcHostapd_sta_set_flags,
	.read_sta_data		= QcHostapd_read_sta_driver_data,
	.send_eapol		= QcHostapd_send_eapol,
	.sta_disassoc		= QcHostapd_sta_disassoc,
	.sta_deauth		= QcHostapd_sta_deauth,
	.set_ssid		= QcHostapd_set_ssid,
	.get_ssid		= QcHostapd_get_ssid,
	.set_countermeasures	= QcHostapd_set_countermeasures,
	.sta_clear_stats        = QcHostapd_sta_clear_stats,
	.commit			= QcHostapd_commit,
	.set_wps_beacon_ie	= QcHostapd_set_wps_beacon_ie,
	.set_wps_probe_resp_ie	= QcHostapd_set_wps_probe_resp_ie,
};
