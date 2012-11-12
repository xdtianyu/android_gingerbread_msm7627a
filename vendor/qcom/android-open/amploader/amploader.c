/*
Copyright (c) 2011, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
  @file amploader.c

  This file implements the Bluetooth AMP PAL driver loader for the Android platform.

*/

#ifdef AMP_DEBUG
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0
#endif

#define LOG_TAG "AmpLoader"
#include "cutils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef BOARD_HAS_QCOM_WLAN
#include "wifi.h"
#endif
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"

/* Max idle timer is 60 min */
#define BT_MAX_IDLETIMER 3600000
#define BT_IDLETIMER "7000"

/**
FUNCTION main()

@brief
  This is the main entry point into the AMP PAL driver loader executable.

  This function should be called when AMP PAL services are to be loaded / unloaded.

@param argc : number of cmd line args

@param argv : array of cmd line args

@return 0 (success), 1 (failed)

DEPENDENCIES

SIDE EFFECTS
  None
*/
int main(int argc, char *argv[])
{
    int opt;
    int result = 1;
    int load = 0;
    int unload = 0;
    char prop_val[PROPERTY_VALUE_MAX];
    int hci_fd = -1;
    char *idle_timer = NULL;
    char *out_value = NULL;
    long int input_val;
    int file_write = 0;



    LOGV("At Start Process User ID:%d, Group ID %d", getuid(), getgid());


    if (setegid(AID_SYSTEM) < 0) {
        LOGV("At Start Setting GID error: %s;", strerror(errno) );
    }

    if (setgid(AID_SYSTEM) < 0) {
        LOGV("At Start Setting Real GID error: %s;", strerror(errno) );
    }

    if (seteuid(AID_SYSTEM) < 0){
         LOGV(" At Start Setting UID error: %s;", strerror(errno) );
    }

    /* Read args */
    while ((opt = getopt(argc, argv, "luih")) != EOF) {
        switch (opt) {
            case 'l':
                load = 1;

                if(argc == 3) {
                   idle_timer = argv[2];
                   errno = 0;
                   input_val = strtol(idle_timer, &out_value , 10);
                   if (((errno == ERANGE && (input_val == LONG_MAX || input_val == LONG_MIN))
                             || (errno != 0 && input_val == 0)) || (input_val <= 0)||(*out_value != NULL)
                             || (strlen(idle_timer)> 10)||(input_val > BT_MAX_IDLETIMER)){

                       LOGE("Incorrect Bluetooth Idle timer value %d, %s", input_val,idle_timer);
                       idle_timer = BT_IDLETIMER;
                   }

                }
                else {

                   idle_timer = BT_IDLETIMER;
                   LOGV("Bluetooth idle timeout set to default");
                }

                hci_fd = open("/sys/devices/virtual/bluetooth/hci0/idle_timeout",O_WRONLY);
                if(hci_fd != -1) {
                     file_write = write(hci_fd, idle_timer, (strlen(idle_timer)+1));
                     if(file_write <= 0) {
                         LOGE("Bluetooth hci0 Idle Timer: write failed:");
                     }
                     close(hci_fd);
                }
                else {
                     hci_fd = open("/sys/devices/virtual/bluetooth/hci1/idle_timeout",O_WRONLY);
                     if(hci_fd != -1) {
                          file_write = write(hci_fd, idle_timer, (strlen(idle_timer)+1));
                          LOGI("Bluetooth is using hci 1");
                          if(file_write <= 0) {
                              LOGE("Bluetooth hci1 Idle Timer: write failed ");
                          }
                          close(hci_fd);
                     }
                     else {
                         LOGE("Bluetooth idle_timeout file not found");
                     }
                }
                break;

            case 'u':
                unload = 1;
                break;

            case 'i':
                /* Load and Unload the Wifi driver when Bluetooth and Wifi is not loaded.*/
                if ((!property_get("wlan.driver.status", prop_val, NULL)
                      ||((strcmp(prop_val, "ok") != 0)&&(strcmp(prop_val, "loading") != 0)))
                      &&((!property_get("ro.config.bt.amp", prop_val, NULL)||(strcmp(prop_val, "yes") != 0))
                      ||(!property_get("init.svc.bluetoothd", prop_val, NULL)||(strcmp(prop_val, "running") != 0)))){

#ifdef BOARD_HAS_QCOM_WLAN

                       if (setreuid(AID_ROOT,AID_ROOT) < 0){
                         LOGV("Init setreuid Root UID error: %s;", strerror(errno) );
                       }
                       result = wifi_load_driver();
                       result = wifi_unload_driver();
                       if (setreuid(AID_SYSTEM, AID_SYSTEM) < 0){
                         LOGV("Init setreuid SYSTEM UID error: %s;", strerror(errno) );
                       }
#else
                       LOGV("No QCOM WLAN in this build");
                       result = 0;
#endif
                }
                else {

                     LOGV("WLAN OR Bluetooth is running");
                     result = 0;
                }
                exit(result?1:0);

            case 'h':
                result = 0;
                /* No break, fall into default */

            default:
                LOGV("Options:");
                LOGV("\t-i Init AMP Driver");
                LOGV("\t-l Load AMP Driver and Provide Idle Timer value");
                LOGV("\t-u Unload AMP Driver");
                LOGV("\t-h Help");
                exit(result);
        }
    }

    if ((load + unload) != 1) {
        LOGE("Must specify exactly 1 option");
        exit(1);
    }

    if (!property_get("ro.config.bt.amp", prop_val, NULL)
            || (strcmp(prop_val, "yes") != 0)) {
        LOGI("AMP feature is not enabled");
        exit(0);
    }

#ifdef BOARD_HAS_QCOM_WLAN
    if (property_get("init.svc.wpa_supplicant", prop_val, NULL)
            && (strcmp(prop_val, "running") == 0)) {
        LOGI("WiFi is on; no need to %sload AMP Driver", unload ? "un" : "");
        exit(0);
    }

    if (load) {
        LOGV("Load requested");
        if (setuid(AID_ROOT) < 0){
             LOGV("Setting Effective UID error: %s;", strerror(errno) );
        }
        result = wifi_load_driver();
        if (setuid(AID_SYSTEM) < 0){
             LOGV("Setting Effective UID error: %s;", strerror(errno) );
        }

    } else if (unload) {
        LOGV("Unload requested");
        if (setuid(AID_ROOT) < 0){
             LOGV("Setting Effective UID error: %s;", strerror(errno) );
        }
        result = wifi_unload_driver();
        if (setuid(AID_SYSTEM) < 0){
             LOGV("Setting Effective UID error: %s;", strerror(errno) );
        }
    }
#else
    LOGV("No QCOM WLAN in this build");
    result = 0;
#endif

    if (result != 0) result = 1;
    LOGV("exit(%d)", result);
    exit(result);
}

