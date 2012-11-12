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


package com.qualcomm.wifi.softap.ss;

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
 * This class displays the MAC Addresses of all currently associated stations and updating the MAC Addresses 
 * for every association/disassociation of station
 */
public class StationStatus extends Activity {	
	private ListView statusLst;
	private String[] sStatusvalue;
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
		MainMenu.stationEvent = this;		
		setContentView(R.layout.station_status_layout);

		sStatusvalue = getResources().getStringArray(R.array.statuslist);
		statusLst = (ListView) findViewById(R.id.status);
		statusLst.setAdapter(new ArrayAdapter<String>(this,
				android.R.layout.simple_list_item_1, sStatusvalue));
		
		// User is provided with selection options to click
		statusLst.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> a, View view, int position,
					long id) {
				if (statusLst.getItemAtPosition(position).equals(
						"AP Statistics")) {
					Log.d(L10NConstants.TAG_WS, "onItemClick - APstatistics");
					i = new Intent(StationStatus.this, APstatistics.class);
				} else if (statusLst.getItemAtPosition(position).equals(
						"Associated Station List")) {
					Log.d(L10NConstants.TAG_WS, "onItemClick - AssociatedStation");
					i = new Intent(StationStatus.this, AssociatedStationsList.class);
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
		MainMenu.stationEvent = null;
		Log.d("StationStatus","destroying StationStatus");
	}
}
