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

import android.content.Context;
import android.content.SharedPreferences;
import android.telephony.SmsManager;
import android.telephony.TelephonyManager;

/*
 * Control gsm broadcast configuration using this class.
 * As of now there is support for message_id 50 only.
 * This can be extended to support any number of message_ids.
 */
public class GsmBroadcastConfigurator {
    private static GsmBroadcastConfigurator sInstance = null;
    int phoneCount = TelephonyManager.getPhoneCount();
    boolean mConfig[][] = new boolean[phoneCount][1];
    private Context mC;

    // Message IDs.
    private static final int AREA_INFO_MSG_ID = 50;

    // String keys for shared preference lookup
    private static final String SP_FILE_NAME = "GsmUmtsSharedPref";
    private static final String SP_KEY = "ch50_";

    private GsmBroadcastConfigurator(Context c) {
        mC = c;

        // Read saved info from shared preference.
        SharedPreferences sp = mC.getSharedPreferences(SP_FILE_NAME, 0);
        for (int i = 0; i < phoneCount; i++) {
            String spKey = SP_KEY + i;
            mConfig[i][0] = sp.getBoolean(spKey, false);
        }
    }

    public static GsmBroadcastConfigurator getInstance(Context c) {
        if (sInstance == null && c != null) {
            sInstance = new GsmBroadcastConfigurator(c);
        }
        return sInstance;
    }

    public boolean switchService(boolean newStateIsOn, int subscription) {
        boolean ret = true;

        if (mConfig[subscription][0] != newStateIsOn) {
            ret = smsManagerSwitchService(newStateIsOn, subscription);
            if (ret) {
                mConfig[subscription][0] = newStateIsOn;

                SharedPreferences sp = mC.getSharedPreferences(SP_FILE_NAME, 0);
                SharedPreferences.Editor spe = sp.edit();
                String spKey = SP_KEY + subscription;
                spe.putBoolean(spKey, newStateIsOn);
                spe.commit();
            }
        }

        return ret;
    }

    private boolean smsManagerSwitchService(boolean newStateIsOn, int subscription) {
        SmsManager sm = SmsManager.getDefault();
        if (newStateIsOn) {
            return sm.enableCellBroadcastOnSubscription(AREA_INFO_MSG_ID, subscription);
        } else {
            return sm.disableCellBroadcastOnSubscription(AREA_INFO_MSG_ID, subscription);
        }
    }

    public void configStoredValue(int subscription) {
        // If the message is enabled in the previous power cycle configure it.
        if (mConfig[subscription][0]) {
            if (!smsManagerSwitchService(mConfig[subscription][0], subscription)) {
                // If enabling message fails reset the status.
                mConfig[subscription][0] = false;
            }
        }
    }

    public boolean getMessageStatus(int subscription) {
        return mConfig[subscription][0];
    }

    public static boolean isMsgIdSupported(int msgId) {
        if (msgId == AREA_INFO_MSG_ID) {
            return true;
        }

        return false;
    }
}
