/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
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


package com.qualcomm.wifi.softap.ws;

import java.util.ArrayList;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceManager;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.EditText;
import android.widget.Toast;

import com.qualcomm.wifi.softap.L10NConstants;
import com.qualcomm.wifi.softap.MainMenu;
import com.qualcomm.wifi.softap.QWiFiSoftApCfg;
import com.qualcomm.wifi.softap.R;

/**
 * This class gives the configuration settings for WPA-PSK, WPA2_PSK and WPA_WPA2 Mixed Security Mode 
 */
public class WPASettings extends PreferenceActivity implements OnPreferenceChangeListener, 
OnKeyListener, OnPreferenceClickListener{   
	private AlertDialog wss_wpapskalertdialog;
	private Builder wss_wpapskbuilder;
	private QWiFiSoftApCfg qwifisoftAPCfg;
	private static String sWpaAlert;	
	private String sSecurityMode;
	private String[] sKeys;

	private SharedPreferences defSharPref;
	private ListPreference wpa_pairwiseLst, rsn_pairwiseLst;
	private EditTextPreference wpaPassEdit, wpaGrpEdit;		
	private PreferenceCategory wss_wpapsk_catag;		
	private EditText passEdit, groupEdit;	
	private ArrayList<Preference> prefLst;

	/**
	 * Method initializes the Activity from  <i>wss_pref_wpapsk.xml</i> preference file
	 * It compares the current security mode with previous selected Security Mode and updating the UI 
	 * 
	 * @param savedInstanceState If the activity is being re-initialized after previously being shut down 
	 * then this Bundle contains the data it most recently supplied in onSaveInstanceState(Bundle)
	 */
	@Override
	public void onCreate(Bundle savedInstanceState) {		
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.wpapsk_wireless_security_settings);
		qwifisoftAPCfg=MainMenu.mSoftAPCfg;
		MainMenu.wirelessSecurityWPAPSKEvent = this;

		// Initialize array list to include the preferences
		prefLst = new ArrayList<Preference>();

		//get the encryption algorithm keys for WPA/WPA2 Encryption algorithm/Passphrase/group key
		defSharPref = PreferenceManager.getDefaultSharedPreferences(this);
		sKeys = getResources().getStringArray(R.array.str_arr_wpapsk_keys);

		wpa_pairwiseLst = (ListPreference)findPreference(L10NConstants.WPA_PAIR_KEY);
		rsn_pairwiseLst = (ListPreference) findPreference(L10NConstants.RSN_PAIR_KEY);	
		wpaPassEdit = (EditTextPreference) findPreference(L10NConstants.WPA_PASSPHRASE_KEY);		
		wpaGrpEdit = (EditTextPreference) findPreference(L10NConstants.WPA_GRP_KEY);		
		wss_wpapsk_catag = (PreferenceCategory) findPreference(L10NConstants.WSS_PREF_CATEG_KEY);

		//add the preference's inside the array List
		prefLst.add(wpa_pairwiseLst); prefLst.add(rsn_pairwiseLst);
		prefLst.add(wpaPassEdit); prefLst.add(wpaGrpEdit);

		//get the security mode value set previously in the wireless security settings where you are calling this class		
		sSecurityMode = getIntent().getExtras().getString(L10NConstants.SM_EXTRA_KEY);		
		wss_wpapsk_catag.setTitle(sSecurityMode);	

		//set wpa_pairwise or rsn_pair
		if (sSecurityMode.equals(L10NConstants.WPA_PSK)){
			wpa_pairwiseLst.setEnabled(true);
			rsn_pairwiseLst.setEnabled(false);
		} else if (sSecurityMode.equals(L10NConstants.WPA2_PSK)){
			wpa_pairwiseLst.setEnabled(false);
			rsn_pairwiseLst.setEnabled(true);
		} else{
			wpa_pairwiseLst.setEnabled(true);
			rsn_pairwiseLst.setEnabled(true);
		}

		//set on change/click listener for WPA/WPA2 Encryption algorithm/Passphrase/group key
		for(int i = 0; i < sKeys.length; i++) {        	
			String sGetConfigMode = defSharPref.getString(sKeys[i], null);
			prefLst.get(i).setOnPreferenceChangeListener(this);
			prefLst.get(i).setOnPreferenceClickListener(this);
			if(sGetConfigMode != null){	
				if (sGetConfigMode.equals("TKIP CCMP")){
					sGetConfigMode = "Mixed";
				}
				prefLst.get(i).setSummary(sGetConfigMode);				
			}
		} 
		//set key listener for passphrase/group key
		passEdit = wpaPassEdit.getEditText();
		passEdit.setOnKeyListener(this);
		groupEdit = wpaGrpEdit.getEditText();
		groupEdit.setOnKeyListener(this);		
	}

	/**
	 * Validating the keys dynamically through onKey listener of the particular Edit Text Preference
	 * 
	 * @param v The view the key has been dispatched to.
	 * @param keyCode The code for the physical key that was pressed
	 * @param event The KeyEvent object containing full information about the event.
	 * 
	 * @return boolean True if the listener has consumed the event, false otherwise.
	 */
	public boolean onKey(View v, int keyCode, KeyEvent event) {
		if( v instanceof EditText){			
			String sTextVal = ((EditText)v).getText().toString();
			if(!sTextVal.equals("")){
				if(v == passEdit){
					if(sTextVal.length() < 8){
						((EditText)v).setError("Min 8 chars");
					}
				} else if(v == groupEdit){	
					try{
						if(Integer.parseInt(sTextVal) < 600) {
							((EditText)v).setError(L10NConstants.OUT_GTR_RANGE);				
						}
					}catch(Exception e){
						((EditText)v).setError(L10NConstants.OUT_RANGE);
					}				
				}
			}else
				((EditText)v).setError(L10NConstants.ERROR_NULL);
		}		
		return false;
	}
	/**
	 * Setting NULL value to the setError method for all the edit Text Preference
	 * 
	 * @param preference edit preference that was clicked. 
	 * @return boolean true on success.
	 */
	public boolean onPreferenceClick(Preference preference) {
		if(preference instanceof EditTextPreference){			
			((EditTextPreference) preference).getEditText().setError(null);
		}
		return true;
	}

	/**
	 * This method verify Network mode=n/n-only & encryption algorithm=TKIP to throw warning message
	 * 
	 * @param preference The changed Preference.
	 * @param newValue The new value of the Preference.
	 * 
	 * @return boolean True to update the state of the Preference with the new value.
	 */
	public boolean onPreferenceChange(Preference preference, Object newValue) {		
		if(preference instanceof ListPreference){
			int index = ((ListPreference) preference).findIndexOfValue(newValue.toString());
			String sNM = defSharPref.getString(L10NConstants.HW_MODE_KEY, "");			
			String sAlgmEntry = (String) ((ListPreference) preference).getEntries()[index];

			if(newValue.equals(L10NConstants.WPA_ALG_TKIP) || newValue.equals(L10NConstants.WPA_ALG_MIXED)){
				if(sNM.equals(L10NConstants.SM_N_ONLY)){
					sWpaAlert = getString(R.string.wpa_screen_alert_N_TKIPorMIXED) + " " +
					getString(R.string.common_append_alert_wpa);
					showAlertDialog();
					return false;
				}else if(sNM.equals(L10NConstants.SM_N)){
					sWpaAlert = getString(R.string.wpa_screen_alert_BGN_TKIPorMIXED) + " " +
					getString(R.string.common_append_alert_wpa);
					showAlertDialog();
					return false;
				}else{
					((ListPreference) preference).setSummary(sAlgmEntry);
				}				
			}else
				((ListPreference) preference).setSummary(sAlgmEntry);					

			//selecting the same previously set value should not allow save settings button to be enabled 
			//in MainMenuSettings file
			if(((ListPreference) preference) == wpa_pairwiseLst){
				String sWpa_pairwiseCheck = defSharPref.getString(L10NConstants.WPA_PAIR_KEY, "");				
				if(!sWpa_pairwiseCheck.equals(newValue)){
					MainMenu.bPreferenceChanged = true;	
				} 
			} else {			
				String sRsn_pairwiseCheck = defSharPref.getString(L10NConstants.RSN_PAIR_KEY, "");				
				if(!sRsn_pairwiseCheck.equals(newValue)){
					MainMenu.bPreferenceChanged = true;	
				}
			}
		} else if (preference instanceof EditTextPreference){			
			String sEditVal = ((EditTextPreference) preference).getEditText().getEditableText().toString();
			if(!sEditVal.equals("")){
				if((EditTextPreference) preference == wpaPassEdit){					
					if( sEditVal.length()>=8 && sEditVal.length()<=63) {
						wpaPassEdit.setSummary(sEditVal);	
					} else {
						Toast.makeText(this, L10NConstants.INVALID_ENTRY, 1).show();
						return false;
					}					
				} else if((EditTextPreference) preference == wpaGrpEdit){
					try{
						if(Integer.parseInt(sEditVal) >= 600){
							wpaGrpEdit.setSummary(sEditVal);
						}else{
							Toast.makeText(this, L10NConstants.INVALID_ENTRY, 1).show();
							return false;
						}
					}catch(NumberFormatException nfe){
						Toast.makeText(this, L10NConstants.OUT_RANGE, 1).show();
						return false;
					}					
				}
			}else{
				Toast.makeText(this, L10NConstants.ERROR_NULL, 1).show();
				return false;
			}
			MainMenu.bPreferenceChanged = true;
		}		
		return true;
	}
	/**
	 * Shows up alert dialog box for displaying warning message
	 */
	private void showAlertDialog(){
		wss_wpapskbuilder=new AlertDialog.Builder(this);				                
		wss_wpapskbuilder.setTitle(getString(R.string.str_dialog_warning));
		wss_wpapskbuilder.setMessage(sWpaAlert);
		wss_wpapskalertdialog=wss_wpapskbuilder.setPositiveButton(getString(R.string.alert_dialog_rename_ok), new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {			
				// No Action
			}			
		}).create();
		wss_wpapskalertdialog.show();	
	}

	public void EventHandler(String sEvt) {		
		if(sEvt.contains(L10NConstants.STATION_105)) 
			finish();
	}
	public void onDestroy(){
		super.onDestroy();
		MainMenu.wirelessSecurityWPAPSKEvent = null;
		if(wss_wpapskalertdialog != null && wss_wpapskalertdialog.isShowing()){
			wss_wpapskalertdialog.cancel();
		}
		Log.d(L10NConstants.TAG_WSS_WPAPSK, "destroying wss_wpapsk");
	}
}
