/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of Code Aurora nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth.thermometer;

import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.ParcelUuid;
import android.util.Log;
import android.widget.Toast;

public class BluetoothThermometerReceiver extends BroadcastReceiver {

    private final String TAG = "BluetoothThermometerReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        Intent in = new Intent();
        in.putExtras(intent);
        in.setClass(context, BluetoothThermometerServices.class);
        in.putExtra("action", action);

        if (action
                .equalsIgnoreCase((BluetoothThermometerServices.THERMOMETER_SERVICE_WAKEUP))) {
            BluetoothDevice remoteDevice = intent
                    .getParcelableExtra(BluetoothThermometerServices.THERMOMETER_DEVICE);
            String deviceName = remoteDevice.getName();
            Log.d(TAG, "Received BT device selected intent, bt device: "
                    + remoteDevice.getAddress());
            String toastMsg;
            toastMsg = " The user selected the device named " + deviceName;
            Toast.makeText(context, toastMsg, Toast.LENGTH_SHORT).show();
            context.startService(in);
        } else if (action.equals(BluetoothDevice.ACTION_GATT)) {

            Log.d(TAG,
                    " ACTION GATT INTENT RECVD as a result of gatGattService");
            BluetoothDevice remoteDevice = intent
                    .getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            Log.d(TAG, "Remote Device: " + remoteDevice.getAddress());

            ParcelUuid uuid = (ParcelUuid) intent
                    .getExtra(BluetoothDevice.EXTRA_UUID);
            Log.d(TAG, " UUID: " + uuid);

            BluetoothThermometerServices.mDevice.BDevice = remoteDevice;

            String[] ObjectPathArray = (String[]) intent
                    .getExtra(BluetoothDevice.EXTRA_GATT);
            BluetoothThermometerServices.mDevice.ServiceObjPathArray = ObjectPathArray;

            if (ObjectPathArray == null) {
                Log.d(TAG, " +++  ERROR NO OBJECT PATH HANDLE FOUND +++++++");
                String deviceName = remoteDevice.getName();
                String toastMsg;
                toastMsg = " ERROR: The user selected the device named "
                        + deviceName + " doesnt have the service ";
                Toast.makeText(context, toastMsg, Toast.LENGTH_SHORT).show();
                return;
            } else {

                BluetoothThermometerServices.mDevice.ServiceUUIDArray = new ParcelUuid[ObjectPathArray.length];

                Log.d(TAG, " Object Path (length): " + ObjectPathArray.length);
                for (int i = 0; i < ObjectPathArray.length; i++) {
                    Log.d(TAG, " Object Path at " + i + ": "
                            + ObjectPathArray[i]);
                    BluetoothThermometerServices.mDevice.ServiceUUIDArray[i] = uuid;
                }
            }
        }
    }
}
