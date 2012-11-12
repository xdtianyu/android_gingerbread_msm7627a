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
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <linux/errno.h>

#include "loc_api_v02_client.h"
#include "qmi_loc_v02.h"
#include "loc_eng.h"
#include "loc_api_sync_req.h"

//logging
#define LOG_TAG "loc_eng_v02"
#include "loc_util_log.h"

#ifndef LOC_UTIL_TARGET_OFF_TARGET

#include <cutils/properties.h>
#include <cutils/sched_policy.h>
#include <utils/SystemClock.h>

#endif //LOC_UTIL_TARGET_OFF_TARGET



#define DEBUG_NI_REQUEST_EMU 0

#define LOC_DATA_DEFAULT false          // Default data connection status (1=ON, 0=OFF)
#define SUCCESS true
#define FAILURE false

//??? dummy session ID,
#define LOC_FIX_SESSION_ID_DEFAULT (1)

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


// Function declarations for sLocEngAGpsInterface
static void loc_eng_agps_init(AGpsCallbacks* callbacks);
static int loc_eng_data_conn_open(const char* apn, AGpsBearerType bearerType);
static int loc_eng_data_conn_closed();
static int loc_eng_data_conn_failed();
static int loc_eng_set_server(AGpsType type, const char *hostname, int port);
static int loc_eng_set_server_proxy(AGpsType type, const char *hostname, int port);

static bool resolve_in_addr(const char *host_addr,
                            struct in_addr *in_addr_ptr);

// Internal functions
static int loc_eng_deinit();

static void loc_event_cb(
   locClientHandleType client_handle,
   uint32_t loc_event,
   const locClientEventIndUnionType event_payload);

static void loc_resp_cb (
   locClientHandleType client_handle,
   uint32_t resp_id,
   const locClientRespIndUnionType resp_payload);

static void loc_eng_report_position (const qmiLocEventPositionReportIndMsgT_v02 *location_report_ptr);
static void loc_eng_report_sv (const qmiLocEventGnssSvInfoIndMsgT_v02 *gnss_report_ptr);

static void loc_inform_gps_status(GpsStatusValue status);

static void loc_eng_report_engine_state (
   const qmiLocEventEngineStateIndMsgT_v02 *engine_state_ptr);

static void loc_eng_report_fix_session_state (
   const qmiLocEventFixSessionStateIndMsgT_v02 *fix_session_state_ptr);

static void loc_eng_report_nmea (const qmiLocEventNmeaIndMsgT_v02 *nmea_report_ptr);

static void loc_eng_deferred_action_thread(void* arg);

static void loc_eng_delete_aiding_data_action(void);

//ATL functions
static void loc_eng_atl_state_reset();
static bool loc_eng_enqueue_atl_conn_handle(
   uint32_t conn_handle, bool responded);
static bool loc_eng_dequeue_atl_conn_handle( uint32_t conn_handle);
static bool loc_eng_is_atl_conn_handle_queue_empty();
static void loc_eng_process_conn_request(
   const qmiLocEventLocationServerConnectionReqIndMsgT_v02 * server_request_ptr);
static void loc_eng_send_data_status(
   uint32_t conn_handle, bool status, qmiLocServerRequestEnumT_v02 request_type);
static void loc_eng_process_conn_ind(loc_eng_atl_ind_e_type ind);
static void loc_eng_process_conn_request(
   const qmiLocEventLocationServerConnectionReqIndMsgT_v02 * server_request_ptr);
static int loc_eng_set_apn (const char* apn);

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

// Global data structure for location engine
loc_eng_data_s_type loc_eng_data;
int loc_eng_inited = 0; /* not initialized */

//ATL state
loc_eng_atl_info_s_type loc_eng_atl_state;

static const AGpsInterface sLocEngAGpsInterface =
{
   sizeof(AGpsInterface),
   loc_eng_agps_init,
   loc_eng_data_conn_open,
   loc_eng_data_conn_closed,
   loc_eng_data_conn_failed,
   loc_eng_set_server_proxy
};

// Address buffers, for addressing setting before init
int    supl_host_set = 0;
char   supl_host_buf[100];
int    supl_port_buf;
int    c2k_host_set = 0;
char   c2k_host_buf[100];
int    c2k_port_buf;



/*********************************************************************
 * Initialization checking macros
 *********************************************************************/
#define INIT_CHECK_RET(x, c) \
  if (!loc_eng_inited) \
  { \
     /* Not intialized, abort */ \
     LOC_UTIL_LOGE("%s: GPS not initialized.", x); \
     return c; \
  }
#define INIT_CHECK(x) INIT_CHECK_RET(x, (-EPERM))
#define INIT_CHECK_VOID(x) INIT_CHECK_RET(x, )

/*===========================================================================
FUNCTION    gps_get_hardware_interface

DESCRIPTION
   Returns the GPS hardware interface based on LOC API v2.0
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
#ifndef LOC_UTIL_TARGET_OFF_TARGET
   char propBuf[PROPERTY_VALUE_MAX];

   // check to see if GPS should be disabled
   property_get("gps.disable", propBuf, "");
   if (propBuf[0] == '1')
   {
      LOC_UTIL_LOGD("gps_get_interface returning NULL because gps.disable=1\n");
      return NULL;
   }
#endif //LOC_UTIL_TARGET_OFF_TARGET
   return &sLocEngInterface;
}

/*===========================================================================
FUNCTION    loc_eng_init

DESCRIPTION
   Initialize the location engine, this includes setting up global data
   and registering location engine with loc api service.

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
// fully shutting down the GPS is temporarily disabled to avoid intermittent
// BP crash

#define DISABLE_CLEANUP 1

static int loc_eng_init(GpsCallbacks* callbacks)
{
   locClientStatusEnumType status;
   LOC_UTIL_LOGV("loc_eng_init version 2.0 entering\n");

   if (NULL == callbacks)
   {
      LOC_UTIL_LOGE(" loc_eng_init: callback passed are null\n");
      return (-EINVAL);
   }

#if DISABLE_CLEANUP
   if (loc_eng_data.deferred_action_thread) {
        LOC_UTIL_LOGD("loc_eng_init. already initialized so return SUCCESS\n");
        return 0;
   }
#endif

   // set capability
   callbacks->set_capabilities_cb(GPS_CAPABILITY_SCHEDULING |
                                  GPS_CAPABILITY_MSA | GPS_CAPABILITY_MSB);

   // Avoid repeated initialization. Call de-init to clean up first.
   if (loc_eng_inited == 1)
   {
      loc_eng_deinit();       /* stop the active client */
#ifdef FEATURE_GNSS_BIT_API
      gpsone_loc_api_server_unblock();
      gpsone_loc_api_server_join();
#endif /* FEATURE_GNSS_BIT_API */
      loc_eng_inited = 0;
   }

   // Process gps.conf
   loc_read_gps_conf();

   // clear loc_eng_data
   memset(&loc_eng_data, 0, sizeof (loc_eng_data_s_type));

   // Save callbacks
   loc_eng_data.location_cb  = callbacks->location_cb;
   loc_eng_data.sv_status_cb = callbacks->sv_status_cb;
   loc_eng_data.status_cb    = callbacks->status_cb;
   loc_eng_data.nmea_cb      = callbacks->nmea_cb;
   loc_eng_data.acquire_wakelock_cb = callbacks->acquire_wakelock_cb;
   loc_eng_data.release_wakelock_cb = callbacks->release_wakelock_cb;


   // Loc engine module data initialization
   loc_eng_data.engine_status = GPS_STATUS_NONE;
   loc_eng_data.fix_session_status = GPS_STATUS_NONE;

   //event ID
   loc_eng_data.loc_event_id = 0;

   // deferred action flags
   loc_eng_data.deferred_action_flags = 0;

   // Mute session
   loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;

   //initialize the fix criteria to default (standalone)
   loc_eng_data.fix_criteria.mode = GPS_POSITION_MODE_STANDALONE;
   loc_eng_data.fix_criteria.min_interval = 1000; // in ms
   loc_eng_data.fix_criteria.preferred_accuracy = 100; // in m
   loc_eng_data.fix_criteria.recurrence = GPS_POSITION_RECURRENCE_PERIODIC;

   //TBD: currently timeout is ignored, should the driver implement
   // a timeout based on this value internally?
   loc_eng_data.fix_criteria.preferred_time = 30000; //in ms

   //reset ATL state
   loc_eng_atl_state_reset();

   loc_eng_data.aiding_data_for_deletion = 0;

   // initialize the synchronous request API, this API
   // allows the caller to wait for an indication after making
   // a request.
   loc_sync_req_init();

   // Open client
   locClientEventMaskType event_mask =
        QMI_LOC_EVENT_POSITION_REPORT_V02  |
        QMI_LOC_EVENT_SATELLITE_REPORT_V02 |
        QMI_LOC_EVENT_PREDICTED_ORBITS_REQUEST_V02  |
        QMI_LOC_EVENT_POSITION_INJECTION_REQUEST_V02 |
        QMI_LOC_EVENT_TIME_INJECTION_REQUEST_V02 |
        QMI_LOC_EVENT_ENGINE_STATE_REPORT_V02 |
        QMI_LOC_EVENT_FIX_SESSION_STATUS_REPORT_V02 |
        QMI_LOC_EVENT_NMEA_POSITION_REPORT_V02 |
        QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQUEST_V02|
        QMI_LOC_EVENT_LOCATION_SERVER_REQUEST_V02;

   status = locClientOpen(event_mask, loc_event_cb, loc_resp_cb,
                           &loc_eng_data.client_handle);

   if (eLOC_CLIENT_SUCCESS != status ||
        loc_eng_data.client_handle == LOC_CLIENT_INVALID_HANDLE_VALUE )
   {
      LOC_UTIL_LOGE ("loc_eng_init: locClientOpen failed, status = %d\n", status);
      return (-EIO);
   }

   loc_eng_data.client_opened = (loc_eng_data.client_handle >= 0);

   pthread_mutex_init(&loc_eng_data.mute_session_lock, NULL);
   pthread_mutex_init(&loc_eng_data.deferred_action_mutex, NULL);
   pthread_cond_init(&loc_eng_data.deferred_action_cond, NULL);

   //mutex used for managing stop request when ATL session is ongoing
   pthread_mutex_init (&(loc_eng_data.deferred_stop_mutex), NULL);

   // Create threads (if not yet created)
   if (!loc_eng_inited)
   {
      loc_eng_data.deferred_action_thread =
         callbacks->create_thread_cb("loc_api",loc_eng_deferred_action_thread, NULL);

#ifdef FEATURE_GNSS_BIT_API
      gpsone_loc_api_server_launch(NULL, NULL);
#endif /* FEATURE_GNSS_BIT_API */
   }


   // XTRA module data initialization
   pthread_mutex_init(&loc_eng_data.xtra_module_data.lock, NULL);
   loc_eng_data.xtra_module_data.download_request_cb = NULL;

   loc_eng_inited = 1;

   LOC_UTIL_LOGD("loc_eng_init created client, id = %d\n",  loc_eng_data.client_handle);
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
   int rc = 0;
   LOC_UTIL_LOGV("loc_eng_deinit called\n");

   //??? why 3 inited flags, [handle, client_opened, inited?]
   if (loc_eng_inited == 1)
   {
      if (loc_eng_data.navigating)
      {
         LOC_UTIL_LOGD("loc_eng_init: fix not stopped. stop it now.\n");
         if ( (rc = loc_eng_stop()) >= 0)
         {
            loc_eng_data.navigating = false;
         }
      }

      if (loc_eng_data.client_opened)
      {
         LOC_UTIL_LOGD("loc_eng_init: client opened. close it now.\n");
         if( eLOC_CLIENT_SUCCESS == locClientClose(loc_eng_data.client_handle))
         {
            loc_eng_data.client_opened = false;
            loc_eng_inited = 0;
         }

         else
         {
            //TBD define errors for loc_eng
            rc = -EIO;
         }
      }
   }

   return rc;
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

   //stop_mutex not needed.
   pthread_mutex_destroy (&loc_eng_data.deferred_stop_mutex);
   pthread_cond_destroy  (&loc_eng_data.deferred_action_cond);
   pthread_mutex_destroy (&loc_eng_data.deferred_action_mutex);
   pthread_mutex_destroy (&loc_eng_data.mute_session_lock);
#endif //DISABLE_CLEANUP
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

   locClientStatusEnumType status;
   locClientReqUnionType req_union;

   qmiLocStartReqMsgT_v02 start_msg;

   qmiLocSetOperationModeReqMsgT_v02 set_mode_msg;
   qmiLocSetOperationModeIndMsgT_v02 set_mode_ind;

   // clear all fields, validity masks
   memset (&start_msg, 0, sizeof(start_msg));
   memset (&set_mode_msg, 0, sizeof(set_mode_msg));
   memset (&set_mode_ind, 0, sizeof(set_mode_ind));

   LOC_UTIL_LOGD("loc_eng_start called \n");

   // fill in the start request
   switch(loc_eng_data.fix_criteria.mode)
   {
      case GPS_POSITION_MODE_MS_BASED:
         set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSB_V02;
         break;

      case GPS_POSITION_MODE_MS_ASSISTED:
         set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSA_V02;
         break;

      default:
         set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_STANDALONE_V02;
         break;
   }

   req_union.pSetOperationModeReq = &set_mode_msg;

   // send the mode first, before the start message.
   status = loc_sync_send_req(loc_eng_data.client_handle,
                              QMI_LOC_SET_OPERATION_MODE_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_SET_OPERATION_MODE_IND_V02,
                              &set_mode_ind); // NULL?

   if (status != eLOC_CLIENT_SUCCESS ||
       eQMI_LOC_SUCCESS_V02 != set_mode_ind.status)
   {
      LOC_UTIL_LOGE ("loc_eng_start set opertion mode failed, "
                "status = %d, ind..status = %d\n",
                 status, set_mode_ind.status);
      return -EIO; // error
   }

   if(loc_eng_data.fix_criteria.min_interval > 0)
   {
      start_msg.minInterval_valid = 1;
      start_msg.minInterval = loc_eng_data.fix_criteria.min_interval;
   }

   if(loc_eng_data.fix_criteria.preferred_accuracy > 0)
   {
      start_msg.horizontalAccuracyLevel_valid = 1;

      if (loc_eng_data.fix_criteria.preferred_accuracy <= 100)
      {
         // fix needs high accuracy
         start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_HIGH_V02;
      }
      else if (loc_eng_data.fix_criteria.preferred_accuracy <= 1000)
      {
         //fix needs med accuracy
         start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_MED_V02;
      }
      else
      {
         //fix needs low accuracy
         start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_LOW_V02;
      }
   }

   start_msg.fixRecurrence_valid = 1;
   if(GPS_POSITION_RECURRENCE_SINGLE == loc_eng_data.fix_criteria.recurrence)
   {
      start_msg.fixRecurrence = eQMI_LOC_RECURRENCE_SINGLE_V02;
   }
   else
   {
      start_msg.fixRecurrence = eQMI_LOC_RECURRENCE_PERIODIC_V02;
   }

   //dummy session id
   // TBD: store session ID, check for session id in pos reports.
   start_msg.sessionId = LOC_FIX_SESSION_ID_DEFAULT;

   req_union.pStartReq = &start_msg;

   status = locClientSendReq(loc_eng_data.client_handle, QMI_LOC_START_REQ_V02,
                             req_union );
   // if success
   if( eLOC_CLIENT_SUCCESS == status)
   {
      loc_eng_data.navigating = true;
      return 0;
   }
   // start_fix failed so MO fix is not in progress
   loc_eng_data.navigating = false;
   return -EIO;
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

   locClientStatusEnumType status;
   locClientReqUnionType req_union;

   qmiLocStopReqMsgT_v02 stop_msg;

   LOC_UTIL_LOGD("loc_eng_stop called \n");

   memset(&stop_msg, 0, sizeof(stop_msg));

   pthread_mutex_lock(&(loc_eng_data.deferred_stop_mutex));
    // work around problem with loc_eng_stop when AGPS requests are pending
    // we defer stopping the engine until the AGPS request is done
   if (loc_eng_data.agps_request_pending)
   {
      loc_eng_data.stop_request_pending = true;
      LOC_UTIL_LOGD("loc_eng_stop - deferring stop until AGPS data call is finished\n");
      pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));
      return 0;
   }
   pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));

   // dummy session id
   stop_msg.sessionId = LOC_FIX_SESSION_ID_DEFAULT;

   req_union.pStopReq = &stop_msg;

   status = locClientSendReq(loc_eng_data.client_handle, QMI_LOC_STOP_REQ_V02,
                             req_union);
   // if success
   if( eLOC_CLIENT_SUCCESS == status)
   {
      if (loc_eng_data.fix_session_status != GPS_STATUS_SESSION_BEGIN)
      {
         loc_inform_gps_status(GPS_STATUS_SESSION_END);
      }

      loc_eng_data.navigating = false;
      return 0;
   }

   LOC_UTIL_LOGE("loc_eng_stop: error = %d\n", status);
   return (-EIO);
}

/*===========================================================================
FUNCTION    loc_eng_mute_one_session

DESCRIPTION
   Mutes one session
   TBD: Be more descriptive here?

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
   LOC_UTIL_LOGD("loc_eng_mute_one_session \n");

   pthread_mutex_lock(&loc_eng_data.mute_session_lock);
   loc_eng_data.mute_session_state = LOC_MUTE_SESS_WAIT;
   LOC_UTIL_LOGV("loc_eng_report_status: mute_session_state changed to WAIT");
   pthread_mutex_unlock(&loc_eng_data.mute_session_lock);
}

/*===========================================================================
FUNCTION    loc_eng_set_position_mode

DESCRIPTION
   Sets the mode and fix frequency for the tracking session.
   If engine is not "navigating" the mode is stored and is sent
   along the start request. If the engine is navigating the this is
   interpreted as a "restart" with new fix criteria.

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
   int rc = 0;
   INIT_CHECK("loc_eng_set_position_mode");
   LOC_UTIL_LOGD ("loc_eng_set_position mode, client = %d, interval = %d, mode = %d\n",
          loc_eng_data.client_handle, min_interval, mode);

   //store the fix criteria
   loc_eng_data.fix_criteria.mode = mode;
   loc_eng_data.fix_criteria.recurrence = recurrence;
   if(min_interval == 0)
   {
      loc_eng_data.fix_criteria.min_interval = MIN_POSSIBLE_FIX_INTERVAL;
   }
   else
   {
      loc_eng_data.fix_criteria.min_interval = min_interval;
   }

   loc_eng_data.fix_criteria.preferred_accuracy = preferred_accuracy;
   loc_eng_data.fix_criteria.preferred_time = preferred_time;

   if(true == loc_eng_data.navigating)
   {
      //fix is in progress, send a restart
      LOC_UTIL_LOGD ("loc_eng_set_position mode called when fix is in progress, "
                     ": restarting the fix\n");
      rc = loc_eng_start();
   }
   return rc;
}


/*===========================================================================
FUNCTION    loc_eng_inject_time

DESCRIPTION
   This is used by Java native function to do time injection.

DEPENDENCIES
   None

RETURN VALUE
  0

SIDE EFFECTS
   N/A

===========================================================================*/
static int loc_eng_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
   locClientReqUnionType req_union;
   locClientStatusEnumType status;
   qmiLocInjectUtcTimeReqMsgT_v02  inject_time_msg;
   qmiLocInjectUtcTimeIndMsgT_v02 inject_time_ind;

   INIT_CHECK("loc_eng_inject_time");

   memset(&inject_time_msg, 0, sizeof(inject_time_msg));

   inject_time_ind.status = eQMI_LOC_GENERAL_FAILURE_V02;

   inject_time_msg.timeUtc = time;

#ifndef LOC_UTIL_TARGET_OFF_TARGET
   inject_time_msg.timeUtc += (int64_t)(android::elapsedRealtime() - timeReference);
#endif //LOC_UTIL_TARGET_OFF_TARGET

   inject_time_msg.timeUnc = uncertainty;

   req_union.pInjectUtcTimeReq = &inject_time_msg;

   LOC_UTIL_LOGD ("loc_eng_inject_time, uncertainty = %d\n", uncertainty);

   status = loc_sync_send_req(loc_eng_data.client_handle,
                              QMI_LOC_INJECT_UTC_TIME_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_INJECT_UTC_TIME_IND_V02,
                              &inject_time_ind); // NULL?

   if (status != eLOC_CLIENT_SUCCESS ||
       eQMI_LOC_SUCCESS_V02 != inject_time_ind.status)
   {
      LOC_UTIL_LOGE ("loc_eng_inject_time failed, status = %d, ind..status = %d\n",
                 status, inject_time_ind.status);
      return -EIO; // error
   }

   return 0;
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
   locClientReqUnionType req_union;
   locClientStatusEnumType status;
   qmiLocInjectPositionReqMsgT_v02 inject_pos_msg;
   qmiLocInjectPositionIndMsgT_v02 inject_pos_ind;

   INIT_CHECK("loc_eng_inject_location");

   memset(&inject_pos_msg, 0, sizeof(inject_pos_msg));

   inject_pos_msg.latitude = latitude;
   inject_pos_msg.longitude = longitude;
   inject_pos_msg.horUncCircular_valid = 1;
   inject_pos_msg.horUncCircular = accuracy; //meters assumed
   inject_pos_msg.horConfidence_valid = 1;
   inject_pos_msg.horConfidence = 63; // 63% (1 std dev assumed)

   /* Log */
   LOC_UTIL_LOGD("Inject coarse position Lat=%lf, Lon=%lf, Acc=%.2lf\n",
            inject_pos_msg.latitude,
            inject_pos_msg.longitude,
            inject_pos_msg.horUncCircular);

   req_union.pInjectPositionReq = &inject_pos_msg;

   status = loc_sync_send_req(loc_eng_data.client_handle,
                              QMI_LOC_INJECT_POSITION_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_INJECT_POSITION_IND_V02,
                              &inject_pos_ind);

   if (status != eLOC_CLIENT_SUCCESS ||
       eQMI_LOC_SUCCESS_V02 != inject_pos_ind.status)
   {
      LOC_UTIL_LOGE ("loc_eng_inject_position failed\n, status = %d, inject_pos_ind.status = %d\n"
                , status, inject_pos_ind.status);
      return -EIO;
   }
   return  0;
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

    // Currently, LOC API only support deletion of all aiding data,
   if (f)
      loc_eng_data.aiding_data_for_deletion = GPS_DELETE_ALL;

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

   LOC_UTIL_LOGD("loc_inform_gps_status, status: %d\n",  status);

   if (loc_eng_data.status_cb)
   {
      loc_eng_data.status_cb(&gs);

      //??? why is this needed?
      //Restore session begin if needed
      if (status == GPS_STATUS_ENGINE_ON && last_status == GPS_STATUS_SESSION_BEGIN)
      {
         GpsStatus gs_sess_begin = { sizeof(gs_sess_begin),GPS_STATUS_SESSION_BEGIN };
         loc_eng_data.status_cb(&gs_sess_begin);
      }
   }

   last_status = status;
}


/*===========================================================================
FUNCTION   copy_event_payload

DESCRIPTION
   Utility function to copy event payload into a buffer

DEPENDENCIES
   None

RETURN VALUE
   true on Success, false on Failure

SIDE EFFECTS
   N/A

===========================================================================*/

static bool copy_event_payload (
    uint32_t event_id,
    locClientEventIndUnionType *pEventPayloadDest,
    locClientEventIndUnionType eventPayloadSrc)
{
   void *evtBuffer = NULL;
   size_t event_size = 0;
   locClientEventIndUnionType eventPayloadTmp;

   if (false == locClientGetSizeByEventIndId(event_id, &event_size))
   {
      LOC_UTIL_LOGD ("copy_event_payload: locClientGetSizeByEventIndId "
                "returned false \n");
      return false;
   }

   LOC_UTIL_LOGV("copy_event_payload: event_id = %d, size = %ld\n", event_id,
            event_size);

   // allocate memory for event
   evtBuffer = malloc(event_size);
   if (NULL == evtBuffer )
   {
      LOC_UTIL_LOGD ("copy_event_payload: error allocating memory to copy event\n");
      return false;
   }

   memcpy(evtBuffer, eventPayloadSrc.pEngineState, event_size);

   // use pEngineState as a "dummy" pointer to hold
   // event payload, the payload should be "freed"
   // after event callback has been called

   eventPayloadTmp.pEngineState =
      (qmiLocEventEngineStateIndMsgT_v02*)evtBuffer;

   *pEventPayloadDest = eventPayloadTmp;

   return true;
}


/*===========================================================================
FUNCTION    loc_event_cb

DESCRIPTION
   This is the callback function registered by loc_open.

DEPENDENCIES
   N/A

RETURN VALUE
  NONE

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_event_cb
(
    locClientHandleType client_handle,
    uint32_t loc_event_id,
    const locClientEventIndUnionType event_payload )
{
   INIT_CHECK_VOID("loc_event_cb");

   LOC_UTIL_LOGD ("loc_event_cb: handle %d received event : %d \n",
              client_handle, loc_event_id);

   //loc_eng_callback_log_header(client_handle, loc_event_id, event_payload);

   if (client_handle != loc_eng_data.client_handle)
   {
      LOC_UTIL_LOGE("loc client mismatch: received = %d, expected = %d \n",
             client_handle, loc_eng_data.client_handle);
      return ; /* Exit with OK */
   }

   // loc_eng_callback_log(loc_event_id, event_payload);
   pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);

   // copy event info to global structure loc_eng_data
   loc_eng_data.loc_event_id = loc_event_id;

   if( true == copy_event_payload(loc_event_id, &loc_eng_data.loc_event_payload,
                               event_payload))
   {
      /* hold a wake lock while events are pending for
         deferred_action_thread */
      loc_eng_data.acquire_wakelock_cb();
      loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_EVENT;

      pthread_cond_signal(&loc_eng_data.deferred_action_cond);
   }

   pthread_mutex_unlock (&loc_eng_data.deferred_action_mutex);

   return ;
}

/*===========================================================================
FUNCTION    loc_resp_cb

DESCRIPTION
   This is the callback function registered by loc_open.

DEPENDENCIES
   N/A

RETURN VALUE
  NONE

SIDE EFFECTS
   N/A

===========================================================================*/

static void loc_resp_cb (locClientHandleType client_handle,
                         uint32_t resp_id,
                         const locClientRespIndUnionType resp_payload)
{
   LOC_UTIL_LOGV ("loc_resp_cb, client = %d, resp id = %d\n",
          client_handle, resp_id);
   // process the sync call
   // use pDeleteAssistDataInd as a dummy pointer
   loc_sync_process_ind(client_handle, resp_id,
                        (void *)resp_payload.pDeleteAssistDataInd);
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
static void loc_eng_report_position(
   const qmiLocEventPositionReportIndMsgT_v02 *location_report_ptr)
{
   GpsLocation location;

   memset(&location, 0, sizeof (GpsLocation));
   location.size = sizeof(location);
   // Process the position from final and intermediate reports

   if ( (location_report_ptr->sessionStatus == eQMI_LOC_SESS_STATUS_SUCCESS_V02) ||
        ( (location_report_ptr->sessionStatus == eQMI_LOC_SESS_STATUS_IN_PROGESS_V02)
           && (0 != gps_conf.INTERMEDIATE_POS)
        )
      )
   {
      // Time stamp (UTC)
      if (location_report_ptr->timestampUtc_valid == 1)
      {
         location.timestamp = location_report_ptr->timestampUtc;
      }

      // Latitude & Longitude
      if ( (location_report_ptr->latitude_valid == 1 ) &&
           (location_report_ptr->longitude_valid == 1) )
      {
         location.flags    |= GPS_LOCATION_HAS_LAT_LONG;
         location.latitude  = location_report_ptr->latitude;
         location.longitude = location_report_ptr->longitude;
      }

      // Altitude
      if (location_report_ptr->altitudeWrtEllipsoid_valid == 1  )
      {
         location.flags    |= GPS_LOCATION_HAS_ALTITUDE;
         location.altitude = location_report_ptr->altitudeWrtEllipsoid;
      }

         // Speed
      if ((location_report_ptr->speedHorizontal_valid == 1) &&
          (location_report_ptr->speedVertical_valid ==1 ) )
      {
         location.flags    |= GPS_LOCATION_HAS_SPEED;
         location.speed = sqrt(
            (location_report_ptr->speedHorizontal * location_report_ptr->speedHorizontal) +
            (location_report_ptr->speedVertical * location_report_ptr->speedVertical) );
      }

      // Heading
      if (location_report_ptr->heading_valid == 1)
      {
         location.flags    |= GPS_LOCATION_HAS_BEARING;
         location.bearing = location_report_ptr->heading;
      }

      // Uncertainty (circular)
      if ( (location_report_ptr->horUncCircular_valid ) )
      {
         location.flags    |= GPS_LOCATION_HAS_ACCURACY;
         location.accuracy = location_report_ptr->horUncCircular;
      }

         // Filtering
      bool filter_out = false;

      // Filter any 0,0 positions
      if (location.latitude == 0.0 && location.longitude == 0.0)
      {
         filter_out = true;
      }

         // Turn-off intermediate positions outside required accuracy
      if ((gps_conf.ACCURACY_THRES != 0) &&
          (location_report_ptr->sessionStatus == eQMI_LOC_SESS_STATUS_IN_PROGESS_V02)
          && (location_report_ptr->horUncCircular_valid == 1)
          && (location_report_ptr->horUncCircular > gps_conf.ACCURACY_THRES))
      {
         LOC_UTIL_LOGD("loc_eng_report_position: ignore intermediate position with "
                  "error %.2f > %ld meters\n",
                  location_report_ptr->horUncCircular, gps_conf.ACCURACY_THRES);
         filter_out = true;
      }


      if (loc_eng_data.location_cb != NULL &&  false == filter_out)
      {
         LOC_UTIL_LOGV("loc_eng_report_position: fire callback\n");
         loc_eng_data.location_cb(&location);
      }
   }
   else
   {
      LOC_UTIL_LOGV("loc_eng_report_position: ignore position report when session status = %d\n",
                  location_report_ptr->sessionStatus);
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
static void loc_eng_report_sv (const qmiLocEventGnssSvInfoIndMsgT_v02 *gnss_report_ptr)
{
   GpsSvStatus     SvStatus;
   int             num_svs_max, i;
   const qmiLocSvInfoStructT_v02 *sv_info_ptr;

   LOC_UTIL_LOGD ("loc_eng_report_sv: num of sv = %d\n", gnss_report_ptr->svList_len);

   num_svs_max = 0;
   memset (&SvStatus, 0, sizeof (GpsSvStatus));
   if (gnss_report_ptr->svList_valid == 1)
   {
      num_svs_max = gnss_report_ptr->svList_len;
      if (num_svs_max > GPS_MAX_SVS)
      {
         num_svs_max = GPS_MAX_SVS;
      }
      SvStatus.num_svs = 0;
      for (i = 0; i < num_svs_max; i++)
      {
         sv_info_ptr = &(gnss_report_ptr->svList[i]);
         if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_SYSTEM_V02)
         {
            if (sv_info_ptr->system == eQMI_LOC_SV_SYSTEM_GPS_V02)
            {
               SvStatus.sv_list[SvStatus.num_svs].size = sizeof(GpsSvStatus);
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn;

               // We only have the data field to report gps eph and alm mask
               if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_SVINFO_MASK_V02)
               {
                  if(sv_info_ptr->svInfoMask & QMI_LOC_SVINFO_MASK_HAS_EPHEMERIS_V02)
                  {
                     SvStatus.ephemeris_mask |= (1 << (sv_info_ptr->prn-1));
                  }
                  if(sv_info_ptr->svInfoMask & QMI_LOC_SVINFO_MAKS_HAS_ALMANAC_V02)
                  {
                     SvStatus.almanac_mask |= (1 << (sv_info_ptr->prn-1));
                  }
               }

               if ((sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_PROCESS_STATUS_V02)
                    &&
                   (sv_info_ptr->svStatus == eQMI_LOC_SV_STATUS_TRACK_V02))
               {
                  SvStatus.used_in_fix_mask |= (1 << (sv_info_ptr->prn-1));
               }
            }
            // SBAS: GPS RPN: 120-151,
            // In exteneded measurement report, we follow nmea standard, which is from 33-64.
            else if (sv_info_ptr->system == eQMI_LOC_SV_SYSTEM_SBAS_V02)
            {
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn + 33 - 120;
            }
            // Gloness: Slot id: 1-32
            // In extended measurement report, we follow nmea standard, which is 65-96
            else if (sv_info_ptr->system == eQMI_LOC_SV_SYSTEM_GLONASS_V02)
            {
               SvStatus.sv_list[SvStatus.num_svs].prn = sv_info_ptr->prn + (65-1);
            }
            // Unsupported SV system
            else
            {
               continue;
            }
         }

         if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_SNR_V02 )
         {
            SvStatus.sv_list[SvStatus.num_svs].snr = sv_info_ptr->snr;
         }

         if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_ELEVATION_V02)
         {
            SvStatus.sv_list[SvStatus.num_svs].elevation = sv_info_ptr->elevation;
         }

         if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_VALID_AZIMUTH_V02)
         {
            SvStatus.sv_list[SvStatus.num_svs].azimuth = sv_info_ptr->azimuth;
         }

         SvStatus.num_svs++;
      }
   }

   LOC_UTIL_LOGD ("num_svs = %d, eph mask = %d, alm mask = %d\n", SvStatus.num_svs,
              SvStatus.ephemeris_mask, SvStatus.almanac_mask );
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

static void loc_eng_report_engine_state (
   const qmiLocEventEngineStateIndMsgT_v02 *engine_state_ptr)
{
   GpsStatusValue status;

   LOC_UTIL_LOGD("loc_eng_report_status: event = %d\n",
            engine_state_ptr->engineState);

   status = GPS_STATUS_NONE;
   if (engine_state_ptr->engineState == eQMI_LOC_ENGINE_STATE_ON_V02)
   {
      // GPS_STATUS_SESSION_BEGIN implies GPS_STATUS_ENGINE_ON
      // ??? Why should it send session begin instead of engine_on
      //status = GPS_STATUS_SESSION_BEGIN;
      status = GPS_STATUS_ENGINE_ON;
   }
   else if (engine_state_ptr->engineState == eQMI_LOC_ENGINE_STATE_OFF_V02)
   {
      // GPS_STATUS_SESSION_END implies GPS_STATUS_ENGINE_OFF
      status = GPS_STATUS_ENGINE_OFF;
   }

   loc_inform_gps_status(status);

#if 0
   pthread_mutex_lock(&loc_eng_data.mute_session_lock);

   // Switch from WAIT to MUTE, for "engine on" or "session begin" event
   if (status == GPS_STATUS_SESSION_BEGIN || status == GPS_STATUS_ENGINE_ON)
   {
      if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_WAIT)
      {
         LOC_UTIL_LOGV("loc_eng_report_status: mute_session_state changed from WAIT to IN SESSION");
         loc_eng_data.mute_session_state = LOC_MUTE_SESS_IN_SESSION;
      }
   }

   // Switch off MUTE session
   if (loc_eng_data.mute_session_state == LOC_MUTE_SESS_IN_SESSION &&
       (status == GPS_STATUS_SESSION_END || status == GPS_STATUS_ENGINE_OFF))
   {
      LOC_UTIL_LOGV("loc_eng_report_status: mute_session_state changed from IN SESSION to NONE");
      loc_eng_data.mute_session_state = LOC_MUTE_SESS_NONE;
   }

   // Session End is not reported during Android navigating state
   if (status != GPS_STATUS_NONE && !(status == GPS_STATUS_SESSION_END && loc_eng_data.navigating))
   {
      LOC_UTIL_LOGV("loc_eng_report_status: issue callback with status %d\n", status);

      if (loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
      {
         // Inform GpsLocationProvider about mNavigating status
         loc_inform_gps_status(status);
      }
      else {
         LOC_UTIL_LOGV("loc_eng_report_status: muting the status report.");
      }
   }

   pthread_mutex_unlock(&loc_eng_data.mute_session_lock);
#endif

   loc_eng_data.engine_status = status;

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

static void loc_eng_report_fix_session_state (
   const qmiLocEventFixSessionStateIndMsgT_v02 *fix_session_state_ptr)
{
   GpsStatusValue status;

   LOC_UTIL_LOGD("loc_eng_report_status: event = %d\n",
            fix_session_state_ptr->sessionState);
   status = GPS_STATUS_NONE;
   if (fix_session_state_ptr->sessionState == eQMI_LOC_FIX_SESSION_STARTED_V02)
   {
      // GPS_STATUS_SESSION_BEGIN implies GPS_STATUS_ENGINE_ON(???)
      status = GPS_STATUS_SESSION_BEGIN;
   }
   else if (fix_session_state_ptr->sessionState == eQMI_LOC_FIX_SESSION_FINISHED_V02)
   {
      // GPS_STATUS_SESSION_END implies GPS_STATUS_ENGINE_OFF
      status = GPS_STATUS_SESSION_END;
   }
   loc_inform_gps_status(status);
   loc_eng_data.fix_session_status = status;
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
static void loc_eng_report_nmea (const qmiLocEventNmeaIndMsgT_v02 *nmea_report_ptr)
{
   if (loc_eng_data.nmea_cb != NULL)
   {
      struct timeval tv;

      gettimeofday(&tv, (struct timezone *) NULL);
      long long now = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

#if (AMSS_VERSION==3200)
      loc_eng_data.nmea_cb(now, nmea_report_ptr->nmea, strlen(nmea_report_ptr->nmea));
#else
      loc_eng_data.nmea_cb(now, nmea_report_ptr->nmea, strlen(nmea_report_ptr->nmea));

      LOC_UTIL_LOGD("loc_eng_report_nmea: $%c%c%c\n",
         nmea_report_ptr->nmea[3], nmea_report_ptr->nmea[4],
               nmea_report_ptr->nmea[5]);

#endif /* #if (AMSS_VERSION==3200) */
   }
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
static void loc_eng_process_loc_event (uint32_t loc_event_id,
        locClientEventIndUnionType loc_event_payload)
{
   LOC_UTIL_LOGD("loc_eng_process_loc_event: %d\n", loc_event_id);
   // Parsed report

   switch(loc_event_id)
   {
      //Position Report
      case QMI_LOC_EVENT_POSITION_REPORT_IND_V02:
         if(loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
         {
            loc_eng_report_position(loc_event_payload.pPositionReportEvent);
         }
         break;

      // Satellite report
      case QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02:
         if(loc_eng_data.mute_session_state != LOC_MUTE_SESS_IN_SESSION)
         {
            loc_eng_report_sv(loc_event_payload.pGnssSvInfoReportEvent);
         }
         break;

      // Status report
      case QMI_LOC_EVENT_ENGINE_STATE_IND_V02:
         loc_eng_report_engine_state(loc_event_payload.pEngineState);
         break;

      case QMI_LOC_EVENT_FIX_SESSION_STATE_IND_V02:
         loc_eng_report_fix_session_state(loc_event_payload.pFixSessionState);
         break;

     // NMEA
      case QMI_LOC_EVENT_NMEA_IND_V02:
         loc_eng_report_nmea(loc_event_payload.pNmeaReportEvent);
         break;

      // XTRA
      case QMI_LOC_EVENT_PREDICTED_ORBITS_INJECT_REQ_IND_V02:
         LOC_UTIL_LOGD("loc_eng_process_loc_event: XTRA download request\n");
         // Call Registered callback
         //?? doesn't copy XTRA server information?
         if (loc_eng_data.xtra_module_data.download_request_cb != NULL)
         {
            loc_eng_data.xtra_module_data.download_request_cb();
         }
         break;

      case QMI_LOC_EVENT_TIME_INJECT_REQ_IND_V02:
          LOC_UTIL_LOGD("loc_eng_process_loc_event: "
                        "Time download request... not supported\n");
          break;

      case QMI_LOC_EVENT_POSITION_INJECT_REQ_IND_V02:
          LOC_UTIL_LOGD("loc_eng_process_loc_event: Postion request... not supported\n");
          break;

      case QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND_V02:
         loc_eng_ni_callback(loc_event_payload.pNiNotifyVerifyReqEvent);
         break;

      // AGPS data request
      case QMI_LOC_EVENT_LOCATION_SERVER_CONNECTION_REQ_IND_V02:
         loc_eng_process_conn_request(loc_event_payload.pLocationServerConnReqEvent);
         break;
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

   LOC_UTIL_LOGV("loc_eng_agps_init called\n");

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
   unsigned len;
   locClientReqUnionType req_union;
   locClientStatusEnumType status;
   qmiLocSetServerReqMsgT_v02 set_server_req;
   qmiLocSetServerIndMsgT_v02 set_server_ind;

   struct in_addr addr;
   char url[256];

   memset(&set_server_req, 0, sizeof(set_server_req));

   LOC_UTIL_LOGD("loc_eng_set_server, type = %d, hostname = %s, port = %d\n", (int) type, hostname, port);

   // Needed length
   len = snprintf(url, sizeof(url), "%s:%u", hostname, (unsigned) port);
   if (len >= sizeof(url))
   {
      LOC_UTIL_LOGE("loc_eng_set_server, URL too long (len=%d).\n", len);
      return -EINVAL;
   }

  switch (type) {
   case AGPS_TYPE_SUPL:
      set_server_req.serverType = eQMI_LOC_SERVER_TYPE_UMTS_SLP_V02;
      set_server_req.urlAddr_valid = 1;
      strlcpy(set_server_req.urlAddr, url, sizeof(set_server_req.urlAddr));
      break;

   case AGPS_TYPE_C2K:
      if (!resolve_in_addr(hostname, &addr))
      {
         LOC_UTIL_LOGE("loc_eng_set_server, hostname %s cannot be resolved.\n", hostname);
         return -EIO;
      }

      set_server_req.serverType = eQMI_LOC_SERVER_TYPE_CDMA_PDE_V02;
      set_server_req.ipv4Addr_valid = 1;
      set_server_req.ipv4Addr.addr = (uint32_t) htonl(addr.s_addr);
      set_server_req.ipv4Addr.port = port;
      LOC_UTIL_LOGD ("loc_eng_set_server, addr = %X:%d\n",
                (unsigned int) set_server_req.ipv4Addr.addr,
                (unsigned int) port);

      break;

  default:
      LOC_UTIL_LOGE("loc_eng_set_server, unknown server type = %d", (int) type);
      return 0; /* note: error not indicated, since JNI doesn't check */
   }

  req_union.pSetServerReq = &set_server_req;

  status = loc_sync_send_req(loc_eng_data.client_handle,
                              QMI_LOC_SET_SERVER_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_SET_SERVER_IND_V02,
                              &set_server_ind);

   if (status != eLOC_CLIENT_SUCCESS ||
       eQMI_LOC_SUCCESS_V02 != set_server_ind.status)
   {
      LOC_UTIL_LOGE ("loc_eng_set_server failed\n, status = %d, "
                "set_server_ind.status = %d\n", status, set_server_ind.status);
      return -EIO;
   }
   else
   {
      LOC_UTIL_LOGV("loc_eng_set_server successful\n");
   }

   return 0;
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
      LOC_UTIL_LOGW("set_server called before init. save the address, type: %d, hostname: %s, port: %d",
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
         LOC_UTIL_LOGE("loc_eng_set_server_proxy, unknown server type = %d", (int) type);
      }
   }
   return 0;
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
static void loc_eng_delete_aiding_data_action(void)
{
   // Currently, we only support deletion of all aiding data,
   // since the Android defined aiding data mask matches with modem,
   // so just pass them down without any translation

   locClientStatusEnumType    status;
   locClientReqUnionType      req_union;

   qmiLocDeleteAssistDataReqMsgT_v02 delete_data_req;
   qmiLocDeleteAssistDataIndMsgT_v02 delete_data_ind;

   // only delete all mask is supported

   memset(&delete_data_req, 0, sizeof(delete_data_req));

   delete_data_req.deleteMask =  QMI_LOC_ASSIST_DATA_MASK_ALL_V02;
   req_union.pDeleteAssistDataReq = &delete_data_req;

   loc_eng_data.aiding_data_for_deletion = 0;

  status = loc_sync_send_req(loc_eng_data.client_handle,
                             QMI_LOC_DELETE_ASSIST_DATA_REQ_V02,
                             req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                             QMI_LOC_DELETE_ASSIST_DATA_IND_V02,
                             &delete_data_ind);

   LOC_UTIL_LOGV("loc_eng_delete_aiding_data_action returned: %d "
            "delete_data_ind.status = %d\n", status, delete_data_ind.status);
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
                                       unsigned char *ipv6_addr)
{
   AGpsStatus agpsStatus;

   if (loc_eng_data.agps_status_cb == NULL)
   {
      LOC_UTIL_LOGE("loc_eng_report_agps_status, callback not initialized.\n");
      return;
   }

   LOC_UTIL_LOGI("loc_eng_report_agps_status, type = %d, status = %d, ipv4_addr = %u\n",
         (int) type, (int) status,  (int) ipv4_addr);

   agpsStatus.size = sizeof(agpsStatus);
   agpsStatus.type = type;
   agpsStatus.status = status;
   agpsStatus.ipv4_addr = ipv4_addr;
   if(NULL != ipv6_addr)
   {
      memcpy(agpsStatus.ipv6_addr, ipv6_addr, 16);
   }
   else
   {
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
FUNCTION    loc_eng_send_data_status

DESCRIPTION
   This function makes  sends the status of data call to the modem

DEPENDENCIES
   Requires loc_eng_data.apn_name

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_send_data_status(
   uint32_t conn_handle, bool status, qmiLocServerRequestEnumT_v02 request_type)
{
   locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
   locClientReqUnionType req_union;
   qmiLocInformLocationServerConnStatusReqMsgT_v02 conn_status_req;
   qmiLocInformLocationServerConnStatusIndMsgT_v02 conn_status_ind;

   memset(&conn_status_req, 0, sizeof(conn_status_req));
   memset(&conn_status_ind, 0, sizeof(conn_status_ind));

      // Fill in data
   conn_status_req.connHandle = conn_handle;

   conn_status_req.requestType = request_type;

   // TBD handle multiple server protocols
  // conn_status_req.serverProtocol_valid = 1;
  // conn_status_req.serverProtocol = loc_eng_atl_state.server_protocol;

   conn_status_req.statusType = (status == true) ?
      eQMI_LOC_SERVER_REQ_STATUS_SUCCESS_V02:eQMI_LOC_SERVER_REQ_STATUS_FAILURE_V02;

   if(request_type == eQMI_LOC_SERVER_REQUEST_OPEN_V02 && true == status)
   {
      //TBD :handle multiple apn types
      strlcpy(conn_status_req.apnProfile.apnName, loc_eng_atl_state.apn_profile.apnName,
              sizeof(conn_status_req.apnProfile.apnName) );
      conn_status_req.apnProfile_valid = 1;

      LOC_UTIL_LOGI("%s:%d]: ATL open APN name = [%s]\n",
                     __func__, __LINE__,
                    loc_eng_atl_state.apn_profile.apnName);
   }

   req_union.pInformLocationServerConnStatusReq = &conn_status_req;

   result = loc_sync_send_req(loc_eng_data.client_handle,
                              QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02,
                              &conn_status_ind);

   if(result != eLOC_CLIENT_SUCCESS ||
       eQMI_LOC_SUCCESS_V02 != conn_status_ind.status)
   {
      LOC_UTIL_LOGE ("%s:%d]: Error status = %d, ind..status = %d "
                     "request_type = %d\n", __func__, __LINE__, result,
                     conn_status_ind.status, request_type);
   }
}

#define FEATURE_ALLOW_NULL_APN (1)

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

#ifndef FEATURE_ALLOW_NULL_APN
   if (NULL == apn)
   {
      return 0;
   }
#endif

   LOC_UTIL_LOGD("%s:%d]: APN name = [%s]\n",
                 __func__, __LINE__, apn);
   pthread_mutex_lock(&(loc_eng_data.deferred_action_mutex));
   loc_eng_set_apn(apn);
   /* hold a wake lock while events are pending for deferred_action_thread */
   loc_eng_data.acquire_wakelock_cb();
   loc_eng_data.deferred_action_flags |= DEFERRED_ACTION_AGPS_DATA_OPENED;
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

   LOC_UTIL_LOGD("loc_eng_data_conn_closed");
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
   LOC_UTIL_LOGD("loc_eng_data_conn_failed");

   pthread_mutex_lock(&(loc_eng_data.deferred_action_mutex));
   //TBD: what to do with the loc_eng_data.data_connection_is_on here?
   // If this is called only in response to the open request then false;
   // if it can sent as a response to close request, the connection state will
   // be unknown. Also can it be called asynchronously, for example if data
   // connection is lost during AGPS session?

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
   LOC_UTIL_LOGI("%s:%d]: APN Name = [%s]\n", __func__, __LINE__, apn);
   strlcpy(loc_eng_atl_state.apn_profile.apnName, apn,
           sizeof(loc_eng_atl_state.apn_profile.apnName));

   //TBD: Right now we are assuming default values for SUPL,
   loc_eng_atl_state.apn_profile.pdnType = eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4_V02;
   return 0;
}

/*===========================================================================

FUNCTION resolve_in_addr

DESCRIPTION
   Translates a hostname to in_addr struct

DEPENDENCIES
   n/a

RETURN VALUE
   true if successful

SIDE EFFECTS
   n/a

===========================================================================*/
static bool resolve_in_addr(const char *host_addr,
                            struct in_addr *in_addr_ptr)
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
         LOC_UTIL_LOGE("DNS query on '%s' failed\n", host_addr);
         return false;
      }
   }
   return true;
}

/*===========================================================================
FUNCTION    lloc_eng_atl_state_reset

DESCRIPTION
   This is used to reset the atl state to default
   request

DEPENDENCIES
   None

RETURN VALUE
  true on SUCCESS; false otherwise

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_atl_state_reset()
{
   int idx;
   loc_eng_atl_state.atl_state = LOC_CONN_IDLE;
   for(idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
   {
      loc_eng_atl_state.atl_open_conn_handle_list[idx].conn_handle =
         INVALID_ATL_CONNECTION_HANDLE;
      loc_eng_atl_state.atl_open_conn_handle_list[idx].in_use = false;
      loc_eng_atl_state.atl_open_conn_handle_list[idx].responded = true;
   }
   // TBD: Assuming SUPL as default
   loc_eng_atl_state.apn_profile.pdnType = eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4_V02;
}

/*===========================================================================
FUNCTION    loc_eng_enqueue_atl_conn_handle

DESCRIPTION
   This is used to enqueue a conn_handle that came with an atl open
   request

DEPENDENCIES
   None

RETURN VALUE
  true on SUCCESS; false otherwise

SIDE EFFECTS
   N/A

===========================================================================*/

static bool loc_eng_enqueue_atl_conn_handle(
   uint32_t conn_handle, bool responded)
{
   int idx;
   loc_eng_atl_list_type *atl_list = loc_eng_atl_state.atl_open_conn_handle_list;
   for ( idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
   {
      if (atl_list[idx].in_use == false)
      {
         atl_list[idx].in_use =  true;
         atl_list[idx].responded = responded;
         atl_list[idx].conn_handle = conn_handle;
         LOC_UTIL_LOGV("%s:%d], conn handle %u enqueued at idx %d\n",
                       __func__, __LINE__, conn_handle, idx);
         return true;
      }
   }
   // no entry available return error
   return false;
}


/*===========================================================================
FUNCTION    loc_eng_dequeue_atl_conn_handle

DESCRIPTION
   This is used to dequeue an atl connection handle

DEPENDENCIES
   None

RETURN VALUE
  true on SUCCESS; false otherwise

SIDE EFFECTS
   N/A

===========================================================================*/

static bool loc_eng_dequeue_atl_conn_handle( uint32_t conn_handle)
{
   int idx;
   loc_eng_atl_list_type *atl_list = loc_eng_atl_state.atl_open_conn_handle_list;
   for ( idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
   {
      if (atl_list[idx].in_use == true &&
          atl_list[idx].conn_handle == conn_handle)
      {
         LOC_UTIL_LOGV("%s:%d], conn handle %u dequeued from idx %d\n",
                       __func__, __LINE__, conn_handle, idx);

         atl_list[idx].in_use =  false;
         atl_list[idx].responded = true;
         atl_list[idx].conn_handle = INVALID_ATL_CONNECTION_HANDLE;
         return true;
      }
   }
   // conn_handle not found, return
   return false;
}

/*===========================================================================
FUNCTION    loc_eng_is_atl_conn_handle_queue_empty

DESCRIPTION
   This is used to check if the atl conn handle queue is does not
   have any open request pending

DEPENDENCIES
   None

RETURN VALUE
   true on SUCCESS; false otherwise

SIDE EFFECTS
   N/A

===========================================================================*/
static bool loc_eng_is_atl_conn_handle_queue_empty()
{
   int idx;
   loc_eng_atl_list_type *atl_list = loc_eng_atl_state.atl_open_conn_handle_list;
   for ( idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
   {
      if (atl_list[idx].in_use == true)
      {
        return false;
      }
   }
   // no in_use entry, return success
    LOC_UTIL_LOGV("%s:%d], conn handle queue is empty\n",
                  __func__, __LINE__);
   return true;
}

/*===========================================================================
FUNCTION    loc_eng_process_conn_request

DESCRIPTION
   This is used to process a ATL data connection open/close request.

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
static void loc_eng_process_conn_request(
   const qmiLocEventLocationServerConnectionReqIndMsgT_v02 * server_request_ptr)
{
   AGpsType agps_type = AGPS_TYPE_ANY;

   LOC_UTIL_LOGV("%s:%d] : atl state = %d  atl request = %d\n", __func__, __LINE__,
                 loc_eng_atl_state.atl_state, server_request_ptr->requestType);

   switch(loc_eng_atl_state.atl_state)
   {
      case LOC_CONN_IDLE:
         if(server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_OPEN_V02 )
         {
            if( !loc_eng_enqueue_atl_conn_handle(server_request_ptr->connHandle, false))
            {
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        false, server_request_ptr->requestType);
               LOC_UTIL_LOGE("%s:%d]: could not enqueue the server open request\n",
                             __func__, __LINE__);
            }
            else
            {
               loc_eng_atl_state.atl_state = LOC_CONN_OPEN_REQ;
               // Ask Android layer to open an ATL connection
               loc_eng_report_agps_status(
                  agps_type, GPS_REQUEST_AGPS_DATA_CONN,
                  INADDR_NONE, NULL);
            }
         }
         else if(server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_CLOSE_V02)
         {
            LOC_UTIL_LOGE("%s:%d]: engine sent close request in IDLE state, send failure back to "
                          "engine\n", __func__, __LINE__);
            //TBD: can send success here if needed
            loc_eng_send_data_status(server_request_ptr->connHandle,
                                     false, server_request_ptr->requestType);
         }
         break;

      case LOC_CONN_OPEN_REQ :
         if(server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_OPEN_V02 )
         {
            //enqueue the request
            if(!loc_eng_enqueue_atl_conn_handle(server_request_ptr->connHandle, false))
            {
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        false, server_request_ptr->requestType);
               LOC_UTIL_LOGE("%s:%d]: could not enqueue the server open request\n",
                             __func__, __LINE__);
            }
            else
            {
               // enqueued
               LOC_UTIL_LOGV("%s:%d]: Enqueued atl open request from"
                             " the engine when the conn state is LOC_CONN_OPEN_REQ\n",
                             __func__, __LINE__);
            }
         }
         else if (server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_CLOSE_V02)
         {
            if(!loc_eng_dequeue_atl_conn_handle(server_request_ptr->connHandle))
            {
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        false, server_request_ptr->requestType);
               LOC_UTIL_LOGE("%s:%d]: could not find open request corresponding to"
                             "this close request\n",__func__, __LINE__);
            }
            else
            {
               //dequeued open request, send success for close back to engine
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        true, server_request_ptr->requestType);
               LOC_UTIL_LOGV("%s:%d]: sending success for close request for handle %d\n",
                             __func__, __LINE__, server_request_ptr->connHandle );

               // check if empty
               if(loc_eng_is_atl_conn_handle_queue_empty())
               {
                  // TBD: the connection is not up here, we should
                  // set the state to IDLE, and handle open_ind in the IDLE state
                  // correctly
                  loc_eng_atl_state_reset();
               }
            }
         }
         break;

      case LOC_CONN_OPEN:
         if(server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_OPEN_V02 )
         {
            //enqueue the request
            if(!loc_eng_enqueue_atl_conn_handle(server_request_ptr->connHandle, true))
            {
               //could not enqueue
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        false, server_request_ptr->requestType);
               LOC_UTIL_LOGE("%s:%d]: could not enqueue the server open request\n",
                             __func__, __LINE__);
            }
            else
            {
               //enqueued, send a response since the connection is already open
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        true, server_request_ptr->requestType);
               LOC_UTIL_LOGV("%s:%d]: Enqueued and responded to a open request "
                             "from the engine when the conn state is LOC_CONN_OPEN\n",
                             __func__, __LINE__);
            }
         }
         else if (server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_CLOSE_V02)
         {
            if(!loc_eng_dequeue_atl_conn_handle(server_request_ptr->connHandle))
            {
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        false, server_request_ptr->requestType);
               LOC_UTIL_LOGE("%s:%d]: could not find open request corresponding to"
                             "this close request\n",__func__, __LINE__);
            }
            else
            {
               //dequeued open request, send success for close back to engine
               loc_eng_send_data_status(server_request_ptr->connHandle,
                                        true, server_request_ptr->requestType);
               LOC_UTIL_LOGV("%s:%d]: sending success for close request for handle %d\n",
                             __func__, __LINE__, server_request_ptr->connHandle );

               // check if empty
               if(loc_eng_is_atl_conn_handle_queue_empty())
               {
                  // ask the android layer to close ATL connection
                  loc_eng_report_agps_status(
                  agps_type, GPS_RELEASE_AGPS_DATA_CONN,
                  INADDR_NONE, NULL);
                  // set state to idle
                  // TBD: Is there a need for a close_req state to check
                  //  if the connection was actually torn down
                  loc_eng_atl_state_reset();
               }
            }
         }
         break;
   } // end state machine switch
   return;
}

/*===========================================================================
FUNCTION loc_eng_process_conn_ind

DESCRIPTION
   Process the conn_ind from the Android layer

DEPENDENCIES
   None

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/

static void loc_eng_process_conn_ind(loc_eng_atl_ind_e_type ind)
{
   LOC_UTIL_LOGV("%s:%d] : atl state = %d  atl indication = %d\n", __func__, __LINE__,
                 loc_eng_atl_state.atl_state, ind);

   switch(loc_eng_atl_state.atl_state)
   {
      case LOC_CONN_IDLE:
         // do nothing here
         // TBD: if the state transitioned from open_req, after the last
         // close_req was called, we may get a open indication here

         break;

      case LOC_CONN_OPEN_REQ:
         if(LOC_ATL_OPEN_IND == ind)
         {
            // send response back for all queued open requests
            int idx;
            loc_eng_atl_list_type *atl_list =
               loc_eng_atl_state.atl_open_conn_handle_list;
            for(idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
            {
               if( ( true == atl_list[idx].in_use)  &&
                   ( false == atl_list[idx].responded) )
               {
                  //send open success back to engine
                  LOC_UTIL_LOGV("%s:%d]: sending atl open response"
                                " for idx = %u, handle = %u\n", __func__,
                                __LINE__, idx, atl_list[idx].conn_handle);
                  loc_eng_send_data_status(atl_list[idx].conn_handle,
                                        true, eQMI_LOC_SERVER_REQUEST_OPEN_V02);
                  atl_list[idx].responded = true;
               }
            }
            loc_eng_atl_state.atl_state = LOC_CONN_OPEN;
         }
         else if(LOC_ATL_FAILURE_IND == ind || LOC_ATL_CLOSE_IND == ind)
         {
            //move to idle state and send failure back for
            // all queued open requests
             // send response back for all queued open requests
            int idx;
            loc_eng_atl_list_type *atl_list =
               loc_eng_atl_state.atl_open_conn_handle_list;
            for(idx = 0; idx < MAX_NUM_ATL_CONNECTIONS; idx++)
            {
               if( ( true == atl_list[idx].in_use)  &&
                   ( false == atl_list[idx].responded) )
               {
                  //send open failure back to engine
                  loc_eng_send_data_status(atl_list[idx].conn_handle,
                                        false, eQMI_LOC_SERVER_REQUEST_OPEN_V02);
               }
            }
            //TBD: what to do if any close requests come
            //after the state is moved to IDLE??
            loc_eng_atl_state_reset();
         }
         break;

      case LOC_CONN_OPEN:
         if (LOC_ATL_OPEN_IND == ind)
         {
            //do nothing
            LOC_UTIL_LOGD("%s:%d]: Got open indication when state is open!!\n",
                          __func__, __LINE__);
         }
         if(LOC_ATL_CLOSE_IND == ind || LOC_ATL_FAILURE_IND)
         {
            LOC_UTIL_LOGE("%s:%d]: Error unexpected indication %d in OPEN state!!\n",
                          __func__, __LINE__, ind);
            loc_eng_atl_state_reset();
            //TBD: What to do with close requests that may come after a failure
            // should we check the previous state in IDLE??
         }
         break;
   }
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
   LOC_UTIL_LOGD("loc_eng_deferred_action_thread started\n");

   // make sure we do not run in background scheduling group
#ifndef LOC_UTIL_TARGET_OFF_TARGET

   set_sched_policy(gettid(), SP_FOREGROUND);

#endif //LOC_UTIL_TARGET_OFF_TARGET

   while (1)
   {
      GpsAidingData              aiding_data_for_deletion;
      GpsStatusValue             engine_status;
      uint32_t                   loc_event_id;
      locClientEventIndUnionType loc_event_payload;

      // Wait until we are signalled to do a deferred action, or exit
      pthread_mutex_lock(&loc_eng_data.deferred_action_mutex);

      // If we have an event we should process it immediately,
      // otherwise wait until we are signalled
      if (loc_eng_data.deferred_action_flags == 0)
      {
         // do not hold a wake lock while waiting for an event...
         loc_eng_data.release_wakelock_cb();
         LOC_UTIL_LOGD("loc_eng_deferred_action_thread. waiting for events\n");

         pthread_cond_wait(&loc_eng_data.deferred_action_cond,
                           &loc_eng_data.deferred_action_mutex);

         LOC_UTIL_LOGD("loc_eng_deferred_action_thread signalled\n");

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
     loc_event_id = loc_eng_data.loc_event_id;

     if ( (loc_event_id != 0) &&
          (true == copy_event_payload( loc_event_id,
                                       &loc_event_payload,
                                       loc_eng_data.loc_event_payload) ) )
     {
        void *dummy = NULL;
        LOC_UTIL_LOGD("loc_eng_deferred_action_thread event %d\n",loc_event_id);
        loc_eng_data.loc_event_id = 0;

        // free the memory assigned to loc_eng_data event payload
        dummy = (void *)loc_eng_data.loc_event_payload.pEngineState;
        free(dummy);
     }

     //TBD: consider copying the flags into a local flag and then
     // checking the local flag for the action to perform
     int flags = loc_eng_data.deferred_action_flags;
     loc_eng_data.deferred_action_flags = 0;
     engine_status = loc_eng_data.engine_status;
     aiding_data_for_deletion = loc_eng_data.aiding_data_for_deletion;
     status = loc_eng_data.agps_status;
     loc_eng_data.agps_status = 0;

     // perform all actions after releasing the mutex to avoid blocking RPCs from the ARM9
     pthread_mutex_unlock(&(loc_eng_data.deferred_action_mutex));

     if ((flags & DEFERRED_ACTION_EVENT) || (loc_event_id != 0))
     {
        loc_eng_process_loc_event(loc_event_id, loc_event_payload);
        //TBD: one way to serialize is to "continue" from here
        // continue;
     }

     //TBD: Can process the XTRA and delete aiding data when engine is off
     //Set a signal flag for for engine turning off and check is here
     // and "continue" once processed

     // Send_delete_aiding_data must be done when GPS engine is off
     if ((engine_status != GPS_STATUS_ENGINE_ON) && (aiding_data_for_deletion != 0))
     {
        loc_eng_delete_aiding_data_action();
        loc_eng_data.aiding_data_for_deletion = 0;
     }

     // Inject XTRA data when GPS engine is off
     if ((loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON) &&
          (loc_eng_data.xtra_module_data.xtra_data_for_injection != NULL))
     {
        loc_eng_inject_xtra_data_in_buffer();
     }

     //Process connectivity manager events at this point
     if(flags & DEFERRED_ACTION_AGPS_DATA_OPENED)
     {
        loc_eng_process_conn_ind(LOC_ATL_OPEN_IND);
     }
     else if(flags & DEFERRED_ACTION_AGPS_DATA_CLOSED)
     {
        loc_eng_process_conn_ind(LOC_ATL_CLOSE_IND);
     }
     else if(flags & DEFERRED_ACTION_AGPS_DATA_FAILED)
     {
        loc_eng_process_conn_ind(LOC_ATL_FAILURE_IND);
     }

      if (flags & (DEFERRED_ACTION_AGPS_DATA_OPENED |
                   DEFERRED_ACTION_AGPS_DATA_CLOSED |
                   DEFERRED_ACTION_AGPS_DATA_FAILED))
      {
          pthread_mutex_lock(&(loc_eng_data.deferred_stop_mutex));
          // work around problem with loc_eng_stop when AGPS requests are pending
          // we defer stopping the engine until the AGPS request is done
          loc_eng_data.agps_request_pending = false;
          pthread_mutex_unlock(&(loc_eng_data.deferred_stop_mutex));

          if (loc_eng_data.stop_request_pending)
          {
             loc_eng_stop();
          }
      }

   }

   LOC_UTIL_LOGD("loc_eng_deferred_action_thread exiting\n");
   loc_eng_data.release_wakelock_cb();
   loc_eng_data.deferred_action_thread = 0;
}

// for gps.c
extern "C" const GpsInterface* get_gps_interface()
{
    return &sLocEngInterface;
}



