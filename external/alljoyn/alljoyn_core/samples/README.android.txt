AllJoyn C++ Samples for Android README
--------------------------------------

This directory contains the following AllJoyn C++ API samples:

simple -      
         This sample shows how to connect to a local AllJoyn daemon in order
         to publish an AllJoyn object that implements a very simple
         interface. The sample also shows how to connect to the local daemon
         in order to execute a method on the published object.

chat -   
         This sample shows how to use AllJoyn's C++ API to discover and connect
         with other AllJoyn enabled devices.

bus-daemon-service -  			  
         This sample creates an AllJoyn daemon that can be launched by the end 
         user via a standard APK file. Please note that a daemon that is
         launched in this way will not be capable of using the Bluetooth
         physical transport.

bus-daemon-user -
         This sample shows how to launch the user launchable daemon 
         (bus-daemon-service) from within an AllJoyn client application
         using the android.content.Intent Java class.



