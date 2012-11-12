/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

package com.android.bluetooth.map;



import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.CursorJoiner;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.Parcelable;
import android.os.Process;
import android.text.format.Time;
import android.util.Log;

import com.android.bluetooth.map.MapUtils.MapUtils;
import com.android.bluetooth.map.MapUtils.EmailUtils;
import com.android.bluetooth.map.MapUtils.SmsMmsUtils;

import javax.obex.*;

/**
 * This class run an MNS session.
 */
public class BluetoothMns {
    private static final String TAG = "BtMns";

    private static final boolean D = BluetoothMasService.DEBUG;

    private static final boolean V = BluetoothMasService.VERBOSE;

    public static final int RFCOMM_ERROR = 10;

    public static final int RFCOMM_CONNECTED = 11;

    public static final int SDP_RESULT = 12;

    public static final int MNS_CONNECT = 13;

    public static final int MNS_DISCONNECT = 14;

    public static final int MNS_SEND_EVENT = 15;

    private static final int CONNECT_WAIT_TIMEOUT = 45000;

    private static final int CONNECT_RETRY_TIME = 100;

    private static final short MNS_UUID16 = 0x1133;

    public static final String NEW_MESSAGE = "NewMessage";

    public static final String DELIVERY_SUCCESS = "DeliverySuccess";

    public static final String SENDING_SUCCESS = "SendingSuccess";

    public static final String DELIVERY_FAILURE = "DeliveryFailure";

    public static final String SENDING_FAILURE = "SendingFailure";

    public static final String MEMORY_FULL = "MemoryFull";

    public static final String MEMORY_AVAILABLE = "MemoryAvailable";

    public static final String MESSAGE_DELETED = "MessageDeleted";

    public static final String MESSAGE_SHIFT = "MessageShift";

    public static final int MMS_HDLR_CONSTANT = 100000;

    public static final int EMAIL_HDLR_CONSTANT = 200000;

    private static final int MSG_CP_INBOX_TYPE = 1;

    private static final int MSG_CP_SENT_TYPE = 2;

    private static final int MSG_CP_DRAFT_TYPE = 3;

    private static final int MSG_CP_OUTBOX_TYPE = 4;

    private static final int MSG_CP_FAILED_TYPE = 5;

    private static final int MSG_CP_QUEUED_TYPE = 6;

    private static final int MSG_META_DATA_TYPE = 130;

    private static final int MSG_DELIVERY_RPT_TYPE = 134;

    private Context mContext;

    private BluetoothAdapter mAdapter;

    private BluetoothMnsObexSession mSession;

    private int mStartId = -1;

    private ObexTransport mTransport;

    private HandlerThread mHandlerThread;

    private EventHandler mSessionHandler;

    private BluetoothDevice mDestination;

    private MapUtils mu = null;

    public static final ParcelUuid BluetoothUuid_ObexMns = ParcelUuid
            .fromString("00001133-0000-1000-8000-00805F9B34FB");

    private long mTimestamp;

    public String deletedFolderName = null;
    private EmailFolderContentObserverClass[] arrObj;
    private SmsMmsFolderContentObserverClass[] arrObjSmsMms;

    List<String> folderList;
    List<String> folderListSmsMms;
    public BluetoothMns(Context context) {
        /* check Bluetooth enable status */
        /*
         * normally it's impossible to reach here if BT is disabled. Just check
         * for safety
         */

        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mContext = context;

        mDestination = BluetoothMasService.mRemoteDevice;

        mu = new MapUtils();

        if (!mAdapter.isEnabled()) {
            Log.e(TAG, "Can't send event when Bluetooth is disabled ");
            return;
        }

        if (mHandlerThread == null) {
            if (V) Log.v(TAG, "Create handler thread for batch ");
            mHandlerThread = new HandlerThread("Bt MNS Transfer Handler",
                    Process.THREAD_PRIORITY_BACKGROUND);
            mHandlerThread.start();
            mSessionHandler = new EventHandler(mHandlerThread.getLooper());
        }
        SmsMmsUtils smu = new SmsMmsUtils();
        folderListSmsMms = new ArrayList<String>();
        folderListSmsMms = smu.folderListSmsMmsMns(folderListSmsMms);
        arrObjSmsMms = new SmsMmsFolderContentObserverClass[folderListSmsMms.size()];

        EmailUtils eu = new EmailUtils();
        folderList = eu.folderListMns(mContext);
        arrObj = new EmailFolderContentObserverClass[folderList.size()];
    }

    public Handler getHandler() {
        return mSessionHandler;
    }

    /*
     * Receives events from mConnectThread & mSession back in the main thread.
     */
    private class EventHandler extends Handler {
        public EventHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, " Handle Message " + msg.what);
            }
            switch (msg.what) {
            case MNS_CONNECT:
                if (mSession != null) {
                    if (Log.isLoggable(TAG, Log.VERBOSE)){
                        Log.v(TAG, "Disconnect previous obex connection");
                    }
                    mSession.disconnect();
                    mSession = null;
                }
                start((BluetoothDevice) msg.obj);
                break;
            case MNS_DISCONNECT:
                deregisterUpdates();
                stop();
                break;
            case SDP_RESULT:
                if (V) Log.v(TAG, "SDP request returned " + msg.arg1
                            + " (" + (System.currentTimeMillis() - mTimestamp + " ms)"));
                if (!((BluetoothDevice) msg.obj).equals(mDestination)) {
                    return;
                }
                try {
                    mContext.unregisterReceiver(mReceiver);
                } catch (IllegalArgumentException e) {
                    // ignore
                }
                if (msg.arg1 > 0) {
                    mConnectThread = new SocketConnectThread(mDestination,
                            msg.arg1);
                    mConnectThread.start();
                } else {
                    /* SDP query fail case */
                    Log.e(TAG, "SDP query failed!");
                }

                break;

            /*
             * RFCOMM connect fail is for outbound share only! Mark batch
             * failed, and all shares in batch failed
             */
            case RFCOMM_ERROR:
                if (V) Log.v(TAG, "receive RFCOMM_ERROR msg");
                mConnectThread = null;

                break;
            /*
             * RFCOMM connected. Do an OBEX connect by starting the session
             */
            case RFCOMM_CONNECTED:
                if (V) Log.v(TAG, "Transfer receive RFCOMM_CONNECTED msg");
                mConnectThread = null;
                mTransport = (ObexTransport) msg.obj;
                startObexSession();
                registerUpdates();

                break;

            /* Handle the error state of an Obex session */
            case BluetoothMnsObexSession.MSG_SESSION_ERROR:
                if (V) Log.v(TAG, "receive MSG_SESSION_ERROR");
                deregisterUpdates();
                mSession.disconnect();
                mSession = null;
                break;

            case BluetoothMnsObexSession.MSG_CONNECT_TIMEOUT:
                if (V) Log.v(TAG, "receive MSG_CONNECT_TIMEOUT");
                /*
                 * for outbound transfer, the block point is
                 * BluetoothSocket.write() The only way to unblock is to tear
                 * down lower transport
                 */
                try {
                    if (mTransport == null) {
                        Log.v(TAG,"receive MSG_SHARE_INTERRUPTED but " +
                                "mTransport = null");
                    } else {
                        mTransport.close();
                    }
                } catch (IOException e) {
                    Log.e(TAG, "failed to close mTransport");
                }
                if (V) Log.v(TAG, "mTransport closed ");

                break;

            case MNS_SEND_EVENT:
                sendEvent((String) msg.obj);
                break;
            }
        }
    }

    /*
     * Class to hold message handle for MCE Initiated operation
     */
    public class BluetoothMnsMsgHndlMceInitOp {
        public String msgHandle;
        Time time;
    }

    /*
     * Keep track of Message Handles on which the operation was
     * initiated by MCE
     */
    List<BluetoothMnsMsgHndlMceInitOp> opList = new ArrayList<BluetoothMnsMsgHndlMceInitOp>();

    /*
     * Adds the Message Handle to the list for tracking
     * MCE initiated operation
     */
    public void addMceInitiatedOperation(String msgHandle) {
        BluetoothMnsMsgHndlMceInitOp op = new BluetoothMnsMsgHndlMceInitOp();
        op.msgHandle = msgHandle;
        op.time = new Time();
        op.time.setToNow();
        opList.add(op);
    }
    /*
     * Removes the Message Handle from the list for tracking
     * MCE initiated operation
     */
    public void removeMceInitiatedOperation(int location) {
        opList.remove(location);
    }

    /*
     * Finds the location in the list of the given msgHandle, if
     * available. "+" indicates the next (any) operation
     */
    public int findLocationMceInitiatedOperation( String msgHandle) {
        int location = -1;

        Time currentTime = new Time();
        currentTime.setToNow();

        for (BluetoothMnsMsgHndlMceInitOp op: opList) {
            // Remove stale entries
            if (currentTime.toMillis(false) - op.time.toMillis(false) > 10000) {
                opList.remove(op);
            }
        }

        for (BluetoothMnsMsgHndlMceInitOp op: opList) {
            if (op.msgHandle.equalsIgnoreCase(msgHandle)){
                location = opList.indexOf(op);
                break;
            }
        }

        if (location == -1) {
            for (BluetoothMnsMsgHndlMceInitOp op: opList) {
                if (op.msgHandle.equalsIgnoreCase("+")) {
                    location = opList.indexOf(op);
                    break;
                }
            }
        }
        return location;
    }


    /**
     * Post a MNS Event to the MNS thread
     */
    public void sendMnsEvent(String msg, String handle, String folder,
            String old_folder, String msgType) {
        int location = -1;

        /* Send the notification, only if it was not initiated
         * by MCE. MEMORY_FULL and MEMORY_AVAILABLE cannot be
         * MCE initiated
         */
        if (msg.equals(MEMORY_AVAILABLE) || msg.equals(MEMORY_FULL)) {
            location = -1;
        } else {
            location = findLocationMceInitiatedOperation(handle);
        }

        if (location == -1) {
            String str = mu.mapEventReportXML(msg, handle, folder, old_folder,
                    msgType);
            mSessionHandler.obtainMessage(MNS_SEND_EVENT, -1, -1, str)
            .sendToTarget();
        } else {
            removeMceInitiatedOperation(location);
        }
    }

    /**
     * Push the message over Obex client session
     */
    private void sendEvent(String str) {
        if (str != null && (str.length() > 0)) {

            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, "--------------");
                Log.v(TAG, " CONTENT OF EVENT REPORT FILE: " + str);
            }

            final String FILENAME = "EventReport";
            FileOutputStream fos = null;
            File file = new File(mContext.getFilesDir() + "/" + FILENAME);
            file.delete();
            try {
                fos = mContext.openFileOutput(FILENAME, Context.MODE_PRIVATE);
                fos.write(str.getBytes());
                fos.flush();
                fos.close();
            } catch (FileNotFoundException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            File fileR = new File(mContext.getFilesDir() + "/" + FILENAME);
            if (fileR.exists() == true) {
                if (Log.isLoggable(TAG, Log.VERBOSE)){
                    Log.v(TAG, " Sending event report file ");
                }
                mSession.sendEvent(fileR, (byte) 0);
            } else {
                if (Log.isLoggable(TAG, Log.VERBOSE)){
                    Log.v(TAG, " ERROR IN CREATING SEND EVENT OBJ FILE");
                }
            }
        }
    }

    private boolean updatesRegistered = false;

    /**
     * Register with content provider to receive updates
     * of change on cursor.
     */
    private void registerUpdates() {

        Log.d(TAG, "REGISTER MNS UPDATES");

        Uri smsUri = Uri.parse("content://sms/");
        crSmsA = mContext.getContentResolver().query(smsUri,
                new String[] { "_id", "body", "type"}, null, null, "_id asc");
        crSmsB = mContext.getContentResolver().query(smsUri,
                new String[] { "_id", "body", "type"}, null, null, "_id asc");

        Uri smsObserverUri = Uri.parse("content://mms-sms/");
        mContext.getContentResolver().registerContentObserver(smsObserverUri,
                true, smsContentObserver);

        Uri mmsUri = Uri.parse("content://mms/");
        crMmsA = mContext.getContentResolver()
                .query(mmsUri, new String[] { "_id", "read", "m_type", "m_id" }, null,
                        null, "_id asc");
        crMmsB = mContext.getContentResolver()
                .query(mmsUri, new String[] { "_id", "read", "m_type", "m_id" }, null,
                        null, "_id asc");

        if (folderListSmsMms != null && folderListSmsMms.size() > 0){
            for (int i=0; i < folderListSmsMms.size(); i++){
                folderNameSmsMms = folderListSmsMms.get(i);
                Uri smsFolderUri =  Uri.parse("content://sms/"+folderNameSmsMms.trim()+"/");
                crSmsFolderA = mContext.getContentResolver().query(smsFolderUri,
                        new String[] { "_id", "body", "type"}, null, null, "_id asc");
                crSmsFolderB = mContext.getContentResolver().query(smsFolderUri,
                        new String[] { "_id", "body", "type"}, null, null, "_id asc");
                Uri mmsFolderUri;
                if (folderNameSmsMms != null
                        && folderNameSmsMms.equalsIgnoreCase(BluetoothMasAppIf.Draft)){
                    mmsFolderUri = Uri.parse("content://mms/"+BluetoothMasAppIf.Drafts+"/");
                } else{
                    mmsFolderUri = Uri.parse("content://mms/"+folderNameSmsMms.trim()+"/");
                }
                crMmsFolderA = mContext.getContentResolver()
                .query(mmsFolderUri, new String[] { "_id", "read", "m_type", "m_id"},
                        null, null, "_id asc");
                crMmsFolderB = mContext.getContentResolver()
                .query(mmsFolderUri, new String[] { "_id", "read", "m_type", "m_id"},
                        null, null, "_id asc");

                arrObjSmsMms[i] = new SmsMmsFolderContentObserverClass(folderNameSmsMms,
                        crSmsFolderA, crSmsFolderB, crMmsFolderA, crMmsFolderB,
                        CR_SMS_FOLDER_A, CR_MMS_FOLDER_A);

                Uri smsFolderObserverUri = Uri.parse("content://mms-sms/"+folderNameSmsMms.trim());
                mContext.getContentResolver().registerContentObserver(
                        smsFolderObserverUri, true, arrObjSmsMms[i]);
            }
        }

        Uri emailUri = Uri.parse("content://com.android.email.provider/message");
        crEmailA = mContext.getContentResolver().query(emailUri,
                new String[] { "_id", "mailboxkey"}, null, null, "_id asc");
        crEmailB = mContext.getContentResolver().query(emailUri,
                new String[] { "_id", "mailboxkey"}, null, null, "_id asc");

        EmailUtils eu = new EmailUtils();

        Uri emailObserverUri = Uri.parse("content://com.android.email.provider/message");
        mContext.getContentResolver().registerContentObserver(emailObserverUri,
                true, emailContentObserver);

        if(folderList != null && folderList.size() > 0){
            for(int i=0; i < folderList.size(); i++){
                folderName = folderList.get(i);
                String emailFolderCondition = eu.getWhereIsQueryForTypeEmail(folderName, mContext);
                crEmailFolderA = mContext.getContentResolver().query(emailUri,
                        new String[] {  "_id", "mailboxkey"}, emailFolderCondition, null, "_id asc");
                crEmailFolderB = mContext.getContentResolver().query(emailUri,
                        new String[] {"_id", "mailboxkey"}, emailFolderCondition, null, "_id asc");
                arrObj[i] = new EmailFolderContentObserverClass(folderName,
                        crEmailFolderA, crEmailFolderB, CR_EMAIL_FOLDER_A);
                Uri emailFolderObserverUri = Uri.parse("content://com.android.email.provider/message");
                mContext.getContentResolver().registerContentObserver(
                        emailFolderObserverUri, true, arrObj[i]);
            }
        }

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_LOW);
        filter.addAction(Intent.ACTION_DEVICE_STORAGE_OK);
        mContext.registerReceiver(mStorageStatusReceiver, filter);

        updatesRegistered = true;
        Log.d(TAG, " ---------------- ");
        Log.d(TAG, " REGISTERED MNS UPDATES ");
        Log.d(TAG, " ---------------- ");
    }

    /**
     * Stop listening to changes in cursor
     */
    private void deregisterUpdates() {

        if (updatesRegistered == true){
            updatesRegistered = false;
            Log.d(TAG, "DEREGISTER MNS SMS UPDATES");

            mContext.getContentResolver().unregisterContentObserver(
                    smsContentObserver);

            mContext.getContentResolver().unregisterContentObserver(
                    emailContentObserver);

            mContext.unregisterReceiver(mStorageStatusReceiver);

            crSmsA.close();
            crSmsB.close();
            currentCRSms = CR_SMS_A;

            crMmsA.close();
            crMmsB.close();
            currentCRMms = CR_MMS_A;

            crEmailA.close();
            crEmailB.close();
            currentCREmail = CR_EMAIL_A;

            if (arrObj != null && arrObj.length > 0){
                for (int i=0; i < arrObj.length; i++){
                    arrObj[i].crEmailFolderA.close();
                    arrObj[i].crEmailFolderB.close();
                    arrObj[i].currentCREmailFolder = CR_EMAIL_FOLDER_A;
                }
            }

            if (arrObjSmsMms != null && arrObj.length > 0){
                for (int i=0; i < arrObj.length; i++){
                    arrObjSmsMms[i].crSmsFolderA.close();
                    arrObjSmsMms[i].crSmsFolderB.close();
                    arrObjSmsMms[i].currentCRSmsFolder = CR_SMS_FOLDER_A;
                    arrObjSmsMms[i].crMmsFolderA.close();
                    arrObjSmsMms[i].crMmsFolderB.close();
                    arrObjSmsMms[i].currentCRMmsFolder = CR_MMS_FOLDER_A;
                }
            }

        }

    }

    private SmsContentObserverClass smsContentObserver = new SmsContentObserverClass();
    private EmailContentObserverClass emailContentObserver = new EmailContentObserverClass();

    private Cursor crSmsA = null;
    private Cursor crSmsB = null;
    private Cursor crSmsFolderA = null;
    private Cursor crSmsFolderB = null;

    private Cursor crMmsA = null;
    private Cursor crMmsB = null;
    private Cursor crMmsFolderA = null;
    private Cursor crMmsFolderB = null;

    private Cursor crEmailA = null;
    private Cursor crEmailB = null;
    private Cursor crEmailFolderA = null;
    private Cursor crEmailFolderB = null;

    private final int CR_SMS_A = 1;
    private final int CR_SMS_B = 2;
    private int currentCRSms = CR_SMS_A;
    private final int CR_SMS_FOLDER_A = 1;
    private final int CR_SMS_FOLDER_B = 2;

    private final int CR_MMS_A = 1;
    private final int CR_MMS_B = 2;
    private int currentCRMms = CR_MMS_A;
    private final int CR_MMS_FOLDER_A = 1;
    private final int CR_MMS_FOLDER_B = 2;

    private final int CR_EMAIL_A = 1;
    private final int CR_EMAIL_B = 2;
    private int currentCREmail = CR_EMAIL_A;

    private final int CR_EMAIL_FOLDER_A = 1;
    private final int CR_EMAIL_FOLDER_B = 2;
    private int currentCREmailFolder = CR_EMAIL_FOLDER_A;
    public String folderName = "";
    public String folderNameSmsMms = "";


    /**
     * Get the folder name (MAP representation) based on the
     * folder type value in SMS database
     */
    private String getMAPFolder(int type) {
        String folder = null;
        switch (type) {
        case 1:
            folder = "inbox";
            break;
        case 2:
            folder = "sent";
            break;
        case 3:
            folder = "draft";
            break;
        case 4:
        case 5:
        case 6:
            folder = "outbox";
            break;
        default:
            break;
        }
        return folder;
    }

    /**
     * Get the folder name based on the type in SMS ContentProvider
     */
    private String getFolder(int type) {
        String folder = null;
        switch (type) {
        case 1:
            folder = "inbox";
            break;
        case 2:
            folder = "sent";
            break;
        case 3:
            folder = "draft";
            break;
        case 4:
            folder = "outbox";
            break;
        case 5:
            folder = "failed";
            break;
        case 6:
            folder = "queued";
            break;
        default:
            break;
        }
        return folder;
    }

    /**
     * Gets the table type (as in Sms Content Provider) for the
     * given id
     */
    private int getMessageType(String id) {
        Cursor cr = mContext.getContentResolver().query(
                Uri.parse("content://sms/" + id),
                new String[] { "_id", "type"}, null, null, null);
        if (cr.moveToFirst()) {
            return cr.getInt(cr.getColumnIndex("type"));
        }
        cr.close();
        return -1;
    }
    /**
     * Gets the table type (as in Email Content Provider) for the
     * given id
     */
    private int getDeletedFlagEmail(String id) {
        int deletedFlag =0;
        Cursor cr = mContext.getContentResolver().query(
                Uri.parse("content://com.android.email.provider/message/" + id),
                new String[] { "_id", "mailboxKey"}, null, null, null);
        int folderId = -1;
        if (cr.moveToFirst()) {
            folderId = cr.getInt(cr.getColumnIndex("mailboxKey"));
        }

        Cursor cr1 = mContext.getContentResolver().query(
                Uri.parse("content://com.android.email.provider/mailbox"),
                new String[] { "_id", "displayName"}, "_id ="+ folderId, null, null);
        String folderName = null;
        if (cr1.moveToFirst()) {
            folderName = cr1.getString(cr1.getColumnIndex("displayName"));
        }
        if (folderName !=null && (folderName.equalsIgnoreCase("Trash") ||
                folderName.toUpperCase().contains("TRASH"))){
            deletedFlag = 1;
        }
        cr.close();
        cr1.close();
        return deletedFlag;
    }

    /**
     * Get the folder name (table name of Sms Content Provider)
     */
    private String getContainingFolder(String oldFolder, String id,
            String dateTime) {
        String newFolder = null;
        Cursor cr = mContext.getContentResolver().query(
                Uri.parse("content://sms/"),
                new String[] { "_id", "date", "type"}, " _id = " + id, null,
                null);
        if (cr.moveToFirst()) {
            return getFolder(cr.getInt(cr.getColumnIndex("type")));
        }
        cr.close();
        return newFolder;
    }


    private BroadcastReceiver mStorageStatusReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Intent.ACTION_DEVICE_STORAGE_LOW)) {
                Log.d(TAG, " Memory Full ");
                sendMnsEvent(MEMORY_FULL, null, null, null, null);
            } else if (intent.getAction().equals(Intent.ACTION_DEVICE_STORAGE_OK)) {
                Log.d(TAG, " Memory Available ");
                sendMnsEvent(MEMORY_AVAILABLE, null, null, null, null);
            }
        }
    };
    /**
     * This class listens for changes in Email Content Provider tables
     * It acts, only when an entry gets removed from the table
     */
    private class EmailFolderContentObserverClass extends ContentObserver {
        private String folder;
        private Cursor crEmailFolderA;
        private Cursor crEmailFolderB;
        private int currentCREmailFolder;

        public EmailFolderContentObserverClass(String folderName, Cursor crEmailFolderA,
                Cursor crEmailFolderB, int currentCREmailFolder) {
            super(null);
            this.folder = folderName;
            this.crEmailFolderA = crEmailFolderA;
            this.crEmailFolderB = crEmailFolderB;
            this.currentCREmailFolder = currentCREmailFolder;
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);

            int currentItemCount = 0;
            int newItemCount = 0;
            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG,"Folder name in Observer class ::"+folderName);
                Log.v(TAG,"Flag value name in Observer class ::"+currentCREmailFolder);
            }

            if (currentCREmailFolder == CR_EMAIL_FOLDER_A) {
                currentItemCount = crEmailFolderA.getCount();
                crEmailFolderB.requery();
                newItemCount = crEmailFolderB.getCount();
            } else {
                currentItemCount = crEmailFolderB.getCount();
                crEmailFolderA.requery();
                newItemCount = crEmailFolderA.getCount();
            }

            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, "EMAIL Deleted folder current " + currentItemCount + " new "
                        + newItemCount);
            }
            if (currentItemCount > newItemCount) {
                crEmailFolderA.moveToFirst();
                crEmailFolderB.moveToFirst();

                CursorJoiner joiner = new CursorJoiner(crEmailFolderA,
                        new String[] { "_id"}, crEmailFolderB,
                        new String[] { "_id"});

                CursorJoiner.Result joinerResult;
                while (joiner.hasNext()) {
                    joinerResult = joiner.next();
                    switch (joinerResult) {
                    case LEFT:
                        // handle case where a row in cursor1 is unique
                        if (currentCREmailFolder == CR_EMAIL_FOLDER_A) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " EMAIL DELETED FROM FOLDER ");
                            }
                            String id = crEmailFolderA.getString(crEmailFolderA
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED EMAIL ID " + id);
                            }
                            int deletedFlag = getDeletedFlagEmail(id);
                            if (deletedFlag == 1){
                                id = Integer.toString(Integer.valueOf(id)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom+"/"+
                                        BluetoothMasAppIf.Msg+"/"+
                                        folder, null, "EMAIL");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, "Shouldn't reach here as you cannot "
                                            + "move msg from Inbox to any other folder");
                                }
                                if (folder != null && folder.equalsIgnoreCase("outbox")){
                                    Cursor cr1 = null;
                                    int folderId;
                                    String containingFolder = null;
                                    EmailUtils eu = new EmailUtils();
                                    Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                                    if (Integer.valueOf(id) > 200000){
                                        id = Integer.toString(Integer.valueOf(id)
                                                - EMAIL_HDLR_CONSTANT);
                                    }
                                    String whereClause = " _id = " + id;
                                    cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                            null);

                                    if (cr1.getCount() > 0) {
                                        cr1.moveToFirst();
                                        folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                        containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                                    }
                                    cr1.close();
                                    String newFolder = containingFolder;
                                    id = Integer.toString(Integer.valueOf(id)
                                            + EMAIL_HDLR_CONSTANT);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase("outbox"))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom+"/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/"+ BluetoothMasAppIf.Outbox, "EMAIL");
                                        if (newFolder.equalsIgnoreCase("sent")) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/"+
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "EMAIL");
                                        }
                                    }
                                } else if (folder !=null){
                                    Cursor cr1 = null;
                                    int folderId;
                                    String containingFolder = null;
                                    EmailUtils eu = new EmailUtils();
                                    Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                                    String whereClause = " _id = " + id;
                                    cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                            null);

                                    if (cr1.getCount() > 0) {
                                        cr1.moveToFirst();
                                        folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                        containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                                    }
                                    cr1.close();
                                    String newFolder = containingFolder;
                                    id = Integer.toString(Integer.valueOf(id)
                                            + EMAIL_HDLR_CONSTANT);
                                    sendMnsEvent(MESSAGE_SHIFT, id, 
                                            BluetoothMasAppIf.Telecom + "/"+
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom +
                                            "/"+BluetoothMasAppIf.Msg + "/"
                                            +folder,
                                            "EMAIL");
                                }
                            }
                        } else {
                            // TODO - The current(old) query doesn't have this row;
                            // implies it was added
                        }
                        break;
                    case RIGHT:
                        // handle case where a row in cursor2 is unique
                        if (currentCREmailFolder == CR_EMAIL_FOLDER_B) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " EMAIL DELETED FROM FOLDER ");
                            }
                            String id = crEmailFolderB.getString(crEmailFolderB
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED EMAIL ID " + id);
                            }
                            int deletedFlag = getDeletedFlagEmail(id); //TODO
                            if (deletedFlag == 1){
                                id = Integer.toString(Integer.valueOf(id)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/" +folder, null, "EMAIL");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, "Shouldn't reach here as you cannot "
                                            + "move msg from Inbox to any other folder");
                                }
                                if (folder != null && folder.equalsIgnoreCase("outbox")){
                                    Cursor cr1 = null;
                                    int folderId;
                                    String containingFolder = null;
                                    EmailUtils eu = new EmailUtils();
                                    Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                                    if (Integer.valueOf(id) > 200000){
                                        id = Integer.toString(Integer.valueOf(id)
                                                - EMAIL_HDLR_CONSTANT);
                                    }
                                    String whereClause = " _id = " + id;
                                    cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                            null);

                                    if (cr1.getCount() > 0) {
                                        cr1.moveToFirst();
                                        folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                        containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                                    }
                                    cr1.close();
                                    String newFolder = containingFolder;
                                    id = Integer.toString(Integer.valueOf(id)
                                            + EMAIL_HDLR_CONSTANT);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase("outbox"))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/"+
                                                BluetoothMasAppIf.Outbox, "EMAIL");
                                        if (newFolder.equalsIgnoreCase("sent")) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/"+
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "EMAIL");
                                        }
                                    }
                                } else if (folder !=null){
                                    Cursor cr1 = null;
                                    int folderId;
                                    String containingFolder = null;
                                    EmailUtils eu = new EmailUtils();
                                    Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                                    String whereClause = " _id = " + id;
                                    cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                            null);

                                    if (cr1.getCount() > 0) {
                                        cr1.moveToFirst();
                                        folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                        containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                                    }
                                    cr1.close();
                                    String newFolder = containingFolder;
                                    id = Integer.toString(Integer.valueOf(id)
                                            + EMAIL_HDLR_CONSTANT);
                                    sendMnsEvent(MESSAGE_SHIFT, id, 
                                            BluetoothMasAppIf.Telecom + "/"+
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom + "/"+
                                            BluetoothMasAppIf.Msg + "/"+ folder,
                                            "EMAIL");
                                }
                            }
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                        }
                        break;
                    case BOTH:
                        // handle case where a row with the same key is in both
                        // cursors
                        break;
                    }
                }
            }
            if (currentCREmailFolder == CR_EMAIL_FOLDER_A) {
                currentCREmailFolder = CR_EMAIL_FOLDER_B;
            } else {
                currentCREmailFolder = CR_EMAIL_FOLDER_A;
            }
        }
    }

    /**
     * This class listens for changes in Sms MMs Content Provider's folders
     * It acts, only when an entry gets removed from the table
     */
    private class SmsMmsFolderContentObserverClass extends ContentObserver {
        private String folder;
        private Cursor crSmsFolderA;
        private Cursor crSmsFolderB;
        private int currentCRSmsFolder;
        private Cursor crMmsFolderA;
        private Cursor crMmsFolderB;
        private int currentCRMmsFolder;

        public SmsMmsFolderContentObserverClass(String folderNameSmsMms,
                Cursor crSmsFolderA, Cursor crSmsFolderB, Cursor crMmsFolderA,
                Cursor crMmsFolderB, int currentCRSmsFolder, int currentCRMmsFolder) {
            super(null);
            this.folder = folderNameSmsMms;
            this.crSmsFolderA = crSmsFolderA;
            this.crSmsFolderB = crSmsFolderB;
            this.currentCRSmsFolder = currentCRSmsFolder;
            this.crMmsFolderA = crMmsFolderA;
            this.crMmsFolderB = crMmsFolderB;
            this.currentCRMmsFolder = currentCRMmsFolder;
        }

        @Override
                public void onChange(boolean selfChange) {
            super.onChange(selfChange);

            int currentItemCount = 0;
            int newItemCount = 0;

            if (folder != null
                    && !folder.equalsIgnoreCase(BluetoothMasAppIf.Failed)
                    && !folder.equalsIgnoreCase(BluetoothMasAppIf.Queued)){
                currentCRMmsFolder = checkMmsFolder(folder, crMmsFolderA, crMmsFolderB, currentCRMmsFolder);
            }

            if (currentCRSmsFolder == CR_SMS_FOLDER_A) {
                currentItemCount = crSmsFolderA.getCount();
                crSmsFolderB.requery();
                newItemCount = crSmsFolderB.getCount();
            } else {
                currentItemCount = crSmsFolderB.getCount();
                crSmsFolderA.requery();
                newItemCount = crSmsFolderA.getCount();
            }

            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, "SMS FOLDER current " + currentItemCount + " new "
                        + newItemCount);
            }

            if (currentItemCount > newItemCount) {
                crSmsFolderA.moveToFirst();
                crSmsFolderB.moveToFirst();

                CursorJoiner joiner = new CursorJoiner(crSmsFolderA,
                        new String[] { "_id"}, crSmsFolderB,
                        new String[] { "_id"});

                CursorJoiner.Result joinerResult;
                while (joiner.hasNext()) {
                    joinerResult = joiner.next();
                    switch (joinerResult) {
                    case LEFT:
                        // handle case where a row in cursor1 is unique
                        if (currentCRSmsFolder == CR_SMS_FOLDER_A) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " SMS DELETED FROM FOLDER ");
                            }
                            String body = crSmsFolderA.getString(crSmsFolderA
                                    .getColumnIndex("body"));
                            String id = crSmsFolderA.getString(crSmsFolderA
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED SMS ID " + id + " BODY "
                                        + body);
                            }
                            int msgType = getMessageType(id);
                            if (msgType == -1) {
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/" +
                                        folder, null, "SMS_GSM");
                            } else {
                                /*if (Log.isLoggable(TAG, Log.VERBOSE)){
                                        Log.v(TAG, "Shouldn't reach here as you cannot " +
                                            "move msg from this folder to any other folder");
                                }*/
                                if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Draft)){
                                    String newFolder = getMAPFolder(msgType);
                                    sendMnsEvent(MESSAGE_SHIFT, id, BluetoothMasAppIf.Telecom
                                            + "/" + BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/" +
                                            BluetoothMasAppIf.Draft, "SMS_GSM");
                                    if (newFolder.equalsIgnoreCase("sent")) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "SMS_GSM");
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Outbox)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/"+
                                                    BluetoothMasAppIf.Msg +"/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                    if ((msgType == MSG_CP_QUEUED_TYPE) ||
                                            (msgType == MSG_CP_FAILED_TYPE)) {
                                        // Message moved from outbox to queue or
                                        // failed folder
                                        sendMnsEvent(SENDING_FAILURE, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, null, "SMS_GSM");
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Failed)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/"+
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/" +
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Queued)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/" +
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                }

                            }
                        } else {
                            // TODO - The current(old) query doesn't have this row;
                            // implies it was added
                        }
                        break;
                    case RIGHT:
                        // handle case where a row in cursor2 is unique
                        if (currentCRSmsFolder == CR_SMS_FOLDER_B) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " SMS DELETED FROM FOLDER");
                            }
                            String body = crSmsFolderB.getString(crSmsFolderB
                                    .getColumnIndex("body"));
                            String id = crSmsFolderB.getString(crSmsFolderB
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED SMS ID " + id + " BODY "
                                        + body);
                            }
                            int msgType = getMessageType(id);
                            if (msgType == -1) {
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom + "/"+
                                        BluetoothMasAppIf.Msg + "/"+
                                        folder, null, "SMS_GSM");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG,"Shouldn't reach here as you cannot " +
                                            "move msg from this folder to any other folder");
                                }
                                if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Draft)){
                                    String newFolder = getMAPFolder(msgType);
                                    sendMnsEvent(MESSAGE_SHIFT, id, BluetoothMasAppIf.Telecom
                                            + "/" + BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/" +
                                            BluetoothMasAppIf.Draft, "SMS_GSM");
                                    if (newFolder.equalsIgnoreCase("sent")) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "SMS_GSM");
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Outbox)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/"+
                                                    BluetoothMasAppIf.Msg +"/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                    if ((msgType == MSG_CP_QUEUED_TYPE) ||
                                            (msgType == MSG_CP_FAILED_TYPE)) {
                                        // Message moved from outbox to queue or
                                        // failed folder
                                        sendMnsEvent(SENDING_FAILURE, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, null, "SMS_GSM");
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Failed)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" +
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/" +
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                } else if (folder != null &&
                                        folder.equalsIgnoreCase(BluetoothMasAppIf.Queued)){
                                    String newFolder = getMAPFolder(msgType);
                                    if ((newFolder != null)
                                            && (!newFolder
                                            .equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                        // The message has moved on MAP virtual
                                        // folder representation.
                                        sendMnsEvent(MESSAGE_SHIFT, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/"+
                                                BluetoothMasAppIf.Outbox, "SMS_GSM");
                                        if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                            sendMnsEvent(SENDING_SUCCESS, id,
                                                    BluetoothMasAppIf.Telecom + "/" +
                                                    BluetoothMasAppIf.Msg + "/" + newFolder,
                                                    null, "SMS_GSM");
                                        }
                                    }
                                }
                            }
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                        }
                        break;
                    case BOTH:
                        // handle case where a row with the same key is in both
                        // cursors
                        break;
                    }
                }
            }
            if (currentCRSmsFolder == CR_SMS_FOLDER_A) {
                currentCRSmsFolder = CR_SMS_FOLDER_B;
            } else {
                currentCRSmsFolder = CR_SMS_FOLDER_A;
            }
        }
    }

    /**
     * This class listens for changes in Sms Content Provider
     * It acts, only when a new entry gets added to database
     */
    private class SmsContentObserverClass extends ContentObserver {

        public SmsContentObserverClass() {
            super(null);
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);

            int currentItemCount = 0;
            int newItemCount = 0;

            checkMmsAdded();

            // Synchronize this?
            if (currentCRSms == CR_SMS_A) {
                currentItemCount = crSmsA.getCount();
                crSmsB.requery();
                newItemCount = crSmsB.getCount();
            } else {
                currentItemCount = crSmsB.getCount();
                crSmsA.requery();
                newItemCount = crSmsA.getCount();
            }

            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, "SMS current " + currentItemCount + " new "
                        + newItemCount);
            }
            if (newItemCount > currentItemCount) {
                crSmsA.moveToFirst();
                crSmsB.moveToFirst();

                CursorJoiner joiner = new CursorJoiner(crSmsA,
                        new String[] { "_id"}, crSmsB, new String[] { "_id"});

                CursorJoiner.Result joinerResult;
                while (joiner.hasNext()) {
                    joinerResult = joiner.next();
                    switch (joinerResult) {
                    case LEFT:
                        // handle case where a row in cursor1 is unique
                        if (currentCRSms == CR_SMS_A) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " SMS ADDED TO INBOX ");
                            }
                            String body1 = crSmsA.getString(crSmsA
                                    .getColumnIndex("body"));
                            String id1 = crSmsA.getString(crSmsA
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED SMS ID " + id1 + " BODY "
                                        + body1);
                            }
                            String folder = getMAPFolder(crSmsA.getInt(crSmsA
                                    .getColumnIndex("type")));
                            if (folder != null &&
                                    folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                sendMnsEvent(NEW_MESSAGE, id1, BluetoothMasAppIf.Telecom +
                                        "/" + BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "SMS_GSM");
                            } else if (folder != null &&
                                    !folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)){
                                sendMnsEvent(MESSAGE_SHIFT, id1, BluetoothMasAppIf.Telecom +
                                        "/" + BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "SMS_GSM");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                                }
                            }
                        }
                        break;
                    case RIGHT:
                        // handle case where a row in cursor2 is unique
                        if (currentCRSms == CR_SMS_B) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " SMS ADDED ");
                            }
                            String body1 = crSmsB.getString(crSmsB
                                    .getColumnIndex("body"));
                            String id1 = crSmsB.getString(crSmsB
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED SMS ID " + id1 + " BODY "
                                        + body1);
                            }
                            String folder = getMAPFolder(crSmsB.getInt(crSmsB
                                    .getColumnIndex("type")));
                            if (folder != null &&
                                    folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                sendMnsEvent(NEW_MESSAGE, id1, BluetoothMasAppIf.Telecom + "/"+
                                        BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "SMS_GSM");
                            } else if (folder != null &&
                                    !folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)){
                                sendMnsEvent(MESSAGE_SHIFT, id1, BluetoothMasAppIf.Telecom +
                                        "/" + BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "SMS_GSM");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                                }
                            }
                        }
                        break;
                    case BOTH:
                        // handle case where a row with the same key is in both
                        // cursors
                        break;
                    }
                }
            }
            if (currentCRSms == CR_SMS_A) {
                currentCRSms = CR_SMS_B;
            } else {
                currentCRSms = CR_SMS_A;
            }
        }
    }

    /**
     * This class listens for changes in Email Content Provider
     * It acts, only when a new entry gets added to database
     */
    private class EmailContentObserverClass extends ContentObserver {

        public EmailContentObserverClass() {
            super(null);
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);

            int currentItemCount = 0;
            int newItemCount = 0;
            EmailUtils eu = new EmailUtils();
            String containingFolder = null;

            // Synchronize this
            if (currentCREmail == CR_EMAIL_A) {
                currentItemCount = crEmailA.getCount();
                crEmailB.requery();
                newItemCount = crEmailB.getCount();
            } else {
                currentItemCount = crEmailB.getCount();
                crEmailA.requery();
                newItemCount = crEmailA.getCount();
            }

            if (Log.isLoggable(TAG, Log.VERBOSE)){
                Log.v(TAG, "Email current " + currentItemCount + " new "
                        + newItemCount);
            }

            if (newItemCount > currentItemCount) {
                crEmailA.moveToFirst();
                crEmailB.moveToFirst();

                CursorJoiner joiner = new CursorJoiner(crEmailA,
                        new String[] { "_id"}, crEmailB, new String[] { "_id"});

                CursorJoiner.Result joinerResult;
                while (joiner.hasNext()) {
                    joinerResult = joiner.next();
                    switch (joinerResult) {
                    case LEFT:
                        // handle case where a row in cursor1 is unique
                        if (currentCREmail == CR_EMAIL_A) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " EMAIL ADDED TO INBOX ");
                            }
                            String id1 = crEmailA.getString(crEmailA
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED EMAIL ID " + id1);
                            }
                            Cursor cr1 = null;
                            int folderId;
                            Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                            String whereClause = " _id = " + id1;
                            cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                    null);
                            if (cr1.moveToFirst()) {
                                do {
                                    for (int i=0;i<cr1.getColumnCount();i++){
                                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                                            Log.v(TAG, " Column Name: "+ cr1.getColumnName(i) + " Value: " + cr1.getString(i));
                                        }
                                    }
                                } while (cr1.moveToNext());
                            }

                            if (cr1.getCount() > 0) {
                                cr1.moveToFirst();
                                folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                            }
                            cr1.close();
                            if (containingFolder != null
                                    && containingFolder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " containingFolder:: "+containingFolder);
                                }
                                id1 = Integer.toString(Integer.valueOf(id1)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(NEW_MESSAGE, id1, BluetoothMasAppIf.Telecom
                                        + "/"+ BluetoothMasAppIf.Msg + "/"
                                        + containingFolder, null, "EMAIL");
                            } else if (containingFolder != null
                                    && !containingFolder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " containingFolder:: "+containingFolder);
                                }
                                id1 = Integer.toString(Integer.valueOf(id1)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(MESSAGE_SHIFT, id1, BluetoothMasAppIf.Telecom
                                        + "/"+ BluetoothMasAppIf.Msg + "/"
                                        + containingFolder, null, "EMAIL");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                                }
                            }
                        }
                        break;
                    case RIGHT:
                        // handle case where a row in cursor2 is unique
                        if (currentCREmail == CR_EMAIL_B) {
                            // The new query doesn't have this row; implies it
                            // was deleted
                        } else {
                            // The current(old) query doesn't have this row;
                            // implies it was added
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " EMAIL ADDED ");
                            }
                            String id1 = crEmailB.getString(crEmailB
                                    .getColumnIndex("_id"));
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED EMAIL ID " + id1);
                            }
                            Cursor cr1 = null;
                            int folderId;
                            Uri uri1 = Uri.parse("content://com.android.email.provider/message");
                            String whereClause = " _id = " + id1;
                            cr1 = mContext.getContentResolver().query(uri1, null, whereClause, null,
                                    null);

                            if (cr1.moveToFirst()) {
                                do {
                                    for (int i=0;i<cr1.getColumnCount();i++){
                                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                                            Log.v(TAG, " Column Name: "+ cr1.getColumnName(i) +
                                                    " Value: " + cr1.getString(i));
                                        }
                                    }
                                } while (cr1.moveToNext());
                            }

                            if (cr1.getCount() > 0) {
                                cr1.moveToFirst();
                                folderId = cr1.getInt(cr1.getColumnIndex("mailboxKey"));
                                containingFolder = eu.getContainingFolderEmail(folderId, mContext);
                            }
                            cr1.close();
                            if (containingFolder != null
                                    && containingFolder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " containingFolder:: "+containingFolder);
                                }
                                id1 = Integer.toString(Integer.valueOf(id1)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(NEW_MESSAGE, id1, BluetoothMasAppIf.Telecom
                                        + "/" + BluetoothMasAppIf.Msg + "/"
                                        + containingFolder, null, "EMAIL");
                            } else if (containingFolder != null
                                    && !containingFolder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " containingFolder:: "+containingFolder);
                                }
                                id1 = Integer.toString(Integer.valueOf(id1)
                                        + EMAIL_HDLR_CONSTANT);
                                sendMnsEvent(MESSAGE_SHIFT, id1, BluetoothMasAppIf.Telecom
                                        + "/" + BluetoothMasAppIf.Msg + "/"
                                        + containingFolder, null, "EMAIL");
                            } else {
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                                }
                            }
                        }
                        break;
                    case BOTH:
                        // handle case where a row with the same key is in both
                        // cursors
                        break;
                    }
                }
            }
            if (currentCREmail == CR_EMAIL_A) {
                currentCREmail = CR_EMAIL_B;
            } else {
                currentCREmail = CR_EMAIL_A;
            }
        }
    }

    /**
     * Start MNS connection
     */
    public void start(BluetoothDevice mRemoteDevice) {
        /* check Bluetooth enable status */
        /*
         * normally it's impossible to reach here if BT is disabled. Just check
         * for safety
         */
        if (!mAdapter.isEnabled()) {
            Log.e(TAG, "Can't send event when Bluetooth is disabled ");
            return;
        }

        mDestination = mRemoteDevice;
        int channel = -1;
        // TODO Fix this below

        if (channel != -1) {
            if (D) {
                Log.d(TAG, "Get MNS channel " + channel + " from cache for "
                        + mDestination);
            }
            mTimestamp = System.currentTimeMillis();
            mSessionHandler.obtainMessage(SDP_RESULT, channel, -1, mDestination)
                    .sendToTarget();
        } else {
            sendMnsSdp();
        }
    }

    /**
     * Stop the transfer
     */
    public void stop() {
        if (V)
            Log.v(TAG, "stop");
        if (mConnectThread != null) {
            try {
                mConnectThread.interrupt();
                if (V) Log.v(TAG, "waiting for connect thread to terminate");
                mConnectThread.join();
            } catch (InterruptedException e) {
                if (V) Log.v(TAG,
                            "Interrupted waiting for connect thread to join");
            }
            mConnectThread = null;
        }
        if (mSession != null) {
            if (V)
                Log.v(TAG, "Stop mSession");
            mSession.disconnect();
            mSession = null;
        }
        // TODO Do this somewhere else - Should the handler thread be gracefully closed.
    }

    /**
     * Connect the MNS Obex client to remote server
     */
    private void startObexSession() {

        if (V)
            Log.v(TAG, "Create Client session with transport "
                    + mTransport.toString());
        mSession = new BluetoothMnsObexSession(mContext, mTransport);
        mSession.connect();
    }

    /**
     * Check if local database contains remote device's info
     * Else start sdp query
     */
    private void sendMnsSdp() {
        if (V)
            Log.v(TAG, "Do Opush SDP request for address " + mDestination);

        mTimestamp = System.currentTimeMillis();

        int channel = -1;

        Method m;
        try {
            m = mDestination.getClass().getMethod("getServiceChannel",
                    new Class[] { ParcelUuid.class});
            channel = (Integer) m.invoke(mDestination, BluetoothUuid_ObexMns);
        } catch (SecurityException e2) {
            // TODO Auto-generated catch block
            e2.printStackTrace();
        } catch (NoSuchMethodException e2) {
            // TODO Auto-generated catch block
            e2.printStackTrace();
        } catch (IllegalArgumentException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }

        // TODO: channel = mDestination.getServiceChannel(BluetoothUuid_ObexMns);

        if (channel != -1) {
            if (D)
                Log.d(TAG, "Get MNS channel " + channel + " from SDP for "
                        + mDestination);

            mSessionHandler
                    .obtainMessage(SDP_RESULT, channel, -1, mDestination)
                    .sendToTarget();
            return;

        } else {

            boolean result = false;
            if (V)
                Log.v(TAG, "Remote Service channel not in cache");

            Method m2;
            try {
                m2 = mDestination.getClass().getMethod("fetchUuidsWithSdp",
                        new Class[] {});
                result = (Boolean) m2.invoke(mDestination);

            } catch (SecurityException e1) {
                // TODO Auto-generated catch block
                e1.printStackTrace();
            } catch (NoSuchMethodException e1) {
                // TODO Auto-generated catch block
                e1.printStackTrace();
            } catch (IllegalArgumentException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (InvocationTargetException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            if (result == false) {
                Log.e(TAG, "Start SDP query failed");
            } else {
                // we expect framework send us Intent ACTION_UUID. otherwise we
                // will fail
                if (V)
                    Log.v(TAG, "Start new SDP, wait for result");
                IntentFilter intentFilter = new IntentFilter(
                        "android.bleutooth.device.action.UUID");
                mContext.registerReceiver(mReceiver, intentFilter);
                return;
            }
        }
        Message msg = mSessionHandler.obtainMessage(SDP_RESULT, channel, -1,
                mDestination);
        mSessionHandler.sendMessageDelayed(msg, 2000);
    }

    /**
     * Receives the response of SDP query
     */
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {

            Log.d(TAG, " MNS BROADCAST RECV intent: " + intent.getAction());

            if (intent.getAction().equals(
                    "android.bleutooth.device.action.UUID")) {
                BluetoothDevice device = intent
                        .getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (V)
                    Log.v(TAG, "ACTION_UUID for device " + device);
                if (device.equals(mDestination)) {
                    int channel = -1;
                    Parcelable[] uuid = intent
                            .getParcelableArrayExtra("android.bluetooth.device.extra.UUID");
                    if (uuid != null) {
                        ParcelUuid[] uuids = new ParcelUuid[uuid.length];
                        for (int i = 0; i < uuid.length; i++) {
                            uuids[i] = (ParcelUuid) uuid[i];
                        }

                        boolean result = false;

                        // TODO Fix this error
                        result = true;

                        try {
                            Class c = Class
                                    .forName("android.bluetooth.BluetoothUuid");
                            Method m = c.getMethod("isUuidPresent",
                                    new Class[] { ParcelUuid[].class,
                                        ParcelUuid.class});

                            Boolean bool = false;
                            bool = (Boolean) m.invoke(c, uuids,
                                    BluetoothUuid_ObexMns);
                            result = bool.booleanValue();

                        } catch (ClassNotFoundException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (SecurityException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (NoSuchMethodException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (IllegalArgumentException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (IllegalAccessException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (InvocationTargetException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        }

                        if (result) {
                            // TODO: Check if UUID IS PRESENT
                            if (V)
                                Log.v(TAG, "SDP get MNS result for device "
                                        + device);

                            // TODO: Get channel from mDestination
                            // TODO: .getServiceChannel(BluetoothUuid_ObexMns);
                            Method m1;
                            try {

                                m1 = device.getClass().getMethod(
                                        "getServiceChannel",
                                        new Class[] { ParcelUuid.class});
                                Integer chan = (Integer) m1.invoke(device,
                                        BluetoothUuid_ObexMns);

                                channel = chan.intValue();
                                Log.d(TAG, " MNS SERVER Channel no " + channel);
                                if (channel == -1) {
                                    channel = 2;
                                    Log.d(TAG, " MNS SERVER USE TEMP CHANNEL "
                                            + channel);
                                }
                            } catch (SecurityException e) {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            } catch (NoSuchMethodException e) {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            } catch (IllegalArgumentException e) {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            } catch (IllegalAccessException e) {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            } catch (InvocationTargetException e) {
                                // TODO Auto-generated catch block
                                e.printStackTrace();
                            }
                        }
                    }
                    mSessionHandler.obtainMessage(SDP_RESULT, channel, -1,
                            mDestination).sendToTarget();
                }
            }
        }
    };

    private SocketConnectThread mConnectThread;

    /**
     * This thread is used to establish rfcomm connection to
     * remote device
     */
    private class SocketConnectThread extends Thread {
        private final String host;

        private final BluetoothDevice device;

        private final int channel;

        private boolean isConnected;

        private long timestamp;

        private BluetoothSocket btSocket = null;

        /* create a Rfcomm Socket */
        public SocketConnectThread(BluetoothDevice device, int channel) {
            super("Socket Connect Thread");
            this.device = device;
            this.host = null;
            this.channel = channel;
            isConnected = false;
        }

        public void interrupt() {
        }

        @Override
        public void run() {

            timestamp = System.currentTimeMillis();

            /* Use BluetoothSocket to connect */
            try {
                try {
                    Method m = device.getClass().getMethod(
                            "createInsecureRfcommSocket",
                            new Class[] { int.class });
                    btSocket = (BluetoothSocket) m.invoke(device, channel);
                } catch (SecurityException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                } catch (NoSuchMethodException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                } catch (IllegalArgumentException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                } catch (IllegalAccessException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                } catch (InvocationTargetException e) {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
            } catch (Exception e1) {
                // TODO Auto-generated catch block
                Log.e(TAG, "Rfcomm socket create error");
                markConnectionFailed(btSocket);
                return;
            }

            try {
                btSocket.connect();
                if (V) Log.v(TAG, "Rfcomm socket connection attempt took "
                        + (System.currentTimeMillis() - timestamp) + " ms");
                BluetoothMnsRfcommTransport transport;
                transport = new BluetoothMnsRfcommTransport(btSocket);

                BluetoothMnsPreference.getInstance(mContext).setChannel(device,
                        MNS_UUID16, channel);
                BluetoothMnsPreference.getInstance(mContext).setName(device,
                        device.getName());

                if (V) Log.v(TAG, "Send transport message "
                        + transport.toString());

                mSessionHandler.obtainMessage(RFCOMM_CONNECTED, transport)
                        .sendToTarget();
            } catch (IOException e) {
                Log.e(TAG, "Rfcomm socket connect exception ");
                BluetoothMnsPreference.getInstance(mContext).removeChannel(
                        device, MNS_UUID16);
                markConnectionFailed(btSocket);
                return;
            }
        }

        /**
         * RFCOMM connection failed
         */
        private void markConnectionFailed(Socket s) {
            try {
                s.close();
            } catch (IOException e) {
                Log.e(TAG, "TCP socket close error");
            }
            mSessionHandler.obtainMessage(RFCOMM_ERROR).sendToTarget();
        }

        /**
         * RFCOMM connection failed
         */
        private void markConnectionFailed(BluetoothSocket s) {
            try {
                s.close();
            } catch (IOException e) {
                if (V) Log.e(TAG, "Error when close socket");
            }
            mSessionHandler.obtainMessage(RFCOMM_ERROR).sendToTarget();
            return;
        }
    }

    /**
     * Check for change in MMS folder and send a notification if there is
     * a change
     */
    private int checkMmsFolder(String folderMms, Cursor crMmsFolderA,
            Cursor crMmsFolderB, int currentCRMmsFolder) {

        int currentItemCount = 0;
        int newItemCount = 0;

        if (currentCRMmsFolder == CR_MMS_FOLDER_A) {
            currentItemCount = crMmsFolderA.getCount();
            crMmsFolderB.requery();
            newItemCount = crMmsFolderB.getCount();
        } else {
            currentItemCount = crMmsFolderB.getCount();
            crMmsFolderA.requery();
            newItemCount = crMmsFolderA.getCount();
        }

        if (Log.isLoggable(TAG, Log.VERBOSE)){
            Log.v(TAG, "FOLDER Name::" + folderMms);
            Log.v(TAG, "MMS FOLDER current " + currentItemCount + " new "
                    + newItemCount);
        }

        if (currentItemCount > newItemCount) {
            crMmsFolderA.moveToFirst();
            crMmsFolderB.moveToFirst();

            CursorJoiner joiner = new CursorJoiner(crMmsFolderA,
                    new String[] { "_id"}, crMmsFolderB, new String[] { "_id"});

            CursorJoiner.Result joinerResult;
            while (joiner.hasNext()) {
                joinerResult = joiner.next();
                switch (joinerResult) {
                case LEFT:
                    // handle case where a row in cursor1 is unique
                    if (currentCRMmsFolder == CR_MMS_FOLDER_A) {
                        // The new query doesn't have this row; implies it
                        // was deleted
                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                            Log.v(TAG, " MMS DELETED FROM FOLDER ");
                        }
                        String id = crMmsFolderA.getString(crMmsFolderA
                                .getColumnIndex("_id"));
                        int msgInfo = 0;
                        msgInfo = crMmsFolderA.getInt(crMmsFolderA.getColumnIndex("m_type"));
                        String mId = crMmsFolderA.getString(crMmsFolderA.getColumnIndex("m_id"));
                        int msgType = getMmsContainingFolder(Integer
                                .parseInt(id));
                        if (msgType == -1) {
                            // Convert to virtual handle for MMS
                            id = Integer.toString(Integer.valueOf(id)
                                    + MMS_HDLR_CONSTANT);
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED MMS ID " + id);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/" +
                                        folderMms, null, "MMS");
                            }
                        } else {
                            if (folderMms != null &&
                                    folderMms.equalsIgnoreCase(BluetoothMasAppIf.Draft)){
                                // Convert to virtual handle for MMS
                                id = Integer.toString(Integer.valueOf(id)
                                        + MMS_HDLR_CONSTANT);
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " DELETED MMS ID " + id);
                                }
                                String newFolder = getMAPFolder(msgType);
                                if ((newFolder != null)
                                        && (!newFolder.equalsIgnoreCase(BluetoothMasAppIf.Draft))) {
                                    // The message has moved on MAP virtual
                                    // folder representation.
                                    sendMnsEvent(MESSAGE_SHIFT, id,
                                            BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom +
                                            "/" + BluetoothMasAppIf.Msg + "/" +
                                            BluetoothMasAppIf.Draft, "MMS");
                                    if (newFolder.equalsIgnoreCase("sent")) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "MMS");
                                    }
                                }
                            } else if (folderMms != null &&
                                    folderMms.equalsIgnoreCase(BluetoothMasAppIf.Outbox)){
                                String newFolder = getMAPFolder(msgType);
                                // Convert to virtual handle for MMS
                                id = Integer.toString(Integer.valueOf(id)
                                        + MMS_HDLR_CONSTANT);

                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " MESSAGE_SHIFT MMS ID " + id);
                                }
                                if ((newFolder != null)
                                        && (!newFolder.equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                    // The message has moved on MAP virtual
                                    // folder representation.
                                    sendMnsEvent(MESSAGE_SHIFT, id,
                                            BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom + "/"+
                                            BluetoothMasAppIf.Msg + "/"+
                                            BluetoothMasAppIf.Outbox, "MMS");
                                    if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/"+
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "MMS");
                                    }
                                }
                                /* Mms doesn't have failed or queued type
                                 * Cannot send SENDING_FAILURE as there
                                 * is no indication if Sending failed
                                 */
                            }
                        }
                    } else {
                        // TODO - The current(old) query doesn't have this
                        // row;
                        // implies it was added
                    }
                    break;
                case RIGHT:
                    // handle case where a row in cursor2 is unique
                    if (currentCRMmsFolder == CR_MMS_FOLDER_B) {
                        // The new query doesn't have this row; implies it
                        // was deleted
                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                            Log.v(TAG, " MMS DELETED FROM "+folderMms);
                        }
                        String id = crMmsFolderB.getString(crMmsFolderB
                                .getColumnIndex("_id"));
                        int msgInfo = 0;
                        msgInfo = crMmsFolderB.getInt(crMmsFolderB.getColumnIndex("m_type"));
                        String mId = crMmsFolderB.getString(crMmsFolderB.getColumnIndex("m_id"));
                        int msgType = getMmsContainingFolder(Integer
                                .parseInt(id));
                        if (msgType == -1) {
                            // Convert to virtual handle for MMS
                            id = Integer.toString(Integer.valueOf(id)
                                    + MMS_HDLR_CONSTANT);
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " DELETED MMS ID " + id);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(MESSAGE_DELETED, id,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/" +
                                        folderMms, null, "MMS");
                            }
                        } else {
                            if (folderMms != null &&
                                    folderMms.equalsIgnoreCase(BluetoothMasAppIf.Draft)){
                                // Convert to virtual handle for MMS
                                id = Integer.toString(Integer.valueOf(id)
                                        + MMS_HDLR_CONSTANT);
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " DELETED MMS ID " + id);
                                }
                                String newFolder = getMAPFolder(msgType);
                                if ((newFolder != null)
                                        && (!newFolder.equalsIgnoreCase(BluetoothMasAppIf.Draft))) {
                                    // The message has moved on MAP virtual
                                    // folder representation.
                                    sendMnsEvent(MESSAGE_SHIFT, id,
                                            BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom +
                                            "/" + BluetoothMasAppIf.Msg + "/" +
                                            BluetoothMasAppIf.Draft, "MMS");
                                    if (newFolder.equalsIgnoreCase("sent")) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "MMS");
                                    }
                                }
                            } else if (folderMms != null &&
                                    folderMms.equalsIgnoreCase(BluetoothMasAppIf.Outbox)){
                                // Convert to virtual handle for MMS
                                id = Integer.toString(Integer.valueOf(id)
                                        + MMS_HDLR_CONSTANT);
                                if (Log.isLoggable(TAG, Log.VERBOSE)){
                                    Log.v(TAG, " DELETED MMS ID " + id);
                                }
                                String newFolder = getMAPFolder(msgType);
                                if ((newFolder != null)
                                        && (!newFolder.equalsIgnoreCase(BluetoothMasAppIf.Outbox))) {
                                    // The message has moved on MAP virtual
                                    // folder representation.
                                    sendMnsEvent(MESSAGE_SHIFT, id,
                                            BluetoothMasAppIf.Telecom + "/" +
                                            BluetoothMasAppIf.Msg + "/"
                                            + newFolder, BluetoothMasAppIf.Telecom +
                                            "/" + BluetoothMasAppIf.Msg + "/" +
                                            BluetoothMasAppIf.Outbox, "MMS");
                                    if (newFolder.equalsIgnoreCase(BluetoothMasAppIf.Sent)) {
                                        sendMnsEvent(SENDING_SUCCESS, id,
                                                BluetoothMasAppIf.Telecom + "/" +
                                                BluetoothMasAppIf.Msg + "/" + newFolder,
                                                null, "MMS");
                                    }
                                }
                                /* Mms doesn't have failed or queued type
                                 * Cannot send SENDING_FAILURE as there
                                 * is no indication if Sending failed
                                 */
                            }
                        }
                    } else {
                        // The current(old) query doesn't have this row;
                        // implies it was added
                    }
                    break;
                case BOTH:
                    // handle case where a row with the same key is in both
                    // cursors
                    break;
                }
            }
        }
        if (currentCRMmsFolder == CR_MMS_FOLDER_A) {
            currentCRMmsFolder = CR_MMS_FOLDER_B;
        } else {
            currentCRMmsFolder = CR_MMS_FOLDER_A;
        }
        return currentCRMmsFolder;
    }

    /**
     * Check for MMS message being added and send a notification if there is a
     * change
     */
    private void checkMmsAdded() {

        int currentItemCount = 0;
        int newItemCount = 0;

        if (currentCRMms == CR_MMS_A) {
            currentItemCount = crMmsA.getCount();
            crMmsB.requery();
            newItemCount = crMmsB.getCount();
        } else {
            currentItemCount = crMmsB.getCount();
            crMmsA.requery();
            newItemCount = crMmsA.getCount();
        }

        if (Log.isLoggable(TAG, Log.VERBOSE)){
            Log.v(TAG, "MMS current " + currentItemCount + " new " + newItemCount);
        }

        if (newItemCount > currentItemCount) {
            crMmsA.moveToFirst();
            crMmsB.moveToFirst();

            CursorJoiner joiner = new CursorJoiner(crMmsA,
                    new String[] { "_id"}, crMmsB, new String[] { "_id"});

            CursorJoiner.Result joinerResult;
            while (joiner.hasNext()) {
                joinerResult = joiner.next();
                switch (joinerResult) {
                case LEFT:
                    // handle case where a row in cursor1 is unique
                    if (currentCRMms == CR_MMS_A) {
                        // The new query doesn't have this row; implies it
                        // was deleted
                    } else {
                        // The current(old) query doesn't have this row;
                        // implies it was added
                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                            Log.v(TAG, " MMS ADDED TO INBOX ");
                        }
                        String id1 = crMmsA.getString(crMmsA
                                .getColumnIndex("_id"));
                        int msgInfo = 0;
                        msgInfo = crMmsA.getInt(crMmsA.getColumnIndex("m_type"));
                        String mId = crMmsA.getString(crMmsA.getColumnIndex("m_id"));
                        int msgType = getMmsContainingFolder(Integer
                                .parseInt(id1));
                        String folder = getMAPFolder(msgType);
                        if (folder != null &&
                                folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                            // Convert to virtual handle for MMS
                            id1 = Integer.toString(Integer.valueOf(id1)
                                    + MMS_HDLR_CONSTANT);
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED MMS ID " + id1);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(NEW_MESSAGE, id1, 
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "MMS");
                            }
                        } else if (folder != null &&
                                !folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                            // Convert to virtual handle for MMS
                            id1 = Integer.toString(Integer.valueOf(id1)
                                    + MMS_HDLR_CONSTANT);
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED MMS ID " + id1);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(MESSAGE_SHIFT, id1,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "MMS");
                            }
                        }

                        else {
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                            }
                        }
                    }
                    break;
                case RIGHT:
                    // handle case where a row in cursor2 is unique
                    if (currentCRMms == CR_MMS_B) {
                        // The new query doesn't have this row; implies it
                        // was deleted
                    } else {
                        // The current(old) query doesn't have this row;
                        // implies it was added
                        if (Log.isLoggable(TAG, Log.VERBOSE)){
                            Log.v(TAG, " MMS ADDED ");
                        }
                        String id1 = crMmsB.getString(crMmsB
                                .getColumnIndex("_id"));
                        int msgInfo = 0;
                        msgInfo = crMmsB.getInt(crMmsB.getColumnIndex("m_type"));
                        String mId = crMmsB.getString(crMmsB.getColumnIndex("m_id"));
                        int msgType = getMmsContainingFolder(Integer
                                .parseInt(id1));
                        String folder = getMAPFolder(msgType);
                        if (folder != null &&
                                folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                            // Convert to virtual handle for MMS
                            id1 = Integer.toString(Integer.valueOf(id1)
                                    + MMS_HDLR_CONSTANT);

                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED MMS ID " + id1);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(NEW_MESSAGE, id1, 
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "MMS");
                            }
                        } else if (folder != null &&
                                !folder.equalsIgnoreCase(BluetoothMasAppIf.Inbox)) {
                            // Convert to virtual handle for MMS
                            id1 = Integer.toString(Integer.valueOf(id1)
                                    + MMS_HDLR_CONSTANT);

                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED MMS ID " + id1);
                            }
                            if (((msgInfo > 0) && (msgInfo != MSG_META_DATA_TYPE)
                                    && (msgInfo != MSG_DELIVERY_RPT_TYPE))
                                    && (mId != null && mId.length() > 0)){
                                sendMnsEvent(MESSAGE_SHIFT, id1,
                                        BluetoothMasAppIf.Telecom + "/" +
                                        BluetoothMasAppIf.Msg + "/"
                                        + folder, null, "MMS");
                            }
                        } else {
                            if (Log.isLoggable(TAG, Log.VERBOSE)){
                                Log.v(TAG, " ADDED TO UNKNOWN FOLDER");
                            }
                        }
                    }
                    break;
                case BOTH:
                    // handle case where a row with the same key is in both
                    // cursors
                    break;
                }
            }
        }
        if (currentCRMms == CR_MMS_A) {
            currentCRMms = CR_MMS_B;
        } else {
            currentCRMms = CR_MMS_A;
        }
    }
    /**
     * Get the folder name (MAP representation) based on the message Handle
     */
    private int getMmsContainingFolder(int msgID) {
        int folderNum = -1;
        String whereClause = " _id= " + msgID;
        Uri uri = Uri.parse("content://mms/");
        ContentResolver cr = mContext.getContentResolver();
        Cursor cursor = cr.query(uri, null, whereClause, null, null);
        if (cursor.getCount() > 0) {
            cursor.moveToFirst();
            int msgboxInd = cursor.getColumnIndex("msg_box");
            folderNum = cursor.getInt(msgboxInd);
        }
        cursor.close();
        return folderNum;
    }

}
