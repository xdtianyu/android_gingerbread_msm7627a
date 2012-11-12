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
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.mms.transaction;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;
import android.util.Log;

public class BroadcastConfigInstantiator extends BroadcastReceiver {
    private static final String LOG_TAG = "BroadcastConfigInstantiator";
    int mPhoneCount =  TelephonyManager.getPhoneCount();
    private ServiceStateListener[] mServiceStateListener = new ServiceStateListener[mPhoneCount];
    private Context mContext;
    private int[] mServiceState = new int[mPhoneCount];

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(LOG_TAG, "Received " + intent);

        if (!intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            Log.e(LOG_TAG, "Wrong intent. Ignore");
            return;
        }

        mContext = context;
        TelephonyManager tm = (TelephonyManager)context.getSystemService(Context.TELEPHONY_SERVICE);

        for (int i = 0; i < mPhoneCount; i++) {
            Log.d(LOG_TAG, "Sub: " + i + ", registering for ServiceState updates.");
            mServiceState[i] = -1;
            mServiceStateListener[i] = new ServiceStateListener(i);
            tm.listen(mServiceStateListener[i], PhoneStateListener.LISTEN_SERVICE_STATE);
        }
    }

    private class ServiceStateListener extends PhoneStateListener {
        int mSubscription;

        ServiceStateListener(int subscription) {
            mSubscription = subscription;
        }

        @Override
        public void onServiceStateChanged(ServiceState ss) {
            if (ss.getState() != mServiceState[mSubscription]) {
                Log.d(LOG_TAG, "Sub: " + mSubscription + ", service state changed! " +
                      ss.getState() + " Full:  " + ss);
                if (ss.getState() == ServiceState.STATE_IN_SERVICE ||
                    ss.getState() == ServiceState.STATE_EMERGENCY_ONLY) {
                    TelephonyManager tm = (TelephonyManager)mContext.getSystemService(
                          Context.TELEPHONY_SERVICE);
                    if (tm.getPhoneType(mSubscription) == TelephonyManager.PHONE_TYPE_GSM) {
                        Log.d(LOG_TAG, "Instantiating GSM CB configurator on sub: " +
                                mSubscription);
                        GsmBroadcastConfigurator gbc = GsmBroadcastConfigurator.getInstance(mContext);
                        Log.d(LOG_TAG, "Configure MsgId 50 from SP on sub: " + mSubscription);
                        gbc.configStoredValue(mSubscription);
                    } else {
                        Log.d(LOG_TAG, "Instantiating CDMA CB configurator on sub: " +
                                mSubscription);
                        // Instantiating CdmaBroadcastConfigurator triggers setting of cdma bc config
                        CdmaBroadcastConfigurator cc = CdmaBroadcastConfigurator.getInstance(mContext);
                    }

                    // Unregister
                    tm.listen(mServiceStateListener[mSubscription],
                          PhoneStateListener.LISTEN_NONE);
                }
                mServiceState[mSubscription] = ss.getState();
            }
        }
   }
}
