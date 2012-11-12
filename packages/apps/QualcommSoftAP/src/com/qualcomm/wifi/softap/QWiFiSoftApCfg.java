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


package com.qualcomm.wifi.softap;

import android.util.Log;

/**
 * This interface declares EventHandler method to handle events. 
 */
interface QWiFiSoftApEvent {
	public void EventHandler(String evt);
}

/**
 * This class declares all the native methods used in softAP and also loads the library
 * where native methods are implemented.
 */
public class QWiFiSoftApCfg {	
	private QWiFiSoftApEvent mEventCallback;	
	public native String SapSendCommand(String cmd);
	public native boolean SapOpenNetlink();
	public native String SapWaitForEvent();
	public native void SapCloseNetlink();
	/**
	 * Its a class initializer, which loads the shared library. 
	 */
	public QWiFiSoftApCfg( Object caller){		
		System.loadLibrary("QWiFiSoftApCfg");		
		mEventCallback = (QWiFiSoftApEvent)caller;							
	}

	/**
	 * This method creates a APEventMonitor thread if it is not created previously otherwise previous APEventMonitor thread is utilized 
	 */
	public void SapStartEventMonitor() {
		if(APEventMonitor.KILLED){

			APEventMonitor.KILLED=false;

			if(APEventMonitor.RETURNED){
				new APEventMonitor(this,mEventCallback);
			}

			APEventMonitor.RETURNED=false;
		}
	}
	public void SapStopEventMonitor() {
		APEventMonitor.KILLED=true;
	}
}

/**
 * 
 * This class implements access point monitor thread, which waits for
 * an incoming events via native method calls and broadcasts it.
 *
 */
class APEventMonitor implements Runnable {
	private static final String TAG = "APEventMonitor";
	private String eventstr;
	private QWiFiSoftApCfg qcsoftapcfg;
	private QWiFiSoftApEvent eventHandler;

	private Thread thread;
	static boolean KILLED = true,RETURNED=true;

	/**
	 *  Its an initialization part of APEventMonitor class.
	 *  It also starts the APEventMonitor thread.     
	 */
	public APEventMonitor(QWiFiSoftApCfg ref,QWiFiSoftApEvent eventHandler) {
		qcsoftapcfg=ref;		
		this.eventHandler=eventHandler;
		thread = new Thread(this);
		thread.start();	
	}
	/**
	 * Its a thread body, which calls native methods to receive events and
	 * broadcasts it.
	 */
	public void run() {
		Log.d("APEventMonitor", "Thread Started");
		if(qcsoftapcfg.SapOpenNetlink()) {
			Log.d(TAG, "Connection success");
			
			while(!KILLED) {					
				Log.e(TAG, "Waiting For Broadcast");
				eventstr=qcsoftapcfg.SapWaitForEvent();
				if(KILLED) break;
				if (eventstr == null) {
					Log.e(TAG, "Null Event Received");						
					continue;
				}			
				eventHandler.EventHandler(eventstr);
				Log.e(TAG, "Event Received, broadcasting it");
			}
			Log.e(TAG, "Killing Thread");
			qcsoftapcfg.SapCloseNetlink();		

		} else {
			Log.d(TAG, "Connection Failed");
		}
		Log.d(TAG, "Returning from APEventMonitor");
		RETURNED=true;
	}
}
