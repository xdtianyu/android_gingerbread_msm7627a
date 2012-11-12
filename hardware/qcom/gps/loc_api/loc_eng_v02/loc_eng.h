/* Copyright (c) 2009,2011 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 *
 */

#ifndef LOC_ENG_H
#define LOC_ENG_H

// Uncomment to keep all LOG messages (LOGD, LOGI, LOGV, etc.)
// #define LOG_NDEBUG 0

#include <stdbool.h>
#include "loc_api_v02_client.h"
#include <loc_eng_xtra.h>
#include <loc_eng_cfg.h>

#ifdef LOC_UTIL_TARGET_OFF_TARGET

#include "gps.h"

#else

#include <hardware/gps.h>

#endif //LOC_UTIL_TARGET_OFF_TARGET

#include <loc_eng_ni.h>

// The data connection minimal open time
#define DATA_OPEN_MIN_TIME        (1)  /* sec */

// The system sees GPS engine turns off after inactive for this period of time
#define GPS_AUTO_OFF_TIME         (2)  /* secs */
//To signify that when requesting a data connection HAL need not specify whether CDMA or UMTS
#define DONT_CARE                 (0)

#define MAX_NUM_ATL_CONNECTIONS   (2)

#define INVALID_ATL_CONNECTION_HANDLE  0xFFFFFFFF

#define MIN_POSSIBLE_FIX_INTERVAL   (1000) // in milliseconds

typedef enum {
   LOC_MUTE_SESS_NONE,
   LOC_MUTE_SESS_WAIT,
   LOC_MUTE_SESS_IN_SESSION
}loc_mute_session_e_type;

#define DEFERRED_ACTION_EVENT               (0x01)
#define DEFERRED_ACTION_DELETE_AIDING       (0x02)
#define DEFERRED_ACTION_AGPS_CONN_REQUEST   (0x04)
#define DEFERRED_ACTION_AGPS_DATA_OPENED    (0x08)
#define DEFERRED_ACTION_AGPS_DATA_CLOSED    (0x10)
#define DEFERRED_ACTION_AGPS_DATA_FAILED    (0x20)
#define DEFERRED_ACTION_QUIT                (0x40)

typedef enum
{
  LOC_ATL_OPEN_IND,
  LOC_ATL_CLOSE_IND,
  LOC_ATL_FAILURE_IND
}loc_eng_atl_ind_e_type;


typedef struct
{
  GpsPositionMode mode;
  GpsPositionRecurrence recurrence;
  uint32_t min_interval ;
  uint32_t preferred_accuracy ;
  uint32_t preferred_time;
}loc_eng_fix_criteria_s_type;

typedef enum
{
  // There is no connection request pending,
  // The data connection is not up, the open q should
  // be empty
  LOC_CONN_IDLE,
  // There is an outstanding open request,
  // waiting for the android layer to respond to open request
  LOC_CONN_OPEN_REQ,
  //the data connection is open, all outstanding atl open requests
  // from the engine have been responded to
  LOC_CONN_OPEN
  // outstanding close request, but android layer has not
  // torn down the data connection yet
 // LOC_CONN_CLOSE_REQ
}loc_eng_atl_session_state_e_type;

// This list contains the conn_handles that were specified in a
// ATL open reqyest from the modem but the corresponding
// close request has not been called.
typedef struct
{
  bool in_use;
  bool responded; // if response to the engine has been sent for this
  // handle
  uint32_t conn_handle;
}loc_eng_atl_list_type;

typedef struct
{
  // TBD: ATL server protocol
  //qmiLocServerProtocolEnumT_v02 server_protocol;

  // APN profile
  qmiLocApnProfilesStructT_v02 apn_profile;

  // current ATL state
  loc_eng_atl_session_state_e_type atl_state;

  // TBD: previous ATL state
  //loc_eng_atl_session_state_e_type prev_atl_state;

  // list of conn_handles specified in the open_request
  // that are not yet closed
  loc_eng_atl_list_type atl_open_conn_handle_list[MAX_NUM_ATL_CONNECTIONS];
}loc_eng_atl_info_s_type;

// global ATL state variable
extern loc_eng_atl_info_s_type loc_eng_atl_state;


// Module data
typedef struct
{
  locClientHandleType            client_handle;
  gps_location_callback          location_cb;
  gps_status_callback            status_cb;
  gps_sv_status_callback         sv_status_cb;
  agps_status_callback           agps_status_cb;
  gps_nmea_callback              nmea_cb;
  gps_ni_notify_callback         ni_notify_cb;
  gps_acquire_wakelock           acquire_wakelock_cb;
  gps_release_wakelock           release_wakelock_cb;

  //stored fix criteria
  loc_eng_fix_criteria_s_type    fix_criteria;

  loc_eng_xtra_data_s_type       xtra_module_data;

  // AGPS specific variables
  AGpsStatusValue                agps_status;
  // used to defer stopping the GPS engine until AGPS data calls are done
  bool                           agps_request_pending;
  // used to denote that a stop reuqest is pending because of AGPS session
  bool                           stop_request_pending;
  pthread_mutex_t                deferred_stop_mutex;

  // data from loc_event_cb
  int32_t                        loc_event_id;
  locClientEventIndUnionType     loc_event_payload;

  bool                           client_opened;

  // set to true when the client calls loc_eng_start and
  // set to false when the client calls loc_eng_stop
  // indicates client state before fix_session_status
  // is set through the loc api event
  bool                           navigating;

  // GPS engine status
  GpsStatusValue                 engine_status;
  // GPS
  GpsStatusValue                 fix_session_status;

  // Aiding data information to be deleted, aiding data can only be deleted when GPS engine is off
  GpsAidingData                  aiding_data_for_deletion;

  // Data variables used by deferred action thread
  pthread_t                      deferred_action_thread;

  // Mutex used by deferred action thread
  pthread_mutex_t                deferred_action_mutex;
  // Condition variable used by deferred action thread
  pthread_cond_t                 deferred_action_cond;
  // flags for pending events for deferred action thread
  int                            deferred_action_flags;
  // For muting session broadcast
  pthread_mutex_t                mute_session_lock;
  loc_mute_session_e_type        mute_session_state;

} loc_eng_data_s_type;

extern loc_eng_data_s_type loc_eng_data;

extern void loc_eng_mute_one_session();

#endif // LOC_ENG_H
