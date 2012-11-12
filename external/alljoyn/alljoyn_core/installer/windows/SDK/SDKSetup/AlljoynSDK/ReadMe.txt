
NOTICE :  for Win XP installations.

Currently there is an issue with the XP version of the 32 bit Alljoyn Daemon Service. 

Even after receiving an “Installation Completed Successfully”  message, the service will not start and returns the message “The service did not respond to the start or control request in a timely manner”.


This issue is being investigated. 


Workaround.


In the interim there is a command line version  of the daemon. It is installed in the <alljoyn>\SDK\bin folder and is named alljoyn-daemon.exe. 

To run the daemon, open a Command Prompt window. 

	1.	Open the Start Menu  and select :“Run…”.
	2.	When the Run dialog opens type: cmd and click OK (or press Enter).
	3.	When the Command Prompt window opens enter the drive letter that point to where you installed the 			SDK, usually this is the C: drive. Type C: Enter.
	4.	Navigate to the SDK binaries folder. Type cd <alljoyn> \SDK\bin Enter.
	5.	Type alljoyn-daemon Enter. This will  start the daemon with a default  configuration.

Type alljoyn-daemon  --help Enter to list additional available options.

The command prompt window will remain open. Closing the window will terminate the daemon, the window may be minimized to be less intrusive.

