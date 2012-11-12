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

import java.util.ArrayList;
import java.util.StringTokenizer;
import com.qualcomm.wifi.softap.L10NConstants;
import com.qualcomm.wifi.softap.MainMenu;
import com.qualcomm.wifi.softap.QWiFiSoftApCfg;
import com.qualcomm.wifi.softap.R;


import android.os.Bundle;
import android.os.CountDownTimer;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.util.Log;

public class APstatistics extends PreferenceActivity{	
	private PreferenceScreen prefScr;
	private Preference pref;
	private String sKeyVal;
	private StringTokenizer sToken;	
	private timer timer;
	public static String sTag;
	private QWiFiSoftApCfg mSoftAPCfg;
	private ArrayList<String> apStatArrange;
	/**
	 * This method inflates UI views from <i>station_status.xml</i> It is
	 * getting all the associated stations from the underlined daemon and
	 * updating the screen
	 * 
	 *@param savedInstanceState, This could be null or some state information previously
	 *            saved by the onSaveInstanceState method
	 */
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		MainMenu.apStatisticsEvent = this;
		addPreferencesFromResource(R.xml.ap_statistics);
		prefScr = getPreferenceScreen();
		mSoftAPCfg = MainMenu.mSoftAPCfg;
		sTag = getString(R.string.tag) + "SS";
		apStatArrange = new ArrayList<String>();

		getAPStatisticsList();
		startTimer();
	}

	/**
	 * This methods displays the associated station's MAC Addresses
	 * 
	 * @param macLst
	 *            MAC Address List from underlined daemon
	 */
	private void updateStationLst(String sMacLst) {
		String sPackets = "";		
		String sBytes = "";	
		String sApStaticValue;	
		prefScr.removeAll();	

		sToken = new StringTokenizer(sMacLst);
		String[] sApStatisticsAbbr = getResources().getStringArray(
				R.array.apStatisticsAbbrivations);
		while (sToken.hasMoreTokens()) {
			apStatArrange.add(sToken.nextToken());				
		}	

		for(int i = 0;i<sApStatisticsAbbr.length; i++){
			
			String sApStatTitle = sApStatisticsAbbr[i].substring(sApStatisticsAbbr[i].indexOf("=") + 1, sApStatisticsAbbr[i]
			                                                                                                          .length());
			
			for(int j = 0; j<apStatArrange.size(); j++){
				String sApStat= apStatArrange.get(j).substring(0, apStatArrange.get(j).indexOf("="));				
				sApStaticValue = apStatArrange.get(j).substring(apStatArrange.get(j).indexOf("=") + 1, apStatArrange.get(j)
						.length());
				if(sApStatisticsAbbr[i].contains(sApStat)){
					if((sApStat.contains("RUF")) || (sApStat.contains("TUF"))){					
						sPackets = sApStaticValue;						
						apStatArrange.remove(j);
						break;
					} else if((sApStat.contains("RUB")) || (sApStat.contains("TUB"))){					
						sBytes = sApStaticValue;							
						apStatArrange.remove(j);
						break;
					}
				}
			}
			if(!sPackets.equals("") && !sBytes.equals("")){				
				pref = new Preference(this);
				pref.setTitle(sApStatTitle);
				pref.setSummary("Packets="+sPackets+"  Bytes="+sBytes);
				prefScr.addPreference(pref);				
				sPackets = "";
				sBytes = "";
			}
		}
	}

	public void startTimer() {
		if (timer != null)
			timer.cancel();
		timer = new timer(L10NConstants.APSTAT_TIME, 1000);
		timer.start();
	}

	private class timer extends CountDownTimer {

		public timer(long millisInFuture, long countDownInterval) {
			super(millisInFuture, countDownInterval);
		}

		@Override
		public void onFinish() {
			getAPStatisticsList();		
			startTimer();			
		}

		@Override
		public void onTick(long millisUntilFinished) {

		}
	}

	public void getAPStatisticsList() {
		Log.d(sTag, "Getting Command " + L10NConstants.GET_CMD_PREFIX
				+ "apstat");
		try{
			sKeyVal = mSoftAPCfg.SapSendCommand(L10NConstants.GET_CMD_PREFIX
					+ "apstat");
		}catch(Exception e){
			Log.d(sTag, "Exception :"+e);
		}		
		Log.d(sTag, "Received response " + sKeyVal);		
		// Pulls only mac_addresses from success result sent by daemon
		if (!sKeyVal.equals("")) {
			if (sKeyVal.contains("success")) {
				try{
					int index = sKeyVal.indexOf("=");
					updateStationLst(sKeyVal.substring(index + 1));
				}
				catch (Exception e){					
					Log.e(sTag,"Exceptioon " + e);
				}
			}
		}
	}

	public void onDestroy(){
		super.onDestroy();
		if (timer != null) {
			timer.cancel();
		}
		MainMenu.apStatisticsEvent = null;
		Log.d("APStatistics","destroying APstatistics");
	}

	public void EventHandler(String sEvt) {		
		if(sEvt.contains(L10NConstants.STATION_105)) 
			finish();
	}
}
