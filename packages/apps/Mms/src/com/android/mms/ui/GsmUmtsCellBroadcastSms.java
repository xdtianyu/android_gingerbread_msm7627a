/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
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

package com.android.mms.ui;

import com.android.mms.R;
import com.android.mms.transaction.GsmBroadcastConfigurator;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.telephony.SmsManager;
import android.util.Log;
import android.widget.Toast;

public class GsmUmtsCellBroadcastSms extends PreferenceActivity {
    // debug data
    private static final String LOG_TAG = "GsmUmtsCellBroadcastSms";

    // String keys for preference lookup
    private static final String AREA_INFO_PREFERENCE_KEY = "area_info_msgs_key";

    // Preference instance variables.
    private CheckBoxPreference mAreaInfoPreference;

    // Instance variables
    private int mSubscription = 0;

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
            Preference preference) {
        if (preference == mAreaInfoPreference) {
            boolean state = mAreaInfoPreference.isChecked();
            Log.d(LOG_TAG, "onPreferenceTreeClick: AreaInfo - " + state);
            GsmBroadcastConfigurator gbc = GsmBroadcastConfigurator.getInstance(this);
            if (!gbc.switchService(mAreaInfoPreference.isChecked(), mSubscription)) {
                displayErrorToast(state);
                // Since swithService failed, reset the state of the
                // preference.
                mAreaInfoPreference.setChecked(!state);
            }
            return true;
        }

        return false;
    }

    void displayErrorToast(boolean state) {
        String msg = state ? "Enabling " : "Disabling ";
        msg = msg + "Msg_Id 50 on sub" + mSubscription + " failed." + " Try after some time";
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        addPreferencesFromResource(R.xml.gsm_umts_cell_broadcast_sms);
        mAreaInfoPreference = (CheckBoxPreference) findPreference(AREA_INFO_PREFERENCE_KEY);
        mSubscription = getIntent().getIntExtra(MessagingPreferenceActivity.SUBSCRIPTION, 0);
        Log.d(LOG_TAG, "onCreate: mSubscription is: " + mSubscription);

    }

    @Override
    protected void onResume() {
        super.onResume();

        GsmBroadcastConfigurator gbc = GsmBroadcastConfigurator.getInstance(this);
        mAreaInfoPreference.setChecked(gbc.getMessageStatus(mSubscription));
    }

    @Override
    protected void onPause() {
        super.onPause();
    }
}
