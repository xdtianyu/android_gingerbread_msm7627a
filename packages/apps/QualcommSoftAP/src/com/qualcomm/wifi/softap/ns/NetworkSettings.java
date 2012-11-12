/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.

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


package com.qualcomm.wifi.softap.ns;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.AdapterView.OnItemClickListener;

import com.qualcomm.wifi.softap.L10NConstants;
import com.qualcomm.wifi.softap.MainMenu;
import com.qualcomm.wifi.softap.R;

/** 
 * This class provides navigation control to MAC filter settings class 
 * which provides adding/removing Allow/Deny list of MAC addresses
 * 
 *{@link com.qualcomm.wifi.softap.ns.MACFilterSettings}
 */
public class NetworkSettings extends Activity {	
	private ListView nwList;
	private String[] sNwLstvalue;
	
	/**
	 * This method inflates MAC Filter Setting View on the screen from <b>network.xml</b>
	 * 
	 * @param icicle, This could be null or some state information previously saved 
	 * by the onSaveInstanceState method 
	 */	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);	
		MainMenu.networkSettingsEvent = this;
		Log.d(L10NConstants.TAG_NS, "onCreate - NetworkSettings");
		setContentView(R.layout.network_settings_layout);
		sNwLstvalue  = getResources().getStringArray(R.array.nwslist);
		nwList = (ListView) findViewById(R.id.nwlstview);
		nwList.setAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, sNwLstvalue));		
		nwList.setOnItemClickListener(new OnItemClickListener()
		{
			public void onItemClick(AdapterView<?> a, View view, int position, long id) 
			{			
				if(nwList.getItemAtPosition(position).equals("MAC Filter Settings") ) {															
					startActivity(new Intent(NetworkSettings.this, MACFilterSettings.class));
				}
			}
		});
	}

	public void EventHandler(String sEvt) {		
		if(sEvt.contains(L10NConstants.STATION_105)){
			setResult(RESULT_CANCELED);
			finish();
		}
	}
	public void onDestroy(){
		super.onDestroy();
		MainMenu.networkSettingsEvent = null;
		Log.d(L10NConstants.TAG_NS,"destroying NetworkSettings");
	}
}
