/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.settings.deviceinfo;

import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

import com.android.internal.telephony.TelephonyProperties;
import com.android.settings.R;
import com.android.settings.SelectSubscription;
import android.util.Log;

import java.lang.ref.WeakReference;

/**
 * Display the following information
 * # Battery Strength  : TODO
 * # Uptime
 * # Awake Time
 * # XMPP/buzz/tickle status : TODO
 *
 */
public class Status extends PreferenceActivity {

    private static final String KEY_WIMAX_MAC_ADDRESS = "wimax_mac_address";
    private static final String KEY_WIFI_MAC_ADDRESS = "wifi_mac_address";
    private static final String KEY_BT_ADDRESS = "bt_address";

    private static final int EVENT_UPDATE_STATS = 500;
    private static final String BUTTON_SELECT_SUB_KEY = "button_aboutphone_msim_status";

    private final int mResources[] = {R.xml.device_info_status,
                                      R.xml.device_info_msim_status};

    private TelephonyManager mTelephonyManager;
    private PhoneStateListener[] mPhoneStateListener;
    private Resources mRes;
    private Preference mUptime;

    private static String sUnknown;
    private int mResId = 0;
    private int mNumPhones = 0;

    private Preference mBatteryStatus;
    private Preference mBatteryLevel;
    private int mDataState = TelephonyManager.DATA_DISCONNECTED;

    private Handler mHandler;

    private static class MyHandler extends Handler {
        private WeakReference<Status> mStatus;

        public MyHandler(Status activity) {
            mStatus = new WeakReference<Status>(activity);
        }

        @Override
        public void handleMessage(Message msg) {
            Status status = mStatus.get();
            if (status == null) {
                return;
            }

            switch (msg.what) {
                case EVENT_UPDATE_STATS:
                    status.updateTimes();
                    sendEmptyMessageDelayed(EVENT_UPDATE_STATS, 1000);
                    break;
            }
        }
    }

    private BroadcastReceiver mBatteryInfoReceiver = new BroadcastReceiver() {

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (Intent.ACTION_BATTERY_CHANGED.equals(action)) {

                int level = intent.getIntExtra("level", 0);
                int scale = intent.getIntExtra("scale", 100);

                mBatteryLevel.setSummary(String.valueOf(level * 100 / scale) + "%");

                int plugType = intent.getIntExtra("plugged", 0);
                int status = intent.getIntExtra("status", BatteryManager.BATTERY_STATUS_UNKNOWN);
                String statusString;
                if (status == BatteryManager.BATTERY_STATUS_CHARGING) {
                    statusString = getString(R.string.battery_info_status_charging);
                    if (plugType > 0) {
                        statusString = statusString + " " + getString(
                                (plugType == BatteryManager.BATTERY_PLUGGED_AC)
                                        ? R.string.battery_info_status_charging_ac
                                        : R.string.battery_info_status_charging_usb);
                    }
                } else if (status == BatteryManager.BATTERY_STATUS_DISCHARGING) {
                    statusString = getString(R.string.battery_info_status_discharging);
                } else if (status == BatteryManager.BATTERY_STATUS_NOT_CHARGING) {
                    statusString = getString(R.string.battery_info_status_not_charging);
                } else if (status == BatteryManager.BATTERY_STATUS_FULL) {
                    statusString = getString(R.string.battery_info_status_full);
                } else {
                    statusString = getString(R.string.battery_info_status_unknown);
                }
                mBatteryStatus.setSummary(statusString);
            }
        }
    };

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Preference removablePref;

        mHandler = new MyHandler(this);

        mTelephonyManager = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);

        mResId = getIntent().getIntExtra("RESOURCE_INDEX", 0);
        if (mResId < mResources.length) {
            addPreferencesFromResource(mResources[mResId]);
        }

        mNumPhones = TelephonyManager.getPhoneCount();
        mPhoneStateListener = new PhoneStateListener[mNumPhones];

        for (int i=0; i < mNumPhones; i++) {
            mPhoneStateListener[i] = getPhoneStateListener(i);
            mTelephonyManager.listen(mPhoneStateListener[i],
                            PhoneStateListener.LISTEN_DATA_CONNECTION_STATE);
        }

        mBatteryLevel = findPreference("battery_level");
        mBatteryStatus = findPreference("battery_status");

        PreferenceScreen selectSub = (PreferenceScreen) findPreference(BUTTON_SELECT_SUB_KEY);
        if (selectSub != null) {
            Intent intent = selectSub.getIntent();
            intent.putExtra(SelectSubscription.PACKAGE, "com.android.settings");
            intent.putExtra(SelectSubscription.TARGET_CLASS, "com.android.settings.deviceinfo.MSimStatus");
        }

        mRes = getResources();
        if (sUnknown == null) {
            sUnknown = mRes.getString(R.string.device_info_default);
        }

        mUptime = findPreference("up_time");

        /* TODO: align with 2.3.4_r1
        setWimaxStatus();
        */
        setWifiStatus();
        setBtStatus();
    }

    @Override
    protected void onResume() {
        super.onResume();

        registerReceiver(mBatteryInfoReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        updateDataState();
        for (int i=0; i < mNumPhones; i++) {
            mTelephonyManager.listen(mPhoneStateListener[i], PhoneStateListener.LISTEN_DATA_CONNECTION_STATE);
        }

        mHandler.sendEmptyMessage(EVENT_UPDATE_STATS);
    }

    @Override
    public void onPause() {
        super.onPause();

        for (int i=0; i < mNumPhones; i++) {
            mTelephonyManager.listen(mPhoneStateListener[i], PhoneStateListener.LISTEN_NONE);
        }
        unregisterReceiver(mBatteryInfoReceiver);
        mHandler.removeMessages(EVENT_UPDATE_STATS);
    }

    private PhoneStateListener getPhoneStateListener(int subscription) {
        PhoneStateListener phoneStateListener = new PhoneStateListener(subscription) {
            @Override
            public void onDataConnectionStateChanged(int state) {
                mDataState = state;
                updateDataState();
                updateNetworkType();
            }
        };
        return phoneStateListener;
    }

    /**
     * @param preference The key for the Preference item
     * @param property The system property to fetch
     * @param alt The default value, if the property doesn't exist
     */
    private void setSummary(String preference, String property, String alt) {
        try {
            findPreference(preference).setSummary(
                    SystemProperties.get(property, alt));
        } catch (RuntimeException e) {

        }
    }

    private void setSummaryText(String preference, String text) {
            if (TextUtils.isEmpty(text)) {
               text = sUnknown;
             }
             // some preferences may be missing
             if (findPreference(preference) != null) {
                 findPreference(preference).setSummary(text);
             }
    }

    private void updateNetworkType() {
        // Whether EDGE, UMTS, etc...
        setSummary("network_type", TelephonyProperties.PROPERTY_DATA_NETWORK_TYPE, sUnknown);
    }

    private void updateDataState() {
        String display = mRes.getString(R.string.radioInfo_unknown);

        switch (mDataState) {
            case TelephonyManager.DATA_CONNECTED:
                display = mRes.getString(R.string.radioInfo_data_connected);
                break;
            case TelephonyManager.DATA_SUSPENDED:
                display = mRes.getString(R.string.radioInfo_data_suspended);
                break;
            case TelephonyManager.DATA_CONNECTING:
                display = mRes.getString(R.string.radioInfo_data_connecting);
                break;
            case TelephonyManager.DATA_DISCONNECTED:
                display = mRes.getString(R.string.radioInfo_data_disconnected);
                break;
        }

        setSummaryText("data_state", display);
    }

    /* TODO: align with 2.3.4_r1
    private void setWimaxStatus() {
        ConnectivityManager cm = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        NetworkInfo ni = cm.getNetworkInfo(ConnectivityManager.TYPE_WIMAX);

        if (ni == null) {
            PreferenceScreen root = getPreferenceScreen();
            Preference ps = (Preference) findPreference(KEY_WIMAX_MAC_ADDRESS);
            if (ps != null)
                root.removePreference(ps);
        } else {
            Preference wimaxMacAddressPref = findPreference(KEY_WIMAX_MAC_ADDRESS);
            String macAddress = SystemProperties.get("net.wimax.mac.address",
                    getString(R.string.status_unavailable));
            wimaxMacAddressPref.setSummary(macAddress);
        }
    }
    */

    private void setWifiStatus() {
        WifiManager wifiManager = (WifiManager) getSystemService(WIFI_SERVICE);
        WifiInfo wifiInfo = wifiManager.getConnectionInfo();

        Preference wifiMacAddressPref = findPreference(KEY_WIFI_MAC_ADDRESS);
        String macAddress = wifiInfo == null ? null : wifiInfo.getMacAddress();
        wifiMacAddressPref.setSummary(!TextUtils.isEmpty(macAddress) ? macAddress
                : getString(R.string.status_unavailable));
    }

    private void setBtStatus() {
        BluetoothAdapter bluetooth = BluetoothAdapter.getDefaultAdapter();
        Preference btAddressPref = findPreference(KEY_BT_ADDRESS);

        if (bluetooth == null) {
            // device not BT capable
            getPreferenceScreen().removePreference(btAddressPref);
        } else {
            String address = bluetooth.isEnabled() ? bluetooth.getAddress() : null;
            btAddressPref.setSummary(!TextUtils.isEmpty(address) ? address
                    : getString(R.string.status_unavailable));
        }
    }

    void updateTimes() {
        long at = SystemClock.uptimeMillis() / 1000;
        long ut = SystemClock.elapsedRealtime() / 1000;

        if (ut == 0) {
            ut = 1;
        }

        mUptime.setSummary(convert(ut));
    }

    private String pad(int n) {
        if (n >= 10) {
            return String.valueOf(n);
        } else {
            return "0" + String.valueOf(n);
        }
    }

    private String convert(long t) {
        int s = (int)(t % 60);
        int m = (int)((t / 60) % 60);
        int h = (int)((t / 3600));

        return h + ":" + pad(m) + ":" + pad(s);
    }
}
