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

#define LOG_NDDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>         /* struct sockaddr_in */
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <rpc/types.h>
#include "loc_api_rpc_glue.h"
#include "loc_apicb_appinit.h"

#include <cutils/properties.h>
#include <cutils/sched_policy.h>
#include <utils/SystemClock.h>
#include <string.h>

#include <loc_eng.h>
#include <loc_eng_dmn_conn.h>

#define LOG_TAG "libloc"
#include <utils/Log.h>

#define DEBUG_NI_REQUEST_EMU 0

#define LOC_DATA_DEFAULT FALSE          // Default data connection status (1=ON, 0=OFF)
#define TENMSDELAY   (10000)
#define SUCCESS TRUE
#define FAILURE FALSE

typedef void (*voidFuncPtr)(void);

// 2nd half of init(), singled out for
// modem restart to use.
static int loc_eng_reinit();
// Function declarations for sLocEngInterface
static int  loc_eng_init(GpsCallbacks* callbacks);
static int  loc_eng_start();
static int  loc_eng_stop();
static int  loc_eng_set_position_mode(GpsPositionMode mode, GpsPositionRecurrence recurrence,
            uint32_t min_interval, uint32_t preferred_accuracy, uint32_t preferred_time);
static void loc_eng_cleanup();
static int  loc_eng_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty);
static int loc_eng_inject_location(double latitude, double longitude, float accuracy);
static void loc_eng_delete_aiding_data(GpsAidingData f);
static const void* loc_eng_get_extension(const char* name);

// 2nd half of init(), singled out for
// modem restart to use.
static void loc_eng_agps_reinit();
// Function declarations for sLocEngAGpsInterface
static void loc_eng_agps_init(AGpsCallbacks* callbacks);
static int loc_eng_data_conn_open(const char* apn, AGpsBearerType bearerType);
static int loc_eng_data_conn_closed();
static int loc_eng_data_conn_failed();
static int loc_eng_set_server(AGpsType type, const char *hostname, int port);
static int loc_eng_set_server_proxy(AGpsType type, const char *hostname, int port);

// Internal functions
static int loc_eng_deinit();
static int loc_eng_set_apn(const char* apn);
static int32 loc_event_cb(rpc_loc_client_handle_type client_handle,
                          rpc_loc_event_mask_type loc_event,
                          const rpc_loc_event_payload_u_type* loc_event_payload);
static void loc_eng_rpc_global_cb(CLIENT* clnt, enum rpc_reset_event event);
static void loc_eng_report_position(const rpc_loc_parsed_position_s_type *location_report_ptr);
static void loc_eng_report_sv(const rpc_loc_gnss_info_s_type *gnss_report_ptr);
static void loc_inform_gps_status(GpsStatusValue status);
static void loc_eng_report_status(const rpc_loc_status_event_s_type *status_report_ptr);
static void loc_eng_report_nmea(const rpc_loc_nmea_report_s_type *nmea_report_ptr);
static void loc_eng_process_conn_request(const rpc_loc_server_request_s_type *server_request_ptr);

static void loc_eng_deferred_action_thread(void* arg);
static void loc_eng_process_atl_action(rpc_loc_server_connection_handle conn_handle,
                                       AGpsStatusValue status, AGpsType agpsType);

static void loc_eng_delete_aiding_data_action(GpsAidingData delete_bits);
/* Helper functions to manage the state machine for each ATL session*/
static boolean check_if_any_connection(loc_eng_atl_session_state_e_type conn_state,int session_index);
static boolean check_if_all_connection(loc_eng_atl_session_state_e_type conn_state,int session_index);
static int loc_eng_get_index(rpc_loc_server_connection_handle active_conn_handle);
static void loc_eng_ioctl_data_close_status(int is_succ);
static void loc_eng_report_modem_state(rpc_loc_engine_state_e_type state) ;
static void loc_eng_set_deferred_action(unsigned int bits, voidFuncPtr setupLocked);

static void loc_eng_agps_ril_init( AGpsRilCallbacks* callbacks );
static void loc_eng_agps_ril_set_ref_location(const AGpsRefLocation *agps_reflocation, size_t sz_struct);
static void loc_eng_agps_ril_set_set_id(AGpsSetIDType type, const char* setid);
static void loc_eng_agps_ril_ni_message(uint8_t *msg, size_t len);
static void loc_eng_agps_ril_update_network_state(int connected, int type, int roaming, const char* extra_info);
static void loc_eng_agps_ril_update_network_availability(int avaiable, const char* apn);

// Defines the GpsInterface in gps.h
static const GpsInterface sLocEngInterface =
{
   sizeof(GpsInterface),
   loc_eng_init,
   loc_eng_start,
   loc_eng_stop,
   loc_eng_cleanup,
   loc_eng_inject_time,
   loc_eng_inject_location,
   loc_eng_delete_aiding_data,
   loc_eng_set_position_mode,
   loc_eng_get_extension,
};

static const AGpsInterface sLocEngAGpsInterface =
{
   sizeof(AGpsInterface),
   loc_eng_agps_init,
   loc_eng_data_conn_open,
   loc_eng_data_conn_closed,
   loc_eng_data_conn_failed,
   loc_eng_set_server_proxy
};

static const AGpsRilInterface sLocEngAGpsRilInterface =
{
   sizeof(AGpsRilInterface),
   loc_eng_agps_ril_init,
   loc_eng_agps_ril_set_ref_location,
   loc_eng_agps_ril_set_set_id,
   loc_eng_agps_ril_ni_message,
   loc_eng_agps_ril_update_network_state,
   loc_eng_agps_ril_update_network_availability
};

// Global data structure for location engine
loc_eng_data_s_type loc_eng_data;
int loc_eng_inited = 0; /* not initialized */

// Address buffers, for addressing setting before init
int    supl_host_set = 0;
char   supl_host_buf[100];
int    supl_port_buf;
int    c2k_host_set = 0;
char   c2k_host_buf[100];
int    c2k_port_buf;

/*********************************************************************
 * C2K support for Donut before Eclair supports on-demand bring up
 *********************************************************************/

/* Force always-on data support for all technologies if set to 1 */
int loc_force_always_on = 0;

/* Flag for availability of C2K PDE address */
int loc_c2k_addr_is_set = 0;

/*********************************************************************
 * Initialization checking macros
 *********************************************************************/
#define INIT_CHECK_RET(x, c) \
  if (!loc_eng_inited) \
  { \
     /* Not intialized, abort */ \
     LOC_LOGE("%s: GPS not initialized.", x); \
     return c; \
  }
#define INIT_CHECK(x) INIT_CHECK_RET(x, RPC_LOC_API_INVALID_HANDLE)
#define INIT_CHECK_VOID(x) INIT_CHECK_RET(x, )

/*===========================================================================
FUNCTION    gps_get_hardware_interface

DESCRIPTION
   Returns the GPS hardware interaface based on LOC API
   if GPS is enabled.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
const GpsInterface* gps_get_hardware_interface ()
{
   char propBuf[PROPERTY_VALUE_MAX];

   // check to see if GPS should be disabled
   property_get("gps.disable", propBuf, "");
   if (propBuf[0] == '1')
   {
      LOC_LOGD("gps_get_interface returning NULL because gps.disable=1\n");
      return NULL;
   }

   return &sLocEngInterface;
}

/*===========================================================================
FUNCTION    loc_eng_init

DESCRIPTION
   Initialize the location engine, this include setting up global datas
   and registers location engien with loc api service.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
// fully shutting down the GPS is temporarily disabled to avoid intermittent BP crash
#define DISABLE_CLEANUP 1

static int loc_eng_init(GpsCallbacks* callbacks)
{
   LOC_LOGD("loc_eng_init entering");
#if DISABLE_CLEANUP
    if (loc_eng_data.deferred_action_thread) {
        LOC_LOGD("loc_eng_init. already initialized so return SUCCESS\n");
        return 0;
    }
#endif

   // Start the LOC api RPC service (if not started yet)
    if (0 == loc_api_glue_init())
        return 0;

   // Avoid repeated initialization. Call de-init to clean up first.
   if (loc_eng_inited == 1)
   {
      loc_eng_deinit();       /* stop the active client */
#ifdef FEATURE_GNSS_BIT_API
      {
          char baseband[PROPERTY_VALUE_MAX];
          property_get("ro.baseband", baseband, "msm");
          if ((strcmp(baseband,"svlte2a") == 0))
          {
              loc_eng_dmn_conn_loc_api_server_unblock();
              loc_eng_dmn_conn_loc_api_server_join();
          }
      }
#endif /* FEATURE_GNSS_BIT_API */
      loc_eng_inited = 0;
   }

   // Process gps.conf
   loc_read_gps_conf();
   callbacks->set_capabilities_cb(gps_conf.CAPABILITIES);

   // Save callbacks
   memset(&loc_eng_data, 0, sizeof (loc_eng_data_s_type));
   loc_eng_data.location_cb  = callbacks->location_cb;
   loc_eng_data.sv_status_cb = callbacks->sv_status_cb;
   loc_eng_data.status_cb    = callbacks->status_cb;
   loc_eng_data.nmea_cb      = callbacks->nmea_cb;
   loc_eng_data.acquire_wakelock_cb = callbacks->acquire_wakelock_cb;
   loc_eng_data.release_wakelock_cb = callbacks->release_wakelock_cb;

   // Loc engine module data initialization
   loc_eng_data.engine_status = GPS_STATUS_NONE;
   loc_eng_data.fix_session_status = GPS_STATUS_NONE;

   loc_eng_data.loc_event = 0;
   loc_eng_data.deferred_action_flags = 0;
   // Mute session
   loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;
   memset(loc_eng_data.apn_name, 0, sizeof loc_eng_data.apn_name);

   loc_eng_data.aiding_data_for_deletion = 0;

   pthread_mutex_init(&loc_eng_data.mute_session_lock, NULL);
   pthread_mutex_init(&loc_eng_data.deferred_action_mutex, NULL);
   pthread_cond_init(&loc_eng_data.deferred_action_cond, NULL);
   pthread_mutex_init (&(loc_eng_data.deferred_stop_mutex), NULL);

   // Create threads (if not yet created)
   if (!loc_eng_inited)
   {
      loc_eng_data.deferred_action_thread = callbacks->create_thread_cb("loc_api",loc_eng_deferred_action_thread, NULL);
#ifdef FEATURE_GNSS_BIT_API
      {
          char baseband[PROPERTY_VALUE_MAX];
          property_get("ro.baseband", baseband, "msm");
          if ((strcmp(baseband,"svlte2a") == 0))
          {
              loc_eng_dmn_conn_loc_api_server_launch(callbacks->create_thread_cb, NULL, NULL);
          }
      }
#endif /* FEATURE_GNSS_BIT_API */
   }

   // XTRA module data initialization
   pthread_mutex_init(&loc_eng_data.xtra_module_data.lock, NULL);
   loc_eng_data.xtra_module_data.download_request_cb = NULL;

   loc_eng_inited = 1;

   return loc_eng_reinit();
}

static int loc_eng_reinit()
{
   // Open client
   rpc_loc_event_mask_type event = RPC_LOC_EVENT_PARSED_POSITION_REPORT |
                                   RPC_LOC_EVENT_SATELLITE_REPORT |
                                   RPC_LOC_EVENT_LOCATION_SERVER_REQUEST |
                                   RPC_LOC_EVENT_ASSISTANCE_DATA_REQUEST |
                                   RPC_LOC_EVENT_IOCTL_REPORT |
                                   RPC_LOC_EVENT_STATUS_REPORT |
                                   RPC_LOC_EVENT_NMEA_1HZ_REPORT |
                                   RPC_LOC_EVENT_NI_NOTIFY_VERIFY_REQUEST;
   loc_eng_data.client_handle = loc_open(event, loc_event_cb, loc_eng_rpc_global_cb);

   loc_eng_data.client_opened = (loc_eng_data.client_handle >= 0);
   LOC_LOGD("loc_eng_init created client, id = %d\n", (int32) loc_eng_data.client_handle);

   rpc_loc_ioctl_data_u_type ioctl_data = {RPC_LOC_IOCTL_SET_SUPL_VERSION, {0}};
   ioctl_data.rpc_loc_ioctl_data_u_type_u.supl_version = gps_conf.SUPL_VER;
   loc_eng_ioctl (loc_eng_data.client_handle,
                  RPC_LOC_IOCTL_SET_SUPL_VERSION,
                  &ioctl_data,
                  LOC_IOCTL_DEFAULT_TIMEOUT,
                  NULL);

   return 0;
}

/*===========================================================================
FUNCTION    loc_eng_deinit

DESCRIPTION
   De-initialize the location engine. This includes stopping fixes and
   closing the client.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_deinit()
{
   LOC_LOGD("loc_eng_deinit called");

   if (loc_eng_inited == 1)
   {
      if (loc_eng_data.navigating)
      {
         LOC_LOGD("loc_eng_init: fix not stopped. stop it now.");
         loc_eng_stop();
         loc_eng_data.navigating = FALSE;
      }

      if (loc_eng_data.client_opened)
      {
         LOC_LOGD("loc_eng_init: client opened. close it now.");
         loc_close(loc_eng_data.client_handle);
         loc_eng_data.client_opened = FALSE;
      }

      loc_eng_inited = 0;
   }

   return 0;
}

/*===========================================================================
FUNCTION    loc_eng_cleanup

DESCRIPTION
   Cleans location engine. The location client handle will be released.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_cleanup()
{
#if DISABLE_CLEANUP
    return;
#else
   // clean up
   loc_eng_deinit();

   if (loc_eng_data.deferred_action_thread)
   {
      /* Terminate deferred action working thread */
      pthread_mutex_lock (&loc_eng_data.deferred_action_mutex);
      /* hold a wake lock while events are pending for deferred_action_thread */
      loc_eng_data.acquire_wakelock_cb();
      loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_QUIT;
      pthread_cond_signal  (&loc_eng_data.deferred_action_cond);
      pthread_mutex_unlock (&loc_eng_data.deferred_action_mutex);

      void* ignoredValue;
      pthread_join(loc_eng_data.deferred_action_thread, &ignoredValue);
      loc_eng_data.deferred_action_thread = NULL;
   }

   pthread_mutex_destroy (&loc_eng_data.xtra_module_data.lock);
   pthread_mutex_destroy (&loc_eng_data.deferred_stop_mutex);
   pthread_cond_destroy  (&loc_eng_data.deferred_action_cond);
   pthread_mutex_destroy (&loc_eng_data.deferred_action_mutex);
   pthread_mutex_destroy (&loc_eng_data.mute_session_lock);
#endif
}


/*===========================================================================
FUNCTION    loc_eng_start

DESCRIPTION
   Starts the tracking session

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_start()
{
   INIT_CHECK("loc_eng_start");

   int ret_val;
   LOC_LOGD("loc_eng_start called");
   //Reset the Stop pending flag to ensure the positioning session continues
   //even when we get a sequence Start-Stop_start from the framework and we
   //do not get a chance to send a loc_stop after CM responds to us
   pthread_mutex_lock(&(loc_eng_data.deferred_stop_mutex));
   loc_eng_data.stop_request_pending = false;
   pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));
   ret_val = loc_start_fix(loc_eng_data.client_handle);

   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE("loc_eng_start error, rc = %d\n", ret_val);
      if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
         loc_eng_data.navigating = TRUE;
         loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
      }
   }
   else {
      loc_eng_data.navigating = TRUE;
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_stop

DESCRIPTION
   Stops the tracking session

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_stop()
{
   INIT_CHECK("loc_eng_stop");

   int ret_val;
   LOC_LOGD("loc_eng_stop called");
   pthread_mutex_lock(&(loc_eng_data.deferred_stop_mutex));
    // work around problem with loc_eng_stop when AGPS requests are pending
    // we defer stopping the engine until the AGPS request is done
    if (loc_eng_data.agps_request_pending)
    {
        loc_eng_data.stop_request_pending = true;
        LOC_LOGD("loc_eng_stop - deferring stop until AGPS data call is finished\n");
        pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));
        return 0;
    }
   pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));

   ret_val = loc_stop_fix(loc_eng_data.client_handle);
   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE("loc_eng_stop error, rc = %d\n", ret_val);
      if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
         loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
      }
   } else {
      if (loc_eng_data.fix_session_status != GPS_STATUS_SESSION_BEGIN)
      {
         loc_inform_gps_status(GPS_STATUS_SESSION_END);
      }
   }
   loc_eng_data.navigating = FALSE;

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_mute_one_session

DESCRIPTION
   Mutes one session

DEPENDENCIES
   None

RETURN VALUE
   0: Success

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_mute_one_session()
{
   INIT_CHECK_VOID("loc_eng_mute_one_session");
   LOC_LOGD("loc_eng_mute_one_session");

   pthread_mutex_lock(&loc_eng_data.mute_session_lock);
   loc_eng_data.mute_session_state = LOC_MUTE_SESS_WAIT;
   LOC_LOGV("loc_eng_report_status: mute_session_state changed to WAIT");
   pthread_mutex_unlock(&loc_eng_data.mute_session_lock);
}

/*===========================================================================
FUNCTION    loc_eng_set_position_mode

DESCRIPTION
   Sets the mode and fix frequency for the tracking session.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int  loc_eng_set_position_mode(GpsPositionMode mode, GpsPositionRecurrence recurrence,
            uint32_t min_interval, uint32_t preferred_accuracy, uint32_t preferred_time)
{
   INIT_CHECK("loc_eng_set_position_mode");

   rpc_loc_ioctl_data_u_type*    ioctl_data = &loc_eng_data.position_mode;
   rpc_loc_fix_criteria_s_type *fix_criteria_ptr;
   rpc_loc_ioctl_e_type         ioctl_type = RPC_LOC_IOCTL_SET_FIX_CRITERIA;
   rpc_loc_operation_mode_e_type op_mode;
   int                          ret_val;

   LOGD ("loc_eng_set_position mode, client = %d, interval = %d, mode = %d\n",
            (int32) loc_eng_data.client_handle, min_interval, mode);

   switch (mode)
   {
   case GPS_POSITION_MODE_MS_BASED:
      op_mode = RPC_LOC_OPER_MODE_MSB;
      break;
   case GPS_POSITION_MODE_MS_ASSISTED:
      op_mode = RPC_LOC_OPER_MODE_MSA;
      break;
   default:
      op_mode = RPC_LOC_OPER_MODE_STANDALONE;
   }

   fix_criteria_ptr = &ioctl_data->rpc_loc_ioctl_data_u_type_u.fix_criteria;
   fix_criteria_ptr->valid_mask = RPC_LOC_FIX_CRIT_VALID_PREFERRED_OPERATION_MODE |
                                  RPC_LOC_FIX_CRIT_VALID_RECURRENCE_TYPE;
   fix_criteria_ptr->min_interval = min_interval;
   fix_criteria_ptr->preferred_operation_mode = op_mode;

   if (min_interval > 0) {
        fix_criteria_ptr->min_interval = min_interval;
        fix_criteria_ptr->valid_mask |= RPC_LOC_FIX_CRIT_VALID_MIN_INTERVAL;
    }else if(min_interval == 0)
    {
        /*If the framework passes in 0 transalate it into the maximum frequency we can report positions
          which is 1 Hz or once very second */
        fix_criteria_ptr->min_interval = MIN_POSSIBLE_FIX_INTERVAL;
        fix_criteria_ptr->valid_mask |= RPC_LOC_FIX_CRIT_VALID_MIN_INTERVAL;
    }
    if (preferred_accuracy > 0) {
        fix_criteria_ptr->preferred_accuracy = preferred_accuracy;
        fix_criteria_ptr->valid_mask |= RPC_LOC_FIX_CRIT_VALID_PREFERRED_ACCURACY;
    }
    if (preferred_time > 0) {
        fix_criteria_ptr->preferred_response_time = preferred_time;
        fix_criteria_ptr->valid_mask |= RPC_LOC_FIX_CRIT_VALID_PREFERRED_RESPONSE_TIME;
    }

     switch (recurrence) {
        case GPS_POSITION_RECURRENCE_SINGLE:
            fix_criteria_ptr->recurrence_type = RPC_LOC_SINGLE_FIX;
            break;
        case GPS_POSITION_RECURRENCE_PERIODIC:
        default:
            fix_criteria_ptr->recurrence_type = RPC_LOC_PERIODIC_FIX;
            break;
    }
   ioctl_data->disc = ioctl_type;

   ret_val = loc_eng_ioctl (loc_eng_data.client_handle,
                            ioctl_type,
                            ioctl_data,
                            LOC_IOCTL_DEFAULT_TIMEOUT,
                            NULL /* No output information is expected*/);

   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE("loc_eng_set_position mode failed\n");
      if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
         loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
      }
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_inject_time

DESCRIPTION
   This is used by Java native function to do time injection.

DEPENDENCIES
   None

RETURN VALUE
   RPC_LOC_API_SUCCESS

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
   INIT_CHECK("loc_eng_inject_time");

   rpc_loc_ioctl_data_u_type        ioctl_data;
   rpc_loc_assist_data_time_s_type *time_info_ptr;
   rpc_loc_ioctl_e_type             ioctl_type = RPC_LOC_IOCTL_INJECT_UTC_TIME;
   int                              ret_val;

   LOC_LOGD ("loc_eng_inject_time, uncertainty = %d\n", uncertainty);

   time_info_ptr = &ioctl_data.rpc_loc_ioctl_data_u_type_u.assistance_data_time;
   time_info_ptr->time_utc = time;
   time_info_ptr->time_utc += (int64_t)(android::elapsedRealtime() - timeReference);
   time_info_ptr->uncertainty = uncertainty; // Uncertainty in ms

   ioctl_data.disc = ioctl_type;
   ret_val = loc_eng_ioctl (loc_eng_data.client_handle,
                            ioctl_type,
                            &ioctl_data,
                            LOC_IOCTL_DEFAULT_TIMEOUT,
                            NULL /* No output information is expected*/);

   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE ("loc_eng_inject_time failed\n");
      if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
         loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
      }
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_inject_location

DESCRIPTION
   This is used by Java native function to do location injection.

DEPENDENCIES
   None

RETURN VALUE
   0          : Successful
   error code : Failure

SIDE EFFECTS
   N/A
===========================================================================*/
static int loc_eng_inject_location(double latitude, double longitude, float accuracy)
{
   INIT_CHECK("loc_eng_inject_location");

   /* IOCTL data */
   rpc_loc_ioctl_data_u_type ioctl_data;
   rpc_loc_assist_data_pos_s_type *assistance_data_position =
      &ioctl_data.rpc_loc_ioctl_data_u_type_u.assistance_data_position;
   int                          ret_val;

   /************************************************
    * Fill in latitude, longitude & accuracy
    ************************************************/

   /* This combo is required */
   assistance_data_position->valid_mask =
      RPC_LOC_ASSIST_POS_VALID_LATITUDE |
      RPC_LOC_ASSIST_POS_VALID_LONGITUDE |
      RPC_LOC_ASSIST_POS_VALID_HOR_UNC_CIRCULAR |
      RPC_LOC_ASSIST_POS_VALID_CONFIDENCE_HORIZONTAL;

   assistance_data_position->latitude = latitude;
   assistance_data_position->longitude = longitude;
   assistance_data_position->hor_unc_circular = accuracy; /* Meters assumed */
   assistance_data_position->confidence_horizontal = 63;  /* 63% (1 std dev) assumed */

   /* Log */
   LOC_LOGD("Inject coarse position Lat=%lf, Lon=%lf, Acc=%.2lf\n",
         (double) assistance_data_position->latitude,
         (double) assistance_data_position->longitude,
         (double) assistance_data_position->hor_unc_circular);

   /* Make the API call */
   ret_val = loc_eng_ioctl (loc_eng_data.client_handle,
                            RPC_LOC_IOCTL_INJECT_POSITION,
                            &ioctl_data,
                            LOC_IOCTL_DEFAULT_TIMEOUT,
                            NULL /* No output information is expected*/);
   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE("loc_eng_inject_injection failed.\n");
      if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
         loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
      }
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_delete_aiding_data

DESCRIPTION
   This is used by Java native function to delete the aiding data. The function
   updates the global variable for the aiding data to be deleted. If the GPS
   engine is off, the aiding data will be deleted. Otherwise, the actual action
   will happen when gps engine is turned off.

DEPENDENCIES
   Assumes the aiding data type specified in GpsAidingData matches with
   LOC API specification.

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_delete_aiding_data(GpsAidingData f)
{
   INIT_CHECK_VOID("loc_eng_delete_aiding_data");

   pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);

   loc_eng_data.aiding_data_for_deletion |= f;

   if (loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON &&
       loc_eng_data.aiding_data_for_deletion != 0)
   {
      /* hold a wake lock while events are pending for deferred_action_thread */
      loc_eng_data.acquire_wakelock_cb();
      loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_DELETE_AIDING;
      pthread_cond_signal(&loc_eng_data.deferred_action_cond);

      // In case gps engine is ON, the assistance data will be deleted when the engine is OFF
   }

   pthread_mutex_unlock(&loc_eng_data.deferred_action_mutex);
}

/*===========================================================================
FUNCTION    loc_eng_get_extension

DESCRIPTION
   Get the gps extension to support XTRA.

DEPENDENCIES
   N/A

RETURN VALUE
   The GPS extension interface.

SIDE EFFECTS
   N/A

===========================================================================*/
static const void* loc_eng_get_extension(const char* name)
{
   if (strcmp(name, GPS_XTRA_INTERFACE) == 0)
   {
      return &sLocEngXTRAInterface;
   }

   else if (strcmp(name, AGPS_INTERFACE) == 0)
   {
      return &sLocEngAGpsInterface;
   }

   else if (strcmp(name, GPS_NI_INTERFACE) == 0)
   {
      return &sLocEngNiInterface;
   }

   else if (strcmp(name, AGPS_RIL_INTERFACE) == 0)
   {
      return &sLocEngAGpsRilInterface;
   }

   return NULL;
}

/*===========================================================================
FUNCTION    loc_inform_gps_state

DESCRIPTION
   Informs the GPS Provider about the GPS status

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_inform_gps_status(GpsStatusValue status)
{
   static GpsStatusValue last_status = GPS_STATUS_NONE;

   GpsStatus gs = { sizeof(gs),status };

   LOC_LOGD("loc_inform_gps_status, status: %s", loc_get_gps_status_name(status));

   if (loc_eng_data.status_cb)
   {
      loc_eng_data.status_cb(&gs);

      // Restore session begin if needed
      if (status == GPS_STATUS_ENGINE_ON && last_status == GPS_STATUS_SESSION_BEGIN)
      {
         GpsStatus gs_sess_begin = { sizeof(gs_sess_begin),GPS_STATUS_SESSION_BEGIN };
         loc_eng_data.status_cb(&gs_sess_begin);
      }
   }

   last_status = status;
}

#if DEBUG_NI_REQUEST_EMU == 1
/*===========================================================================
FUNCTION    emulate_ni_request

DESCRIPTION
   DEBUG tool: simulate an NI request

DEPENDENCIES
   N/A

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void* emulate_ni_request(void* arg)
{
   static int busy = 0;

   if (busy) return NULL;

   busy = 1;

   sleep(5);

   rpc_loc_client_handle_type           client_handle;
   rpc_loc_event_mask_type              loc_event;
   rpc_loc_event_payload_u_type         payload;
   rpc_loc_ni_event_s_type             *ni_req;
   rpc_loc_ni_supl_notify_verify_req_s_type *supl_req;

   client_handle = (rpc_loc_client_handle_type) arg;

   loc_event = RPC_LOC_EVENT_NI_NOTIFY_VERIFY_REQUEST;
   payload.disc = loc_event;

   ni_req = &payload.rpc_loc_event_payload_u_type_u.ni_request;
   ni_req->event = RPC_LOC_NI_EVENT_SUPL_NOTIFY_VERIFY_REQ;
   supl_req = &ni_req->payload.rpc_loc_ni_event_payload_u_type_u.supl_req;

   // Encodings for Spirent Communications
   char client_name[80]  = {0x53,0x78,0x5A,0x5E,0x76,0xD3,0x41,0xC3,0x77,
         0xBB,0x5D,0x77,0xA7,0xC7,0x61,0x7A,0xFA,0xED,0x9E,0x03};
   char requestor_id[80] = {0x53,0x78,0x5A,0x5E,0x76,0xD3,0x41,0xC3,0x77,
         0xBB,0x5D,0x77,0xA7,0xC7,0x61,0x7A,0xFA,0xED,0x9E,0x03};

   supl_req->flags = RPC_LOC_NI_CLIENT_NAME_PRESENT |
                     RPC_LOC_NI_REQUESTOR_ID_PRESENT |
                     RPC_LOC_NI_ENCODING_TYPE_PRESENT;

   supl_req->datacoding_scheme = RPC_LOC_NI_SUPL_GSM_DEFAULT;

   supl_req->client_name.data_coding_scheme = RPC_LOC_NI_SUPL_GSM_DEFAULT; // no coding
   supl_req->client_name.client_name_string.client_name_string_len = strlen(client_name);
   supl_req->client_name.client_name_string.client_name_string_val = client_name;
   supl_req->client_name.string_len = strlen(client_name);

   supl_req->requestor_id.data_coding_scheme = RPC_LOC_NI_SUPL_GSM_DEFAULT;
   supl_req->requestor_id.requestor_id_string.requestor_id_string_len = strlen(requestor_id);
   supl_req->requestor_id.requestor_id_string.requestor_id_string_val = requestor_id;
   supl_req->requestor_id.string_len = strlen(requestor_id);

   supl_req->notification_priv_type = RPC_LOC_NI_USER_NOTIFY_VERIFY_ALLOW_NO_RESP;
   supl_req->user_response_timer = 10;

   loc_event_cb(client_handle, loc_event, &payload);

   busy = 0;

   return NULL;
}
#endif /* DEBUG_NI_REQUEST_EMU */

/*===========================================================================
FUNCTION    loc_event_cb

DESCRIPTION
   This is the callback function registered by loc_open.

DEPENDENCIES
   N/A

RETURN VALUE
   RPC_LOC_API_SUCCESS

SIDE EFFECTS
   N/A

===========================================================================*/
static int32 loc_event_cb
(
      rpc_loc_client_handle_type           client_handle,
      rpc_loc_event_mask_type              loc_event,
      const rpc_loc_event_payload_u_type*  loc_event_payload
)
{
   if(loc_event == RPC_LOC_EVENT_IOCTL_REPORT)
      return RPC_LOC_API_SUCCESS;

   INIT_CHECK("loc_event_cb");
   loc_eng_callback_log_header(client_handle, loc_event, loc_event_payload);

   if (client_handle != loc_eng_data.client_handle)
   {
      LOC_LOGE("loc client mismatch: received = %d, expected = %d \n",
            (int32) client_handle, (int32) loc_eng_data.client_handle);
      return RPC_LOC_API_SUCCESS; /* Exit with OK */
   }

   loc_eng_callback_log(loc_event, loc_event_payload);
   pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);
   loc_eng_data.loc_event = loc_event;
   memcpy(&loc_eng_data.loc_event_payload, loc_event_payload, sizeof(*loc_event_payload));
   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_EVENT;
   pthread_cond_signal  (&loc_eng_data.deferred_action_cond);
   pthread_mutex_unlock (&loc_eng_data.deferred_action_mutex);

   return RPC_LOC_API_SUCCESS;//We simply want to return sucess here as we do not want to
                              // cause any issues in RPC thread context
}

/*===========================================================================
FUNCTION    loc_eng_rpc_global_cb

DESCRIPTION
   This is the callback function registered by loc_open for RPC global events

DEPENDENCIES
   N/A

RETURN VALUE
   RPC_LOC_API_SUCCESS

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_rpc_global_cb(CLIENT* clnt, enum rpc_reset_event event) {
    switch (event) {
    case RPC_SUBSYSTEM_RESTART_BEGIN:
        loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
        break;
    case RPC_SUBSYSTEM_RESTART_END:
        loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_UP_DETECTED, NULL);
    }
}

/*===========================================================================
FUNCTION    loc_eng_report_position

DESCRIPTION
   Reports position information to the Java layer.

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_position(const rpc_loc_parsed_position_s_type *location_report_ptr)
{
   GpsLocation location;

   memset(&location, 0, sizeof (GpsLocation));
   location.size = sizeof(location);
   if (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_SESSION_STATUS)
   {
      // Process the position from final and intermediate reports
      if (location_report_ptr->session_status == RPC_LOC_SESS_STATUS_SUCCESS ||
          (location_report_ptr->session_status == RPC_LOC_SESS_STATUS_IN_PROGESS &&
          gps_conf.INTERMEDIATE_POS))
      {
         // Time stamp (UTC)
         if (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_TIMESTAMP_UTC)
         {
            location.timestamp = location_report_ptr->timestamp_utc;
         }

         // Latitude & Longitude
         if ( (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_LATITUDE) &&
               (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_LONGITUDE) )
         {
            location.flags    |= GPS_LOCATION_HAS_LAT_LONG;
            location.latitude  = location_report_ptr->latitude;
            location.longitude = location_report_ptr->longitude;
         }

         // Altitude
         if (location_report_ptr->valid_mask &  RPC_LOC_POS_VALID_ALTITUDE_WRT_ELLIPSOID )
         {
            location.flags    |= GPS_LOCATION_HAS_ALTITUDE;
            location.altitude = location_report_ptr->altitude_wrt_ellipsoid;
         }

         // Speed
         if ((location_report_ptr->valid_mask & RPC_LOC_POS_VALID_SPEED_HORIZONTAL) &&
             (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_SPEED_VERTICAL))
         {
            location.flags    |= GPS_LOCATION_HAS_SPEED;
            location.speed = sqrt(location_report_ptr->speed_horizontal * location_report_ptr->speed_horizontal +
                                  location_report_ptr->speed_vertical * location_report_ptr->speed_vertical);
         }

         // Heading
         if (location_report_ptr->valid_mask &  RPC_LOC_POS_VALID_HEADING)
         {
            location.flags    |= GPS_LOCATION_HAS_BEARING;
            location.bearing = location_report_ptr->heading;
         }

         // Uncertainty (circular)
         if ( (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_HOR_UNC_CIRCULAR) )
         {
            location.flags    |= GPS_LOCATION_HAS_ACCURACY;
            location.accuracy = location_report_ptr->hor_unc_circular;
         }

         // Filtering
         boolean filter_out = FALSE;

         // Filter any 0,0 positions
         if (location.latitude == 0.0 && location.longitude == 0.0)
         {
            filter_out = TRUE;
         }

         // Turn-off intermediate positions outside required accuracy
         if (gps_conf.ACCURACY_THRES != 0 &&
             location_report_ptr->session_status == RPC_LOC_SESS_STATUS_IN_PROGESS &&
             (location_report_ptr->valid_mask & RPC_LOC_POS_VALID_HOR_UNC_CIRCULAR) &&
               location_report_ptr->hor_unc_circular > gps_conf.ACCURACY_THRES)
         {
            LOC_LOGW("loc_eng_report_position: ignore intermediate position with error %.2f > %ld meters\n",
                  location_report_ptr->hor_unc_circular, gps_conf.ACCURACY_THRES);
            filter_out = TRUE;
         }

         if (loc_eng_data.location_cb != NULL && !filter_out)
         {
            LOC_LOGV("loc_eng_report_position: fire callback\n");
            loc_eng_data.location_cb(&location);
         }
      }
      else
      {
         LOC_LOGV("loc_eng_report_position: ignore position report when session status = %d\n", location_report_ptr->session_status);
      }
   }
   else
   {
      LOC_LOGV("loc_eng_report_position: ignore position report when session status is not set\n");
   }
}

/*===========================================================================
FUNCTION    loc_eng_report_sv

DESCRIPTION
   Reports GPS satellite information to the Java layer.

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_sv (const rpc_loc_gnss_info_s_type *gnss_report_ptr)
{
   GpsSvStatus     SvStatus;
   int             num_svs_max, i;
    const rpc_loc_sv_info_s_type *sv_info_ptr;

   // LOC_LOGD ("loc_eng_report_sv: valid_mask = 0x%x, num of sv = %d\n",
   //        (uint32) gnss_report_ptr->valid_mask,
   //        gnss_report_ptr->sv_count);

   num_svs_max = 0;
   memset (&SvStatus, 0, sizeof (GpsSvStatus));
   if (gnss_report_ptr->valid_mask & RPC_LOC_GNSS_INFO_VALID_SV_COUNT)
   {
      num_svs_max = gnss_report_ptr->sv_count;
      if (num_svs_max > GPS_MAX_SVS)
      {
         num_svs_max = GPS_MAX_SVS;
      }
   }

   if (gnss_report_ptr->valid_mask & RPC_LOC_GNSS_INFO_VALID_SV_LIST)
   {
      SvStatus.num_svs = 0;

      for (i = 0; i < num_svs_max; i++)
      {
         sv_info_ptr = &(gnss_report_ptr->sv_list.sv_list_val[i]);
         if (sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_SYSTEM)
         {
            if (sv_info_ptr->system == RPC_LOC_SV_SYSTEM_GPS)
            {
               SvStatus.sv_list[SvStatus.num_svs].size = sizeof(GpsSvStatus);
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn;

               // We only have the data field to report gps eph and alm mask
               if ((sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_HAS_EPH) &&
                   (sv_info_ptr->has_eph == 1))
               {
                  SvStatus.ephemeris_mask |= (1 << (sv_info_ptr->prn-1));
               }

               if ((sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_HAS_ALM) &&
                   (sv_info_ptr->has_alm == 1))
               {
                  SvStatus.almanac_mask |= (1 << (sv_info_ptr->prn-1));
               }

               if ((sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_PROCESS_STATUS) &&
                   (sv_info_ptr->process_status == RPC_LOC_SV_STATUS_TRACK))
               {
                  SvStatus.used_in_fix_mask |= (1 << (sv_info_ptr->prn-1));
               }
            }
            // SBAS: GPS RPN: 120-151,
            // In exteneded measurement report, we follow nmea standard, which is from 33-64.
            else if (sv_info_ptr->system == RPC_LOC_SV_SYSTEM_SBAS)
            {
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn + 33 - 120;
            }
            // Gloness: Slot id: 1-32
            // In extended measurement report, we follow nmea standard, which is 65-96
            else if (sv_info_ptr->system == RPC_LOC_SV_SYSTEM_GLONASS)
            {
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn + (65-1);
            }
            // Unsupported SV system
            else
            {
               continue;
            }
         }

         if (sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_SNR)
         {
            SvStatus.sv_list[SvStatus.num_svs].snr = sv_info_ptr->snr;
         }

         if (sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_ELEVATION)
         {
            SvStatus.sv_list[SvStatus.num_svs].elevation = sv_info_ptr->elevation;
         }

         if (sv_info_ptr->valid_mask & RPC_LOC_SV_INFO_VALID_AZIMUTH)
         {
            SvStatus.sv_list[SvStatus.num_svs].azimuth = sv_info_ptr->azimuth;
         }

         SvStatus.num_svs++;
      }
   }

   // LOC_LOGD ("num_svs = %d, eph mask = %d, alm mask = %d\n", SvStatus.num_svs, SvStatus.ephemeris_mask, SvStatus.almanac_mask );
   if ((SvStatus.num_svs != 0) && (loc_eng_data.sv_status_cb != NULL))
   {
      loc_eng_data.sv_status_cb(&SvStatus);
   }
}

/*===========================================================================
FUNCTION    loc_eng_report_status

DESCRIPTION
   Reports GPS engine state to Java layer.

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_status (const rpc_loc_status_event_s_type *status_report_ptr)
{
   GpsStatusValue status;

   LOC_LOGD("loc_eng_report_status: event = %d engine_state = %d\n",
            status_report_ptr->event, status_report_ptr->payload.rpc_loc_status_event_payload_u_type_u.engine_state);

   status = GPS_STATUS_NONE;

   if (status_report_ptr->event == RPC_LOC_STATUS_EVENT_ENGINE_STATE)
   {
      if (status_report_ptr->payload.rpc_loc_status_event_payload_u_type_u.engine_state == RPC_LOC_ENGINE_STATE_ON)
      {
         status = GPS_STATUS_ENGINE_ON;
      }
      else if (status_report_ptr->payload.rpc_loc_status_event_payload_u_type_u.engine_state == RPC_LOC_ENGINE_STATE_OFF)
      {
         status = GPS_STATUS_ENGINE_OFF;
      }
   }

   if (status_report_ptr->event == RPC_LOC_STATUS_EVENT_FIX_SESSION_STATE)
   {
      if (status_report_ptr->payload.rpc_loc_status_event_payload_u_type_u.fix_session_state == RPC_LOC_FIX_SESSION_STATE_BEGIN)
      {
         status = GPS_STATUS_SESSION_BEGIN;

      }
      else if (status_report_ptr->payload.rpc_loc_status_event_payload_u_type_u.fix_session_state == RPC_LOC_FIX_SESSION_STATE_END)
      {
         status = GPS_STATUS_SESSION_END;

      }
   }

   pthread_mutex_lock(&loc_eng_data.mute_session_lock);

   // Switch from WAIT to MUTE, for "engine on" or "session begin" event
   if (status == GPS_STATUS_SESSION_BEGIN || status == GPS_STATUS_ENGINE_ON)
   {
      if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_WAIT)
      {
         LOC_LOGV("loc_eng_report_status: mute_session_state changed from WAIT to IN SESSION");
         loc_eng_data.mute_session_state = LOC_MUTE_SESS_IN_SESSION;
      }
   }

   // Switch off MUTE session
   if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_IN_SESSION &&
       (status == GPS_STATUS_SESSION_END || status == GPS_STATUS_ENGINE_OFF))
   {
      LOC_LOGV("loc_eng_report_status: mute_session_state changed from IN SESSION to NONE");
      loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;
   }

   // Session End is not reported during Android navigating state
   if (status != GPS_STATUS_NONE && !(status == GPS_STATUS_SESSION_END && loc_eng_data.navigating) && !(status == GPS_STATUS_SESSION_BEGIN && !loc_eng_data.navigating))
   {
      LOC_LOGV("loc_eng_report_status: issue callback with status %d\n", status);

      if (loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
      {
         // Inform GpsLocationProvider about mNavigating status
         loc_inform_gps_status(status);
      }
      else {
         LOC_LOGV("loc_eng_report_status: muting the status report.");
      }
   }

   pthread_mutex_unlock(&loc_eng_data.mute_session_lock);

   // Only keeps ENGINE ON/OFF in engine_status
   if (status == GPS_STATUS_ENGINE_ON || status == GPS_STATUS_ENGINE_OFF)
   {
      loc_eng_data.engine_status = status;
   }

   // Only keeps SESSION BEGIN/END in fix_session_status
   if (status == GPS_STATUS_SESSION_BEGIN || status == GPS_STATUS_SESSION_END)
   {
      loc_eng_data.fix_session_status = status;
   }

   pthread_mutex_lock (&loc_eng_data.deferred_action_mutex);

   // Wake up the thread for aiding data deletion, XTRA injection, etc.
   if (loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON &&
       (loc_eng_data.aiding_data_for_deletion != 0 ||
        loc_eng_data.xtra_module_data.xtra_data_for_injection != NULL))
   {
       /* hold a wake lock while events are pending for deferred_action_thread */
       loc_eng_data.acquire_wakelock_cb();
       loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_DELETE_AIDING;
       pthread_cond_signal(&loc_eng_data.deferred_action_cond);
      // In case gps engine is ON, the assistance data will be deleted when the engine is OFF
   }
   pthread_mutex_unlock (&loc_eng_data.deferred_action_mutex);
}

/*===========================================================================
FUNCTION    loc_eng_report_nmea

DESCRIPTION
   Reports NMEA string to GPS HAL

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_nmea(const rpc_loc_nmea_report_s_type *nmea_report_ptr)
{
   if (loc_eng_data.nmea_cb != NULL)
   {
      struct timeval tv;

      gettimeofday(&tv, (struct timezone *) NULL);
      long long now = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

#if (AMSS_VERSION==3200)
      loc_eng_data.nmea_cb(now, nmea_report_ptr->nmea_sentences.nmea_sentences_val,
            nmea_report_ptr->nmea_sentences.nmea_sentences_len);
#else
      loc_eng_data.nmea_cb(now, nmea_report_ptr->nmea_sentences, nmea_report_ptr->length);
      LOC_LOGD("loc_eng_report_nmea: $%c%c%c\n",
         nmea_report_ptr->nmea_sentences[3], nmea_report_ptr->nmea_sentences[4],
               nmea_report_ptr->nmea_sentences[5]);

#endif /* #if (AMSS_VERSION==3200) */
   }
}

/*===========================================================================
FUNCTION    loc_eng_process_conn_request

DESCRIPTION
   Requests data connection to be brought up/tore down with the location server.

DEPENDENCIES
   N/A

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_process_conn_request (const rpc_loc_server_request_s_type *server_request_ptr)
{
   LOC_LOGD("loc_eng_process_conn_request: get loc event location server request, event = %d\n", server_request_ptr->event);
   pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);

   switch (server_request_ptr->event)
   {
   case RPC_LOC_SERVER_REQUEST_MULTI_OPEN:
      loc_eng_data.agps_status = GPS_REQUEST_AGPS_DATA_CONN;
      loc_eng_data.conn_handle = server_request_ptr->payload.rpc_loc_server_request_u_type_u.multi_open_req.conn_handle;
      loc_eng_data.conn_type = (server_request_ptr->payload.rpc_loc_server_request_u_type_u.multi_open_req.connection_type
                                == RPC_LOC_SERVER_CONNECTION_LBS) ? AGPS_TYPE_SUPL : AGPS_TYPE_WWAN_ANY;
      loc_eng_data.agps_request_pending = true;
      break;
   case RPC_LOC_SERVER_REQUEST_OPEN:
      loc_eng_data.agps_status = GPS_REQUEST_AGPS_DATA_CONN;
      loc_eng_data.conn_handle = server_request_ptr->payload.rpc_loc_server_request_u_type_u.open_req.conn_handle;
      loc_eng_data.conn_type = AGPS_TYPE_INVALID;
      loc_eng_data.agps_request_pending = true;
      break;
   default:
      loc_eng_data.agps_status = GPS_RELEASE_AGPS_DATA_CONN;
      loc_eng_data.conn_handle = server_request_ptr->payload.rpc_loc_server_request_u_type_u.close_req.conn_handle;
      loc_eng_data.agps_request_pending = false;
      break;
   }

   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_AGPS_STATUS;
   pthread_cond_signal(&loc_eng_data.deferred_action_cond);
   pthread_mutex_unlock(&loc_eng_data.deferred_action_mutex);

}

/*===========================================================================
FUNCTION    loc_eng_process_loc_event

DESCRIPTION
   This is used to process events received from the location engine.

DEPENDENCIES
   None

RETURN VALUE
   N/A

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_process_loc_event (rpc_loc_event_mask_type loc_event,
        rpc_loc_event_payload_u_type* loc_event_payload)
{
   LOC_LOGD("loc_eng_process_loc_event: %x\n", (int) loc_event);
   // Parsed report
   if ( (loc_event & RPC_LOC_EVENT_PARSED_POSITION_REPORT) &&
         loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
   {
      loc_eng_report_position(&loc_event_payload->rpc_loc_event_payload_u_type_u.
            parsed_location_report);
   }

   // Satellite report
   if ( (loc_event & RPC_LOC_EVENT_SATELLITE_REPORT) &&
         loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
   {
      loc_eng_report_sv(&loc_event_payload->rpc_loc_event_payload_u_type_u.
            gnss_report);
   }

   // Status report
   if (loc_event & RPC_LOC_EVENT_STATUS_REPORT)
   {
      loc_eng_report_status(&loc_event_payload->rpc_loc_event_payload_u_type_u.
            status_report);
   }

   // NMEA
   if (loc_event & RPC_LOC_EVENT_NMEA_1HZ_REPORT)
   {
      loc_eng_report_nmea(&(loc_event_payload->rpc_loc_event_payload_u_type_u.nmea_report));
   }
   // XTRA support: supports only XTRA download
   if (loc_event & RPC_LOC_EVENT_ASSISTANCE_DATA_REQUEST)
   {
      if (loc_event_payload->rpc_loc_event_payload_u_type_u.assist_data_request.event ==
         RPC_LOC_ASSIST_DATA_PREDICTED_ORBITS_REQ)
      {
         LOC_LOGD("loc_event_cb: XTRA download request");

         // Call Registered callback
         if (loc_eng_data.xtra_module_data.download_request_cb != NULL)
         {
            loc_eng_data.xtra_module_data.download_request_cb();
         }
      }
      if (loc_event_payload->rpc_loc_event_payload_u_type_u.assist_data_request.event ==
         RPC_LOC_ASSIST_DATA_TIME_REQ)
      {
         LOC_LOGD("loc_event_cb: XTRA time download request... not supported");
      }
   }

   // IOCTL status report
   if (loc_event & RPC_LOC_EVENT_IOCTL_REPORT)
   {
      // Process the received RPC_LOC_EVENT_IOCTL_REPORT
   }

   // AGPS data request
   if (loc_event & RPC_LOC_EVENT_LOCATION_SERVER_REQUEST)
   {
      loc_eng_process_conn_request(&loc_event_payload->rpc_loc_event_payload_u_type_u.
            loc_server_request);
   }

   loc_eng_ni_callback(loc_event, loc_event_payload);
}
/*===========================================================================
FUNCTION    loc_eng_agps_reinit

DESCRIPTION
   2nd half of loc_eng_agps_init(), singled out for modem restart to use.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_reinit()
{
   // Data connection for AGPS
   loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
   int i=0;
   for(i=0;i <= MAX_NUM_ATL_CONNECTIONS; i++ )
   {
      loc_eng_data.atl_conn_info[i].active = FALSE;
      loc_eng_data.atl_conn_info[i].conn_state = LOC_CONN_IDLE;
      loc_eng_data.atl_conn_info[i].conn_handle = INVALID_ATL_CONNECTION_HANDLE; //since connection handles can be 0
   }

    // Set server addresses which came before init
   if (supl_host_set)
   {
      loc_eng_set_server(AGPS_TYPE_SUPL, supl_host_buf, supl_port_buf);
   }

   if (c2k_host_set)
   {
      // loc_c2k_addr_is_set will be set in here
      loc_eng_set_server(AGPS_TYPE_C2K, c2k_host_buf, c2k_port_buf);
   }
}
/*===========================================================================
FUNCTION    loc_eng_agps_init

DESCRIPTION
   Initialize the AGps interface.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_init(AGpsCallbacks* callbacks)
{
   loc_eng_data.agps_status_cb = callbacks->status_cb;

   loc_c2k_addr_is_set = 0;

   loc_eng_agps_reinit();
}

/*===========================================================================
FUNCTION    loc_eng_ioctl_data_status

DESCRIPTION
   This function makes an IOCTL call to return data call status to modem

DEPENDENCIES
   Requires loc_eng_data.apn_name

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_ioctl_data_open_status(int is_succ)
{
    rpc_loc_ioctl_data_u_type           ioctl_data;
    int                                 ret_val;

    //Go through all the active connection states to determine which command to send to LOC MW and the
    //state machine updates that need to be done
    for (int i=0;i< MAX_NUM_ATL_CONNECTIONS;i++)
    {
        LOC_LOGD("loc_eng_ioctl_data_open_status, is_active = %d, handle = %d, state = %d, bearer: %d\n",
                 loc_eng_data.atl_conn_info[i].active, loc_eng_data.atl_conn_info[i].conn_handle,
                 loc_eng_data.atl_conn_info[i].conn_state,
                 loc_eng_data.data_connection_bearer);

        rpc_loc_server_open_status_e_type open_status = is_succ ? RPC_LOC_SERVER_OPEN_SUCCESS : RPC_LOC_SERVER_OPEN_FAIL;

        if ((loc_eng_data.atl_conn_info[i].active == TRUE) &&
            (loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_OPEN_REQ))
        {
            //update the session states
            loc_eng_data.atl_conn_info[i].conn_state = is_succ ? LOC_CONN_OPEN : LOC_CONN_IDLE;

            //Delay the call into the modem as there is a chance that this IOCTL into LOC MW
            //will be invoked before we return from loc_process_conn_request() into the RPC context
            usleep(TENMSDELAY);

            if (AGPS_TYPE_INVALID == loc_eng_data.conn_type) {
                rpc_loc_server_open_status_s_type  *conn_open_status_ptr =
                    &ioctl_data.rpc_loc_ioctl_data_u_type_u.conn_open_status;

                // Fill in data
                ioctl_data.disc = RPC_LOC_IOCTL_INFORM_SERVER_OPEN_STATUS;
                conn_open_status_ptr->conn_handle = loc_eng_data.atl_conn_info[i].conn_handle;
                conn_open_status_ptr->open_status = open_status;
#if (AMSS_VERSION==3200)
                conn_open_status_ptr->apn_name = loc_eng_data.apn_name; /* requires APN */
#else
                strlcpy(conn_open_status_ptr->apn_name, loc_eng_data.apn_name,
                        sizeof conn_open_status_ptr->apn_name);
#endif /* #if (AMSS_VERSION==3200) */

                LOC_LOGD("ATL RPC_LOC_IOCTL_INFORM_SERVER_OPEN_STATUS open %s, APN name = [%s]\n",
                         log_succ_fail_string(is_succ),
                         loc_eng_data.apn_name);
            } else {
                rpc_loc_server_multi_open_status_s_type  *conn_multi_open_status_ptr =
                    &ioctl_data.rpc_loc_ioctl_data_u_type_u.multi_conn_open_status;

                // Fill in data
                ioctl_data.disc = RPC_LOC_IOCTL_INFORM_SERVER_MULTI_OPEN_STATUS;
                conn_multi_open_status_ptr->conn_handle = loc_eng_data.atl_conn_info[i].conn_handle;
                conn_multi_open_status_ptr->open_status = open_status;
                strlcpy(conn_multi_open_status_ptr->apn_name, loc_eng_data.apn_name,
                        sizeof conn_multi_open_status_ptr->apn_name);

                switch(loc_eng_data.data_connection_bearer)
                {
                case AGPS_APN_BEARER_IPV4:
                    conn_multi_open_status_ptr->pdp_type = RPC_LOC_SERVER_PDP_IP;
                    break;
                case AGPS_APN_BEARER_IPV6:
                    conn_multi_open_status_ptr->pdp_type = RPC_LOC_SERVER_PDP_IPV6;
                    break;
                case AGPS_APN_BEARER_IPV4V6:
                    conn_multi_open_status_ptr->pdp_type = RPC_LOC_SERVER_PDP_IPV4V6;
                    break;
                default:
                    conn_multi_open_status_ptr->pdp_type = RPC_LOC_SERVER_PDP_PPP;
                }

                LOC_LOGD("ATL RPC_LOC_IOCTL_INFORM_SERVER_MULTI_OPEN_STATUSopen %s, APN name = [%s], pdp_type = %d\n",
                         log_succ_fail_string(is_succ),
                         loc_eng_data.apn_name,
                         conn_multi_open_status_ptr->pdp_type);
            }

            // Make the IOCTL call
            ret_val = loc_eng_ioctl(loc_eng_data.client_handle,
                                    ioctl_data.disc,
                                    &ioctl_data,
                                    LOC_IOCTL_DEFAULT_TIMEOUT,
                                    NULL);
        }
    }
}
/*===========================================================================
FUNCTION    loc_eng_ioctl_data_close_status

DESCRIPTION
   This function makes an IOCTL call to return data call close status to modem

DEPENDENCIES

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_ioctl_data_close_status(int is_succ)
{
   rpc_loc_server_close_status_s_type *conn_close_status_ptr;
   rpc_loc_ioctl_data_u_type           ioctl_data;
   time_t                              time_now;
   int                                 ret_val;

   for (int i=0;i< MAX_NUM_ATL_CONNECTIONS;i++)
   {
      LOC_LOGD("loc_eng_ioctl_data_close_status, is_active = %d, handle = %d, state = %d, bearer: %d\n",
            loc_eng_data.atl_conn_info[i].active, loc_eng_data.atl_conn_info[i].conn_handle,
            loc_eng_data.atl_conn_info[i].conn_state,
            loc_eng_data.data_connection_bearer);

      if ( loc_eng_data.atl_conn_info[i].active == TRUE &&
           ((loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_CLOSE_REQ )||
            (loc_eng_data.atl_conn_info[i].conn_state == LOC_CONN_IDLE)))
      {
         // Fill in data
         ioctl_data.disc = RPC_LOC_IOCTL_INFORM_SERVER_CLOSE_STATUS;
         conn_close_status_ptr = &ioctl_data.rpc_loc_ioctl_data_u_type_u.conn_close_status;
         conn_close_status_ptr->conn_handle = loc_eng_data.atl_conn_info[i].conn_handle;
         conn_close_status_ptr->close_status = is_succ ? RPC_LOC_SERVER_CLOSE_SUCCESS : RPC_LOC_SERVER_CLOSE_FAIL;
         //Delay the call into the modem as there is a chance that this IOCTL into LOC MW
         //will be invoked before we return from loc_process_conn_request() into the RPC context
         usleep(TENMSDELAY);
         // Make the IOCTL call
         ret_val = loc_eng_ioctl(loc_eng_data.client_handle,
                                 ioctl_data.disc,
                                 &ioctl_data,
                                 LOC_IOCTL_DEFAULT_TIMEOUT,
                                 NULL);

         //update the session states
         loc_eng_data.atl_conn_info[i].conn_state = LOC_CONN_IDLE;
         loc_eng_data.atl_conn_info[i].active = FALSE;
         loc_eng_data.atl_conn_info[i].conn_handle = INVALID_ATL_CONNECTION_HANDLE;
      }
   }
}

/*===========================================================================
FUNCTION    loc_eng_data_conn_open

DESCRIPTION
   This function is called when on-demand data connection opening is successful.
It should inform ARM 9 about the data open result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_data_conn_open(const char* apn, AGpsBearerType bearerType)
{
   INIT_CHECK("loc_eng_data_conn_open");

   LOC_LOGD("loc_eng_data_conn_open APN name = [%s]", apn);
   pthread_mutex_lock(&(loc_eng_data.deferred_action_mutex));
   loc_eng_data.data_connection_bearer = bearerType;
   loc_eng_set_apn(apn);
   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_AGPS_DATA_SUCCESS;
   pthread_cond_signal(&(loc_eng_data.deferred_action_cond));
   pthread_mutex_unlock(&(loc_eng_data.deferred_action_mutex));
   return 0;
}

/*===========================================================================
FUNCTION    loc_eng_data_conn_closed

DESCRIPTION
   This function is called when on-demand data connection closing is done.
It should inform ARM 9 about the data close result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_data_conn_closed()
{
   INIT_CHECK("loc_eng_data_conn_closed");

   LOC_LOGD("loc_eng_data_conn_closed");
   pthread_mutex_lock(&(loc_eng_data.deferred_action_mutex));
   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_AGPS_DATA_CLOSED;
   pthread_cond_signal(&(loc_eng_data.deferred_action_cond));
   pthread_mutex_unlock(&(loc_eng_data.deferred_action_mutex));
   return 0;
}

/*===========================================================================
FUNCTION    loc_eng_data_conn_failed

DESCRIPTION
   This function is called when on-demand data connection opening has failed.
It should inform ARM 9 about the data open result.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_data_conn_failed()
{
   INIT_CHECK("loc_eng_data_conn_failed");
   LOC_LOGD("loc_eng_data_conn_failed");

   pthread_mutex_lock(&(loc_eng_data.deferred_action_mutex));
   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_AGPS_DATA_FAILED;
   pthread_cond_signal(&(loc_eng_data.deferred_action_cond));
   pthread_mutex_unlock(&(loc_eng_data.deferred_action_mutex));
   return 0;
}

/*===========================================================================
FUNCTION    loc_eng_set_apn

DESCRIPTION
   This is used to inform the location engine of the apn name for the active
   data connection. If there is no data connection, an empty apn name will
   be used.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_set_apn (const char* apn)
{
   INIT_CHECK("loc_eng_set_apn");

   int apn_len;

   if (apn != NULL)
   {
      LOC_LOGD("loc_eng_set_apn: APN Name = [%s]\n", apn);

      apn_len = strlen (apn);

#if 0 /* hack: temporarily allow NULL apn name */
      if (apn_len == 0)
      {
         loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
      }
      else
#endif

      {
         if (apn_len >= 100)
         {
            LOC_LOGE("loc_eng_set_apn: error, apn name exceeds maximum lenght of 100 chars\n");
            apn_len = 100-1;
         }

         memcpy(loc_eng_data.apn_name, apn, apn_len);
         loc_eng_data.apn_name[apn_len] = '\0';
      }

      rpc_loc_ioctl_data_u_type ioctl_data = {RPC_LOC_IOCTL_SET_LBS_APN_PROFILE, {0}};
      ioctl_data.rpc_loc_ioctl_data_u_type_u.apn_profiles[0].srv_system_type = LOC_APN_PROFILE_SRV_SYS_MAX;
      ioctl_data.rpc_loc_ioctl_data_u_type_u.apn_profiles[0].pdp_type = LOC_APN_PROFILE_PDN_TYPE_IPV4;
      memcpy(&(ioctl_data.rpc_loc_ioctl_data_u_type_u.apn_profiles[0].apn_name), loc_eng_data.apn_name, apn_len+1);

      loc_eng_ioctl (loc_eng_data.client_handle,
                     RPC_LOC_IOCTL_SET_LBS_APN_PROFILE,
                     &ioctl_data,
                     LOC_IOCTL_DEFAULT_TIMEOUT,
                     NULL);
   }

   return 0;
}

/*===========================================================================

FUNCTION resolve_in_addr

DESCRIPTION
   Translates a hostname to in_addr struct

DEPENDENCIES
   n/a

RETURN VALUE
   TRUE if successful

SIDE EFFECTS
   n/a

===========================================================================*/
static boolean resolve_in_addr(const char *host_addr, struct in_addr *in_addr_ptr)
{
   struct hostent             *hp;
   hp = gethostbyname(host_addr);
   if (hp != NULL) /* DNS OK */
   {
      memcpy(in_addr_ptr, hp->h_addr_list[0], hp->h_length);
   }
   else
   {
      /* Try IP representation */
      if (inet_aton(host_addr, in_addr_ptr) == 0)
      {
         /* IP not valid */
         LOC_LOGE("DNS query on '%s' failed\n", host_addr);
         return FALSE;
      }
   }

   return TRUE;
}

/*===========================================================================
FUNCTION    loc_eng_set_server

DESCRIPTION
   This is used to set the default AGPS server. Server address is obtained
   from gps.conf.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_set_server(AGpsType type, const char* hostname, int port)
{
   unsigned                          len;
   rpc_loc_ioctl_data_u_type         ioctl_data;
   rpc_loc_server_info_s_type       *server_info_ptr;
   rpc_loc_ioctl_e_type              ioctl_cmd;
   struct in_addr                    addr;
   int                               ret_val;
   char                              url[256];

   LOC_LOGD("loc_eng_set_server, type = %d, hostname = %s, port = %d\n", (int) type, hostname, port);

   // Needed length
   len = snprintf(url, sizeof url, "%s:%u", hostname, (unsigned) port);
   if (len >= sizeof url)
   {
      LOC_LOGE("loc_eng_set_server, URL too long (len=%d).\n", len);
      return -1;
   }

   // Actual length
   len = strlen(url);

   server_info_ptr = &ioctl_data.rpc_loc_ioctl_data_u_type_u.server_addr;

   switch (type) {
   case AGPS_TYPE_SUPL:
      ioctl_cmd = RPC_LOC_IOCTL_SET_UMTS_SLP_SERVER_ADDR;
      ioctl_data.disc = ioctl_cmd;
      server_info_ptr->addr_type = RPC_LOC_SERVER_ADDR_URL;
      server_info_ptr->addr_info.disc = server_info_ptr->addr_type;
      server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.length = len;
#if (AMSS_VERSION==3200)
      server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr.addr_val = (char*) url;
      server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr.addr_len= len;
      LOC_LOGD ("loc_eng_set_server, addr = %s\n", server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr.addr_val);
#else
      strlcpy(server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr, url,
            sizeof server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr);
      LOC_LOGD ("loc_eng_set_server, addr = %s\n", server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.url.addr);
#endif /* #if (AMSS_VERSION==3200) */
      break;

   case AGPS_TYPE_C2K:
      if (!resolve_in_addr(hostname, &addr))
      {
         LOC_LOGE("loc_eng_set_server, hostname %s cannot be resolved.\n", hostname);
         return -2;
      }

      ioctl_cmd = RPC_LOC_IOCTL_SET_CDMA_PDE_SERVER_ADDR;
      ioctl_data.disc = ioctl_cmd;
      server_info_ptr->addr_type = RPC_LOC_SERVER_ADDR_IPV4;
      server_info_ptr->addr_info.disc = server_info_ptr->addr_type;
      server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.ipv4.addr = (uint32_t) htonl(addr.s_addr);
      server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.ipv4.port = port;
      LOC_LOGD ("loc_eng_set_server, addr = %X:%d\n",
            (unsigned int) server_info_ptr->addr_info.rpc_loc_server_addr_u_type_u.ipv4.addr,
            (unsigned int) port);

      // Set C2K flag for Donut. In Eclair, C2K and UMTS will be identical
      // at the HAL layer, and this will be obsolete.
      loc_c2k_addr_is_set = 1;

      break;
   default:
      LOC_LOGE("loc_eng_set_server, unknown server type = %d", (int) type);
      return 0; /* note: error not indicated, since JNI doesn't check */
   }

   ret_val = loc_eng_ioctl (loc_eng_data.client_handle,
                            ioctl_cmd,
                            &ioctl_data,
                            LOC_IOCTL_DEFAULT_TIMEOUT,
                            NULL /* No output information is expected*/);

   if (ret_val != RPC_LOC_API_SUCCESS)
   {
      LOC_LOGE("loc_eng_set_server failed\n");
   }
   else
   {
      LOC_LOGV("loc_eng_set_server successful\n");
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_set_server_proxy

DESCRIPTION
   If loc_eng_set_server is called before loc_eng_init, it doesn't work. This
   proxy buffers server settings and calls loc_eng_set_server when the client is
   open.

DEPENDENCIES
   NONE

RETURN VALUE
   0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_set_server_proxy(AGpsType type, const char* hostname, int port)
{
   if (loc_eng_inited)
   {
      loc_eng_set_server(type, hostname, port);
   }
   else {
      LOC_LOGW("set_server called before init. save the address, type: %d, hostname: %s, port: %d",
            (int) type, hostname, port);
      switch (type)
      {
      case AGPS_TYPE_SUPL:
         strlcpy(supl_host_buf, hostname, sizeof supl_host_buf);
         supl_port_buf = port;
         supl_host_set = 1;
         break;
      case AGPS_TYPE_C2K:
         strlcpy(c2k_host_buf, hostname, sizeof c2k_host_buf);
         c2k_port_buf = port;
         c2k_host_set = 1;
         break;
      default:
         LOC_LOGE("loc_eng_set_server_proxy, unknown server type = %d", (int) type);
      }
   }
   return 0;
}

// Below stub functions are members of sLocEngAGpsRilInterface
static void loc_eng_agps_ril_init( AGpsRilCallbacks* callbacks ) {}
static void loc_eng_agps_ril_set_ref_location(const AGpsRefLocation *agps_reflocation, size_t sz_struct) {}
static void loc_eng_agps_ril_set_set_id(AGpsSetIDType type, const char* setid) {}
static void loc_eng_agps_ril_ni_message(uint8_t *msg, size_t len) {}
static void loc_eng_agps_ril_update_network_state(int connected, int type, int roaming, const char* extra_info) {}

/*===========================================================================
FUNCTION    loc_eng_agps_ril_update_network_availability

DESCRIPTION
   Sets data call allow vs disallow flag to modem
   This is the only member of sLocEngAGpsRilInterface implemented.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_agps_ril_update_network_availability(int available, const char* apn)
{
    rpc_loc_ioctl_data_u_type ioctl_data = {RPC_LOC_IOCTL_SET_DATA_ENABLE, {0}};

    ioctl_data.rpc_loc_ioctl_data_u_type_u.data_enable = available;
    loc_eng_ioctl (loc_eng_data.client_handle,
                   RPC_LOC_IOCTL_SET_DATA_ENABLE,
                   &ioctl_data,
                   LOC_IOCTL_DEFAULT_TIMEOUT,
                   NULL);

    loc_eng_set_apn(apn);
}

/*===========================================================================
FUNCTION    loc_eng_delete_aiding_data_action

DESCRIPTION
   This is used to remove the aiding data when GPS engine is off.

DEPENDENCIES
   Assumes the aiding data type specified in GpsAidingData matches with
   LOC API specification.

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_delete_aiding_data_action(GpsAidingData bits)
{
    rpc_loc_ioctl_data_u_type ioctl_data = {RPC_LOC_IOCTL_DELETE_ASSIST_DATA, {0}};
   rpc_loc_assist_data_delete_s_type  *assist_data_ptr;
   int                                ret_val;

   ioctl_data.rpc_loc_ioctl_data_u_type_u.assist_data_delete.type = bits;

   ret_val = loc_eng_ioctl (loc_eng_data.client_handle,
                            RPC_LOC_IOCTL_DELETE_ASSIST_DATA,
                            &ioctl_data,
                            LOC_IOCTL_DEFAULT_TIMEOUT,
                            NULL);

   if (ret_val == RPC_LOC_API_RPC_MODEM_RESTART) {
      loc_eng_set_deferred_action(DEFERRED_ACTION_MODEM_DOWN_DETECTED, NULL);
   }
}

/*===========================================================================
FUNCTION    loc_eng_report_agps_status

DESCRIPTION
   This functions calls the native callback function for GpsLocationProvider
to update AGPS status. The expected behavior from GpsLocationProvider is the following.

   For status GPS_REQUEST_AGPS_DATA_CONN, GpsLocationProvider will inform the open
   status of the data connection if it is already open, or try to bring up a data
   connection when it is not.

   For status GPS_RELEASE_AGPS_DATA_CONN, GpsLocationProvider will try to bring down
   the data connection when it is open. (use this carefully)

   Currently, no actions are taken for other status, such as GPS_AGPS_DATA_CONNECTED,
   GPS_AGPS_DATA_CONN_DONE or GPS_AGPS_DATA_CONN_FAILED.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_report_agps_status(AGpsType type,
                                       AGpsStatusValue status,
                                       unsigned long ipv4_addr,
                                       unsigned char * ipv6_addr)
{
   AGpsStatus agpsStatus;
   if (loc_eng_data.agps_status_cb == NULL)
   {
      LOC_LOGE("loc_eng_report_agps_status, callback not initialized.\n");
      return;
   }

   LOC_LOGD("loc_eng_report_agps_status, type = %d, status = %d, ipv4_addr = %d\n",
         (int) type, (int) status,  (int) ipv4_addr);

   agpsStatus.size      = sizeof(agpsStatus);
   agpsStatus.type      = (AGPS_TYPE_INVALID == type) ? AGPS_TYPE_SUPL : type;
   agpsStatus.status    = status;
   agpsStatus.ipv4_addr = ipv4_addr;
   if (ipv6_addr != NULL) {
       memcpy(agpsStatus.ipv6_addr, ipv6_addr, 16);
   } else {
       memset(agpsStatus.ipv6_addr, 0, 16);
   }
   switch (status)
   {
      case GPS_REQUEST_AGPS_DATA_CONN:
         loc_eng_data.agps_status_cb(&agpsStatus);
         break;
      case GPS_RELEASE_AGPS_DATA_CONN:
         // This will not close always-on connection. Comment out if it does.
         loc_eng_data.agps_status_cb(&agpsStatus);
         break;
   }

}


/*===========================================================================
FUNCTION    loc_eng_process_atl_action

DESCRIPTION
   This is used to inform the location engine of the processing status for
   data connection open/close request.

DEPENDENCIES
   None

RETURN VALUE
   RPC_LOC_API_SUCCESS

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_process_atl_action(rpc_loc_server_connection_handle conn_handle,
                                       AGpsStatusValue status, AGpsType agps_type)

{
   boolean                             ret_val;

LOC_LOGE("loc_eng_process_atl_action,handle = %d; status = %d; agps_type = %d\n", conn_handle, status, agps_type);

   //Check if the incoming connection handle already has a atl state which exists and get associated session index
   int session_index = 0;
   session_index = loc_eng_get_index(conn_handle);
   if (session_index == MAX_NUM_ATL_CONNECTIONS)
   {
     //An error has occured and so print out an error message and return. End the call flow
     LOC_LOGE("loc_eng_process_conn_request- session index error,handle = %d\n",
             conn_handle);
     return;
   }
   LOC_LOGD("loc_eng_process_atl_action.session_index = %x, active_session_state = %x ,"
            "active_session_handle = %x session_active %d\n", session_index,
            loc_eng_data.atl_conn_info[session_index].conn_state,
            loc_eng_data.atl_conn_info[session_index].conn_handle,
            loc_eng_data.atl_conn_info[session_index].active);
   //ATL data connection open request from modem
   if(status == GPS_REQUEST_AGPS_DATA_CONN )
   {
      //Go into Open request state only if current state is in IDLE
      if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_IDLE)
      {
       loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
        loc_eng_data.atl_conn_info[session_index].conn_handle = conn_handle;
       if (check_if_any_connection(LOC_CONN_OPEN, session_index))
       {
       //PPP connection has already been opened for some other handle. So simply acknowledge the modem.
       loc_eng_ioctl_data_open_status(SUCCESS);
       }else if (check_if_any_connection(LOC_CONN_OPEN_REQ, session_index))
       {
          //When COnnectivity Manger acknowledges the reqest all ATL requests will be acked
          LOC_LOGD("PPP Open has been requested already for some other handle /n");
       }else
       {//Request connectivity manager to bringup a data connection
         loc_eng_report_agps_status(agps_type,status,INADDR_NONE,NULL);
       }
      }
      else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN)
      {
       //PPP connection has already been opened for this handle. So simply acknowledge the modem.
       loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
       loc_eng_ioctl_data_open_status(SUCCESS);

      }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_CLOSE_REQ)
      {
         //In this case the open request has come in for a handle which is already in
         //CLOSE_REQ.  Here we ack the modem a failure for this request as it came in
         //even before the modem got the ack for the precedding close request for this handle.
         LOC_LOGE("ATL Open req came in for handle %d when in CLOSE_REQ state", conn_handle);
         loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_OPEN_REQ;
         loc_eng_ioctl_data_open_status(FAILURE);
      }else
      {//In this case the open request has come in for a handle which is already in OPEN_REQ.
       //Here ack to the modem request will be sent when we get an equivalent ack from
       //the Connectivity Manager.
         LOC_LOGD("ATL Open req came in for handle %d when in OPEN_REQ state", conn_handle);
      }

   }else if (status == GPS_RELEASE_AGPS_DATA_CONN)
  {
      if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN)
      {
      loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
        if(check_if_all_connection(LOC_CONN_IDLE,session_index))
        {
         loc_eng_report_agps_status(agps_type,status,INADDR_NONE,NULL);
        }else
        {
           //Simply acknowledge to the modem that the connection is closed even through we dont pass
           //that message to the connectivity manager
           loc_eng_ioctl_data_close_status(SUCCESS);
        }
       }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_IDLE)
       {
         loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
        //The connection is already closed previously so simply acknowledge the modem.
        loc_eng_ioctl_data_close_status(SUCCESS);
       }else if(loc_eng_data.atl_conn_info[session_index].conn_state == LOC_CONN_OPEN_REQ)
       {
        //In this case the close request has come in for a handle which is already in
         //OPEN_REQ.  In this case we ack the modem a failure for this request as it came in
         //even before the modem got the ack for the precedding open request for this handle.
         LOC_LOGE("ATL Close req came in for handle %d when in OPEN_REQ state", conn_handle);
         loc_eng_data.atl_conn_info[session_index].conn_state = LOC_CONN_CLOSE_REQ;
         loc_eng_ioctl_data_close_status(FAILURE);
       }else
       {//In this case the close request has come in for a handle which is already in CLOSE_REQ.
       //In this case an ack to the modem request will be sent when we get an equivalent ack
       //from the Connectivity Manager.
         LOC_LOGD("ATL Open req came in for handle %d when in CLOSE_REQ state", conn_handle);
       }
   }
}

/*===========================================================================
FUNCTION loc_eng_report_modem_state

DESCRIPTION
   Calls this function when it is detected that modem restart is happening.
   Either we detected the modem is down or received modem up event.
   This must be called from the deferred thread to avoid race condition.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_report_modem_state(rpc_loc_engine_state_e_type state) {
   rpc_loc_status_event_s_type status = {RPC_LOC_STATUS_EVENT_ENGINE_STATE,
                                         {RPC_LOC_STATUS_EVENT_ENGINE_STATE,
                                          {RPC_LOC_ENGINE_STATE_OFF}}};
   static rpc_loc_engine_state_e_type last_state = RPC_LOC_ENGINE_STATE_MAX;
   if (last_state != state) {
       if (RPC_LOC_ENGINE_STATE_OFF == state) {
           last_state = state;
           loc_ni_reset_on_modem_restart();
           loc_eng_report_status(&status);
       } else if (RPC_LOC_ENGINE_STATE_ON == state) {
           status.payload.rpc_loc_status_event_payload_u_type_u.engine_state = RPC_LOC_ENGINE_STATE_ON;
           last_state = state;
           loc_eng_report_status(&status);

           if (loc_eng_inited == 1) {
               loc_clear(loc_eng_data.client_handle);
               loc_eng_reinit();
           }

           if (loc_eng_data.agps_status_cb != NULL) {
               loc_eng_agps_reinit();
           }

           // modem is back up.  If we crashed in the middle of navigating, we restart.
           if (loc_eng_data.navigating) {
               loc_eng_ioctl (loc_eng_data.client_handle,
                              RPC_LOC_IOCTL_SET_FIX_CRITERIA,
                              &loc_eng_data.position_mode,
                              LOC_IOCTL_DEFAULT_TIMEOUT,
                              NULL);
               // not mutex protected, assuming fw won't call start twice without a
               // stop call in between.
               loc_start_fix(loc_eng_data.client_handle);
           }
       }
   }
}

/*===========================================================================
FUNCTION loc_eng_set_deferred_action

DESCRIPTION
   Sets up the deferred action(s)

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_set_deferred_action(unsigned int bits, voidFuncPtr setupLocked) {
   pthread_mutex_lock (&loc_eng_data.deferred_action_mutex);
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= bits;

   if (NULL != setupLocked)
      (*setupLocked)();

   pthread_cond_signal  (&loc_eng_data.deferred_action_cond);
   pthread_mutex_unlock (&loc_eng_data.deferred_action_mutex);
}

/*===========================================================================
FUNCTION loc_eng_deferred_action_thread

DESCRIPTION
   Main routine for the thread to execute certain commands
   that are not safe to be done from within an RPC callback.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_deferred_action_thread(void* arg)
{
   AGpsStatusValue      status;
   LOC_LOGD("loc_eng_deferred_action_thread started\n");

   // make sure we do not run in background scheduling group
   set_sched_policy(gettid(), SP_FOREGROUND);
   while (1)
   {
      GpsAidingData   aiding_data_for_deletion;
      GpsStatusValue  engine_status;
      boolean         data_connection_succeeded;
      boolean         data_connection_closed;
      boolean         data_connection_failed;
      rpc_loc_event_mask_type         loc_event;
      rpc_loc_event_payload_u_type    loc_event_payload;
      AGpsType        agpsConnType;
      rpc_loc_server_connection_handle  agpsConnHandle;


      // Wait until we are signalled to do a deferred action, or exit
      pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);

     // If we have an event we should process it immediately,
     // otherwise wait until we are signalled
     if (loc_eng_data.deferred_action_flags == 0) {
            // do not hold a wake lock while waiting for an event...
            loc_eng_data.release_wakelock_cb();
            LOC_LOGD("loc_eng_deferred_action_thread. waiting for events\n");
            pthread_cond_wait(&loc_eng_data.deferred_action_cond,
                                &loc_eng_data.deferred_action_mutex);
            LOC_LOGD("loc_eng_deferred_action_thread signalled\n");
            // but after we are signalled reacquire the wake lock
            // until we are done processing the event.
            loc_eng_data.acquire_wakelock_cb();
     }
      if (loc_eng_data.deferred_action_flags & DEFERRED_ACTION_QUIT)
      {
         pthread_mutex_unlock(&loc_eng_data.deferred_action_mutex);
         break; /* exit thread */
      }
     // copy anything we need before releasing the mutex
      loc_event = loc_eng_data.loc_event;
      if (loc_event != 0) {
          LOC_LOGD("loc_eng_deferred_action_thread event %llu\n",loc_event);
          memcpy(&loc_event_payload, &loc_eng_data.loc_event_payload, sizeof(loc_event_payload));
          loc_eng_data.loc_event = 0;
      }
      int flags = loc_eng_data.deferred_action_flags;
      loc_eng_data.deferred_action_flags = 0;
      engine_status = loc_eng_data.engine_status;
      aiding_data_for_deletion = loc_eng_data.aiding_data_for_deletion;
      status = loc_eng_data.agps_status;
      agpsConnType = loc_eng_data.conn_type;
      agpsConnHandle = loc_eng_data.conn_handle;
      loc_eng_data.agps_status = 0;

      // perform all actions after releasing the mutex to avoid blocking RPCs from the ARM9
      pthread_mutex_unlock(&(loc_eng_data.deferred_action_mutex));

      if (loc_event != 0) {
          loc_eng_process_loc_event(loc_event, &loc_event_payload);
      }

      // Send_delete_aiding_data must be done when GPS engine is off
      if ((engine_status != GPS_STATUS_ENGINE_ON) && (aiding_data_for_deletion != 0))
      {
         loc_eng_delete_aiding_data_action(aiding_data_for_deletion);
         loc_eng_data.aiding_data_for_deletion &= ~aiding_data_for_deletion;
      }

      // Inject XTRA data when GPS engine is off
      if (loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON &&
          loc_eng_data.xtra_module_data.xtra_data_for_injection != NULL)
      {
         pthread_mutex_unlock(&loc_eng_data.deferred_action_mutex);

         loc_eng_inject_xtra_data_in_buffer();

         pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);
      }

      //Process connectivity manager events at this point
      if(flags & DEFERRED_ACTION_AGPS_DATA_SUCCESS)
      {
         loc_eng_ioctl_data_open_status(SUCCESS);
         if(flags & DEFERRED_ACTION_AGPS_DATA_CLOSED)
         {
            LOC_LOGE("Error condition in the ATL events posted by GpsLocationProvider\n");
         }
      }else if(flags & DEFERRED_ACTION_AGPS_DATA_CLOSED)
      {
         loc_eng_ioctl_data_close_status(SUCCESS);
         loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
      }else if(flags & DEFERRED_ACTION_AGPS_DATA_FAILED)
      {
         if(loc_eng_data.data_connection_bearer != AGPS_APN_BEARER_INVALID)
         {
            loc_eng_ioctl_data_open_status(FAILURE);
         }else
         {
            loc_eng_ioctl_data_close_status(FAILURE);
         }
         loc_eng_data.data_connection_bearer = AGPS_APN_BEARER_INVALID;
      }
      if (flags & (DEFERRED_ACTION_AGPS_DATA_SUCCESS |
                   DEFERRED_ACTION_AGPS_DATA_CLOSED |
                   DEFERRED_ACTION_AGPS_DATA_FAILED))
      {
          pthread_mutex_lock(&(loc_eng_data.deferred_stop_mutex));
          // work around problem with loc_eng_stop when AGPS requests are pending
          // we defer stopping the engine until the AGPS request is done
          loc_eng_data.agps_request_pending = false;
          if (loc_eng_data.stop_request_pending)
           {
              LOC_LOGD ("handling deferred stop\n");
              if (loc_stop_fix(loc_eng_data.client_handle) != RPC_LOC_API_SUCCESS)
               {
                   LOC_LOGD ("loc_stop_fix failed!\n");
               }
           }
           loc_eng_data.stop_request_pending = false;
           pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));
       }

      if (flags & DEFERRED_ACTION_MODEM_DOWN_DETECTED) {
         loc_eng_report_modem_state(RPC_LOC_ENGINE_STATE_OFF);
      } else if (flags & DEFERRED_ACTION_MODEM_UP_DETECTED) {
         loc_eng_report_modem_state(RPC_LOC_ENGINE_STATE_ON);
      }

      // ATL open/close actions
      if (status != 0 )
      {
          loc_eng_process_atl_action(agpsConnHandle, status, agpsConnType);
         status = 0;
      }
   }

   LOC_LOGD("loc_eng_deferred_action_thread exiting\n");
   loc_eng_data.release_wakelock_cb();
   loc_eng_data.deferred_action_thread = 0;
}

/*===========================================================================
FUNCTION loc_eng_if_wakeup

DESCRIPTION
   For loc_eng_dmn_conn_handler.c to call to bring up or down
   network interface

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_if_wakeup(int if_req, unsigned is_supl, unsigned long ipv4_addr, unsigned char * ipv6_addr)
{
   AGpsType                            agps_type;

   agps_type = is_supl? AGPS_TYPE_SUPL : AGPS_TYPE_ANY;  // No C2k?

   if (if_req == 0)
   {
      // Inform GpsLocationProvider (subject to cancellation if data call should not be bring down)
      loc_eng_report_agps_status(
            agps_type,
            GPS_RELEASE_AGPS_DATA_CONN,
            ipv4_addr,
            ipv6_addr
      );
   }
   else
   {
      // Use GpsLocationProvider to bring up the data call if not yet open
      loc_eng_report_agps_status(
            agps_type,
            GPS_REQUEST_AGPS_DATA_CONN,
            ipv4_addr,
            ipv6_addr
      );
   }
}

// for gps.c
extern "C" const GpsInterface* get_gps_interface()
{
    return &sLocEngInterface;
}
/*===========================================================================
FUNCTION loc_eng_get_index

DESCRIPTION
   Function which is used to determine the index to access the session
   information associated with that patciular connection handle

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_get_index(rpc_loc_server_connection_handle active_conn_handle)
{
   int i = 0;
   //search through all the active sessions to determine the correct session index
    for (i=0;i < MAX_NUM_ATL_CONNECTIONS;i++)
    {
      LOC_LOGD("In loc_eng_get_index Index: %d Active: %d ConnHandle: %d\n", i, loc_eng_data.atl_conn_info[i].active,
               loc_eng_data.atl_conn_info[i].conn_handle);
      if((loc_eng_data.atl_conn_info[i].active == TRUE) &&
         (loc_eng_data.atl_conn_info[i].conn_handle == active_conn_handle))
         return i;
    }
   //If we come here there is no exiting record for this connection handle and so we create a new one
    for (i=0;i < MAX_NUM_ATL_CONNECTIONS;i++)
    {
      if(loc_eng_data.atl_conn_info[i].active == FALSE)
      {
         //set session status to active
         loc_eng_data.atl_conn_info[i].active = TRUE;
         loc_eng_data.atl_conn_info[i].conn_handle = active_conn_handle;
         LOC_LOGD("In loc_eng_get_index Index: %d Active: %d ConnHandle: %d \n", i, loc_eng_data.atl_conn_info[i].active,
               loc_eng_data.atl_conn_info[i].conn_handle);
         return i;
      }
    }
   //If we reach this point an error has occurred
   return MAX_NUM_ATL_CONNECTIONS;
}

/*===========================================================================
FUNCTION check_if_any_connection

DESCRIPTION
   Function which is used to determine if any of the other sessions have the state
   passed in

DEPENDENCIES
   None

RETURN VALUE
   TRUE/FALSE

SIDE EFFECTS
   N/A

===========================================================================*/
static boolean check_if_any_connection(loc_eng_atl_session_state_e_type conn_state,int session_index)
{
      int i=0;
      for(i=0;i < MAX_NUM_ATL_CONNECTIONS; i++)
      {
         if(i == session_index)
            continue; //skip this record as we want to check all others

         if(loc_eng_data.atl_conn_info[i].active == TRUE && loc_eng_data.atl_conn_info[i].conn_state == conn_state)
            return TRUE;
      }
      return FALSE;
}

/*===========================================================================
FUNCTION check_if_all_connection

DESCRIPTION
   Function which is used to determine if all the other sessions have the state
   passed in

DEPENDENCIES
   None

RETURN VALUE
   TRUE/FALSE

SIDE EFFECTS
   N/A

===========================================================================*/
static boolean check_if_all_connection(loc_eng_atl_session_state_e_type conn_state,int session_index)
{
      for(int i=0;i < MAX_NUM_ATL_CONNECTIONS; i++)
      {
         if(i == session_index)
            continue; //skip this record as we want to check all others

         if(loc_eng_data.atl_conn_info[i].conn_state != conn_state)
            return FALSE;
      }
      return TRUE;
}
