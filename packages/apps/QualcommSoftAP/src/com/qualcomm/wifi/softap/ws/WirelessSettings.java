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


package com.qualcomm.wifi.softap.ws;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewDebug.FlagToString;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.AdapterView.OnItemClickListener;

import com.qualcomm.wifi.softap.L10NConstants;
import com.qualcomm.wifi.softap.MainMenu;
import com.qualcomm.wifi.softap.QWiFiSoftApCfg;

import com.qualcomm.wifi.softap.R;


/**
 * This class provides an option to configure various flavors of Wireless Settings ie Basic Wireless settings,
 * Wireless security settings and Advanced wireless settings
 *
 *{@link com.qualcomm.wifi.softap.ws.BasicWirelessSettings}
 *{@link com.qualcomm.wifi.softap.ws.WirelessSecuritySettings}
 *{@link com.qualcomm.wifi.softap.ws.AdvancedWirelessSettings}
 */

public class WirelessSettings extends Activity{
	private QWiFiSoftApCfg qwifisoftAPCfg;
	private ListView wsList;
	private String[] wsLstvalue;
	public boolean bPause=false;
	Intent i;
	/**
	 * This method inflates the UI view from <i>ws.xml</i><br> and provides selection to the user
	 * 
	 * @param icicle If the activity is being re-initialized after previously being shut down 
	 * then this Bundle contains the data it most recently supplied in onSaveInstanceState(Bundle)
	 */	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		qwifisoftAPCfg=MainMenu.mSoftAPCfg;
		MainMenu.wirelessSettingsEvent = this;
		setContentView(R.layout.wireless_settings_layout);

		wsLstvalue = getResources().getStringArray(R.array.wslist);
		wsList = (ListView) findViewById(R.id.wslstview);
		wsList.setAdapter(new ArrayAdapter<String>(this,
				android.R.layout.simple_list_item_1, wsLstvalue));

		// User is provided with selection options to click
		wsList.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> a, View view, int position,
					long id) {
				if (wsList.getItemAtPosition(position).equals(
				"Basic Wireless Settings")) {
					Log.d(L10NConstants.TAG_WS, "onItemClick - Basic Wireless Settings");
					i = new Intent(WirelessSettings.this, BasicWirelessSettings.class);					
				} else if (wsList.getItemAtPosition(position).equals(
				"Wireless Security Settings")) {
					Log.d(L10NConstants.TAG_WS, "onItemClick - Wireless Security Settings");
					i = new Intent(WirelessSettings.this, WirelessSecuritySettings.class);
				} else if (wsList.getItemAtPosition(position).equals(
				"Advanced Wireless Settings")) {
					Log.d(L10NConstants.TAG_WS, "onItemClick - Advanced Wireless Settings");
					i = new Intent(WirelessSettings.this, AdvancedWirelessSettings.class);
				}
				startActivity(i);
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
		MainMenu.wirelessSettingsEvent = null;
		Log.d(L10NConstants.TAG_WS,"destroying WirelessSettings");
	}
}
