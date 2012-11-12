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

import java.util.ArrayList;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnKeyListener;
import android.widget.EditText;
import android.widget.Toast;

import com.qualcomm.wifi.softap.L10NConstants;
import com.qualcomm.wifi.softap.MainMenu;
import com.qualcomm.wifi.softap.R;

/**
 * This class is used to configure the AdvancedWireless settings for WifiSoft
 */
public class AdvancedWirelessSettings extends PreferenceActivity implements
OnPreferenceChangeListener, OnKeyListener, OnPreferenceClickListener{	
	private String[] sKeys, sCheckBoxKeys;
	private String[] sBr_opt, sBr_optValues;
	private String[] sFreqValues;
	private String sNWMode;
	private String sRegulatoryDomain;

	private SharedPreferences defSharPref;
	private SharedPreferences.Editor defPrefEditor;

	private CheckBoxPreference protectChk, intrabssChk, chkbx;	
	private EditTextPreference fragmentETP, rtsETP, beaconETP, dtimETP, transmitPwr;
	private EditText rtsEdit, beaconEdit, dtimEdit, fragmentEdit, txpowerEdit;
	private ListPreference dataRatesLst, countryCodeLst, apShutdownLst, wmmLst;
	public static  ListPreference energyDetect;

	private ArrayList<Preference> prefLst = new ArrayList<Preference>();
	private ArrayList<CheckBoxPreference> checkPrefLst = new ArrayList<CheckBoxPreference>();

	/**
	 * Method used to initialize resources such as inflating XML UI into the<br>
	 * screen and initializing the default values present in the preference<br>
	 * by calling setDefaultValues() method.
	 * 
	 * @param savedInstanceState
	 *            Previous saved instance state of the <b>AdvancedWireless</b>
	 *            activity
	 */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);		
		MainMenu.advancedWirelessEvent = this;
		addPreferencesFromResource(R.xml.advanced_wireless_settings);

		// get the keys for trasmit power/data rate/frag threshold/rts
		// threshold/beacon interval/dtim period
		sKeys = getResources().getStringArray(R.array.str_arr_aws_keys);

		// get the keys for checkbox preference ie protection flag/wmm/intra
		// bss/802.11d
		sCheckBoxKeys = getResources().getStringArray(
				R.array.str_arr_aws_checkbx_keys);

		defSharPref = PreferenceManager.getDefaultSharedPreferences(this);
		defPrefEditor = defSharPref.edit();

		// get Network mode key value
		sNWMode = defSharPref.getString(L10NConstants.HW_MODE_KEY, "");

		// get the reference to protection flag/wmm/intra bss/802.11d
		protectChk = (CheckBoxPreference) findPreference("protect_flag");		
		intrabssChk = (CheckBoxPreference) findPreference("intra_bss");
		chkbx = (CheckBoxPreference) findPreference("d_chk");		

		// get the reference to transmit power/data rate/frag threshold/rts
		// threshold/beacon interval/dtim period
		dataRatesLst = (ListPreference) findPreference("data_rate");
		countryCodeLst = (ListPreference) findPreference("country_code");
		energyDetect = (ListPreference) findPreference("energy_detect_threshold");
		apShutdownLst = (ListPreference) findPreference("auto_shut_off_time");	
		transmitPwr = (EditTextPreference) findPreference("tx_power");		
		fragmentETP = (EditTextPreference) findPreference("fragm_threshold");
		rtsETP = (EditTextPreference) findPreference("rts_threshold");
		wmmLst = (ListPreference) findPreference("wmm_enabled");
		beaconETP = (EditTextPreference) findPreference("beacon_int");
		dtimETP = (EditTextPreference) findPreference("dtim_period");			

		// Preferences are added to ArrayList in order
		prefLst.add(transmitPwr);
		prefLst.add(dataRatesLst);
		prefLst.add(fragmentETP);
		prefLst.add(rtsETP);
		prefLst.add(beaconETP);
		prefLst.add(dtimETP);
		prefLst.add(countryCodeLst);
		prefLst.add(apShutdownLst);
		prefLst.add(energyDetect);
		prefLst.add(wmmLst);

		checkPrefLst.add(protectChk);		
		checkPrefLst.add(intrabssChk);
		checkPrefLst.add(chkbx);

		for (int j = 0; j < sCheckBoxKeys.length; j++) {
			String sCheckBoxVal = defSharPref.getString(sCheckBoxKeys[j], "");
			checkPrefLst.get(j).setOnPreferenceChangeListener(this);
			// set default values for checkbox's returned initially from the
			// preference file
			if (!sCheckBoxVal.equals("")) {
				if (sCheckBoxVal.equals(L10NConstants.VAL_ZERO)) {
					checkPrefLst.get(j).setChecked(false);
				} else if (sCheckBoxVal.equals(L10NConstants.VAL_ONE))
					checkPrefLst.get(j).setChecked(true);
			} else
				checkPrefLst.get(j).setChecked(false);
		}

		// Set data rate entry/values based on the value of network mode
		if (sNWMode.equals(L10NConstants.SM_B)) {
			setDataRateEntry(R.array.dataRatesArrayB, R.array.dataRatesValuesB);
		} else if ((sNWMode.equals(L10NConstants.SM_G_ONLY))
				|| (sNWMode.equals(L10NConstants.SM_G))) {
			setDataRateEntry(R.array.dataRatesArrayGBG,
					R.array.dataRatesValuesGBG);
		} else if ((sNWMode.equals(L10NConstants.SM_N_ONLY))
				|| (sNWMode.equals(L10NConstants.SM_N))) {
			setDataRateEntry(R.array.dataRatesArrayNBGN,
					R.array.dataRatesValuesNBGN);
			fragmentETP.setEnabled(false);
		}
		// set the default value for trasmit power/data rate/frag threshold/rts
		// threshold/beacon interval/dtim period
		setDefaultValues();

		// set onkey listener for trasmit power/data rate/frag threshold/rts
		// threshold/beacon interval/dtim period
		txpowerEdit = transmitPwr.getEditText();
		fragmentEdit = fragmentETP.getEditText();
		rtsEdit = rtsETP.getEditText();
		beaconEdit = beaconETP.getEditText();
		dtimEdit = dtimETP.getEditText();

		txpowerEdit.setOnKeyListener(this);
		rtsEdit.setOnKeyListener(this);
		beaconEdit.setOnKeyListener(this);
		dtimEdit.setOnKeyListener(this);
		fragmentEdit.setOnKeyListener(this);		
		energyDetect.setOnPreferenceClickListener(this);
	}	

	/**
	 * Set data rate entry/values based on the value of network mode
	 * 
	 * @param dataratesarrayb
	 *            entries associated with network mode
	 * @param dataratesvaluesb
	 *            values associated with network mode
	 */
	private void setDataRateEntry(int dataratesarrayb, int dataratesvaluesb) {
		sBr_opt = getResources().getStringArray(dataratesarrayb);
		dataRatesLst.setEntries(sBr_opt);
		sBr_optValues = getResources().getStringArray(dataratesvaluesb);
		dataRatesLst.setEntryValues(sBr_optValues);
	}

	/**
	 * Method which implement the preference click event handler for editText
	 * preference's
	 * 
	 * @param preference
	 *            represents edit text preference
	 * @return true on success
	 */
	public boolean onPreferenceClick(Preference preference) {
		if (preference instanceof EditTextPreference) {
			((EditTextPreference) preference).getEditText().setError(null);
		}
		if (preference.equals(energyDetect)) {
			energyDetect.getDialog().setTitle("Energy Detect Threshold");
		}
		
		return true;
	}

	/**
	 * Method implements validation of UI objects on the AdvancedWireless screen
	 * 
	 * @param v
	 *            The view the key has been dispatched to.
	 * @param keyCode
	 *            The code for the physical key that was pressed.
	 * @param event
	 *            The KeyEvent object containing full information about the
	 *            event.
	 * @return boolean True if the listener has consumed the event, false
	 *         otherwise.
	 */
	public boolean onKey(View v, int keyCode, KeyEvent event) {
		String sTextVal = ((EditText) v).getText().toString();
		if (!sTextVal.equals("")) {
			if (v == txpowerEdit) {
				validateEditPrefRange((EditText) v, sTextVal, 30, 2);
			} else if (v == beaconEdit) {
				validateEditPrefRange((EditText) v, sTextVal, 65535, 1);
			} else if (v == dtimEdit) {
				validateEditPrefRange((EditText) v, sTextVal, 255, 1);
			} else if (v == fragmentEdit) {
				validateEditPrefRange((EditText) v, sTextVal, 2346, 256);
			} else if (v == rtsEdit) {
				if (Integer.parseInt(sTextVal) > 2347)
					((EditText) v).setError(sTextVal + " "
							+ L10NConstants.ERROR_OUT_RANGE);
			}
		} else
			((EditText) v).setError(L10NConstants.ERROR_NULL);

		return false;
	}

	/**
	 * this method validate the entry in edit preference,sets error for maximum
	 * and minimum range
	 * 
	 * @param editText is EditText which got changed
	 * @param textVal  is entry to the EditText
	 * @param maxRange is maximum range for the entry
	 * @param minRange is minimum range for the entry *
	 */
	private void validateEditPrefRange(EditText editText, String sTextVal,
			int maxRange, int minRange) {
		if (Integer.parseInt(sTextVal) > maxRange) {
			editText.setError(sTextVal + " " + L10NConstants.ERROR_OUT_RANGE);
		} else if (Integer.parseInt(sTextVal) < minRange) {
			editText.setError(sTextVal + " " + L10NConstants.ERROR_BELOW_RANGE);
		}
	}

	/**
	 * Method which implement's change event handler for Advanced Wireless UI
	 * objects
	 * 
	 * @param preference Changed preference object
	 * @param newValue New value of changed preference
	 * @return boolean
	 */
	public boolean onPreferenceChange(Preference preference, Object newValue) {
		int m;
		String sLstEntry = "";
		if (preference instanceof ListPreference) {
			// get the list value from default preference file for
			// data_rate/country_code
			String sDateRateCheck = defSharPref.getString(
					L10NConstants.DATA_RATE_KEY, "");
			String sCountryCodeCheck = defSharPref.getString(
					L10NConstants.COUNTRY_KEY, "");
			String sApShutTimerCheck = defSharPref.getString(
					L10NConstants.AP_SHUT_TIMER, "");
			String sEnergyModeCheck = defSharPref.getString(
					L10NConstants.ENERGY_DETECT_THRESHOLD, "");
			String sWmmCheck = defSharPref.getString(
					L10NConstants.WMM_LST_KEY, "");
			if (preference == dataRatesLst) {
				if (!sDateRateCheck.equals(newValue)) {
					MainMenu.bPreferenceChanged = true;
				}
			}
			int index = ((ListPreference) preference).findIndexOfValue(newValue
					.toString());
			if (index != -1) {
				sLstEntry = (String) ((ListPreference) preference)
				.getEntries()[index];				

				// update the default preference file by extracting country code
				// and regulatory domain
				// else update data rate value
				if (newValue.toString().contains(",")) {
					int indexValue = newValue.toString().indexOf(",");
					String sValue = newValue.toString().substring(0, indexValue);
					defPrefEditor.putString(L10NConstants.COUNTRY_KEY, sValue);
					defPrefEditor.putString("Regulatory_domain", newValue
							.toString().substring(indexValue + 1,
									newValue.toString().length()));
					defPrefEditor.commit();
					((ListPreference) preference).setSummary(sLstEntry);
				} else
					((ListPreference) preference).setSummary(sLstEntry);
			}

			// Doesnt allow save settings button to be enabled when you click
			// same country code which is selected before
			if (preference == countryCodeLst) {
				if (!sCountryCodeCheck.equals(newValue)) {
					MainMenu.bPreferenceChanged = true;
				}
				String sCountryCode = newValue.toString();
				String sChannel = defSharPref.getString(L10NConstants.CHNL_KEY, "");

				// extract country code and regulatory domain from key value
				// selected
				if (sCountryCode.contains(",")) {
					sRegulatoryDomain = sCountryCode.substring(sCountryCode
							.indexOf(",") + 1, sCountryCode.length());
				}

				if (sRegulatoryDomain.equals("REGDOMAIN_FCC")
						|| sRegulatoryDomain.equals("REGDOMAIN_WORLD")
						|| sRegulatoryDomain.equals("REGDOMAIN_N_AMER_EXC_FCC")) {
					checkChannelAvailability(R.array.freqelevenValues, sChannel);
				} else if (sRegulatoryDomain.equals("REGDOMAIN_ETSI")
						|| sRegulatoryDomain.equals("REGDOMAIN_APAC")
						|| sRegulatoryDomain.equals("REGDOMAIN_KOREA")
						|| sRegulatoryDomain.equals("REGDOMAIN_HI_5GHZ")
						|| sRegulatoryDomain.equals("REGDOMAIN_NO_5GHZ")) {
					checkChannelAvailability(R.array.freqthirteenValues,
							sChannel);
				}
			}			
			if (preference == apShutdownLst) {
				if (!sApShutTimerCheck.equals(newValue)) {
					MainMenu.bPreferenceChanged = true;
				}						
			}		
			if (preference == energyDetect) {	
				if(newValue.equals("128")){
					((ListPreference) preference).setSummary("Disabled");					
				} else {
					((ListPreference) preference).setSummary("Enabled (Threshold: "+sLstEntry+")");
				}
				if (!sEnergyModeCheck.equals(newValue)) {					
					MainMenu.bPreferenceChanged = true;
				}					
			}
			if(preference == wmmLst) {
				if(!newValue.equals("0")) {
					((ListPreference) preference).setSummary(sLstEntry+"d");
				}
				if(!sWmmCheck.equals(newValue)) {
					MainMenu.bPreferenceChanged = true;
				}
			}

		} else if (preference instanceof EditTextPreference) {
			EditTextPreference etPref = (EditTextPreference) preference;
			String sValue = etPref.getEditText().getEditableText().toString();
			if (!sValue.equals("")) {
				if (etPref == transmitPwr) {
					return validateEditPrefOnChange(etPref, sValue, 30, 2);
				} else if (etPref == fragmentETP) {
					return validateEditPrefOnChange(etPref, sValue, 2346, 256);
				} else if (etPref == rtsETP) {
					return validateEditPrefOnChange(etPref, sValue, 2347, 0);
				} else if (etPref == beaconETP) {
					return validateEditPrefOnChange(etPref, sValue, 65535, 1);
				} else if (etPref == dtimETP)
					return validateEditPrefOnChange(etPref, sValue, 255, 1);
			} else {
				Toast.makeText(this, L10NConstants.ERROR_NULL, 1).show();
				return false;
			}
		} else if (preference instanceof CheckBoxPreference) {
			for (m = 0; m < checkPrefLst.size(); m++) {
				if (checkPrefLst.get(m) == (CheckBoxPreference) preference) {
					if (!((CheckBoxPreference) preference).isChecked()) {
						defPrefEditor.putString(sCheckBoxKeys[m],
								L10NConstants.VAL_ONE);
					} else {
						defPrefEditor.putString(sCheckBoxKeys[m],
								L10NConstants.VAL_ZERO);
					}
					defPrefEditor.commit();
					MainMenu.bPreferenceChanged = true;
					break;
				}
			}	
		}
		return true;
	}

	/**
	 * this method validate the entry in edit preference,if valid sets the
	 * summary else give a toast
	 * 
	 * @param editPref is EditTextPreference which got changed
	 * @param value is entry to the EditTextPreference
	 * @param maxRange for the entry
	 * @param minRange for the entry
	 * @return Dialog Returns the Dialog box based on the ID
	 */
	private boolean validateEditPrefOnChange(EditTextPreference editPref,
			String sValue, int maxRange, int minRange) {
		MainMenu.bPreferenceChanged = true;
		int i = Integer.parseInt(sValue);
		if (i >= minRange && i <= maxRange) {
			editPref.setSummary(Integer.toString(i));
		} else {
			Toast.makeText(this, "Invalid " + editPref.getTitle(), 1).show();
			return false;
		}
		return true;
	}

	/**
	 * Check channel value from preference file is present in the key entry list
	 * if not present save value '0'
	 * 
	 * @param channelAvailability
	 *            list of channel available
	 * @param sChannel
	 *            channel value from the preference file to be verified
	 */
	public void checkChannelAvailability(int channelAvailability,
			String sChannel) {
		sFreqValues = getResources().getStringArray(channelAvailability);
		if (!isAvailable(sFreqValues, sChannel)) {
			defPrefEditor.putString(L10NConstants.CHNL_KEY,
					L10NConstants.VAL_ZERO);
			defPrefEditor.commit();
		}
	}

	/**
	 * Validate channel value is present in the list entry
	 * 
	 * @param dr
	 *            list of channels passed as string array
	 * @param sChannel
	 *            channel value from the preference file to be verified
	 * @return
	 */
	public boolean isAvailable(String[] sDr, String sChannel) {
		boolean available = false;
		for (int i = 0; i < sDr.length; i++) {
			if (sChannel.equals(sDr[i])) {
				available = true;
				break;
			}
		}
		return available;
	}

	/**
	 * Method which set the default initial values for transmit power/data
	 * rate/<b frag threshold/rts threshold/beacon interval/dtim period
	 * 
	 * @return void
	 */
	public void setDefaultValues() {
		for (int i = 0; i < sKeys.length; i++) {
			String sGetConfigMode = defSharPref.getString(sKeys[i], "");
			Preference pref = prefLst.get(i);
			pref.setOnPreferenceChangeListener(this);
			pref.setOnPreferenceClickListener(this);
			if (pref instanceof ListPreference) {
				if (pref == dataRatesLst) {
					((ListPreference) pref).setSummary(dataRatesLst.getEntry());
				} else if (pref == countryCodeLst) {
					((ListPreference) pref).setSummary(countryCodeLst.getEntry());
				} else if (pref == apShutdownLst){
					((ListPreference) pref).setSummary(apShutdownLst.getEntry());
				} else if (pref == wmmLst){
					if(!sGetConfigMode.equals("0")) {
						((ListPreference) pref).setSummary(wmmLst.getEntry()+"d");
					} else {						
						((ListPreference) pref).setSummary(wmmLst.getEntry());
					}					
				} else if (pref == energyDetect){
					if(defSharPref.getString(L10NConstants.ENERGY_DETECT_THRESHOLD,L10NConstants.VAL_SEVEN)
							.equals(L10NConstants.VAL_ONEHUNDREDTWENTYEIGHT)){
						((ListPreference) pref).setSummary("Disabled");
					} else { 
					((ListPreference) pref).setSummary("Enabled (Threshold: "+energyDetect.getEntry()+")");
					}
				} else
					((ListPreference) pref).setSummary(sGetConfigMode);
			} else
				pref.setSummary(sGetConfigMode);
		}
	}

	public void EventHandler(String sEvt) {		
		if(sEvt.contains(L10NConstants.STATION_105)) 
			finish();
	}

	public void onDestroy(){
		super.onDestroy();
		MainMenu.advancedWirelessEvent = null;
		Log.d(L10NConstants.TAG_AWS,"destroying Advanced wireless");
	}
}
