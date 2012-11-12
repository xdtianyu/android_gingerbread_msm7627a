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

package com.android.mms.transaction;

import com.android.internal.telephony.RILConstants;

import android.content.Context;
import android.content.SharedPreferences;
import android.telephony.SmsManager;

/**
 * Control cdma broadcast configuration using this class
 */
public class CdmaBroadcastConfigurator {
    private static final int NO_OF_SERVICE_CATEGORIES = 32;
    private static CdmaBroadcastConfigurator mInstance = null;
    private Context mC;
    private boolean mIsCdmaBroadcastAllowed;
    // Index is service category (0..31)
    private boolean[] mConfig = new boolean[NO_OF_SERVICE_CATEGORIES];

    private static final String PREFERENCE_FILE_NAME = "CdmaBroadcastConfig";
    private static final String SP_KEY = "cdma_broadcast_";

    private CdmaBroadcastConfigurator (Context c) {
        mC = c;
        mIsCdmaBroadcastAllowed = android.provider.Settings.Secure.getInt(
                mC.getContentResolver(),
                android.provider.Settings.Secure.CDMA_CELL_BROADCAST_SMS,
                RILConstants.CDMA_CELL_BROADCAST_SMS_DISABLED)
                == RILConstants.CDMA_CELL_BROADCAST_SMS_ENABLED;
        // Restore preferences
        SharedPreferences sp = c.getSharedPreferences(PREFERENCE_FILE_NAME, 0);
        for (int i = 0; i < NO_OF_SERVICE_CATEGORIES; i++) {
            String prefName = SP_KEY + i;
            switchService(i, sp.getBoolean(prefName, true)); //All on by default
        }
    }

    public static CdmaBroadcastConfigurator getInstance(Context c) {
        if (mInstance == null && c != null) {
            mInstance = new CdmaBroadcastConfigurator(c);
        }
        return mInstance;
    }

    public void switchService(int serviceCategory, boolean newStateIsOn) {
        if (mConfig[serviceCategory] != newStateIsOn) {
            smsManagerSwitchService(serviceCategory, newStateIsOn);
            mConfig[serviceCategory] = newStateIsOn;

            SharedPreferences sp = mC.getSharedPreferences(PREFERENCE_FILE_NAME, 0);
            SharedPreferences.Editor spe = sp.edit();
            String prefName = SP_KEY + serviceCategory;
            spe.putBoolean(prefName, newStateIsOn);
            spe.commit();
        }
    }

    public boolean isServiceOn(int serviceCategory) {
        // Note that this disregards global flag
        return mConfig[serviceCategory];
    }

    public boolean isCdmaBroadcastAllowed() {
        return mIsCdmaBroadcastAllowed;
    }

    public void enableCdmaBroadcast(boolean isNewStateOn) {
        if (mIsCdmaBroadcastAllowed != isNewStateOn) {
            mIsCdmaBroadcastAllowed = isNewStateOn;
            // Just update SmsManager config. No need to touch SharedPreferences
            updateFullSmsManagerConfig();
        }
    }

    private void smsManagerSwitchService(int serviceCategory, boolean newStateIsOn) {
        SmsManager sm = SmsManager.getDefault();
        if (newStateIsOn && mIsCdmaBroadcastAllowed) {
            sm.enableCdmaBroadcast(serviceCategory);
        } else {
            sm.disableCdmaBroadcast(serviceCategory);
        }
    }

    private void updateFullSmsManagerConfig() {
        for (int i = 0; i < NO_OF_SERVICE_CATEGORIES; i++) {
            smsManagerSwitchService(i, mConfig[i]);
        }
    }
}