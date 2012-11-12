/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *        * Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials provided
 *           with the distribution.
 *        * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *           contributors may be used to endorse or promote products derived
 *           from this software without specific prior written permission.
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
package com.android.bluetooth.bpp;

import com.android.bluetooth.R;
import com.android.bluetooth.opp.BluetoothOppService;

import android.app.Activity;
import android.os.Bundle;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.content.Context;

/**
 * This activity class shows file transfer progress bar.
 * There is "Cancel" button to cancel current operation.
 */
public class BluetoothBppStatusActivity extends Activity{
    private static final String TAG = "BluetoothBppStatusActivity";
    private static final boolean D = BluetoothBppConstant.DEBUG;
    private static final boolean V = BluetoothBppConstant.VERBOSE;

    public static ProgressBar mTrans_Progress, mPrint_Progress;
    public static TextView mTrans_View, mPrint_View;
    static volatile Context mContext = null;
    BluetoothBppTransfer bf;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        // Menu window only show during the last BPP operation.
        int id = BluetoothOppService.mBppTransId - 1;
        bf = BluetoothOppService.mBppTransfer.get(id);

        mContext = this;
        setContentView(R.layout.print_status);
        mTrans_View = (TextView) findViewById(R.id.transfer_percent);
        mTrans_Progress = (ProgressBar) findViewById(R.id.transfer_status);

        mTrans_View.setText("File Transfer");

        Button cancel = (Button) findViewById(R.id.bpp_cancel_button);

        cancel.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                if (V) Log.v(TAG, "Click'd bpp_cancel_button");

                bf.mSessionHandler.obtainMessage(
                            BluetoothBppTransfer.CANCEL, -1).sendToTarget();
                finish();
            }
        });
    }

    public static void updateProgress(int trans, int print){
        if(V)Log.v(TAG,"trans: " + ((trans*100)/mTrans_Progress.getMax()) + "%");
        mTrans_Progress.setProgress(trans);
    }

    @Override
    protected void onDestroy() {
        if (V) Log.v(TAG, "onDestroy()");
        mContext = null;
        super.onDestroy();
    }

    @Override
    protected void onStop() {
        /* When home button, it will not destroy current Activity context,
                so next time when it comes to the same menu, it will show the previous window.
                To prevent this, it needs to call finish() in onStop().
               */
        if (V) Log.v(TAG, "onStop");
        super.onStop();
        TelephonyManager tm = (TelephonyManager)
            mContext.getSystemService(Context.TELEPHONY_SERVICE);
        if (V) Log.v(TAG, "Call State: " + tm.getCallState());

        if(bf.mForceClose
                ||(tm.getCallState() != TelephonyManager.CALL_STATE_RINGING)) {
            finish();
        }
    }
}
