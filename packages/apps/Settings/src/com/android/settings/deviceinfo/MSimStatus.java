/*
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

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Message;
import android.os.SystemProperties;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.telephony.PhoneNumberUtils;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.settings.R;

import java.lang.ref.WeakReference;

/**
 * Display the following information
 * # Phone Number
 * # Network
 * # Roaming
 * # Device Id (IMEI in GSM and MEID in CDMA)
 * # Network type
 * # Signal Strength
 * # Awake Time
 * # XMPP/buzz/tickle status : TODO
 *
 */
public class MSimStatus extends PreferenceActivity {

    private TelephonyManager mTelephonyManager;
    private Phone mPhone = null;
    private Resources mRes;
    private Preference mSigStrength;
    SignalStrength mSignalStrength;
    ServiceState mServiceState;
    private int mSub = 0;
    private PhoneStateListener mPhoneStateListener;
    public static final String SUBSCRIPTION_ID = "SUBSCRIPTION_ID";

    private static String sUnknown;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Preference removablePref;

        mTelephonyManager = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);

        addPreferencesFromResource(R.xml.device_info_subscription_status);

        // getting selected subscription
        mSub = getIntent().getIntExtra(SUBSCRIPTION_ID, 0);
        Log.d("Status","OnCreate mSub =" + mSub);

        mPhoneStateListener = getPhoneStateListener(mSub);
        mTelephonyManager.listen(mPhoneStateListener,
                                PhoneStateListener.LISTEN_SERVICE_STATE
                                | PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);

        mRes = getResources();
        if (sUnknown == null) {
            sUnknown = mRes.getString(R.string.device_info_default);
        }

        mPhone = PhoneFactory.getPhone(mSub);
        // Note - missing in zaku build, be careful later...
        mSigStrength = findPreference("signal_strength");

        //NOTE "imei" is the "Device ID" since it represents the IMEI in GSM and the MEID in CDMA
        if (mPhone.getPhoneName().equals("CDMA")) {
            setSummaryText("esn_number", mPhone.getEsn());
            setSummaryText("meid_number", mPhone.getMeid());
            setSummaryText("min_number", mPhone.getCdmaMin());
            setSummaryText("prl_version", mPhone.getCdmaPrlVersion());

            // device is not GSM/UMTS, do not display GSM/UMTS features
            // check Null in case no specified preference in overlay xml
            removablePref = findPreference("imei");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
            removablePref = findPreference("imei_sv");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
        } else {
            setSummaryText("imei", mPhone.getDeviceId());

            setSummaryText("imei_sv", mTelephonyManager.getDeviceSoftwareVersion(mSub));

            // device is not CDMA, do not display CDMA features
            // check Null in case no specified preference in overlay xml
            removablePref = findPreference("prl_version");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
            removablePref = findPreference("esn_number");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
            removablePref = findPreference("meid_number");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
            removablePref = findPreference("min_number");
            if (removablePref != null) {
                getPreferenceScreen().removePreference(removablePref);
            }
        }

        String rawNumber = mPhone.getLine1Number();  // may be null or empty
        String formattedNumber = null;
        if (!TextUtils.isEmpty(rawNumber)) {
            formattedNumber = PhoneNumberUtils.formatNumber(rawNumber);
        }
        // If formattedNumber is null or empty, it'll display as "Unknown".
        setSummaryText("number", formattedNumber);

        // baseband version
        setSummaryText("baseband_version",
            TelephonyManager.getTelephonyProperty("gsm.version.baseband", mSub, ""));
    }

    @Override
    protected void onResume() {
        super.onResume();
        mTelephonyManager.listen(mPhoneStateListener,
                                 PhoneStateListener.LISTEN_SERVICE_STATE
                                 | PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
        updateSignalStrength();
        updateServiceState();
    }

    @Override
    public void onPause() {
        super.onPause();
        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
    }

    private PhoneStateListener getPhoneStateListener(int subscription) {
        PhoneStateListener phoneStateListener = new PhoneStateListener(subscription) {
            @Override
            public void onSignalStrengthsChanged(SignalStrength signalStrength) {
                mSignalStrength = signalStrength;
                updateSignalStrength();
            }
            @Override
            public void onServiceStateChanged(ServiceState state) {
                mServiceState = state;
                updateServiceState();
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

    private void updateServiceState() {
        String display = mRes.getString(R.string.radioInfo_unknown);

        if (mServiceState != null) {
            int state = mServiceState.getState();

            switch (state) {
                case ServiceState.STATE_IN_SERVICE:
                    display = mRes.getString(R.string.radioInfo_service_in);
                    break;
                case ServiceState.STATE_OUT_OF_SERVICE:
                case ServiceState.STATE_EMERGENCY_ONLY:
                    display = mRes.getString(R.string.radioInfo_service_out);
                    break;
                case ServiceState.STATE_POWER_OFF:
                    display = mRes.getString(R.string.radioInfo_service_off);
                    break;
            }

            setSummaryText("service_state", display);

            if (mServiceState.getRoaming()) {
                setSummaryText("roaming_state", mRes.getString(R.string.radioInfo_roaming_in));
            } else {
                setSummaryText("roaming_state", mRes.getString(R.string.radioInfo_roaming_not));
            }
            setSummaryText("operator_name", mServiceState.getOperatorAlphaLong());
        }
    }

    void updateSignalStrength() {
        // not loaded in some versions of the code (e.g., zaku)
        int signalDbm = 0;

        if (mSignalStrength != null) {
            int state = mServiceState.getState();
            Resources r = getResources();

            if ((ServiceState.STATE_OUT_OF_SERVICE == state) ||
                    (ServiceState.STATE_POWER_OFF == state)) {
                mSigStrength.setSummary("0");
            }

            if (!mSignalStrength.isGsm()) {
                signalDbm = mSignalStrength.getCdmaDbm();
            } else {
                int gsmSignalStrength = mSignalStrength.getGsmSignalStrength();
                int asu = (gsmSignalStrength == 99 ? -1 : gsmSignalStrength);
                if (asu != -1) {
                    signalDbm = -113 + 2*asu;
                }
            }
            if (-1 == signalDbm) signalDbm = 0;

            int signalAsu = mSignalStrength.getGsmSignalStrength();
            if (-1 == signalAsu) signalAsu = 0;

            mSigStrength.setSummary(String.valueOf(signalDbm) + " "
                        + r.getString(R.string.radioInfo_display_dbm) + "   "
                        + String.valueOf(signalAsu) + " "
                        + r.getString(R.string.radioInfo_display_asu));
        }
    }

}
