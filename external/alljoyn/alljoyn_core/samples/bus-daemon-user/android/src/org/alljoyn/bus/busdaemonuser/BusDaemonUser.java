/*
 * Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
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
 *
 */
package org.alljoyn.bus.busdaemonuser;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.widget.Toast;

import java.util.ArrayList;

public class BusDaemonUser extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        Intent intent = new Intent("org.alljoyn.bus.daemonservice.START_DAEMON");
        
        //
        // We can send command line options to the spawned daemon just as if
        // we were starting it from the shell by passing them in the intent.
        //
        ArrayList<String> argv = new ArrayList<String>();
        
        argv.add("alljoyn-daemon-service");  // argv[0] is the name (choose one, but must be there).
        argv.add("--config-service");        // argv[1] use a provided configuration.
        argv.add("--no-bt");                 // argv[2] don't use bluetooth.
        argv.add("--verbosity=7");           // argv[3] set verbosity to LOG_DEBUG (see qcc/Logger.h)
        
        intent.putStringArrayListExtra("argv", argv);
        
        //
        // We can either point the daemon to a file via the arguments above,
        // or we can provide one through the intent.
        //
        String config =
        	"<busconfig>" + 
        	"  <type>alljoyn</type>" + 
        	"  <listen>unix:abstract=alljoyn</listen>" + 
        	"  <listen>tcp:addr=0.0.0.0,port=9955</listen>" +
        	"  <policy context=\"default\">" +
        	"    <allow send_interface=\"*\"/>" +
        	"    <allow receive_interface=\"*\"/>" +
        	"    <allow own=\"*\"/>" +
        	"    <allow user=\"*\"/>" +
        	"    <allow send_requested_reply=\"true\"/>" +
        	"    <allow receive_requested_reply=\"true\"/>" +
        	"  </policy>" +
        	"  <limit name=\"auth_timeout\">32768</limit>" +
        	"  <limit name=\"max_incomplete_connections_tcp\">16</limit>" +
        	"  <limit name=\"max_completed_connections_tcp\">64</limit>" +
        	"  <alljoyn module=\"ipns\">" +
        	"    <property interfaces=\"*\"/>" +
        	"  </alljoyn>" +
        	"</busconfig>";
        
        intent.putExtra("config", config);
     
        this.startService(intent);

        Toast.makeText(this, "Starting Bus daemon", Toast.LENGTH_LONG).show();
    }
}
