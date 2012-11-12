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
import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.preference.Preference.OnPreferenceClickListener;
import android.util.Log;
import android.widget.Toast;

public class AssociatedStationsList extends PreferenceActivity implements OnPreferenceClickListener{
	private AlertDialog asalertdialog;
	private Builder asbuilder;
	private PreferenceScreen prefScr;
	private Preference pref;
	private String sKeyVal;
	private StringTokenizer sToken;
	private ArrayList<Preference> macArrayLst;
	private SharedPreferences defSharPref;
	public String sTag;
	public static String sResponse;
	private QWiFiSoftApCfg mSoftAPCfg;	
	public static MainMenu mainMenuStngs;
	
	/**
	 * This method inflates UI views from <i>station_status.xml</i>
	 * It is getting all the associated stations from the underlined daemon and updating the screen
	 * 
	 *@param savedInstanceState, This could be null or some state information previously saved 
	 * by the onSaveInstanceState method
	 */
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		MainMenu.associatedStationEvent = this;
		mainMenuStngs = MainMenu.mMainMenuRef;
		MainMenu.associatedStationRef = this;
		addPreferencesFromResource(R.xml.station_status);
		prefScr = getPreferenceScreen();
		defSharPref = PreferenceManager.getDefaultSharedPreferences(this);
		mSoftAPCfg = MainMenu.mSoftAPCfg;
		sTag = getString(R.string.tag)+"SS";
		macArrayLst = new ArrayList<Preference>();
		getStationsAssociated();
		Intent returnIntent = new Intent();	
		setResult(RESULT_OK, returnIntent);		
	}

	public void getStationsAssociated(){
		Log.d(sTag,"Getting Command "+L10NConstants.GET_CMD_PREFIX +"sta_mac_list");
		try	{
			sKeyVal = mSoftAPCfg.SapSendCommand(L10NConstants.GET_CMD_PREFIX +"sta_mac_list");
		}catch(Exception e){
			Log.d(sTag, "Exception :"+e);
		}
		Log.d(sTag,"Received response "+sKeyVal);

		// Pulls only mac_addresses from success result sent by daemon   
		if(!sKeyVal.equals("")) {			
			if(sKeyVal.contains("success")) {
				int index = sKeyVal.indexOf("=");
				updateStationLst(sKeyVal.substring(index+1));							
			}
		}
	}

	/**
	 * This methods displays the associated station's MAC Addresses  
	 * 
	 * @param macLst MAC Address List from underlined daemon 
	 */
	public void updateStationLst(String sMacLst){
		prefScr.removeAll();
		macArrayLst.clear();
		sToken = new StringTokenizer(sMacLst);
		while(sToken.hasMoreTokens()){
			String sMacAddress = sToken.nextToken(); 
			pref = new Preference(this);
			pref.setTitle(sMacAddress);	
			pref.setOnPreferenceClickListener(this);			
			prefScr.addPreference(pref);				
			macArrayLst.add(pref);	
		}
	}

	/**
	 * This method displays the dialog box to disassociate the MAC Address else cancel the change
	 * 
	 * @param preference the Preference that was clicked.  
	 * @return true/false on success/failure.
	 */
	public boolean onPreferenceClick(final Preference preference) {
		final String sAlertListVal[] = getResources().getStringArray(R.array.station_lst_opt);
		for (Preference prefLst : macArrayLst) {
			if(prefLst == preference) {
				asbuilder=new AlertDialog.Builder(AssociatedStationsList.this);
				asbuilder.setTitle(L10NConstants.SELECT_TITLE);
				asalertdialog=asbuilder.setItems(sAlertListVal, new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						String[] sItems = sAlertListVal;
						if(sItems[which].equals("Disassociate")){							
							Log.d(sTag,"Sending Command "+L10NConstants.SET_CMD_PREFIX+"disassoc_sta ="+ preference.getTitle());
							try{
								sResponse = mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX + "disassoc_sta=" + preference.getTitle());
							}catch(Exception e){
								Log.d(sTag, "Exception :"+e);
							}
							Log.d(sTag, "Received Response ........: " + sResponse);							
							if(sResponse.equals("success")){					
								prefScr.removePreference(preference);				
							}else
								Toast.makeText(AssociatedStationsList.this, "Could not disassociate the station", 1).show();
						}else if(sItems[which].equals("Add to Allow List")) {
							addMacList(L10NConstants.ALLOW, preference.getTitle().toString());
						}else if(sItems[which].equals("Add to Deny List")) {
							addMacList(L10NConstants.DENY, preference.getTitle().toString());
						}else if(sItems[which].equals("Cancel")) {
							// do nothing
						}			
					}
				}).create();
				asalertdialog.show();
			}
			MainMenu.bPreferenceChanged = true;
		}
		return false;
	}

	public void addMacList(String sType, String sTitle){
		int count = 0;
		boolean bMacCheck = false;
		for (int m = 1; m <= L10NConstants.MAX_LENGTH; m++) {
			String sCheckvalue = defSharPref.getString(sType+m, "");	
			if(!sCheckvalue.equals("")) {
				//Duplicate check for the mac address
				if(sCheckvalue.equals(sTitle)) {
					bMacCheck = true;					
					break;
				} else {					
					count++;
				}
			}
		}
		if (count == L10NConstants.MAX_LENGTH) {
			Toast.makeText(this, "List is Full", 1).show();
		} else if (bMacCheck == true) {
			Toast.makeText(this, "MAC Address is already present", 1).show();
		} else {
			mainMenuStngs.addAllowDenyList(defSharPref, sType, sTitle);
			MainMenu.bPreferenceChanged = true;
		}
	}

	public void EventHandler(String sEvt) {
		if(sEvt.contains(L10NConstants.STATION_105)){ 
			finish();
		} else if(sEvt.contains(L10NConstants.STATION_102) ||
				sEvt.contains(L10NConstants.STATION_103)){
			getStationsAssociated();
		}
	}
	public void onDestroy() {
		super.onDestroy();
		MainMenu.associatedStationEvent = null;
		if(asalertdialog!=null && asalertdialog.isShowing()) 
			asalertdialog.cancel();
		Log.d("AssociatedStation","destroying AssociatedStation");
	}
}
