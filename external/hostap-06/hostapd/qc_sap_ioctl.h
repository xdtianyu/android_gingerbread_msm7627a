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


#ifndef _QC_SAP_IOCTL_H_
#define _QC_SAP_IOCTL_H_

/*
 * QCSAP ioctls.
 */

/*
 * Max size of optional information elements.  We artificially
 * constrain this; it's limited only by the max frame size (and
 * the max parameter size of the wireless extensions).
 */
#define	QCSAP_MAX_OPT_IE	256
#define	QCSAP_MAX_WSC_IE	256

typedef struct sSSID
{
    u_int8_t       length;
    u_int8_t       ssId[32];
} tSSID;

typedef struct sSSIDInfo
{
   tSSID     ssid;   
   u_int8_t  ssidHidden;
}tSSIDInfo;

typedef enum {
    eQC_DOT11_MODE_ALL = 0,
    eQC_DOT11_MODE_ABG = 0x0001,    //11a/b/g only, no HT, no proprietary
    eQC_DOT11_MODE_11A = 0x0002,
    eQC_DOT11_MODE_11B = 0x0004,
    eQC_DOT11_MODE_11G = 0x0008,
    eQC_DOT11_MODE_11N = 0x0010,
    eQC_DOT11_MODE_11G_ONLY = 0x0020,
    eQC_DOT11_MODE_11N_ONLY = 0x0040,
    eQC_DOT11_MODE_11B_ONLY = 0x0080,
    eQC_DOT11_MODE_11A_ONLY = 0x0100,
    //This is for WIFI test. It is same as eWNIAPI_MAC_PROTOCOL_ALL except when it starts IBSS in 11B of 2.4GHz
    //It is for CSR internal use
    eQC_DOT11_MODE_AUTO = 0x0200,

} tQcPhyMode;

#define	QCSAP_ADDR_LEN	6

typedef u_int8_t qcmacaddr[QCSAP_ADDR_LEN];

struct qc_mac_acl_entry {
	qcmacaddr addr;
	int vlan_id;
};

typedef enum {
    eQC_AUTH_TYPE_OPEN_SYSTEM,
    eQC_AUTH_TYPE_SHARED_KEY,
    eQC_AUTH_TYPE_AUTO_SWITCH
} eQcAuthType; 

typedef enum {
    eQC_WPS_BEACON_IE,
    eQC_WPS_PROBE_RSP_IE,
    eQC_WPS_ASSOC_RSP_IE
} eQCWPSType;

typedef struct s_CommitConfig {

    tSSIDInfo SSIDinfo;

    u_int32_t beacon_int;   	/* Beacon Interval */

    tQcPhyMode hw_mode;	/* Wireless Mode */

    u_int32_t channel;		/* Operation channel */

    u_int32_t max_num_sta; 	/* maximum number of STAs in station table */

    u_int32_t dtim_period;	/* dtim interval */
    u_int32_t max_listen_interval;

    enum {
		QC_ACCEPT_UNLESS_DENIED = 0,
		QC_DENY_UNLESS_ACCEPTED = 1,
	} qc_macaddr_acl;
    
    struct qc_mac_acl_entry *accept_mac; /* MAC filtering */
    u_int32_t num_accept_mac;
    struct qc_mac_acl_entry *deny_mac;   /* MAC filtering */
    u_int32_t num_deny_mac;

    u_int32_t ap_table_max_size;
    u_int32_t ap_table_expiration_time;

    int qcsap80211d;

    u_int32_t countryCode[3];  //it ignored if [0] is 0.

    u_int32_t ht_op_mode_fixed;
    
    /*HT capability information to enable/diabale protection
     *           bit15   bit14   bit13   bit12 bit11 bit10    bit9 bit8
     * (overlap) from11a from11b from11g Ht20  NonGf LsigTxop Rifs OBSS   
     * bit7    bit6    bit5    bit4 bit3  bit2     bit1 bit0
     * from11a from11b from11g ht20 nonGf lsigTxop rifs obss*/
    u_int16_t ht_capab;

    u_int32_t qcsap80211n;

    eQcAuthType authType;

    u_int8_t privacy;
	
    u_int8_t set_ieee8021x;

    u_int8_t RSNWPAReqIE[QCSAP_MAX_OPT_IE];     //If not null, it has the IE byte stream for RSN/WPA
    u_int16_t RSNWPAReqIELength;  //The byte count in the pRSNReqIE/ WPAIE
    u_int8_t wps_state; //wps_state - disbaled/not configured, configured
} s_CommitConfig_t;


/*
 * MLME state manipulation request.  QCSAP_MLME_ASSOC
 * is used for station mode only.  The other types are used for station or ap mode.
 */
struct sQcSapreq_mlme {
    u_int8_t    im_op;		/* operation to perform */
#define	QCSAP_MLME_ASSOC        1	/* associate station */
#define	QCSAP_MLME_DISASSOC     2	/* disassociate station */
#define	QCSAP_MLME_DEAUTH       3	/* deauthenticate station */
#define	QCSAP_MLME_AUTHORIZE    4	/* authorize station */
#define	QCSAP_MLME_UNAUTHORIZE  5	/* unauthorize station */
#define QCSAP_MLME_MICFAILURE   6   /* TKIP MICFAILURE */
    u_int16_t   im_reason;	/* 802.11 reason code */
    u_int8_t    im_macaddr[QCSAP_ADDR_LEN];
};


/*
 * Retrieve the WPA/RSN information element for an associated station.
 */
struct sQcSapreq_wpaie {
	u_int8_t	wpa_ie[QCSAP_MAX_OPT_IE];
	u_int8_t	wpa_macaddr[QCSAP_ADDR_LEN];
};

/*
 * Retrieve the WSC information element for an associated station.
 */
struct sQcSapreq_wscie {
	u_int8_t	wsc_macaddr[QCSAP_ADDR_LEN];
	u_int8_t	wsc_ie[QCSAP_MAX_WSC_IE];
};


/*
 * Retrieve the WPS PBC Probe Request IEs.
 */
typedef struct sQcSapreq_WPSPBCProbeReqIES {
	u_int8_t	macaddr[QCSAP_ADDR_LEN];
    u_int16_t   probeReqIELen;
    u_int8_t    probeReqIE[512]; 
} sQcSapreq_WPSPBCProbeReqIES_t ;



#ifdef __linux__
/*
 * Wireless Extensions API, private ioctl interfaces.
 *
 * NB: Even-numbered ioctl numbers have set semantics and are privileged!
 *     (regardless of the incorrect comment in wireless.h!)
 */

#define	QCSAP_IOCTL_SETPARAM          (SIOCIWFIRSTPRIV+0)
#define	QCSAP_IOCTL_GETPARAM          (SIOCIWFIRSTPRIV+1)
#define	QCSAP_IOCTL_COMMIT            (SIOCIWFIRSTPRIV+2)
#define QCSAP_IOCTL_SETMLME           (SIOCIWFIRSTPRIV+3)

#define QCSAP_IOCTL_GET_STAWPAIE      (SIOCIWFIRSTPRIV+4)
#define QCSAP_IOCTL_SETWPAIE          (SIOCIWFIRSTPRIV+5)
#define QCSAP_IOCTL_STOPBSS           (SIOCIWFIRSTPRIV+6)
#define QCSAP_IOCTL_VERSION           (SIOCIWFIRSTPRIV+7)
#define QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES       (SIOCIWFIRSTPRIV+8)
#define QCSAP_IOCTL_GET_CHANNEL       (SIOCIWFIRSTPRIV+9)
#define QCSAP_IOCTL_ASSOC_STA_MACADDR (SIOCIWFIRSTPRIV+10)
#define QCSAP_IOCTL_DISASSOC_STA      (SIOCIWFIRSTPRIV+11)
#define QCSAP_IOCTL_AP_STATS          (SIOCIWFIRSTPRIV+12)
#define QCSAP_IOCTL_GET_STATS         (SIOCIWFIRSTPRIV+13)
#define QCSAP_IOCTL_CLR_STATS         (SIOCIWFIRSTPRIV+14)

enum { 
    QCSAP_PARAM_1 = 1,
    QCSAP_PARAM_2 = 2,
};


#endif /* __linux__ */

#endif /*_QC_SAP_IOCTL_H_*/
