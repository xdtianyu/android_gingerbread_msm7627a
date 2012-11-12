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

    {0, NULL},                   //none
    {CNE_RESPONSE_REG_ROLE_MSG, responseInts},
    {CNE_RESPONSE_GET_COMPATIBLE_NWS_MSG, rspCompatibleNws},
    {CNE_RESPONSE_CONFIRM_NW_MSG, responseInts},
    {CNE_RESPONSE_DEREG_ROLE_MSG, responseInts},
    {CNE_REQUEST_BRING_RAT_DOWN_MSG, eventRatChange},
    {CNE_REQUEST_BRING_RAT_UP_MSG, eventRatChange},
    {CNE_NOTIFY_MORE_PREFERED_RAT_AVAIL_MSG, evtMorePrefNw},
    {CNE_NOTIFY_RAT_LOST_MSG, responseInts},
    {CNE_REQUEST_START_SCAN_WLAN_MSG, responseVoid},
    {CNE_NOTIFY_INFLIGHT_STATUS_MSG, responseInts},
    {CNE_NOTIFY_FMC_STATUS_MSG, responseInts},
    {CNE_NOTIFY_HOST_ROUTING_IP_MSG, responseString},
    {CNE_NOTIFY_VENDOR_MSG, responseRaw}
