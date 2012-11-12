
/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 */


package com.qualcomm.wifi.softap;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.ProgressDialog;
import android.app.AlertDialog.Builder;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Looper;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import com.qualcomm.wifi.softap.ns.MACFilterSettings;
import com.qualcomm.wifi.softap.ns.NetworkSettings;
import com.qualcomm.wifi.softap.ss.APstatistics;
import com.qualcomm.wifi.softap.ss.AssociatedStationsList;
import com.qualcomm.wifi.softap.ss.StationStatus;
import com.qualcomm.wifi.softap.ws.AdvancedWirelessSettings;
import com.qualcomm.wifi.softap.ws.BasicWirelessSettings;
import com.qualcomm.wifi.softap.ws.WEPSettings;
import com.qualcomm.wifi.softap.ws.WPASettings;
import com.qualcomm.wifi.softap.ws.WirelessSecuritySettings;
import com.qualcomm.wifi.softap.ws.WirelessSettings;
/** 
 * The class contains the Application Main screen display, it provides UI to turn on/turn off
 * softAP and options for softAP settings. It does provide the 'Save Settings' button to save
 *  
 * {@link com.qualcomm.wifi.softap.ws.BasicWirelessSettings}<br>
 * {@link com.qualcomm.wifi.softap.ns.NetworkSettings}<br>
 * {@link com.qualcomm.wifi.softap.mgmt.Management}<br>
 * {@link com.qualcomm.wifi.softap.prof.ProfileSettings}<br>
 */
public class MainMenu extends PreferenceActivity implements OnPreferenceClickListener, 
OnPreferenceChangeListener, OnClickListener, QWiFiSoftApEvent {	
	private boolean bShutDownEvent=false ;
	//public static boolean bEnergyModeCheck=false;	
	private String sKeyVal;	
	private int iIndex;
	private String sResponse ;	
	public static String sTag;
	public static String sWpsResponse, sCommitResponse;
	private String sAllowAddList = new String("");
	private String sAllowRemoveList = new String("");
	private String sDenyAddList = new String("");
	private String sDenyRemoveList = new String("");
	private String[] sDefaultKey;
	public static boolean bStatus = false, bPreferenceChanged;
	private String sCheckKeyValue;	
	private boolean bIsValid = false;
	private String sAppVersion;
	private String sAndroidSdkVersion,sSDKVersion;
	private String sSdkVersion ="";
	private String sBuild="";
	private String sDisplay="";
	private ProgressDialog onDlg, offDlg, saveDlg, resetDlg, initialDlg;
	private AlertDialog alertDialog, shutdownEvtDialog, noChngsShutdownEvtDialog;
	private Builder dialogBuilder, shutdownEvtDlgBuilder, noChngsShutdownEvtDlgBuilder;

	public static Button btnSave, btnReset;	
	private Preference wirelessSettPref, networkSettPref, statusPref;
	private CheckBoxPreference wifiCheckEnable;
	private SharedPreferences.Editor defPrefEditor, orgPrefEditor;	
	private SharedPreferences defSharPref, orgSharPref;
	private NotificationManager notifyManager;

	private ActivityManager activityMgr;
	public static MainMenu mMainMenuRef;
	private TimeOut mTimer;
	private static Looper looper;
	public static Context context;
	private Intent intent;

	public static QWiFiSoftApCfg mSoftAPCfg;	
	public static AssociatedStationsList associatedStationRef;	
	private ArrayList<String> changesList = new ArrayList<String>();	
	public static QWiFiSoftApEvent mainEvent;
	public static MACFilterSettings macFilterEvent;
	public static NetworkSettings networkSettingsEvent;
	public static WirelessSettings wirelessSettingsEvent;
	public static StationStatus stationEvent;
	public static BasicWirelessSettings basicWirelessEvent;
	public static APstatistics apStatisticsEvent;
	public static AssociatedStationsList associatedStationEvent;
	public static WirelessSecuritySettings wirelessSecurityEvent;
	public static WEPSettings wirelessSecurityWEPEvent;
	public static WPASettings wirelessSecurityWPAPSKEvent;
	public static AdvancedWirelessSettings advancedWirelessEvent;
    
	public void broadcastEvent(String event) {		
		if(mainEvent != null)
			mainEvent.EventHandler(event);
		if(macFilterEvent != null)
			macFilterEvent.EventHandler(event);
		if(networkSettingsEvent != null)
			networkSettingsEvent.EventHandler(event);
		if(apStatisticsEvent != null)
			apStatisticsEvent.EventHandler(event);
		if(associatedStationEvent != null)
			associatedStationEvent.EventHandler(event);
		if(stationEvent != null)
			stationEvent.EventHandler(event);
		if(advancedWirelessEvent != null)
			advancedWirelessEvent.EventHandler(event);
		if(basicWirelessEvent != null)
			basicWirelessEvent.EventHandler(event);
		if(wirelessSecurityEvent != null)
			wirelessSecurityEvent.EventHandler(event);
		if(wirelessSettingsEvent != null)
			wirelessSettingsEvent.EventHandler(event);
		if(wirelessSecurityWEPEvent != null)
			wirelessSecurityWEPEvent.EventHandler(event);
		if(wirelessSecurityWPAPSKEvent != null)
			wirelessSecurityWPAPSKEvent.EventHandler(event);
	}

	/**
	 * This method handles any events received in the string format. two main events Association and Dissociation,
	 * for Association the wps dialog if launched, is dismissed and for either Association/Disassociation the station status
	 * Activity if running, will be restarted. 
	 */
	public void EventHandler(String evt) {		
		Log.e(sTag, evt);		 
		showNotification(evt);
		if(evt.contains(L10NConstants.STATION_105)){
			Log.d(sTag, "Setting shutdown event");
			bShutDownEvent = true;			

			if(wirelessSettingsEvent == null && networkSettingsEvent == null 
					&& stationEvent == null ){
				bShutDownEvent = false;
				mMainMenuRef.runOnUiThread(new Runnable(){
					public void run() {						
						shutdownEvent();						
					}					
				});				
			}

			if(!activityMgr.getRunningTasks(1).get(0).baseActivity.getClassName().
					equals("com.qualcomm.wifi.softap.MainMenuSettings")&& bShutDownEvent){
				Log.e(sTag,"base activity:"+activityMgr.getRunningTasks(1).get(0).baseActivity.getClassName());
				bShutDownEvent = false;
				mMainMenuRef.runOnUiThread(new Runnable(){
					public void run() {						
						shutdownEvent();						
					}					
				});		
			}			
		}
		broadcastEvent(evt);
	}

	/**
	 * This method initialize all the resources such as preferences, notification manager.
	 * It also set the status checked/unchecked based on the daemon value
	 */
	@Override
	protected void onCreate(Bundle savedInstanceState) {		
		super.onCreate(savedInstanceState);
		mMainMenuRef = this;
		activityMgr = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
		addPreferencesFromResource(R.xml.main_menu_settings);		

		//get the application context
		context = getApplicationContext();

		setContentView(R.layout.main_menu_layout);
		sTag = getString(R.string.tag) + "MainMenu";

		//get the reference to QWiFiSoftApCfg class object
		mSoftAPCfg = new QWiFiSoftApCfg(this);

		//Create notification manager
		notifyManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);

		//Edit default and original file
		defSharPref = PreferenceManager.getDefaultSharedPreferences(this);
		defPrefEditor = defSharPref.edit();	
		orgSharPref = getSharedPreferences(L10NConstants.CONFIG_FILE_NAME, MODE_PRIVATE);
		orgPrefEditor = orgSharPref.edit();

		//get the reference to the WI-FI AP,wireless setting,network settings, status 
		wirelessSettPref = (Preference) findPreference(getString(R.string.ws_key));
		networkSettPref = (Preference) findPreference(getString(R.string.ns_key));
		statusPref = (Preference) findPreference(getString(R.string.station_status_key));
		wifiCheckEnable = (CheckBoxPreference) findPreference(getString(R.string.wifi_status_key));
		wirelessSettPref.setOnPreferenceClickListener(this);
		networkSettPref.setOnPreferenceClickListener(this);	
		statusPref.setOnPreferenceClickListener(this);
		wifiCheckEnable.setOnPreferenceChangeListener(this);
		wifiCheckEnable.setOnPreferenceClickListener(this);

		//get the reference to the save and reset buttons and set OnClick listener
		btnSave = (Button) findViewById(R.id.save);
		btnReset = (Button) findViewById(R.id.reset);
		btnSave.setOnClickListener(this);
		btnReset.setOnClickListener(this);

		//set initial save button to false
		btnSave.setEnabled(false);
		bPreferenceChanged = false;

		//get command for enable_softap to get the response from the daemon		
		Log.d(sTag, "Getting Command "+ L10NConstants.GET_CMD_PREFIX + L10NConstants.ENABLE_SOFTAP);
		try{
			sKeyVal = mSoftAPCfg.SapSendCommand(L10NConstants.GET_CMD_PREFIX + L10NConstants.ENABLE_SOFTAP);
			sSDKVersion = mSoftAPCfg.SapSendCommand(L10NConstants.GET_CMD_PREFIX + L10NConstants.SDK_VERSION);
			
			if(sSDKVersion.contains(L10NConstants.SUCCESS)){
				sSdkVersion = sSDKVersion.substring(sSDKVersion.indexOf("=")+1,sSDKVersion.length());
			}
			
		}catch(Exception e){
			Log.d(sTag, "Exception:"+ e);
		}
		Log.d(sTag, "Received response " + sKeyVal);

		//On success from the daemon for enable_softap start a thread to display a looper progress dialog
		//and enable the options
		if(sKeyVal.contains(L10NConstants.SUCCESS)) {
			if(sKeyVal.contains(L10NConstants.VAL_ONE)) {
				new DialogThr(L10NConstants.DIALOG_INITIAL);
				enableOrDisable(true, L10NConstants.VAL_ONE);				
				if(initialDlg != null)
					initialDlg.cancel();
			} else
				enableOrDisable(false, L10NConstants.VAL_ZERO);			
		} else 
			wifiCheckEnable.setEnabled(false);

		orgPrefEditor.putString("wpsKey", "");
		orgPrefEditor.commit();

		//if Wi-Fi app check box is unchecked set both save and reset button disabled
		if(!wifiCheckEnable.isChecked()){
			btnSave.setEnabled(false);
			btnReset.setEnabled(false);
		}
		intent = new Intent();	
		
		try {			
		    ComponentName comp = new ComponentName(context, MainMenu.class);
		    PackageInfo pinfo = context.getPackageManager().getPackageInfo(comp.getPackageName(), 0);
		    sAppVersion = "GUI Version : " + pinfo.versionName;
		    sAndroidSdkVersion = "Android SDK Release : "+Build.VERSION.RELEASE;
		    sSdkVersion = "SoftAP SDK Version : "+sSdkVersion;
		    sBuild = "Build Version : "+Build.DISPLAY;
		    sDisplay = "Platform : "+Build.BOARD;
		  } catch (android.content.pm.PackageManager.NameNotFoundException e) {		    
		  }
	}

	public boolean onCreateOptionsMenu(Menu menu) {
		menu.add(Menu.NONE, L10NConstants.MENU_ABOUT, Menu.NONE, "About").setIcon(R.drawable.ic_menu_about);		
		return true;
	}

	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case L10NConstants.MENU_ABOUT:		
			LayoutInflater factory = LayoutInflater.from(this);
			final View textEntryView = factory.inflate(R.layout.about_dlg_layout, null);
			TextView txtVersionName = (TextView) textEntryView.findViewById(R.id.txt_version_name); 
			TextView txtSDKVer = (TextView) textEntryView.findViewById(R.id.txt_sdk_version);
			TextView txtAndroidSDKVer = (TextView) textEntryView.findViewById(R.id.txt_android_sdk_version);
			TextView txtBoard = (TextView) textEntryView.findViewById(R.id.txt_board);
			TextView txtDisplay = (TextView) textEntryView.findViewById(R.id.txt_display);
			txtVersionName.setText(sAppVersion);			
			txtAndroidSDKVer.setText(sAndroidSdkVersion);
			txtSDKVer.setText(sSdkVersion);
			txtBoard.setText(sDisplay);
			txtDisplay.setText(sBuild);
			AlertDialog aboutDialog = new AlertDialog.Builder(this).create();
			aboutDialog.setTitle(getString(R.string.str_act_about_title));
			aboutDialog.setIcon(R.drawable.ic_menu_about);
			aboutDialog.setView(textEntryView);	
			aboutDialog.setButton("OK", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					// Do Nothing
				} });			
			aboutDialog.show();
			return true;		
		}
		return false;
	}

	/**
	 * This method handles the 'Save Settings' and 'Reset Settings' button click events.
	 */
	public void onClick(View v) {
		int id = v.getId();
		switch(id) {
		case R.id.save:
			new DialogThr(L10NConstants.DIALOG_SAVE);
			saveChanges(defSharPref, orgSharPref, "");
			btnSave.setEnabled(false);
			if(saveDlg != null)
				saveDlg.cancel();		
			break;
		case R.id.reset:
			new DialogThr(L10NConstants.DIALOG_RESET);
			try {
				mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX + "reset_to_default");
			} catch(Exception e) {
				Log.d(sTag, "Exception:"+ e);
			}
			getSoftAPValues();
			btnSave.setEnabled(false);
			if(resetDlg != null) 
				resetDlg.cancel();
			bPreferenceChanged = false;
			break;
		}		
	}

	/**
	 * This method issues the get command to set all the values of softAP.
	 */
	public void getSoftAPValues() {
		sDefaultKey = getResources().getStringArray(R.array.defaultProfileKeys);		
		for(int j = 0; j < sDefaultKey.length; j++) {
			Log.d(sTag, "Getting Command "+L10NConstants.GET_CMD_PREFIX +sDefaultKey[j]);
			try {
				sKeyVal = mSoftAPCfg.SapSendCommand(L10NConstants.GET_CMD_PREFIX +sDefaultKey[j]);
			}catch(Exception e){
				Log.d(sTag, "Exception:"+ e);
			}			
              
			Log.d(sTag, "Received response " + sKeyVal);	
			iIndex = sKeyVal.indexOf("=");

			if(sKeyVal.contains(L10NConstants.SUCCESS)) {
				sCheckKeyValue = sKeyVal.substring(sKeyVal.indexOf(" ")+1, iIndex);

				if(sCheckKeyValue.equals(sDefaultKey[j])) {						
					if(sDefaultKey[j].equals(L10NConstants.CHNL_KEY)) {
						if(sKeyVal.substring(iIndex+1, sKeyVal.length()).contains(",")) {							
							sCheckKeyValue = sKeyVal.substring(iIndex+1,sKeyVal.indexOf(","));
							defPrefEditor.putString("autoChannel", sCheckKeyValue);
							orgPrefEditor.putString("autoChannel", sCheckKeyValue);
							sCheckKeyValue = sKeyVal.substring(sKeyVal.indexOf(",")+1,sKeyVal.length());
							defPrefEditor.putString(L10NConstants.CHNL_KEY, sCheckKeyValue);
							orgPrefEditor.putString(L10NConstants.CHNL_KEY, sCheckKeyValue);
							commitPref();
						} else {
							SetDefaultValues(sDefaultKey[j]);
						}
					} else if(sDefaultKey[j].equals(L10NConstants.ALLOW_LIST)) {				
						getAllowDenyValue(sDefaultKey[j],L10NConstants.ALLOW_LIST);
					} else if(sDefaultKey[j].equals(L10NConstants.DENY_LIST)) {				
						getAllowDenyValue(sDefaultKey[j],L10NConstants.DENY_LIST);
					} else {
						if(sDefaultKey[j].equals(L10NConstants.COUNTRY_KEY)) {							
							sKeyVal = sKeyVal.substring(0, sKeyVal.trim().length()-1);
							Log.d(sTag, "Getting Value and Removing I in Country Code :"+sKeyVal+":");
						} else if(sDefaultKey[j].equals("wep_key0") || sDefaultKey[j].equals("wep_key1")||												
								sDefaultKey[j].equals("wep_key2") || sDefaultKey[j].equals("wep_key3")) {
							if(sKeyVal.contains("\"")) {
								Log.d(sTag, "WEP_KEy"+ sKeyVal.substring(0,sKeyVal.indexOf("=")+1)
										.concat(sKeyVal.substring(sKeyVal.indexOf("=")+2,sKeyVal.length()-1)));
								sKeyVal = sKeyVal.substring(0,sKeyVal.indexOf("=")+1)
								.concat(sKeyVal.substring(sKeyVal.indexOf("=")+2,sKeyVal.length()-1));
							}
						}
						SetDefaultValues(sDefaultKey[j]);						
					}
				}
			} else {
			if(sDefaultKey[j].equals(L10NConstants.AP_SHUT_TIMER)) {
					defPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ZERO);
					orgPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ZERO);
				} else if (sDefaultKey[j].equals(L10NConstants.REGULATORY_DOMAIN)) {
					defPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONE);
					orgPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONE);
				}else if (sDefaultKey[j].equals(L10NConstants.COUNTRY_KEY)) {
					defPrefEditor.putString(sDefaultKey[j], "US,REGDOMAIN_FCC");
					orgPrefEditor.putString(sDefaultKey[j], "US,REGDOMAIN_FCC");
					defPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONE);
					orgPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONE);
				} else if(sDefaultKey[j].equals(L10NConstants.ENERGY_DETECT_THRESHOLD)){
					defPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONEHUNDREDTWENTYEIGHT);
					orgPrefEditor.putString(sDefaultKey[j], L10NConstants.VAL_ONEHUNDREDTWENTYEIGHT);
				}
				else {
					defPrefEditor.putString(sDefaultKey[j], "");
					orgPrefEditor.putString(sDefaultKey[j], "");
				}
			
				commitPref();
			}
		}
	}

	/**
	 * This method shows a dialog box to save settings when application is tried to close without clicking 'Save Settings'
	 * button for changes.
	 */
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		super.onKeyDown(keyCode, event);		
		if(keyCode == KeyEvent.KEYCODE_BACK) {
			if(btnSave.isEnabled()) {
				dialogBuilder = new AlertDialog.Builder(this);
				dialogBuilder.setIcon(android.R.drawable.ic_dialog_alert);
				dialogBuilder.setTitle(getString(R.string.alert_save));
				alertDialog = dialogBuilder.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {	
						saveChanges(defSharPref, orgSharPref, "");
						btnSave.setEnabled(false);						
						finish();
					}
				}).setNegativeButton("No", new DialogInterface.OnClickListener() {					
					public void onClick(DialogInterface dialog, int which) {						
						SetOriginalValues();						
						finish();
					}
				}).create();
				alertDialog.show();
			}			
			return true;
		} else {			
			return super.onKeyDown(keyCode, event);				
		}
	}

	/**
	 * This method configures the softAP with changes made by the user.
	 * 
	 * @param defSharPref Copy Preference file
	 * @param orgSharPref Original Preference file	 
	 * @param bws_flag Basic Wireless Settings
	 */
	public void saveChanges(SharedPreferences defSharPref, SharedPreferences orgSharPref, 
			String bws_flag) {
		int successCount = 0;
		int unsupportedCount = 0;	
		defPrefEditor = defSharPref.edit();	
		orgPrefEditor = orgSharPref.edit();
		ArrayList<String> changedCmdLst = comparePreferenceFiles(defSharPref, orgSharPref);
		Log.d(sTag, "LIST "+ changedCmdLst);
		if(changedCmdLst != null){
			for(int i = 0; i < changedCmdLst.size(); i++) {
				Log.d(sTag, "Sending Command "+L10NConstants.SET_CMD_PREFIX +changedCmdLst.get(i));
				try {
					sResponse = mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX + changedCmdLst.get(i));
				} catch(Exception e) {
					Log.d(sTag, "Exception:"+ e);
				}				
				
				String sSetKey = changedCmdLst.get(i).substring(0, changedCmdLst.get(i).indexOf("="));
				String sSetValue = changedCmdLst.get(i).substring(changedCmdLst.get(i).indexOf("=")+1, 
						changedCmdLst.get(i).length());
				if(sSetKey.equals(L10NConstants.ENERGY_DETECT_THRESHOLD)) {
					sResponse=L10NConstants.SUCCESS;
				}
				Log.d(sTag, "Received Response "+sResponse);
				if(sSetKey.equals(L10NConstants.WPS_KEY)) {
					sWpsResponse = sResponse;
				}
				if(sResponse.equals(L10NConstants.SUCCESS)) {
					if(sSetKey.equals(L10NConstants.ADD_TO_ALLOW_LIST) || sSetKey.equals(L10NConstants.ADD_TO_DENY_LIST)) {
						int index = changedCmdLst.get(i).indexOf("=");
						String value = changedCmdLst.get(i).substring(index+1, changedCmdLst.get(i).length());						
						StringTokenizer st = new StringTokenizer(value);									
						while(st.hasMoreElements()) {
							String nextToken = st.nextToken();																	
							if(sSetKey.contains(L10NConstants.ALLOW)) {	
								addAllowDenyList(orgSharPref, L10NConstants.ALLOW,nextToken);
							} else {	
								addAllowDenyList(orgSharPref, L10NConstants.DENY, nextToken);																			
							}									
						}
					} else if(sSetKey.equals(L10NConstants.REMOVE_FROM_ALLOW_LIST) 
							|| sSetKey.equals(L10NConstants.REMOVE_FROM_DENY_LIST)) {
						int index = changedCmdLst.get(i).indexOf("=");
						String value = changedCmdLst.get(i).substring(index+1, changedCmdLst.get(i).length());						
						StringTokenizer st = new StringTokenizer(value);
						while(st.hasMoreElements()) {										
							String nextToken = st.nextToken();
							if(sSetKey.contains(L10NConstants.ALLOW)) {	
								removeAllowDenyList(orgSharPref, L10NConstants.ALLOW, nextToken);								
							} else {
								removeAllowDenyList(orgSharPref, L10NConstants.DENY, nextToken);								
							}										
						}				
					} else if(sSetKey.equals(L10NConstants.AP_SHUT_TIMER)) {
						Log.d(sTag, "Changed Ap Shutdown timer Code : "+changedCmdLst.get(i));							
						orgPrefEditor.putString(sSetKey, defSharPref.getString(L10NConstants.AP_SHUT_TIMER, ""));
						commitPref();			
					} else {						
						if(sSetValue.contains("\"")) {							
							sSetValue = sSetValue.substring(1, sSetValue.length()-1);
						}
						if(sSetKey.equals(L10NConstants.COUNTRY_KEY)) {
							Log.d(sTag, "Changed Country Code : "+changedCmdLst.get(i));							
							orgPrefEditor.putString(sSetKey, defSharPref.getString(L10NConstants.COUNTRY_KEY, ""));
							commitPref();
						}
						/*if(sSetKey.equals(L10NConstants.ENERGY_DETECT_THRESHOLD)) {
							Log.d(sTag, "Changed energy_detect_threshold : "+changedCmdLst.get(i));							
							orgPrefEditor.putString("energy_threshold", defSharPref.getString(L10NConstants.ENERGY_DETECT_THRESHOLD, ""));							
						} */else {
							orgPrefEditor.putString(sSetKey, sSetValue);							
						}
						commitPref();
					} 
					successCount++;
				} else {
								sSetValue = orgSharPref.getString(sSetKey, "");
					defPrefEditor.putString(sSetKey, sSetValue);
					defPrefEditor.commit();
					unsupportedCount++;					
				} 
			}
			if(changedCmdLst.size() == (successCount+unsupportedCount)) {
				try {
					sResponse = mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX +"commit");
				} catch(Exception e) {
					Log.d(sTag, "Exception:"+ e);
				}
				sCommitResponse = sResponse;				
				Log.d(sTag, "Received Response ..... "+sResponse);
				Log.d(sTag, "Commited Changes");	
				if(!sResponse.equals(L10NConstants.SUCCESS)) {
					for(int i = 0; i < changedCmdLst.size() ; i++) {					
						if(changedCmdLst.get(i).contains(L10NConstants.WPS_KEY)) {							
							defPrefEditor.putString(L10NConstants.WPS_KEY, L10NConstants.VAL_ZERO);
							orgPrefEditor.putString(L10NConstants.WPS_KEY, L10NConstants.VAL_ZERO);
							commitPref();
						}
					}
				}
			}
			bPreferenceChanged = false;
		}
		if(bIsValid) {
			showWarningDialog(bws_flag, defSharPref, orgSharPref);
			bIsValid = false;
		}
	}

	/**
	 * This method will remove the particular MAC Address from the original preference file
	 *  
	 * @param orgFile Original Preference File Name
	 * @param lstType MAC Address List Type
	 * @param macAddr MAC Address, which has to be removed 
	 */	
	private void removeAllowDenyList(SharedPreferences orgFile,
			String lstType, String macAddr) {
		for( int m = 1; m <= L10NConstants.MAX_LENGTH; m++) {												
			String findVal = orgFile.getString(lstType+m, "");	
			if(macAddr.equals(findVal)) {
				orgPrefEditor.putString(lstType+m, "");
				orgPrefEditor.commit();
				for(int l = 1; l < L10NConstants.MAX_LENGTH; l++ ) {
					if(orgFile.getString(lstType+l,"").equals("")) {
						String val = orgFile.getString(lstType+(l+1),"");
						orgPrefEditor.putString(lstType+l,val);	
						orgPrefEditor.putString(lstType+(l+1),"");
						orgPrefEditor.commit();
					}										
				}
			}
		}		
	}

	/**
	 * This method will add MAC address in the Original Preference file
	 * 
	 * @param orgFile Original Preference File Name
	 * @param lstType MAC Address List Type
	 * @param macAddr MAC Address, which has to be added
	 */
	public void addAllowDenyList(SharedPreferences orgFile,
			String lstType, String macAddr) {
		SharedPreferences.Editor editor = orgFile.edit();
		for (int m = 1; m <= L10NConstants.MAX_LENGTH; m++) {
			String checkvalue = orgFile.getString(lstType+m, "");	
			if(checkvalue.equals("")) {
				editor.putString(lstType+m, macAddr);
				editor.commit();
				break;
			}
		}	
	}

	/**
	 * This thread implements a method for launching different progress bar dialog boxes.
	 */
	public class DialogThr implements Runnable {
		int dialogID;
		Thread thread;
		/**
		 * Its a thread initializer and also starts the thread. 
		 */
		public DialogThr(int id) {
			dialogID=id;
			thread = new Thread(this);
			thread.start();
		}
		/**
		 * Thread body to launch the  progress bar dialog.
		 */
		public void run() {
			Looper.prepare();	
			looper = Looper.myLooper();
			switch(dialogID) {
			case L10NConstants.DIALOG_OFF: 
				showDialog(L10NConstants.DIALOG_OFF);
				break;
			case L10NConstants.DIALOG_ON: 
				showDialog(L10NConstants.DIALOG_ON);
				break;
			case L10NConstants.DIALOG_SAVE: 
				showDialog(L10NConstants.DIALOG_SAVE);
				break;
			case L10NConstants.DIALOG_RESET: 
				showDialog(L10NConstants.DIALOG_RESET);
				break;
			case L10NConstants.DIALOG_INITIAL: 
				showDialog(L10NConstants.DIALOG_INITIAL);
				break;			
			}
			Looper.loop();
		}
	}

	/**
	 * This method handles the preference click events to redirect to the corresponding preference activity screen.
	 */
	public boolean onPreferenceClick(Preference preference) {
		if (preference == wirelessSettPref) {			
			intent.setClass(MainMenu.this, WirelessSettings.class);			
			startActivityForResult(intent, 0);
		} else if (preference == networkSettPref) {			
			intent.setClass(MainMenu.this, NetworkSettings.class);		
			startActivityForResult(intent, 1);
		} else if (preference == statusPref) {		
			intent.setClass(MainMenu.this, StationStatus.class);		
			startActivityForResult(intent, 2);	
		} 
		return true;
	}	

	/**
	 * This method handles the preference change event for Wi-Fi Turn Off/Turn On. 
	 */
	public boolean onPreferenceChange(Preference preference, Object newValue) {
		boolean isValid = false;		
		if (preference == wifiCheckEnable) {			
			if (wifiCheckEnable.isChecked()) {
				new DialogThr(L10NConstants.DIALOG_OFF);				
				Log.d(sTag, "Notification Removed");
				notifyManager.cancel(R.string.notification);			
				Log.d(sTag, "Sending Command "+L10NConstants.SET_CMD_PREFIX 
						+ L10NConstants.ENABLE_SOFTAP+"="+L10NConstants.VAL_ZERO);
				try{
					sResponse = mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX 
							+ L10NConstants.ENABLE_SOFTAP+"="+L10NConstants.VAL_ZERO);
				}catch(Exception e){
					Log.d(sTag, "Exception:"+ e);
				}
				Log.d(sTag, "Received Response ........: " + sResponse);				
				if(sResponse.equals(L10NConstants.SUCCESS)) {					
					isValid = enableOrDisable(false, L10NConstants.VAL_ZERO);//enableFalse();					
				} else {
					isValid = enableOrDisable(true, L10NConstants.VAL_ONE);//enableTrue();
				}				
				if(offDlg != null)
					offDlg.cancel();
			} else {
				new DialogThr(L10NConstants.DIALOG_ON);				
				Log.d(sTag, "Sending Command "+L10NConstants.SET_CMD_PREFIX 
						+ L10NConstants.ENABLE_SOFTAP+"="+L10NConstants.VAL_ONE);
				try {
					sResponse = mSoftAPCfg.SapSendCommand(L10NConstants.SET_CMD_PREFIX 
							+ L10NConstants.ENABLE_SOFTAP+"="+L10NConstants.VAL_ONE);
				} catch(Exception e) {
					Log.d(sTag,"Exception:"+ e);
				}
				if(sResponse.equals(L10NConstants.SUCCESS)) {					
					Log.d(sTag, "Received response "+sResponse);					
					isValid = enableOrDisable(true, L10NConstants.VAL_ONE);//enableTrue();					
				} else {
					Log.d(sTag, "Received response "+sResponse);
					isValid = enableOrDisable(false, L10NConstants.VAL_ZERO);//enableFalse();					
				}
				if(onDlg != null)
					onDlg.cancel();
			}		
		}
		return isValid; 
	}

	/**
	 * This method will Enable or Disable the WifiSoftAP based on the parameters
	 * 
	 * @param Enable if bStatus is True or Disable
	 * @param Value to update the preference file
	 * 
	 * @return Returns <b> bStatus <b> Parameter
	 */
	private boolean enableOrDisable(boolean bStatus, String sVal) {	
		if(bStatus) {
			getSoftAPValues();						
			showNotification();	
			Log.d(sTag, "Launching APEventMonitor Thread");
			mSoftAPCfg.SapStartEventMonitor();
		} else {
			btnSave.setEnabled(bStatus);		
			bPreferenceChanged = bStatus;
			mSoftAPCfg.SapStopEventMonitor();
		}
		orgPrefEditor.putString(L10NConstants.ENABLE_SOFTAP, sVal);
		defPrefEditor.putString(L10NConstants.ENABLE_SOFTAP, sVal);		
		defPrefEditor.putBoolean(L10NConstants.WIFI_AP_CHECK, bStatus);		
		commitPref();	
		wifiCheckEnable.setChecked(bStatus);
		btnReset.setEnabled(bStatus);	
		return bStatus;
	}

	/**
	 * This method shows a notification in the status bar for station events.
	 * 
	 * @param New Joined Station MAC Address
	 */
	private void showNotification(String event) {		
		CharSequence text = event;
		Notification notification = new Notification(R.drawable.ic_ap_notify, text, System.currentTimeMillis());
		PendingIntent contentIntent = PendingIntent.getActivity(this, 0,new Intent(this, MainMenu.class), 0);
		notification.setLatestEventInfo(this, text, text, contentIntent);
		notifyManager.notify(L10NConstants.EVENT_ID, notification);
		notifyManager.cancel(L10NConstants.EVENT_ID);
	}

	/**
	 * This method shows a notification in the status bar for Wi-Fi softAP Turn On/Turn Off events.
	 */
	private void showNotification() {		
		CharSequence text = getText(R.string.notification);
		Notification notification = new Notification(R.drawable.ic_ap_notify,text, System.currentTimeMillis());
		PendingIntent contentIntent = PendingIntent.getActivity(this, 0,new Intent(this, MainMenu.class), 0);
		notification.setLatestEventInfo(this, text, text, contentIntent);
		notifyManager.notify(R.string.notification, notification);
	}

	public void onResume(){
		super.onResume();

		if(bPreferenceChanged) {	
			btnSave.setEnabled(true);
		}
	}

	public void onPause() {
		super.onPause();
	}

	/**
	 * This method displays different progress bar dialogs. 
	 */
	protected Dialog onCreateDialog(int id) {
		switch (id) {
		case L10NConstants.DIALOG_OFF: 			
			offDlg = dialogCreater("Turning off SoftAP...");
			return offDlg;	
		case L10NConstants.DIALOG_ON:			
			onDlg = dialogCreater("Turning on SoftAP...");
			return onDlg;		
		case L10NConstants.DIALOG_RESET:
			resetDlg = dialogCreater("Resetting SoftAP...");
			return resetDlg;		
		case L10NConstants.DIALOG_SAVE:
			saveDlg = dialogCreater("Applying Changes to SoftAP...");
			return saveDlg;		
		case L10NConstants.DIALOG_INITIAL:			
			initialDlg = dialogCreater("Initializing SoftAP...");
			return initialDlg;
		default:
			break;
		}
		return null;
	}

	private ProgressDialog dialogCreater(String msg) {
		ProgressDialog temp=new ProgressDialog(this);
		temp.setMessage(msg);
		temp.setProgressStyle(ProgressDialog.STYLE_SPINNER);
		temp.setIndeterminate(true);
		temp.setCancelable(true);
		return temp;
	}

	/**
	 * This method compares the current preference file with the previous preference file. 
	 */
	@SuppressWarnings("unchecked")
	public ArrayList<String> comparePreferenceFiles(SharedPreferences defSharPref, 
			SharedPreferences orgSharPref) {
		changesList.clear();
		Map<String ,?> defMap, orgMap;
		Set<?> defSet, orgSet;
		Iterator<?> defIterator, orgIterator;
		Map.Entry defMapEntry = null, orgMapEntry = null;
		String sPrefKey;
		defMap = defSharPref.getAll();
		orgMap = orgSharPref.getAll();	

		for( int m = 1; m <= L10NConstants.MAX_LENGTH; m++) {		
			bStatus = false;
			String copyAllow = defSharPref.getString(L10NConstants.ALLOW+m, "");			
			String copyDeny = defSharPref.getString(L10NConstants.DENY+m, "");	
			String orgAllow = orgSharPref.getString(L10NConstants.ALLOW+m, "");			
			String orgyDeny = orgSharPref.getString(L10NConstants.DENY+m, "");

			compareAllowDenyList(orgSharPref, copyAllow, L10NConstants.ALLOW);
			compareAllowDenyList(orgSharPref, copyDeny, L10NConstants.DENY);
			compareAllowDenyList(defSharPref, orgAllow, L10NConstants.ALLOW);
			compareAllowDenyList(defSharPref, orgyDeny, L10NConstants.DENY);
		}	

		defSet = defMap.entrySet();				
		defIterator = defSet.iterator();	
		while(defIterator.hasNext()) {
			defMapEntry = (Map.Entry) defIterator.next();

			orgSet = orgMap.entrySet();
			orgIterator = orgSet.iterator();
            
			while(orgIterator.hasNext()) {
				orgMapEntry = (Map.Entry) orgIterator.next();				

				if(defMapEntry.getKey().equals(orgMapEntry.getKey())) {	
					if(orgMapEntry.getValue() instanceof String){		
						if(!defMapEntry.getValue().equals(orgMapEntry.getValue())) {
							sPrefKey = defMapEntry.getKey().toString();						

							if(!sPrefKey.contains(L10NConstants.ALLOW) && 
									!sPrefKey.contains(L10NConstants.DENY)) {
								if(sPrefKey.equals(L10NConstants.WEP_KEY0) || sPrefKey.equals(L10NConstants.WEP_KEY1) ||													
										sPrefKey.equals(L10NConstants.WEP_KEY2) || sPrefKey.equals(L10NConstants.WEP_KEY3)) {										
									if(defMapEntry.getValue().toString().length() == 5
											|| defMapEntry.getValue().toString().length()==13
											|| defMapEntry.getValue().toString().length()==16) {
										changesList.add(defMapEntry.getKey()+"="+"\""+defMapEntry.getValue()+"\"");											
									} else {
										changesList.add(defMapEntry.getKey()+"="+defMapEntry.getValue());										
									}
								} else if (sPrefKey.equals(L10NConstants.COUNTRY_KEY)){
									changesList.add(defMapEntry.getKey()+"="+defMapEntry.getValue().toString().substring(0, defMapEntry.getValue().toString().indexOf(","))+"I");		
									Log.d(sTag, "Changed Value "+defMapEntry.getKey()+"="+defMapEntry.getValue());

								} else if (sPrefKey.equals(L10NConstants.SEC_MODE_KEY) || 
										sPrefKey.equals(L10NConstants.HW_MODE_KEY)){									
									String networkCheck = defSharPref.getString(L10NConstants.HW_MODE_KEY, "");
									String secCheck = defSharPref.getString(L10NConstants.SEC_MODE_KEY, "");
									String rsnCheck = defSharPref.getString(L10NConstants.RSN_PAIR_KEY, "");
									String wpaCheck = defSharPref.getString(L10NConstants.WPA_PAIR_KEY, "");

									if(networkCheck.equals(L10NConstants.SM_N_ONLY) 
											|| networkCheck.equals(L10NConstants.SM_N)) {											
										if((secCheck.equals(L10NConstants.VAL_TWO) && (wpaCheck.equals(L10NConstants.WPA_ALG_TKIP)||wpaCheck.equals(L10NConstants.WPA_ALG_MIXED)))
												|| (secCheck.equals(L10NConstants.VAL_THREE) && (rsnCheck.equals(L10NConstants.WPA_ALG_TKIP)||rsnCheck.equals(L10NConstants.WPA_ALG_MIXED)))
												|| (secCheck.equals(L10NConstants.VAL_FOUR) && (rsnCheck.equals(L10NConstants.WPA_ALG_TKIP)
														|| wpaCheck.equals(L10NConstants.WPA_ALG_TKIP)||rsnCheck.equals(L10NConstants.WPA_ALG_MIXED)
														|| wpaCheck.equals(L10NConstants.WPA_ALG_MIXED)))) {
											bIsValid = true;
										} 
									}
									if(!bIsValid) {
										updateChangesList(defMapEntry);
									}
								} else {
									updateChangesList(defMapEntry);
								}										
							}
						}
					}
					break;
				}
			}
           }
		
		if(!sAllowRemoveList.equals("")) {
			changesList.add(L10NConstants.REMOVE_FROM_ALLOW_LIST + "=" + sAllowRemoveList.substring(0, sAllowRemoveList.length()-1));
			sAllowRemoveList = "";
		} 
		if(!sDenyRemoveList.equals("")) {
			changesList.add(L10NConstants.REMOVE_FROM_DENY_LIST + "=" + sDenyRemoveList.substring(0, sDenyRemoveList.length()-1));
			sDenyRemoveList = "";
		} 
		if(!sAllowAddList.equals("")) {
			changesList.add(L10NConstants.ADD_TO_ALLOW_LIST + "=" + sAllowAddList.substring(0, sAllowAddList.length()-1));
			sAllowAddList = "";
		} 
		if(!sDenyAddList.equals("")) {
			changesList.add(L10NConstants.ADD_TO_DENY_LIST + "=" + sDenyAddList.substring(0, sDenyAddList.length()-1));
			sDenyAddList = "";		
		}	
		return changesList;
	}

	@SuppressWarnings("unchecked")
	private void updateChangesList(Map.Entry Copy) {
		changesList.add(Copy.getKey()+"="+Copy.getValue());		
		Log.d(sTag, "Changed Value "+Copy.getKey()+"="+Copy.getValue());
	}

	private void compareAllowDenyList(SharedPreferences sharedPref, String sMacAddr, String listType) {	
		for( int n = 1; n <= L10NConstants.MAX_LENGTH; n++) {
			String orignAllow = sharedPref.getString(listType + n, "");
			if(!sMacAddr.equals("")) {
				if(sMacAddr.equals(orignAllow)){						
					bStatus = false;
					break;
				} else {	
					bStatus = true;
				}
			}				
		}
		if(bStatus) {
			if(sharedPref.equals(orgSharPref)) {
				if(listType.equals(L10NConstants.ALLOW)){					
					sAllowAddList += sMacAddr+" ";
				} else {
					sDenyAddList += sMacAddr+" ";
				}
			} else {
				if(listType.equals(L10NConstants.ALLOW)) {
					sAllowRemoveList += sMacAddr+" ";
				} else {
					sDenyRemoveList += sMacAddr+" ";
				}
			}
		}
		bStatus = false;		
	}

	/**
	 * This method will show the warning dialog when the network mode is N or BGN with respect to the  
	 * Security mode WPA_PSK, WPA2_PSK & WPA_PSK Mixed 
	 *    
	 * @param bws_flag Basic Wireless Settings 
	 * @param copyFile Copy Preference file
	 * @param orgFile Original preference file
	 */
	public void showWarningDialog(String bws_flag, SharedPreferences copyFile, SharedPreferences orgFile) {
		SharedPreferences.Editor copyEdit =  copyFile.edit();
		if(!bws_flag.equals("BWS")) {					
			dialogBuilder = new AlertDialog.Builder(this);				                
			dialogBuilder.setTitle(getString(R.string.str_dialog_warning));
			dialogBuilder.setMessage(getString(R.string.mms_screen_alert_TKIPorMIXED) +
					getString(R.string.common_append_alert_wpa));				
			alertDialog = dialogBuilder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					// No Action
				}			
			}).create();
			alertDialog.show();	
		}		
		String prevSecurity = orgFile.getString(L10NConstants.SEC_MODE_KEY, "");
		String prevNWMode = orgFile.getString(L10NConstants.HW_MODE_KEY, "");
		copyEdit.putString(L10NConstants.SEC_MODE_KEY, prevSecurity);
		copyEdit.putString(L10NConstants.HW_MODE_KEY, prevNWMode);
		copyEdit.commit();
	}

	/**
	 * This Method Will set the Original values, which is from daemon  
	 */
	@SuppressWarnings("unchecked")
	private void SetOriginalValues() {
		Map<String, ?> allValues = new HashMap<String, String>();		
		allValues = orgSharPref.getAll();
		Set<?> s = allValues.entrySet();
		Iterator<?> it = s.iterator();
		while (it.hasNext()) {
			Map.Entry m = (Map.Entry) it.next();
			String key = (String) m.getKey();
			String value = null;			
			if (m.getValue() instanceof String) {
				value = (String) m.getValue();
				defPrefEditor.putString(key, value);
			} else {
				boolean wificheckVal = Boolean.parseBoolean(m.getValue()
						.toString());
				defPrefEditor.putBoolean(key, wificheckVal);
			}
		}
		defPrefEditor.commit();
		Log.d(sTag, "Commiting not Success");
		bPreferenceChanged = false;		
	}

	/**
	 * Set the Default Values to the WifiSoftAP
	 * 
	 * @param Value will be set to this key
	 * 
	 */
	private void SetDefaultValues(String key) {
		Log.d(sTag, "KEY VALUE " + sKeyVal);

		String value = sKeyVal.substring(iIndex + 1, sKeyVal.length());
		Log.d(sTag,"value "+value);
		if(key.equals(L10NConstants.COUNTRY_KEY)) {	
			Log.d(sTag, "Setting Regulatry Domain for Country Code :"+key+":");
			final String[] countryArray = getResources().getStringArray(R.array.countryCodeValues);
			for(int i = 0; i < countryArray.length; i++) {				
				if(value.equals(countryArray[i].substring(0, countryArray[i].indexOf(",")))) {	
					Log.d(sTag, "Matched Regulatry Domain for Country Code :"+key+":");
					value = value.concat(countryArray[i].substring(countryArray[i].indexOf(","), countryArray[i].length()));
					break;
				}
			}				
		}
		defPrefEditor.putString(key, value);
		orgPrefEditor.putString(key, value);
		commitPref();
	}

	/**
	 * Getting MAC Address allow or deny list from Daemon
	 * 
	 * @param MAC Address
	 * @param Key for that MAC Address
	 */
	private void getAllowDenyValue(String KeyValue, String key) {
		defPrefEditor.putString(key, sKeyVal.substring(iIndex+1,sKeyVal.length()));
		orgPrefEditor.putString(key, sKeyVal.substring(iIndex+1,sKeyVal.length()));
		commitPref();
		iIndex = sKeyVal.indexOf("=");
		String value = sKeyVal.substring(iIndex+1, sKeyVal.length());		
		int KeyCheck = KeyValue.indexOf("_");

		if(!value.trim().equals("")) {
			StringTokenizer st = new StringTokenizer(value);
			int i = 1;
			while(st.hasMoreElements()) {
				String nextToken = st.nextToken();
				defPrefEditor.putString(KeyValue.substring(0,KeyCheck)+i, nextToken);
				orgPrefEditor.putString(KeyValue.substring(0,KeyCheck)+i, nextToken);
				commitPref();
				i++;
			}
			if(i < L10NConstants.MAX_LENGTH) {
				createAllowDenyList(KeyValue, KeyCheck, i);				
			}
		} else {
			createAllowDenyList(KeyValue, KeyCheck, 1);
		} 
	}	
	private void createAllowDenyList(String KeyValue, int KeyCheck, int i) {
		for(int j = i; j <= L10NConstants.MAX_LENGTH; j++) {					
			defPrefEditor.putString(KeyValue.substring(0,KeyCheck)+j, "");
			orgPrefEditor.putString(KeyValue.substring(0,KeyCheck)+j, "");
			commitPref();
		}
	}

	/**
	 * this method does the clean up function for MainMenuSettings Activity like terminating loopers and dialogs.
	 */
	public void onDestroy() {
		super.onDestroy();		
		if(looper!=null) {
			looper.quit();
		}
		if(alertDialog != null && alertDialog.isShowing()) {
			alertDialog.cancel();
		}
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		switch(requestCode) {
		case 0:
		case 1:
		case 2:
			Log.d(sTag, "onActivityResult Enter");
			if(resultCode == RESULT_CANCELED){
				Log.d(sTag, "onActivityResult Enter result_canceled");
				if(bShutDownEvent){
					bShutDownEvent=false;
					Log.e(sTag, "bShutDownEvent onResume");
					shutdownEvent();
				}
			}
		}
	}

	/**
	 * This method commits the changes made.
	 */
	public void commitPref() {
		defPrefEditor.commit();
		orgPrefEditor.commit();
	}

	private class TimeOut extends CountDownTimer {
		//Handler handler = new Handler();
		int counter = 0;

		/**
		 * timer initializer
		 */
		public TimeOut(long millisInFuture, long countDownInterval) {			
			super(millisInFuture, countDownInterval);		
			Log.d(sTag, "Timer Created");
		}

		/**
		 * This method dismisses the wps dialog when timer expires.
		 */
		@Override
		public void onFinish() {
			if(shutdownEvtDialog != null && shutdownEvtDialog.isShowing()){
				shutdownEvtDialog.cancel();				
				onPreferenceChange(wifiCheckEnable, false);
			}
			if(noChngsShutdownEvtDialog != null && noChngsShutdownEvtDialog.isShowing()) {
				noChngsShutdownEvtDialog.cancel();
				onPreferenceChange(wifiCheckEnable, false);
			}		
		}

		@Override
		public void onTick(long millisUntilFinished) {
			Log.d(sTag, "Timer is running count:"+(++counter));
		}
	}
	/**
	 * this method shows a dialog for 105(shutdown) event and runs timer on it. 
	 */
	private void shutdownEvent() {		
		mTimer = new TimeOut(L10NConstants.SHUTDOWN_TIME, 1000);
		mTimer.start();
		if(bPreferenceChanged) {	
			shutdownEvtDlgBuilder = new AlertDialog.Builder(this);
			shutdownEvtDlgBuilder.setIcon(android.R.drawable.ic_dialog_alert);
			shutdownEvtDlgBuilder.setTitle("Warning");
			shutdownEvtDlgBuilder.setMessage("SoftAP turning off because of inactivity, do you want to save the changes?");
			shutdownEvtDialog = shutdownEvtDlgBuilder.setPositiveButton("Yes", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {					
					saveChanges(defSharPref, orgSharPref, "");
					onPreferenceChange(wifiCheckEnable,false);
				}
			}).setNegativeButton("No", new DialogInterface.OnClickListener() {					
				public void onClick(DialogInterface dialog, int which) {
					onPreferenceChange(wifiCheckEnable, false);
				}
			}).create();
			shutdownEvtDialog.show();			
		} else {
			Log.d(sTag, "Displaying the dialog");
			noChngsShutdownEvtDlgBuilder = new AlertDialog.Builder(this);
			noChngsShutdownEvtDlgBuilder.setIcon(android.R.drawable.ic_dialog_alert);
			noChngsShutdownEvtDlgBuilder.setTitle("Warning");
			noChngsShutdownEvtDlgBuilder.setMessage("SoftAP turning off because of inactivity, press Ok");
			noChngsShutdownEvtDialog = noChngsShutdownEvtDlgBuilder.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which) {
					onPreferenceChange(wifiCheckEnable, false);					
				}}).create();
			noChngsShutdownEvtDialog.show();					
		}			
	}
}
