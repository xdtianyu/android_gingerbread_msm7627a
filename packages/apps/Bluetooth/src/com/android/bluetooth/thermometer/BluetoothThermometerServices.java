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

import java.util.UUID;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattService;
import android.content.Intent;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.util.Log;

import com.android.bluetooth.thermometer.ThermoCharTempType.TempType;

public class BluetoothThermometerServices extends Service {

    private static final String TAG = "BluetoothThermometerServices";

    private int mStartId = -1;

    private BluetoothAdapter mAdapter;

    private boolean mHasStarted = false;

    public static UUID GATTServiceUUID = null;

    public static final String USER_DEFINED = "UserDefined";

    public static final String[] StringServicesUUID = {
            "0000180900001000800000805f9b34fb", // Thermo Health
            "0000180900001000800000805f9b34fb", // Thermo Device
            USER_DEFINED };

    public static BluetoothThermometerDevice mDevice;

    public static int THERMOMETER_HEALTH_SERVICE = 0;

    public static int THERMOMETER_DEVICE_SERVICE = 1;

    public static int THERMOMETER_BATTERY_SERVICE = 2;

    public static final int ERROR = Integer.MIN_VALUE;

    public static String[] characteristicsPath = null;

    public static ParcelUuid[] uuidArray = null;

    static final String THERMOMETER_SERVICE_WAKEUP = "android.bluetooth.le.action.THERMOMETER_SERVICE_WAKEUP";

    static final String THERMOMETER_DEVICE = "android.bluetooth.le.action.THERMOMETER_DEVICE";

    static final String THERMOMETER_DEVICE_SERVICE_ON = "android.bluetooth.le.action.THERMOMETER_DEVICE_SERVICE_ON";

    static final String THERMOMETER_BATTERY_SERVICE_ON = "android.bluetooth.le.action.THERMOMETER_BATTERY_SERVICE_ON";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "OnCreate thermometer service");
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        if (!mHasStarted) {
            mHasStarted = true;
            Log.d(TAG, "Starting thermometer service");
            int state = mAdapter.getState();
            if (state == BluetoothAdapter.STATE_ON) {
                Log.d(TAG, "Bluetooth is on");
            }
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        int retCode = 0;
        retCode = super.onStartCommand(intent, flags, startId);
        Log.d(TAG, "onStart thermometer service");
        if (retCode == START_STICKY) {
            mStartId = startId;
            if (mAdapter == null) {
                Log.w(TAG, "Stopping Thremometer service: "
                        + "device does not have BT or device is not ready");
                closeService();

            } else {
                if (intent != null) {
                    parseIntent(intent);
                }
            }
        }
        return retCode;
    }

    private void closeService() {
        // TODO Auto-generated method stub

    }

    private void parseIntent(Intent intent) {
        String action = intent.getStringExtra("action");
        Log.d(TAG, "thermometer service action: " + action);
        if (action.equals(BluetoothThermometerServices.THERMOMETER_SERVICE_WAKEUP)) {
            BluetoothDevice remoteDevice = intent
                    .getParcelableExtra(BluetoothThermometerServices.THERMOMETER_DEVICE);
            Log.d(TAG, "Received BT device selected intent, bt device: "
                    + remoteDevice.getAddress());
            mDevice.BDevice = remoteDevice;
            setGATTServiceUUID();

            boolean gattServices = remoteDevice
                    .getGattServices(BluetoothThermometerServices.GATTServiceUUID);
            Log.d(TAG, "` -----------");
            Log.d(TAG, " Check VALUE OF GATT SERVICES" + gattServices);
        }
    }

    private void configureGattService() {
        mDevice.SelectedServiceUUID = mDevice.ServiceUUIDArray[THERMOMETER_HEALTH_SERVICE];
        Log.d(TAG, " SelectedServiceUUID = " + mDevice.SelectedServiceUUID);
        mDevice.SelectedServiceObjPath = mDevice.ServiceObjPathArray[THERMOMETER_HEALTH_SERVICE];
        Log.d(TAG, " SelectedServiceObjPath = "
                + mDevice.SelectedServiceObjPath);
        BluetoothGattService gattService = new BluetoothGattService(
                mDevice.BDevice, mDevice.SelectedServiceUUID,
                mDevice.SelectedServiceObjPath);
        Log.d(TAG, "gattService.getServiceUuid() ======= "
                + mDevice.gattService.toString());

        mDevice.gattService = gattService;
    }

    private String getServiceName() {
        return mDevice.gattService.getServiceName();
    }

    private void getAllCharacteristics() {
        characteristicsPath = mDevice.gattService.getCharacteristics();
        if (characteristicsPath != null) {
            for (String path : characteristicsPath) {
                Log.d(TAG, " Char Path = " + path);
                Log.d(TAG,
                        " Char value (string) = "
                        + mDevice.gattService
                        .readCharacteristicString(path));
            }
        }

        uuidArray = mDevice.gattService.getCharacteristicUuids();
        if (uuidArray != null) {
            Log.d(TAG, " UUIDs LENGTH ====== " + uuidArray.length);
            for (ParcelUuid uuid : uuidArray) {
                if (uuid != null) {
                    Log.d(TAG, " UUID = " + uuid.toString());
                } else {
                    Log.d(TAG, " UUID = null");
                }
            }
        }

    }

    private ThermoCharTempMeasurement readTemperatureMeasurement(String path) {
        byte[] value = mDevice.gattService.readCharacteristicRaw(path);
        ThermoCharTempMeasurement res = convertValToTempMsr(value);
        return res;
    }

    private ThermoCharDateTime readDateTime(String path) {
        byte[] value = mDevice.gattService.readCharacteristicRaw(path);
        ThermoCharDateTime res = convertValToDateTime(value);
        return res;
    }

    private TempType readTempType(String path) {
        byte[] value = mDevice.gattService.readCharacteristicRaw(path);
        TempType res = convertValToTempType(value);
        return res;
    }

    private ThermoCharMsrInterval readMsrInterval(String path) {
        byte[] value = mDevice.gattService.readCharacteristicRaw(path);
        ThermoCharMsrInterval res = convertValToMsrInterval(value);
        return res;
    }

    private void indicateTempMsr(String path, ThermoCharTempMeasurement val) {
        val.clientConfigDesc = (val.clientConfigDesc | 0x02);
    }

    private void indicateTempInterval(String path, ThermoCharMsrInterval val) {
        val.clientConfigDesc = (val.clientConfigDesc | 0x02);
    }

    private boolean registerCharacteristicWatcher() {
        return mDevice.gattService.registerWatcher();
    }

    private ThermoCharTempMeasurement convertValToTempMsr(byte[] value) {
        // TODO Auto-generated method stub
        return null;
    }

    private ThermoCharDateTime convertValToDateTime(byte[] value) {
        // TODO Auto-generated method stub
        return null;
    }

    private TempType convertValToTempType(byte[] value) {
        // TODO Auto-generated method stub
        return null;
    }

    private ThermoCharMsrInterval convertValToMsrInterval(byte[] value) {
        // TODO Auto-generated method stub
        return null;
    }

    private void setGATTServiceUUID() {
        String uuidString = StringServicesUUID[THERMOMETER_HEALTH_SERVICE];
        GATTServiceUUID = convertUUIDStringToUUID(uuidString);
    }

    private UUID convertUUIDStringToUUID(String UUIDStr) {
        if (UUIDStr.length() != 32) {
            return null;
        }
        String uuidMsB = UUIDStr.substring(0, 16);
        String uuidLsB = UUIDStr.substring(16, 32);

        if (uuidLsB.equals("800000805f9b34fb")) {
            // TODO Long is represented as two complement. Fix this later.
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16), 0x800000805f9b34fbL);
            return uuid;
        } else {
            UUID uuid = new UUID(Long.valueOf(uuidMsB, 16),
                    Long.valueOf(uuidLsB));
            return uuid;
        }
    }

    @Override
    public IBinder onBind(Intent arg0) {
        Log.d(TAG, "onBind thermometer service");
        return null;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy thermometer service");
    }

}

class ThermoCharTempMeasurement {
    public static final String uuid = "0x2A1C";
    public static byte flags;
    public static float tempValue;
    public static ThermoCharTempType tempType;
    public static int clientConfigDesc;
}

class ThermoCharDateTime {
    public static final String uuid = "0x2A08";
    public static int year;
    public static int month;
    public static int day;
    public static int hours;
    public static int minutes;
    public static int seconds;
}

class ThermoCharTempType {
    public static final String uuid = "0x2A1D";

    public static enum TempType {
        ARMPIT, BODY, EAR, FINGER, GASTRO, MOUTH, RECT, TOE, TYMPHANUM
    }
}

class ThermoCharMsrInterval {
    public static final String uuid = "0x2A21";
    public static short msrInterval;
    public static final int rangeMin = 1;
    public static final int rangeMax = 65535;
    public static int clientConfigDesc;
}
