/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
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

package com.qualcomm.wifi.softap;

public class L10NConstants {
	public static final String TAG_NS = "QCSOFTAP_GUI_NS";
	public static final String TAG_MFS = "QCSOFTAP_GUI_MACFS";
	public static final String TAG_WS = "QCSOFTAP_GUI_WS";
	public static final String TAG_BWS = "QCSOFTAP_GUI_BWS";
	public static final String TAG_WSS_WPAPSK = "QCSOFTAP_GUI_WSS_WPAPSK";
	public static final String TAG_AWS = "QCSOFTAP_GUI_AWS";	
	public static final int MENU_ABOUT = 0;
	
	//	--- Original Preference file Name ---	
	public static final String CONFIG_FILE_NAME = "orgConfig";
	
	//	--- Regular Expressions for Validation ---	
	public static final String MAC_PATTERN = "((([0-9a-fA-F]){2}[:]){5}([0-9a-fA-F]){2})";
	public static final String MAC_PATTERN1 = "^[0-9a-fA-F]{1,2}$";
	public static final String HEXA_PATTERN = "[0-9a-fA-F]{10}|[0-9a-fA-F]{26}|[0-9a-fA-F]{32}";
	//public static final String PIN_PATTERN = "^[0-9]{8,32}$";	
	public static final String PIN_PATTERN = "^[0-9]{8}$";
	
	public static final String SET_CMD_PREFIX = "set ";
	public static final String GET_CMD_PREFIX = "get ";
	public static final String SELECT_TITLE = "Select";
	public static final String SUCCESS = "success";
	public static final String WPS_PIN_TITLE = "Enter WPS PIN";
	public static final String OUT_GTR_RANGE = "should be >= 600";
	public static final String OUT_RANGE = "Out of Range";
	public static final String INVALID_ENTRY = "Invalid Entry";
	
	//	--- Preference keys --- 	
	public static final String WPS_KEY = "wps_state";
	public static final String CHNL_KEY = "channel";
	public static final String AUTH_MODE_KEY = "auth_algs";
	public static final String IGNORE_BROAD_SSID_KEY = "ignore_broadcast_ssid";
	public static final String CONFIG_KEY = "config_methods";
	public static final String HW_MODE_KEY = "hw_mode";	
	public static final String DATA_RATE_KEY = "data_rate";
	public static final String COUNTRY_KEY = "country_code";
	public static final String AP_SHUT_TIMER = "auto_shut_off_time";
	public static final String SEC_MODE_KEY = "security_mode";	
	public static final String RSN_PAIR_KEY = "rsn_pairwise";
	public static final String WPA_PAIR_KEY = "wpa_pairwise";
	public static final String ENABLE_SOFTAP = "enable_softap";
	public static final String SSID_BROADCAST_KEY = "broadcast_ssid";
	public static final String SSID_KEY = "ssid";
	public static final String WPA_PASSPHRASE_KEY = "wpa_passphrase";
	public static final String WPA_GRP_KEY = "wpa_group_rekey";
	public static final String WSS_PREF_CATEG_KEY = "wss_wpapsk_catag";
	public static final String WIFI_AP_CHECK="status";
	public static final String ENERGY_DETECT_THRESHOLD="energy_detect_threshold";
	public static final String ENERGY_MODE="energy_mode";
	public static final String ALLOW = "allow";
	public static final String DENY = "deny";
	public static final String WMM_LST_KEY = "wmm_enabled";
	public static final String REGULATORY_DOMAIN = "regulatory_domain";
	public static final String SDK_VERSION = "sdk_version";
	public static final String ALLOW_LIST = "allow_list";
	public static final String DENY_LIST = "deny_list";
	public static final String ADD_TO_ALLOW_LIST = "add_to_allow_list";
	public static final String ADD_TO_DENY_LIST = "add_to_deny_list";
	public static final String REMOVE_FROM_ALLOW_LIST = "remove_from_allow_list";
	public static final String REMOVE_FROM_DENY_LIST = "remove_from_deny_list";
	public static final String WEP_KEY0 = "wep_key0";
	public static final String WEP_KEY1 = "wep_key1";
	public static final String WEP_KEY2 = "wep_key2";
	public static final String WEP_KEY3 = "wep_key3";
	//	--- Dialog IDs ---		
	public static final int DIALOG_WPS = 4,DIALOG_WPS_SESSION=5,DIALOG_WPS_PINENTRY=6;
	public static final int DIALOG_OFF = 0, DIALOG_ON = 1, DIALOG_RESET = 2, 
						DIALOG_SAVE = 3, DIALOG_INITIAL = 99;
	
	public static final int MINUTE = 2*60*1000;
	public static final int SHUTDOWN_TIME = 10000;
	public static final int APSTAT_TIME = 1000;
	public static final int EVENT_ID = 1;	
	public static final int MAX_LENGTH = 15;	
	public static final String VAL_ZERO = "0";
	public static final String VAL_ONE = "1";	
	public static final String VAL_TWO = "2";
	public static final String VAL_THREE = "3";
	public static final String VAL_FOUR = "4";
	public static final String VAL_SEVEN = "7";	
	public static final String VAL_ONEHUNDREDTWENTYEIGHT = "128";
	public static final String STATION_102 = "102";
	public static final String STATION_103 = "103";
	public static final String STATION_105 = "105";
	
	//	--- Network Mode List Values ---	
	public static final String SM_N_ONLY = "n_only";
	public static final String SM_N = "n";
	public static final String SM_G_ONLY = "g_only";
	public static final String SM_G = "g";
	public static final String SM_B = "b";
	
	//	--- Security Mode List Values ---	
	public static final String OPEN = "Open"; 
	public static final String WEP = "WEP";
	public static final String WPA_PSK = "WPA-PSK";
	public static final String WPA2_PSK = "WPA2-PSK";
	public static final String WPA_MIXED = "WPA-WPA2 Mixed";
	public static final String SM_EXTRA_KEY = "SecurityMode";		
	public static final String WPA_ALG_TKIP = "TKIP";
	public static final String WPA_ALG_MIXED = "TKIP CCMP";

	public static final String ERROR_NULL = "Value cannot be null";
	public static final String ERROR_OUT_RANGE = "is out of range";
	public static final String ERROR_BELOW_RANGE = "is below range";
	
	// ---Basic Wireless Settings --
	public static final String WPS_SESSION_MSG="Press the Push Button on the client to connect...";	
	public static final String SECONDS = "seconds";
	
}
