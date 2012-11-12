/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

package android.bluetooth;

import android.os.ParcelUuid;
import android.os.RemoteException;
import android.util.Log;

import java.io.IOException;

import java.util.UUID;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Public API for controlling the Bluetooth GATT based services.
 *
 * @hide
 */

public class BluetoothGattService {
    private static final String TAG = "BluetoothGattService";
    private ParcelUuid mUuid;
    private String mObjPath;
    private BluetoothDevice mDevice;
    private String mName = null;
    private boolean watcherRegistered = false;

    private final HashMap<String, Map<String, String>> mCharacteristicProperties;
    private String[] characteristicPaths = null;
    private boolean discoveryDone = false;

    private final CharacteristicHelper mHelper;

    public BluetoothGattService(BluetoothDevice device, ParcelUuid uuid, String path) {
        mDevice = device;
        mUuid = uuid;
        mObjPath = path;
        mName = getServiceName();

        mCharacteristicProperties = new HashMap<String, Map<String, String>>();
        mHelper = new CharacteristicHelper();

        // Start characteristic discovery right away
        mHelper.doDiscovery();
    }

    public ParcelUuid getServiceUuid() {
        return mUuid;
    }

    public String getServiceName() {

        if (mName != null)
            return  mName;

            IBluetooth service = BluetoothDevice.getService();
            try {
                return service.getGattServiceName(mObjPath);
            } catch (RemoteException e) {Log.e(TAG, "", e);}

            return null;
    }


    public String[] getCharacteristics() {
        if (!discoveryDone)
            mHelper.waitDiscoveryDone();
        return characteristicPaths;
    }

    public ParcelUuid[] getCharacteristicUuids() {

        ArrayList<ParcelUuid>  uuidList = new ArrayList<ParcelUuid>();

        if (!discoveryDone)
            mHelper.waitDiscoveryDone();

        if (characteristicPaths == null)
            return null;

        int count  = characteristicPaths.length;

        for(int i = 0; i< count; i++) {

            String value = getCharacteristicProperty(characteristicPaths[i], "UUID");

            if (value != null)
                uuidList.add(ParcelUuid.fromString(value));

            Log.d (TAG, "Characteristic UUID: " + value);

        }

        ParcelUuid[] uuids = new ParcelUuid[count];

        uuidList.toArray(uuids);

        return uuids;

    }

   public ParcelUuid getCharacteristicUuid(String path) {

       ParcelUuid uuid = null;

        if (!discoveryDone)
            mHelper.waitDiscoveryDone();

        String value = getCharacteristicProperty(path, "UUID");

        if (value != null) {
                uuid = ParcelUuid.fromString(value);

                Log.d (TAG, "Characteristic UUID: " + value);
        }
        return uuid;
    }

    public String getCharacteristicDescription(String path) {
        if (!discoveryDone)
            mHelper.waitDiscoveryDone();
        return getCharacteristicProperty(path, "Description");

    }

     public byte[] readCharacteristicRaw(String path)
    {
        if (!discoveryDone)
            mHelper.waitDiscoveryDone();

        if (characteristicPaths == null)
            return null;

        String value = getCharacteristicProperty(path, "Value");

        byte[] ret = value.getBytes();

        return ret;
    }

    public String readCharacteristicString(String path)
    {
        if (!discoveryDone)
            mHelper.waitDiscoveryDone();

        if (characteristicPaths == null)
            return null;

        String value = (String) getCharacteristicProperty(path, "Representation");

        return value;
    }

    public boolean registerWatcher() {
        if (watcherRegistered == false) {
            watcherRegistered = mHelper.registerCharacteristicsWatcher();
            return watcherRegistered;
        } else {
            return true;
        }
    }

    public boolean deregisterWatcher() {
        if (watcherRegistered == true) {
            watcherRegistered = false;
            return mHelper.deregisterCharacteristicsWatcher();
        }
        return true;
    }

    private String getCharacteristicProperty(String path, String property) {

        Map<String, String> properties = mCharacteristicProperties.get(path);

        if (properties != null)
            return properties.get(property);

        return null;
    }

    private void addCharacteristicProperties(String path, String[] properties) {
        Map<String, String> propertyValues = mCharacteristicProperties.get(path);
        if (propertyValues == null) {
            propertyValues = new HashMap<String, String>();
        }

        for (int i = 0; i < properties.length; i++) {
            String name = properties[i];
            String newValue = null;
            int len;
            if (name == null) {
                Log.e(TAG, "Error: Gatt Characterisitc Property at index" + i + "is null");
                continue;
            }

            newValue = properties[++i];

            propertyValues.put(name, newValue);
        }

        mCharacteristicProperties.put(path, propertyValues);
    }


    /**
     * Helper to perform Characteristic discovery
     */
    private class CharacteristicHelper extends IBluetoothGattService.Stub {
        private final IBluetooth mService;

        CharacteristicHelper() {
            mService = BluetoothDevice.getService();
        }

        /**
         * Throws IOException on failure.
         */
        public boolean doDiscovery() {

            Log.d(TAG, "doDiscovery " + mObjPath);

            try {
                return mService.discoverCharacteristics(mObjPath, this);
            } catch (RemoteException e) {Log.e(TAG, "", e);}

            return false;

        }

        public synchronized void onCharacteristicsDiscovered(String[] paths)
        {
            Log.d(TAG, "onCharacteristicsDiscovered: " + paths);
            if (paths == null) return;
            int count = paths.length;
            Log.d(TAG, "Discovered  " + count + " characteristics for service " + mObjPath + " ( " + mName + " )");

            characteristicPaths = paths;

            for (int i = 0; i < count; i++) {

                String[] properties = null;

                try {
                    properties = mService.getCharacteristicProperties(paths[i]);
                } catch (RemoteException e) {Log.e(TAG, "", e);}

                if (properties != null) {
                    addCharacteristicProperties(paths[i], properties);
                }

            }

            discoveryDone = true;

            this.notify();
        }

        public synchronized boolean registerCharacteristicsWatcher() {
            Log.d(TAG, "registerCharacteristicsWatcher: ");

            try {
                return mService.registerCharacteristicsWatcher(mObjPath);
            } catch (RemoteException e) {Log.e(TAG, "", e);}

            return false;
        }

        public synchronized boolean deregisterCharacteristicsWatcher() {
            Log.d(TAG, "registerCharacteristicsWatcher: ");

            try {
                return mService.deregisterCharacteristicsWatcher(mObjPath);
            } catch (RemoteException e) {Log.e(TAG, "", e);}

            return false;
        }

        public synchronized void waitDiscoveryDone()
        {
            try {
                this.wait(5000);
            } catch (InterruptedException e) { /* Ignore this exception */}
        }

    }

}
