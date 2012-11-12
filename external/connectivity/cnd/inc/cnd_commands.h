/*
** Copyright 2006, The Android Open Source Project
** Copyright (c) 2010, 2011 Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
    {0, NULL, NULL},                   //none
    {CNE_REQUEST_INIT_CMD, dispatchVoid, responseVoid},
    {CNE_REQUEST_REG_ROLE_CMD, dispatchInts, responseInts},
    {CNE_REQUEST_GET_COMPATIBLE_NWS_CMD, dispatchInts, responseInts},
    {CNE_REQUEST_CONFIRM_NW_CMD, dispatchInts, responseInts},
    {CNE_REQUEST_DEREG_ROLE_CMD, dispatchInts, responseInts},
    {CNE_REQUEST_REG_NOTIFICATIONS_CMD, dispatchInts, responseInts},
    {CNE_REQUEST_UPDATE_BATTERY_INFO_CMD, dispatchInts, responseVoid},
    {CNE_REQUEST_UPDATE_WLAN_INFO_CMD, dispatchWlanInfo, responseVoid},
    {CNE_REQUEST_UPDATE_WWAN_INFO_CMD, dispatchWwanInfo, responseVoid},
    {CNE_NOTIFY_RAT_CONNECT_STATUS_CMD, dispatchRatStatus, responseVoid},
    {CNE_NOTIFY_DEFAULT_NW_PREF_CMD, dispatchInts, responseVoid},
    {CNE_REQUEST_UPDATE_WLAN_SCAN_RESULTS_CMD, dispatchWlanScanResults, responseVoid},
    {CNE_NOTIFY_SENSOR_EVENT_CMD, dispatchVoid, responseVoid},
    {CNE_REQUEST_CONFIG_IPROUTE2_CMD, dispatchIproute2Cmd, responseVoid},
    {CNE_NOTIFY_TIMER_EXPIRED_CMD, dispatchVoid, responseVoid},
    {CNE_REQUEST_START_FMC_CMD, dispatchString, responseVoid},
    {CNE_REQUEST_STOP_FMC_CMD, dispatchVoid, responseVoid},
    {CNE_REQUEST_UPDATE_WWAN_DORMANCY_INFO_CMD, dispatchInt, responseVoid},
    {CNE_REQUEST_UPDATE_DEFAULT_NETWORK_INFO_CMD, dispatchInt, responseVoid},
    {CNE_NOTIFY_SOCKET_CLOSED_CMD, dispatchVoid, responseVoid},
    {CNE_REQUEST_VENDOR_CMD, dispatchRaw, responseVoid}
