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

package org.alljoyn.bus.samples.simpleclient;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

public class Client extends Activity {

    private EditText editText;
    private FoundNameAdapter foundNameAdapter;
    private ListView listView;
    private Menu menu;

    // private Menu menu;

    /** Handler used to post messages from C++ into UI thread */
    private Handler handler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            foundNameAdapter.add((FoundName)msg.obj);
            listView.invalidate();
        }
    };

    /** Called when activity's onCreate is called */
    private native int simpleOnCreate();

    /** Called when activity's onDestroy is called */
    private native void simpleOnDestroy();

    /** Called when user chooses to ping a remote SimpleSerivce. Must be connected to name */
    private native String simplePing(String serviceName, String pingStr);

    /** Called when user selects "Connect" */
    public native boolean connect(String busAddr);

    /** Called when user selects "Disconnect" */
    public native boolean disconnect(String busAddr);

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        foundNameAdapter = new FoundNameAdapter(this.getApplicationContext(), this);
        listView = (ListView) findViewById(R.id.ListView);
        listView.setAdapter(foundNameAdapter);

        editText = (EditText) findViewById(R.id.EditText);
        editText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
                                               public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
                                                   if (actionId == EditorInfo.IME_NULL && event.getAction() == KeyEvent.ACTION_UP) {

                                                       int foundNameCount = foundNameAdapter.getCount();
                                                       for (int i = 0; i < foundNameCount; i++) {
                                                           FoundName fn = foundNameAdapter.getItem(i);
                                                           if (fn.isConnected()) {
                                                               simplePing(fn.getBusName(), editText.getText().toString());
                                                           }
                                                       }
                                                   }
                                                   return true;
                                               }
                                           });
        listView.requestFocus();

        // Initialize the native part of the sample
        int ret = simpleOnCreate();
        if (0 != ret) {
            Toast.makeText(this, "simpleOnCreate failed with  " + ret, Toast.LENGTH_LONG).show();
            finish();
            return;
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        // Don't let this app run once it is off screen (it is confusing for a demo)
        this.finish();
    }

    @Override
    protected void onDestroy() {
        simpleOnDestroy();
        super.onDestroy();
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

    public void closeKeyboard() {
        editText.clearFocus();
    }

    public void ping(String serviceName) {
        simplePing(serviceName, editText.getText().toString());
    }

    public void FoundNameCallback(String busName, String busAddr, String guid)
    {
        Log.e("SimpleClient", String.format("Found name %s at %s", busName, busAddr));
        Message msg = handler.obtainMessage(0);
        msg.obj = new FoundName(busName, busAddr, guid);
        handler.sendMessage(msg);
    }

    public void hideInputMethod()
    {
        InputMethodManager inputManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        inputManager.hideSoftInputFromWindow(editText.getWindowToken(), 0);

    }
    static {
        System.loadLibrary("SimpleClient");
    }
}