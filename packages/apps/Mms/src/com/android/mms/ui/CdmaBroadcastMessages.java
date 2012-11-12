/*
 * Copyright (C) 2009 The Android Open Source Project
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


package com.android.mms.ui;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.PreferenceActivity;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.RILConstants;
import com.android.mms.R;
import com.android.mms.transaction.CdmaBroadcastConfigurator;

import android.os.AsyncResult;
import android.os.Handler;
import android.os.Message;
import android.telephony.SmsManager;
import android.util.Log;


/**
 * List of Phone-specific settings screens.
 */
public class CdmaBroadcastMessages extends PreferenceActivity
        implements Preference.OnPreferenceChangeListener{
    // debug data
    private static final String LOG_TAG = "CdmaBroadcastSms";
    private static final boolean DBG = true;

    //String keys for preference lookup
    private static final String BUTTON_ENABLE_DISABLE_BC_SMS_KEY =
        "button_enable_disable_cdma_bc_sms";
    private static final String LIST_LANGUAGE_KEY =
        "list_language";
    private static final String KEY_UNKNOWN_BROADCAST =
        "button_unknown_broadcast";
    private static final String BUTTON_EMERGENCY_BROADCAST_KEY =
        "button_emergency_broadcast";
    private static final String BUTTON_ADMINISTRATIVE_KEY =
        "button_administrative";
    private static final String BUTTON_MAINTENANCE_KEY =
        "button_maintenance";
    private static final String BUTTON_LOCAL_WEATHER_KEY =
        "button_local_weather";
    private static final String BUTTON_ATR_KEY =
        "button_atr";
    private static final String BUTTON_LAFS_KEY =
        "button_lafs";
    private static final String BUTTON_RESTAURANTS_KEY =
        "button_restaurants";
    private static final String BUTTON_LODGINGS_KEY =
        "button_lodgings";
    private static final String BUTTON_RETAIL_DIRECTORY_KEY =
        "button_retail_directory";
    private static final String BUTTON_ADVERTISEMENTS_KEY =
        "button_advertisements";
    private static final String BUTTON_STOCK_QUOTES_KEY =
        "button_stock_quotes";
    private static final String BUTTON_EO_KEY =
        "button_eo";
    private static final String BUTTON_MHH_KEY =
        "button_mhh";
    private static final String BUTTON_TECHNOLOGY_NEWS_KEY =
        "button_technology_news";
    private static final String BUTTON_MULTI_CATEGORY_KEY =
        "button_multi_category";

    private static final String BUTTON_LOCAL_GENERAL_NEWS_KEY =
        "button_local_general_news";
    private static final String BUTTON_REGIONAL_GENERAL_NEWS_KEY =
        "button_regional_general_news";
    private static final String BUTTON_NATIONAL_GENERAL_NEWS_KEY =
        "button_national_general_news";
    private static final String BUTTON_INTERNATIONAL_GENERAL_NEWS_KEY =
        "button_international_general_news";

    private static final String BUTTON_LOCAL_BF_NEWS_KEY =
        "button_local_bf_news";
    private static final String BUTTON_REGIONAL_BF_NEWS_KEY =
        "button_regional_bf_news";
    private static final String BUTTON_NATIONAL_BF_NEWS_KEY =
        "button_national_bf_news";
    private static final String BUTTON_INTERNATIONAL_BF_NEWS_KEY =
        "button_international_bf_news";

    private static final String BUTTON_LOCAL_SPORTS_NEWS_KEY =
        "button_local_sports_news";
    private static final String BUTTON_REGIONAL_SPORTS_NEWS_KEY =
        "button_regional_sports_news";
    private static final String BUTTON_NATIONAL_SPORTS_NEWS_KEY =
        "button_national_sports_news";
    private static final String BUTTON_INTERNATIONAL_SPORTS_NEWS_KEY =
        "button_international_sports_news";

    private static final String BUTTON_LOCAL_ENTERTAINMENT_NEWS_KEY =
        "button_local_entertainment_news";
    private static final String BUTTON_REGIONAL_ENTERTAINMENT_NEWS_KEY =
        "button_regional_entertainment_news";
    private static final String BUTTON_NATIONAL_ENTERTAINMENT_NEWS_KEY =
        "button_national_entertainment_news";
    private static final String BUTTON_INTERNATIONAL_ENTERTAINMENT_NEWS_KEY =
        "button_international_entertainment_news";

    private static final int NO_OF_SERVICE_CATEGORIES = 32;

    //UI objects
    private CheckBoxPreference mButtonBcSms;
    private ListPreference mListLanguage;
    private CheckBoxPreference mCheckBoxes[] = new CheckBoxPreference[NO_OF_SERVICE_CATEGORIES];

    /**
     * Invoked on each preference click in this hierarchy, overrides
     * PreferenceActivity's implementation.  Used to make sure we track the
     * preference click events.
     */
    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
            Preference preference) {
        if (preference == mButtonBcSms) {
            if (DBG) Log.d(LOG_TAG, "onPreferenceTreeClick: preference == mButtonBcSms.");
            if(mButtonBcSms.isChecked()) {
                enableDisableAllCbConfigButtons(true);
            } else {
                enableDisableAllCbConfigButtons(false);
            }
        } else if (preference == mListLanguage) {
            //Do nothing here, because this click will be handled in onPreferenceChange
        } else {
            for (int i = 0; i < mCheckBoxes.length; i++) {
                CheckBoxPreference pref = mCheckBoxes[i];
                if (preference == pref) {
                    CdmaBroadcastConfigurator cbc = CdmaBroadcastConfigurator.getInstance(this);
                    cbc.switchService(i, pref.isChecked());
                    break;
                }
            }
        }
        return true;
    }

    public boolean onPreferenceChange(Preference preference, Object objValue) {
        if (preference == mListLanguage) {
            // set the new language to the array which will be transmitted later
            // TODO: Handle language
            //CellBroadcastSmsConfig.setConfigDataCompleteLanguage(
            //        mListLanguage.findIndexOfValue((String) objValue) + 1);
        }

        // always let the preference setting proceed.
        return true;
    }

    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        addPreferencesFromResource(R.xml.cdma_broadcast_messages);

        PreferenceScreen prefSet = getPreferenceScreen();

        mButtonBcSms = (CheckBoxPreference) prefSet.findPreference(
                BUTTON_ENABLE_DISABLE_BC_SMS_KEY);
        mListLanguage = (ListPreference) prefSet.findPreference(
                LIST_LANGUAGE_KEY);
        // set the listener for the language list preference
        mListLanguage.setOnPreferenceChangeListener(this);

        mCheckBoxes[0] = (CheckBoxPreference) prefSet.findPreference(KEY_UNKNOWN_BROADCAST);
        mCheckBoxes[1] = (CheckBoxPreference) prefSet.findPreference(BUTTON_EMERGENCY_BROADCAST_KEY);
        mCheckBoxes[2] = (CheckBoxPreference) prefSet.findPreference(BUTTON_ADMINISTRATIVE_KEY);
        mCheckBoxes[3] = (CheckBoxPreference) prefSet.findPreference(BUTTON_MAINTENANCE_KEY);
        mCheckBoxes[4] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LOCAL_WEATHER_KEY);
        mCheckBoxes[5] = (CheckBoxPreference) prefSet.findPreference(BUTTON_ATR_KEY);
        mCheckBoxes[6] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LAFS_KEY);
        mCheckBoxes[7] = (CheckBoxPreference) prefSet.findPreference(BUTTON_RESTAURANTS_KEY);
        mCheckBoxes[8] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LODGINGS_KEY);
        mCheckBoxes[9] = (CheckBoxPreference) prefSet.findPreference(BUTTON_RETAIL_DIRECTORY_KEY);
        mCheckBoxes[10] = (CheckBoxPreference) prefSet.findPreference(BUTTON_ADVERTISEMENTS_KEY);
        mCheckBoxes[11] = (CheckBoxPreference) prefSet.findPreference(BUTTON_STOCK_QUOTES_KEY);
        mCheckBoxes[12] = (CheckBoxPreference) prefSet.findPreference(BUTTON_EO_KEY);
        mCheckBoxes[13] = (CheckBoxPreference) prefSet.findPreference(BUTTON_MHH_KEY);
        mCheckBoxes[14] = (CheckBoxPreference) prefSet.findPreference(BUTTON_TECHNOLOGY_NEWS_KEY);
        mCheckBoxes[15] = (CheckBoxPreference) prefSet.findPreference(BUTTON_MULTI_CATEGORY_KEY);
        mCheckBoxes[16] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LOCAL_GENERAL_NEWS_KEY);
        mCheckBoxes[17] = (CheckBoxPreference) prefSet.findPreference(BUTTON_REGIONAL_GENERAL_NEWS_KEY);
        mCheckBoxes[18] = (CheckBoxPreference) prefSet.findPreference(BUTTON_NATIONAL_GENERAL_NEWS_KEY);
        mCheckBoxes[19] = (CheckBoxPreference) prefSet.findPreference(BUTTON_INTERNATIONAL_GENERAL_NEWS_KEY);
        mCheckBoxes[20] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LOCAL_BF_NEWS_KEY);
        mCheckBoxes[21] = (CheckBoxPreference) prefSet.findPreference(BUTTON_REGIONAL_BF_NEWS_KEY);
        mCheckBoxes[22] = (CheckBoxPreference) prefSet.findPreference(BUTTON_NATIONAL_BF_NEWS_KEY);
        mCheckBoxes[23] = (CheckBoxPreference) prefSet.findPreference(BUTTON_INTERNATIONAL_BF_NEWS_KEY);
        mCheckBoxes[24] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LOCAL_SPORTS_NEWS_KEY);
        mCheckBoxes[25] = (CheckBoxPreference) prefSet.findPreference(BUTTON_REGIONAL_SPORTS_NEWS_KEY);
        mCheckBoxes[26] = (CheckBoxPreference) prefSet.findPreference(BUTTON_NATIONAL_SPORTS_NEWS_KEY);
        mCheckBoxes[27] = (CheckBoxPreference) prefSet.findPreference(BUTTON_INTERNATIONAL_SPORTS_NEWS_KEY);
        mCheckBoxes[28] = (CheckBoxPreference) prefSet.findPreference(BUTTON_LOCAL_ENTERTAINMENT_NEWS_KEY);
        mCheckBoxes[29] = (CheckBoxPreference) prefSet.findPreference(BUTTON_REGIONAL_ENTERTAINMENT_NEWS_KEY);
        mCheckBoxes[30] = (CheckBoxPreference) prefSet.findPreference(BUTTON_NATIONAL_ENTERTAINMENT_NEWS_KEY);
        mCheckBoxes[31] = (CheckBoxPreference) prefSet.findPreference(BUTTON_INTERNATIONAL_ENTERTAINMENT_NEWS_KEY);
    }

    @Override
    protected void onResume() {
        super.onResume();

        getPreferenceScreen().setEnabled(true);

        CdmaBroadcastConfigurator cbc = CdmaBroadcastConfigurator.getInstance(this);
        mButtonBcSms.setChecked(cbc.isCdmaBroadcastAllowed());
        for (int i = 0; i < NO_OF_SERVICE_CATEGORIES; i++) {
            mCheckBoxes[i].setChecked(cbc.isServiceOn(i));
        }

        if(mButtonBcSms.isChecked()) {
            enableDisableAllCbConfigButtons(true);
        } else {
            enableDisableAllCbConfigButtons(false);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();


        }

    private void enableDisableAllCbConfigButtons(boolean enable) {
        for (CheckBoxPreference pref : mCheckBoxes) {
            pref.setEnabled(enable);
        }
        mListLanguage.setEnabled(enable);
        CdmaBroadcastConfigurator cbc = CdmaBroadcastConfigurator.getInstance(this);
        cbc.enableCdmaBroadcast(enable);
    }
}
