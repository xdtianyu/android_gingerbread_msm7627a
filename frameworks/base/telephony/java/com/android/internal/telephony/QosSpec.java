/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 *
 */

package com.android.internal.telephony;

import com.android.internal.telephony.RILConstants;
import com.android.internal.telephony.RILConstants.RIL_QosSpecKeys;

import android.os.Parcelable;
import android.os.Parcel;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Set;
import java.util.Map.Entry;
import java.io.File;
import java.io.FileReader;
import java.lang.reflect.Field;

import org.xmlpull.v1.XmlPullParser;

/**
 * A class containing all the QoS parameters
 *
 * This class exposes all the QoS (flow/filter parameters to the higher layers.
 * The various flow/filter parameters will be mapped to the corresponding RIL
 * interface parameters.
 *
 * @hide
 */
public class QosSpec implements Parcelable {
    static final String TAG = "QosSpec";

    public static class QosDirection{
        public static final int QOS_TX = RILConstants.RIL_QosDirection.RIL_QOS_TX;
        public static final int QOS_RX = RILConstants.RIL_QosDirection.RIL_QOS_RX;
    }

    public static class QosClass{
        public static final int CONVERSATIONAL = RILConstants.RIL_QosClass.RIL_QOS_CONVERSATIONAL;
        public static final int STREAMING = RILConstants.RIL_QosClass.RIL_QOS_STREAMING;
        public static final int INTERACTIVE = RILConstants.RIL_QosClass.RIL_QOS_INTERACTIVE;
        public static final int BACKGROUND = RILConstants.RIL_QosClass.RIL_QOS_BACKGROUND;
    }

    /**
     * The set of keys defined for QoS params.
     * A QosSpec object can contain multiple flow/filter parameter set and
     * each group will be identified by a unique index. This enables bundling of
     * many QoS flows in one QoS Spec.
     *
     *  For e.g:
     *  A QoS Spec with one TX flow
     *  SPEC_INDEX=0,FLOW_DIRECTION=0[,FLOW_DATA_RATE_MIN=64000,FLOW_DATA_RATE_MAX=128000, FLOW_LATENCY=100]
     *
     *  A Qos Spec with
     *
     */
    public static class QosSpecKey {
        // Index of a particular spec. This is required to support bundling of
        // multiple QoS specs in one request. [Mandatory]
        public static final int SPEC_INDEX =
                            RIL_QosSpecKeys.RIL_QOS_SPEC_INDEX;

        /* Values from QosDirection [Mandatory] */
        public static final int FLOW_DIRECTION =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_DIRECTION;

        /* Values from QosClass */
        public static final int FLOW_TRAFFIC_CLASS =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_TRAFFIC_CLASS;

        /* Data rate to be specified in bits/sec */
        public static final int FLOW_DATA_RATE_MIN =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_DATA_RATE_MIN;
        public static final int FLOW_DATA_RATE_MAX =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_DATA_RATE_MAX;

        /* Latency to be specified in milliseconds */
        public static final int FLOW_LATENCY =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_LATENCY;

        public static final int FLOW_3GPP2_PROFILE_ID =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_3GPP2_PROFILE_ID;
        public static final int FLOW_3GPP2_PRIORITY =
                            RIL_QosSpecKeys.RIL_QOS_FLOW_3GPP2_PRIORITY;

        /* Values from QosDirection [Mandatory] */
        public static final int FILTER_DIRECTION =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_DIRECTION;

        /* Format: xxx.xxx.xxx.xxx/yy */
        public static final int FILTER_IPV4_SOURCE_ADDR =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV4_SOURCE_ADDR;
        public static final int FILTER_IPV4_DESTINATION_ADDR =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV4_DESTINATION_ADDR;

        /* TOS byte value, Mask is mandatory if TOS value is used.
         * e.g. A mask for one TOS byte should be 255 */
        public static final int FILTER_IPV4_TOS =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV4_TOS;
        public static final int FILTER_IPV4_TOS_MASK =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV4_TOS_MASK;

        /* Port Range is mandatory if port is included. RANGE for a single PORT is 0 */
        public static final int FILTER_TCP_SOURCE_PORT_START =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_TCP_SOURCE_PORT_START;
        public static final int FILTER_TCP_SOURCE_PORT_RANGE =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_TCP_SOURCE_PORT_RANGE;
        public static final int FILTER_TCP_DESTINATION_PORT_START =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_TCP_DESTINATION_PORT_START;
        public static final int FILTER_TCP_DESTINATION_PORT_RANGE =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_TCP_DESTINATION_PORT_RANGE;
        public static final int FILTER_UDP_SOURCE_PORT_START =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_UDP_SOURCE_PORT_START;
        public static final int FILTER_UDP_SOURCE_PORT_RANGE =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_UDP_SOURCE_PORT_RANGE;
        public static final int FILTER_UDP_DESTINATION_PORT_START =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_UDP_DESTINATION_PORT_START;
        public static final int FILTER_UDP_DESTINATION_PORT_RANGE =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_UDP_DESTINATION_PORT_RANGE;

        /* Format: xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx/yyy */
        public static final int FILTER_IPV6_SOURCE_ADDR =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV6_SOURCE_ADDR;
        public static final int FILTER_IPV6_DESTINATION_ADDR =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV6_DESTINATION_ADDR;
        public static final int FILTER_IPV6_TRAFFIC_CLASS =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IPV6_TRAFFIC_CLASS;
        public static final int FILTER_IPV6_FLOW_LABEL =
                            RIL_QosSpecKeys.RIL_QOS_FILTER_IP_NEXT_HEADER_PROTOCOL;

        public static boolean isValid(int key) {
            boolean retVal = false;
            try {
                for (Field k : QosSpecKey.class.getFields()) {
                    if (k.getInt(k) == key)
                        return true;
                }
            } catch(java.lang.IllegalAccessException e) {
                retVal = false;
            }
            return retVal;
        }

        public static int getKey(String key) throws IllegalArgumentException {
            try {
                for (Field k : QosSpecKey.class.getFields()) {
                    if (k.getName().equals(key))
                        return k.getInt(k);
                }
            } catch(java.lang.IllegalAccessException e) {
                throw new IllegalArgumentException();
            }
            return 0;
        }

        public static String getKeyName(int key) {
            String retVal = null;
            try {
                for (Field k : QosSpecKey.class.getFields()) {
                    if (k.getInt(k) == key)
                        return k.getName();
                }
            } catch(java.lang.IllegalAccessException e) {
                Log.w(TAG, "Warning: Invalid key:" + key);
            }
            return retVal;
        }
    }

    /* Overall status of a QoS specification */
    public static class QosStatus {
        public static final int NONE =
                            RILConstants.RIL_QosStatus.RIL_QOS_STATUS_NONE;
        public static final int ACTIVATED =
                            RILConstants.RIL_QosStatus.RIL_QOS_STATUS_ACTIVATED;
        public static final int SUSPENDED =
                            RILConstants.RIL_QosStatus.RIL_QOS_STATUS_SUSPENDED;
    }

    /* Values of QoS Indication Status */
    public static class QosIndStates {
        public static final int INITIATED = 0;
        public static final int NEGOTIATED = 1;
        public static final int ACTIVATED = 2;
        public static final int RELEASING = 3;
        public static final int RELEASED = 4;
        public static final int RELEASED_NETWORK = 5;
        public static final int MODIFIED = 6;
        public static final int MODIFYING = 7;
        public static final int MODIFIED_NETWORK = 8;
        public static final int SUSPENDED = 9;
        public static final int SUSPENDING = 10;
        public static final int SUSPENDED_NETWORK = 11;
        public static final int RESUMED = 12;
        public static final int RESUMED_NETWORK = 13;
        public static final int REQUEST_FAILED = 14;
        public static final int NONE = 15;
    }

    public static class QosIntentKeys {
        /* Status of the QoS operation from QosIndStates. Type of Value: Integer */
        public static final String QOS_INDICATION_STATE = "QosIndicationState";

        /* Status of the QoS Flow (identified by a QoS ID) from QosStatus */
        public static final String QOS_STATUS = "QosStatus";

        /* String error returned from the modem for any failure.
         * Optional param. Type of Value: Integer */
        public static final String QOS_ERROR = "QosError";

        /* Transaction ID that is echoed back to the higher layers.
         * Used only for IND following setup qos request. Type of Value: Integer */
        public static final String QOS_TRANSID = "QosTransId";

        /* Unique QoS ID. Type of Value: Integer */
        public static final String QOS_ID = "QosId";

        /* Parcelable QosSpec object. Used in the indication following getQosStatus */
        public static final String QOS_SPEC = "QosSpec";
    }

    LinkedHashMap<Integer, QosPipe> mQosPipes;
    private static int mPipeId = 0;


    public class QosPipe {

        /** The Map of Keys to Values */
        LinkedHashMap<Integer, String> mQosParams;

        public QosPipe() {
            mQosParams = new LinkedHashMap<Integer, String>();
        }

        /**
         * Remove all capabilities
         */
        public void clear() {
            mQosParams.clear();
        }

        /**
         * Returns whether this map is empty.
         */
        public boolean isEmpty() {
            return mQosParams.isEmpty();
        }

        /**
         * Returns the number of elements in this map.
         *
         * @return the number of elements in this map.
         */
        public int size() {
            return mQosParams.size();
        }

        /**
         * Returns the value of the capability string with the specified key.
         *
         * @param key
         * @return the value of the QoS Param
         * or {@code null} if no mapping for the specified key is found.
         */
        public String get(int key) {
            return mQosParams.get(key);
        }

        /**
         * Store the key/value capability pair.
         *
         * @param key
         * @param value
         * @throws IllegalArgumentException if QosParams does not recognize the key:value pair
         */
        public void put(int key, String value) {

            // check to make sure input is valid, otherwise ignore
            if (QosSpec.QosSpecKey.isValid(key) == false) {
                Log.d(QosSpec.TAG, "Ignoring invalid key:" + key);
                throw new IllegalArgumentException("Invalid Key:" + key);
            }

            mQosParams.put(key, value);
        }

        private String getRilPipeSpec() {
            String rilPipeSpec = "";
            String keyValue = "";

            Set<Integer> qosKeys = mQosParams.keySet();

            for (int i : (Integer[])qosKeys.toArray(new Integer[0])) {
                keyValue = RILConstants.RIL_QosSpecKeys.getName(i)
                                 + "=" + mQosParams.get(i);

                rilPipeSpec += keyValue + ",";
            }

            // Remove last comma
            rilPipeSpec = rilPipeSpec.substring(0, rilPipeSpec.length() - 1);

            return rilPipeSpec;
        }

        /**
         * Convert to string for debugging
         */
        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("{");
            boolean firstTime = true;
            for (Entry<Integer, String> entry : mQosParams.entrySet()) {
                if (firstTime) {
                    firstTime = false;
                } else {
                    sb.append(", ");
                }
                sb.append(entry.getKey().toString());
                sb.append(":\"");
                sb.append(entry.getValue());
                sb.append("\"");
            }
            sb.append("}");
            return sb.toString();
        }
    }


    public QosSpec() {
        mQosPipes = new LinkedHashMap<Integer, QosPipe>();
    }

    public void clear() {
        for(QosPipe pipe : mQosPipes.values()) {
            pipe.clear();
        }
        mQosPipes.clear();
    }

    public boolean isValid(int pipeId) {
        return mQosPipes.containsKey(pipeId);
    }

    public QosPipe createPipe() {
        int pipeId = mPipeId++;

        QosPipe pipe = new QosPipe();
        mQosPipes.put(pipeId, pipe);
        return pipe;
    }

    public QosPipe createPipe(String flowFilterSpec) {
        int pipeId = mPipeId++;

        QosPipe pipe = null;

        if (flowFilterSpec == null) {
            return pipe;
        }

        pipe = new QosPipe();
        mQosPipes.put(pipeId, pipe);

        //Parse the flow/filter spec and add it to pipe
        String keyvalues[] = flowFilterSpec.split(",");
        String kvpair[] = null;
        String keyStr = null;
        String value = null;

        int key;
        for (String kv: keyvalues) {
            try {
                kvpair = kv.split("=");
                keyStr = kvpair[0];
                value = kvpair[1];
                key = RILConstants.RIL_QosSpecKeys.class.getField(keyStr).getInt(
                                                 RILConstants.RIL_QosSpecKeys.class);
                pipe.put(key, value);
            } catch (Throwable t) {
                Log.e(TAG, "Warning: Invalid key:" + keyStr);
            }
        }
        return pipe;
    }

    public Collection<QosPipe> getQosPipes() {
        return mQosPipes.values();
    }

    /* Search for the QosPipe with spec index */
    public QosPipe getQosPipes(String specIndex) {
        for (QosPipe pipe : mQosPipes.values()) {
            if (pipe.get(QosSpec.QosSpecKey.SPEC_INDEX).equals(specIndex))
                return pipe;
        }
        return null;
    }

    public String getQosSpec(int pipeId, int key) {
        String value = null;
        if (isValid(pipeId));
            value = mQosPipes.get(pipeId).get(key);
        return value;
    }

    public Set<Entry<Integer, String>> entrySet(int pipeId) {
        return isValid(pipeId) ? mQosPipes.get(pipeId).mQosParams.entrySet() : null;
    }

    public Set<Integer> pipeKeySet(int pipeId) {
        return isValid(pipeId) ? mQosPipes.get(pipeId).mQosParams.keySet() : null;
    }

    public Collection<String> pipeValues(int pipeId) {
        return isValid(pipeId) ? mQosPipes.get(pipeId).mQosParams.values() : null;
    }

    public int pipeSize(int pipeId) {
        int size = 0;
        if (isValid(pipeId))
            size = mQosPipes.get(pipeId).mQosParams.size();
        else
            Log.e(TAG, "Warning: Invalid pipeId:" + pipeId);
        return size;
    }

    public boolean isEmpty(int pipeId) {
        boolean flag = false;
        if (isValid(pipeId))
            flag = mQosPipes.get(pipeId).mQosParams.isEmpty();
        else
            Log.e(TAG, "Warning: Invalid pipeId:" + pipeId);
        return flag;
    }


    public boolean containsKey(int pipeId, QosSpecKey key) {
        boolean flag = false;
        if (isValid(pipeId))
            flag = mQosPipes.get(pipeId).mQosParams.containsKey(key);
        else
            Log.e(TAG, "Warning: Invalid pipeId:" + pipeId);
        return flag;
    }


    public boolean containsValue(int pipeId, String value) {
        boolean flag = false;
        if (isValid(pipeId))
            flag = mQosPipes.get(pipeId).mQosParams.containsValue(value);
        else
            Log.e(TAG, "Warning: Invalid pipeId:" + pipeId);
        return flag;
    }

    public ArrayList<String> getRilQosSpec() {
        ArrayList<String> rilQosSpec = new ArrayList<String>();
        for (QosPipe pipe : mQosPipes.values()) {
            rilQosSpec.add(pipe.getRilPipeSpec());
        }
        return rilQosSpec;
    }

    /**
     * Implement the Parcelable interface
     *
     * @hide
     */
    public int describeContents() {
        return 0;
    }

    /**
     * Implement the Parcelable interface.
     * @hide
     */
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mQosPipes.size());
        for(QosPipe pipe : mQosPipes.values()) {
            dest.writeInt(pipe.mQosParams.entrySet().size());

            for (Entry<Integer, String> entry : pipe.mQosParams.entrySet()) {
                dest.writeInt(entry.getKey());
                dest.writeString(entry.getValue());
            }
        }
    }

    /**
     * Implement the Parcelable interface.
     *
     * @hide
     */
    public static final Creator<QosSpec> CREATOR = new Creator<QosSpec>() {
        public QosSpec createFromParcel(Parcel in) {
            QosSpec qosSpec = new QosSpec();
            int nPipes = in.readInt();
            while (nPipes-- != 0) {
                int mapSize = in.readInt();
                QosPipe pipe = qosSpec.createPipe();
                while (mapSize-- != 0) {
                    int key = in.readInt();
                    String value = in.readString();
                    pipe.put(key, value);
                }
            }
            return qosSpec;
        }

        /**
         * Required function for implementing Parcelable interface
         */
        public QosSpec[] newArray(int size) {
            return new QosSpec[size];
        }
    };

    /**
     * Debug logging
     */
    protected static void log(String s) {
        Log.d(TAG, s);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("{");
        for (Entry<Integer, QosPipe> entry : mQosPipes.entrySet()) {
            sb.append(entry.toString());
        }
        sb.append("}");
        return sb.toString();
    }
}

