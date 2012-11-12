/*
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

package org.alljoyn.bus.samples.chat;

import java.lang.reflect.Array;

import android.app.Activity;
import android.app.Dialog;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnKeyListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.RadioButton;
import android.widget.TextView;
import android.widget.Toast;

public class Chat extends Activity {

	private EditText editText;    
    private ArrayAdapter<String> listViewArrayAdapter;
    private ListView listView;
    private Menu menu;

    static final int DIALOG_CONNECT = 1;
    static final int DIALOG_ADVERTISE = 2;
    static final int DIALOG_JOIN_SESSION = 3;

    /** Called when activity's onCreate is called */
    private native int jniOnCreate();

    /** Called when activity's onDestroy is called */
    private native void jniOnDestroy();

    /** Called when activity UI sends a chat message */
    private native int sendChatMsg(String str);

    /** Called when user selects "Disconnect" */
    private native boolean disconnect();

    /** Called when user enters a name to be "Advertised" " */
    private native boolean advertise(String wellKnownName);
    
    /** Called when user wants to join a session */
    private native boolean joinSession(String sessionName);

    /** Handler used to post messages from C++ into UI thread */
    private Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            listViewArrayAdapter.add((String)msg.obj);
        }
    };

    /* Prevent orientation changes */
    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        setContentView(R.layout.choice);
        // Ask what would you like to do Advertise a session or join one
        final RadioButton radio_create_seesion = (RadioButton) findViewById(R.id.radio_create_session);
        final RadioButton radio_join_session = (RadioButton) findViewById(R.id.radio_join_session);
        radio_create_seesion.setOnClickListener(radio_listener);
        radio_join_session.setOnClickListener(radio_listener);
                
        setConnectedState(false);

        // Initialize the native part of the sample
        int ret = jniOnCreate();
        if (0 != ret) {
            Toast.makeText(this, "jniOnCreate failed with  " + ret, Toast.LENGTH_LONG).show();
            finish();
            return;
        }
       
    }

    @Override
    protected void onDestroy() {
        jniOnDestroy();
        super.onDestroy();
    }
    
    private OnClickListener radio_listener = new OnClickListener() {
        public void onClick(View v) {
            // Perform action on clicks
            RadioButton rb = (RadioButton) v;
            if(rb.getId() == R.id.radio_create_session){
            	Toast.makeText(Chat.this, "You selected " + rb.getText(), Toast.LENGTH_LONG).show();
            	showDialog(DIALOG_ADVERTISE);
            }
            else if (rb.getId() == R.id.radio_join_session){
            	Toast.makeText(Chat.this, "You selected " + rb.getText(), Toast.LENGTH_LONG).show();
            	showDialog(DIALOG_JOIN_SESSION);
            }
           

        }
    };
    // Send a message to all connected chat clients
    private void chat(String message) {
        // Send the AllJoyn signal to other connected chat clients
        int ret = sendChatMsg(message);
        if (0 != ret) {
            Log.e("AllJoynChat", String.format("sendChatMsg(%s) failed (%d)", message, ret));
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.mainmenu, menu);
        this.menu = menu;
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
        case R.id.quit:
            finish();
            return true;

        default:
            return super.onOptionsItemSelected(item);
        }
    }

    public void ChatCallback(String sender, String chatStr)
    {
        Message msg = handler.obtainMessage(0);
        msg.obj = String.format("Message from %s: \"%s\"", sender.substring(sender.length() - 10), chatStr);
        handler.sendMessage(msg);
    }

    protected Dialog onCreateDialog(int id) {
        Dialog dialog;
        switch (id) {

        case DIALOG_ADVERTISE:
            dialog = new Dialog(Chat.this);
            dialog.setContentView(R.layout.advertise);
            dialog.setTitle("Name to be advertised:");
            Button okButton = (Button) dialog.findViewById(R.id.AdvertiseOk);
            okButton.setOnClickListener(new OnClickListener() {
                                            public void onClick(View v) {
                                                View r = v.getRootView();
                                                TextView text = (TextView) r.findViewById(R.id.AdvertiseText);
                                                boolean isAdvertised = advertise(text.getText().toString());
                                                Toast.makeText(getApplicationContext(), text.getText().toString(), Toast.LENGTH_LONG).show();
                                                dismissDialog(DIALOG_ADVERTISE);
                                                //setContentView(R.layout.main);

                                            }
                                        });
            Button cancelButton = (Button) dialog.findViewById(R.id.AdvertiseCancel);
            cancelButton.setOnClickListener(new OnClickListener() {
                                                public void onClick(View v) {
                                                    dismissDialog(DIALOG_ADVERTISE);
                                                    finish();
                                                }
                                            });
            break;
        	
        case DIALOG_JOIN_SESSION:
        	dialog = new Dialog(Chat.this);
            dialog.setContentView(R.layout.joinsession);
            dialog.setTitle("Name of session you want to join:");
            Button joinokButton = (Button) dialog.findViewById(R.id.JoinSessionOk);
            joinokButton.setOnClickListener(new OnClickListener() {
                                            public void onClick(View v) {
                                                View r = v.getRootView();
                                                TextView text = (TextView) r.findViewById(R.id.JoinSessionText);
                                                boolean isAdvertised = joinSession(text.getText().toString());
                                                Toast.makeText(getApplicationContext(), text.getText().toString(), Toast.LENGTH_LONG).show();
                                                dismissDialog(DIALOG_JOIN_SESSION);
                                                //setContentView(R.layout.main);
                                            }
                                        });
            Button joinCancelButton = (Button) dialog.findViewById(R.id.JoinSessionCancel);
            joinCancelButton.setOnClickListener(new OnClickListener() {
                                                public void onClick(View v) {
                                                    dismissDialog(DIALOG_JOIN_SESSION);
                                                    finish();
                                                }
                                            });
            break;
        default:
            dialog = null;
            break;
        }
        
        setContentView(R.layout.main);
        
        listViewArrayAdapter = new ArrayAdapter<String>(this, R.layout.message);
        listView = (ListView) findViewById(R.id.ListView);
        listView.setAdapter(listViewArrayAdapter);

        editText = (EditText) findViewById(R.id.EditText);
        editText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
        									public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
        										if (actionId == EditorInfo.IME_NULL && event.getAction() == KeyEvent.ACTION_UP) {
        											String message = view.getText().toString();
        											Log.d("Chat","\n Calling Chat method ");
        											chat(message);
        											view.setText("");
        										}
        										return true;
        									}
        });

       
        return dialog;
    }

    private void setConnectedState(boolean isConnected)
    {
        if (null != menu) {
            menu.getItem(0).setEnabled(!isConnected);
            menu.getItem(1).setEnabled(isConnected);
        }
    }

    static {
        System.loadLibrary("Chat");
    }
}