/******************************************************************************
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
 *  ******************************************************************************/

package com.android.settings;

import android.content.ContentResolver;
import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;

import android.preference.CheckBoxPreference;
import android.app.Dialog;
import android.app.TimePickerDialog;
import android.widget.TimePicker;

import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;

import java.util.TimeZone;
import java.lang.System;
import android.provider.Settings;
import android.util.Log;
import android.view.IWindowManager;

public class DynamicMemoryManagerSettings extends PreferenceActivity {

    private static final String TAG = "DMM Settings";

    private static final String KEY_DMM_DPD_ENABLE = "dmm_dpd_enable";
    private static final String KEY_DMM_DPD_START = "dmm_dpd_start_time";
    private static final String KEY_DMM_DPD_STOP = "dmm_dpd_stop_time";

    private static final int DIALOG_START_TIMEPICKER = 0;
    private static final int DIALOG_STOP_TIMEPICKER = 1;

    private CheckBoxPreference pDMMDPDEnabled;
    private Preference pStartTime;
    private Preference pStopTime;

    private int mDMMDPDEnabled = 1;
    private int mStartTimeHr = 20, mStartTimeMin = 0;
    private int mStopTimeHr = 7, mStopTimeMin = 0;

    private IWindowManager mWindowManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ContentResolver resolver = getContentResolver();
        mWindowManager = IWindowManager.Stub.asInterface(ServiceManager.getService("window"));

        addPreferencesFromResource(R.xml.dynamicmemorymanagersettings);
        if(SystemProperties.get("ro.dev.dmm.dpd.start_address", "0").compareTo("0") != 0) {
            pDMMDPDEnabled = (CheckBoxPreference) findPreference(KEY_DMM_DPD_ENABLE);
            pStartTime = findPreference(KEY_DMM_DPD_START);
            pStopTime = findPreference(KEY_DMM_DPD_STOP);

            getSavedSettings();
            updateUI();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    private void updateUI() {
        if(mDMMDPDEnabled == 1) {
            pDMMDPDEnabled.setChecked(true);
            pStartTime.setEnabled(true);
            pStopTime.setEnabled(true);
        }
        else {
            pDMMDPDEnabled.setChecked(false);
            pStartTime.setEnabled(false);
            pStopTime.setEnabled(false);
        }
        pStartTime.setSummary(Integer.toString(mStartTimeHr) + ":"
                            + Integer.toString(mStartTimeMin));
        pStopTime.setSummary(Integer.toString(mStopTimeHr) + ":"
                            + Integer.toString(mStopTimeMin));
    }

    private void getSavedSettings() {
        try {
            mDMMDPDEnabled = SystemProperties.getInt("persist.sys.dpd", 1);
            mStartTimeHr = SystemProperties.getInt("persist.sys.dpd.start_hr", 20);
            mStartTimeMin = SystemProperties.getInt("persist.sys.dpd.start_min", 0);
            mStopTimeHr = SystemProperties.getInt("persist.sys.dpd.stop_hr", 7);
            mStopTimeMin = SystemProperties.getInt("persist.sys.dpd.stop_min", 0);
            /* If DPD properties have not been created.
                Then create them and set them to defaults.*/
            if(SystemProperties.getInt("persist.sys.dpd", -1) == -1) {
                SystemProperties.set("persist.sys.dpd", Integer.toString(mDMMDPDEnabled));
                SystemProperties.set("persist.sys.dpd.start_hr", Integer.toString(mStartTimeHr));
                SystemProperties.set("persist.sys.dpd.start_min", Integer.toString(mStartTimeMin));
                SystemProperties.set("persist.sys.dpd.stop_hr", Integer.toString(mStopTimeHr));
                SystemProperties.set("persist.sys.dpd.stop_min", Integer.toString(mStopTimeMin));
            }
        }
        catch (NullPointerException npe) {
        }
    }

    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
        if (preference == pDMMDPDEnabled) {
            if((pDMMDPDEnabled.isChecked() ? 1 : 0) == 1) {
                pStartTime.setEnabled(true);
                pStopTime.setEnabled(true);
                mDMMDPDEnabled = 1;
            }
            else {
                pStartTime.setEnabled(false);
                pStopTime.setEnabled(false);
                mDMMDPDEnabled = 0;
            }
            SystemProperties.set("persist.sys.dpd", Integer.toString(mDMMDPDEnabled));
        }
        else if(preference == pStartTime) {
            removeDialog(DIALOG_START_TIMEPICKER);
            showDialog(DIALOG_START_TIMEPICKER);
        }
        else if(preference == pStopTime) {
            removeDialog(DIALOG_STOP_TIMEPICKER);
            showDialog(DIALOG_STOP_TIMEPICKER);
        }
        return true;
    }

    private TimePickerDialog.OnTimeSetListener mStartTimeSetListener =
    new TimePickerDialog.OnTimeSetListener() {
        public void onTimeSet(TimePicker view, int hourOfDay, int minute) {
            pStartTime.setSummary(Integer.toString(hourOfDay) + ":"
                                       + Integer.toString(minute));
            mStartTimeHr = hourOfDay;
            mStartTimeMin = minute;
            SystemProperties.set("persist.sys.dpd.start_hr", Integer.toString(mStartTimeHr));
            SystemProperties.set("persist.sys.dpd.start_min", Integer.toString(mStartTimeMin));
        }
    };

    private TimePickerDialog.OnTimeSetListener mStopTimeSetListener =
    new TimePickerDialog.OnTimeSetListener() {
        public void onTimeSet(TimePicker view, int hourOfDay, int minute) {
            pStopTime.setSummary(Integer.toString(hourOfDay) + ":"
                                      + Integer.toString(minute));
            mStopTimeHr = hourOfDay;
            mStopTimeMin = minute;
            SystemProperties.set("persist.sys.dpd.stop_hr", Integer.toString(mStopTimeHr));
            SystemProperties.set("persist.sys.dpd.stop_min", Integer.toString(mStopTimeMin));
        }
    };

    @Override
    public Dialog onCreateDialog(int id) {
        Dialog d;

        switch (id) {
        case DIALOG_START_TIMEPICKER: {
            d = new TimePickerDialog(
                    this,
                    mStartTimeSetListener,
                    mStartTimeHr,
                    mStartTimeMin,
                    true);
            d.setTitle(getResources().getString(R.string.dmm_dpd_start_time));
            break;
        }
        case DIALOG_STOP_TIMEPICKER: {
            d = new TimePickerDialog(
                    this,
                    mStopTimeSetListener,
                    mStopTimeHr,
                    mStopTimeMin,
                    true);
            d.setTitle(getResources().getString(R.string.dmm_dpd_stop_time));
            break;
        }
        default:
            d = null;
            break;
        }

        return d;
    }

    @Override
    public void onPrepareDialog(int id, Dialog d) {
        switch (id) {
        case DIALOG_START_TIMEPICKER: {
            TimePickerDialog startTimePicker = (TimePickerDialog)d;
            startTimePicker.updateTime(
                    mStartTimeHr,
                    mStartTimeMin);
            break;
        }
        case DIALOG_STOP_TIMEPICKER: {
            TimePickerDialog stopTimePicker = (TimePickerDialog)d;
            stopTimePicker.updateTime(
                    mStopTimeHr,
                    mStopTimeMin);
            break;
        }
        default:
            break;
        }
    }

}
