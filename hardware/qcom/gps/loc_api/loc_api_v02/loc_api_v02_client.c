/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 */

#include "qmi.h"
#include "qmi_cci_target.h"
#include "qmi_client.h"
#include "qmi_idl_lib.h"
#include "qmi_cci_common.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "loc_api_v02_client.h"

#define LOG_TAG "loc_api_v02"

#include "loc_util_log.h"

//right now client handle is meaningless since locClient library
//runs in the context of calling process all clients will get the
//same handle

#define LOC_CLIENT_VALID_HANDLE (1234)

// timeout before send_msg_sync should return
#define LOC_CLIENT_ACK_TIMEOUT (5000)
// timeout before a sync request should return
#define LOC_CLIENT_SYNC_REQ_TIMEOUT (5000)

//External functions to send  a synchronous request
// these functions need to defined per platform
// in an external file


extern void loc_sync_req_init();

extern void loc_sync_process_ind(
      locClientHandleType     client_handle,     /* handle of the client */
      uint32_t                ind_id ,      /* respInd id */
      void                    *ind_payload_ptr /* payload              */
);

extern locClientStatusEnumType loc_sync_send_req
(
      locClientHandleType       client_handle,
      uint32_t                  req_id,        /* req id */
      locClientReqUnionType     req_payload,
      uint32_t                  timeout_msec,
      uint32_t                  ind_id,  //ind ID to block for, usually the same as req_id */
      void                      *ind_payload_ptr /* can be NULL*/
);


typedef struct
{
  uint32_t               eventId;
  size_t                 eventSize;
  locClientEventMaskType eventMask;
}locClientEventIndTableStructT;


static locClientEventIndTableStructT locClientEventIndTable[]= {

  // position report ind
  { QMI_LOC_EVENT_POSITION_REPORT_IND_V02,
    sizeof(qmiLocEventPositionReportIndMsgT_v02),
    QMI_LOC_EVENT_POSITION_REPORT_V02},

  // satellite report ind
  { QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02,
    sizeof(qmiLocEventGnssSvInfoIndMsgT_v02),
    QMI_LOC_EVENT_SATELLITE_REPORT_V02},

  // NMEA report ind
  { QMI_LOC_EVENT_NMEA_IND_V02,
    sizeof(qmiLocEventNmeaIndMsgT_v02),
    QMI_LOC_EVENT_NMEA_POSITION_REPORT_V02 },

  //NI event ind
  { QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND_V02,
    sizeof(qmiLocEventNiNotifyVerifyReqIndMsgT_v02),
    QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQUEST_V02},

  //Time Injection Request Ind
  { QMI_LOC_EVENT_TIME_INJECT_REQ_IND_V02,
    sizeof(qmiLocEventTimeInjectReqIndMsgT_v02),
    QMI_LOC_EVENT_TIME_INJECTION_REQUEST_V02 },

  //Predicted Orbits Injection Request
  { QMI_LOC_EVENT_PREDICTED_ORBITS_INJECT_REQ_IND_V02,
    sizeof(qmiLocEventPredictedOrbitsInjectReqIndMsgT_v02),
    QMI_LOC_EVENT_PREDICTED_ORBITS_REQUEST_V02 },

  //Position Injection Request Ind
  { QMI_LOC_EVENT_POSITION_INJECT_REQ_IND_V02,
    sizeof(qmiLocEventPositionInjectionReqIndMsgT_v02),
    QMI_LOC_EVENT_POSITION_INJECTION_REQUEST_V02 } ,

  //Engine State Report Ind
  { QMI_LOC_EVENT_ENGINE_STATE_IND_V02,
    sizeof(qmiLocEventEngineStateIndMsgT_v02),
    QMI_LOC_EVENT_ENGINE_STATE_REPORT_V02 },

  //Fix Session State Report Ind
  { QMI_LOC_EVENT_FIX_SESSION_STATE_IND_V02,
    sizeof(qmiLocEventFixSessionStateIndMsgT_v02),
    QMI_LOC_EVENT_FIX_SESSION_STATUS_REPORT_V02 },

  //Wifi Request Indication
  { QMI_LOC_EVENT_WIFI_REQ_IND_V02,
    sizeof(qmiLocEventWifiRequestIndMsgT_v02),
    QMI_LOC_EVENT_WIFI_REQUEST_V02 },

  //Sensor Streaming Ready Status Ind
  { QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND_V02,
    sizeof(qmiLocEventSensorStreamingReadyStatusIndMsgT_v02),
    QMI_LOC_EVENT_SPI_STREAMING_REQUEST_V02 },

  // Time Sync Request Indication
  { QMI_LOC_EVENT_TIME_SYNC_REQ_IND_V02,
    sizeof(qmiLocEventTimeSyncReqIndMsgT_v02),
    QMI_LOC_EVENT_TIME_SYNC_REQUEST_V02 },

  //Set Spi Streaming Report Event
  { QMI_LOC_EVENT_SET_SPI_STREAMING_REPORT_IND_V02,
    sizeof(qmiLocEventSetSpiStreamingReportIndMsgT_v02),
    QMI_LOC_EVENT_SPI_STREAMING_REQUEST_V02 },

  //Location Server Connection Request event
  { QMI_LOC_EVENT_LOCATION_SERVER_CONNECTION_REQ_IND_V02,
    sizeof(qmiLocEventLocationServerConnectionReqIndMsgT_v02),
    QMI_LOC_EVENT_LOCATION_SERVER_REQUEST_V02 }
};

typedef struct
{
  uint32_t respIndId;
  size_t   respIndSize;
}locClientRespIndTableStructT;

static locClientRespIndTableStructT locClientRespIndTable[]= {

  // get service revision ind
  { QMI_LOC_GET_SERVICE_REVISION_IND_V02,
    sizeof(qmiLocGetServiceRevisionIndMsgT_v02)},

  // Get Fix Criteria Resp Ind
  { QMI_LOC_GET_FIX_CRITERIA_IND_V02,
     sizeof(qmiLocGetFixCriteriaIndMsgT_v02)},

  // NI User Resp In
  { QMI_LOC_NI_USER_RESPONSE_IND_V02,
    sizeof(qmiLocNiUserRespIndMsgT_v02)},

  //Inject Predicted Orbits Data Resp Ind
  { QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_IND_V02,
    sizeof(qmiLocInjectPredictedOrbitsDataIndMsgT_v02)},

  //Get Predicted Orbits Data Src Resp Ind
  { QMI_LOC_GET_PREDICTED_ORBITS_DATA_SOURCE_IND_V02,
    sizeof(qmiLocGetPredictedOrbitsDataSourceIndMsgT_v02)},

  // Get Predicted Orbits Data Validity Resp Ind
   { QMI_LOC_GET_PREDICTED_ORBITS_DATA_VALIDITY_IND_V02,
    sizeof(qmiLocGetPredictedOrbitsDataValidityIndMsgT_v02)},

   // Inject UTC Time Resp Ind
   { QMI_LOC_INJECT_UTC_TIME_IND_V02,
    sizeof(qmiLocInjectUtcTimeIndMsgT_v02)},

   //Inject Position Resp Ind
   { QMI_LOC_INJECT_POSITION_IND_V02,
    sizeof(qmiLocInjectPositionIndMsgT_v02)},

   //Set Engine Lock Resp Ind
   { QMI_LOC_SET_ENGINE_LOCK_IND_V02,
    sizeof(qmiLocSetEngineLockIndMsgT_v02)},

   //Get Engine Lock Resp Ind
   { QMI_LOC_GET_ENGINE_LOCK_IND_V02,
    sizeof(qmiLocGetEngineLockIndMsgT_v02)},

   //Set SBAS Config Resp Ind
   { QMI_LOC_SET_SBAS_CONFIG_IND_V02,
    sizeof(qmiLocSetSbasConfigIndMsgT_v02)},

   //Get SBAS Config Resp Ind
   { QMI_LOC_GET_SBAS_CONFIG_IND_V02,
    sizeof(qmiLocGetSbasConfigIndMsgT_v02)},

   //Set NMEA Types Resp Ind
   { QMI_LOC_SET_NMEA_TYPES_IND_V02,
    sizeof(qmiLocSetNmeaTypesIndMsgT_v02)},

   //Get NMEA Types Resp Ind
   { QMI_LOC_GET_NMEA_TYPES_IND_V02,
    sizeof(qmiLocGetNmeaTypesIndMsgT_v02)},

   //Set Low Power Mode Resp Ind
   { QMI_LOC_SET_LOW_POWER_MODE_IND_V02,
    sizeof(qmiLocSetLowPowerModeIndMsgT_v02)},

   //Get Low Power Mode Resp Ind
   { QMI_LOC_GET_LOW_POWER_MODE_IND_V02,
    sizeof(qmiLocGetLowPowerModeIndMsgT_v02)},

   //Set Server Resp Ind
   { QMI_LOC_SET_SERVER_IND_V02,
    sizeof(qmiLocSetServerIndMsgT_v02)},

   //Get Server Resp Ind
   { QMI_LOC_GET_SERVER_IND_V02,
    sizeof(qmiLocGetServerIndMsgT_v02)},

    //Delete Assist Data Resp Ind
   { QMI_LOC_DELETE_ASSIST_DATA_IND_V02,
    sizeof(qmiLocDeleteAssistDataIndMsgT_v02)},

   //Set XTRA-T Session Control Resp Ind
   { QMI_LOC_SET_XTRA_T_SESSION_CONTROL_IND_V02,
    sizeof(qmiLocSetXtraTSessionControlIndMsgT_v02)},

   //Get XTRA-T Session Control Resp Ind
   { QMI_LOC_GET_XTRA_T_SESSION_CONTROL_IND_V02,
    sizeof(qmiLocGetXtraTSessionControlIndMsgT_v02)},

   //Inject Wifi Position Resp Ind
   { QMI_LOC_INJECT_WIFI_POSITION_IND_V02,
    sizeof(qmiLocInjectWifiPositionIndMsgT_v02)},

   //Notify Wifi Status Resp Ind
   { QMI_LOC_NOTIFY_WIFI_STATUS_IND_V02,
    sizeof(qmiLocNotifyWifiStatusIndMsgT_v02)},

   //Get Registered Events Resp Ind
   { QMI_LOC_GET_REGISTERED_EVENTS_IND_V02,
    sizeof(qmiLocGetRegisteredEventsIndMsgT_v02)},

   //Set Operation Mode Resp Ind
   { QMI_LOC_SET_OPERATION_MODE_IND_V02,
     sizeof(qmiLocSetOperationModeIndMsgT_v02)},

   //Get Operation Mode Resp Ind
   { QMI_LOC_GET_OPERATION_MODE_IND_V02,
     sizeof(qmiLocGetOperationModeIndMsgT_v02)},

   //Set SPI Status Resp Ind
   { QMI_LOC_SET_SPI_STATUS_IND_V02,
     sizeof(qmiLocSetSpiStatusIndMsgT_v02)},

   //Inject Sensor Data Resp Ind
   { QMI_LOC_INJECT_SENSOR_DATA_IND_V02,
     sizeof(qmiLocInjectSensorDataIndMsgT_v02)},

   //Inject Time Sync Data Resp Ind
   { QMI_LOC_INJECT_TIME_SYNC_DATA_IND_V02,
     sizeof(qmiLocInjectTimeSyncDataIndMsgT_v02)},

   //Set Cradle Mount config Resp Ind
   { QMI_LOC_SET_CRADLE_MOUNT_CONFIG_IND_V02,
     sizeof(qmiLocSetCradleMountConfigIndMsgT_v02)},

   //Get Cradle Mount config Resp Ind
   { QMI_LOC_GET_CRADLE_MOUNT_CONFIG_IND_V02,
     sizeof(qmiLocGetCradleMountConfigIndMsgT_v02)},

   //Set External Power config Resp Ind
   { QMI_LOC_SET_EXTERNAL_POWER_CONFIG_IND_V02,
     sizeof(qmiLocSetExternalPowerConfigIndMsgT_v02)},

   //Get External Power config Resp Ind
   { QMI_LOC_GET_EXTERNAL_POWER_CONFIG_IND_V02,
     sizeof(qmiLocGetExternalPowerConfigIndMsgT_v02)},

   //Location server connection status
   { QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02,
     sizeof(qmiLocInformLocationServerConnStatusIndMsgT_v02)}

};



/** whether indication is an event or a response */
typedef enum { eventIndType =0, respIndType = 1 } locClientIndEnumT;


/** @struct locClientInternalState
 */

typedef struct
{
  bool isInitialized;
  qmi_client_type userHandle; //handle for this control point
  locClientHandleType clientHandle;// handle returned to loc API client
  locClientEventIndCbType eventCallback;
  locClientRespIndCbType respCallback;
  locClientEventMaskType eventRegMask;
}locClientInternalState;

// internal state of the Loc Client
static locClientInternalState gLocClientState;

//qmi_cci stuff, need to remove(???)
extern qmi_cci_xport_ops_type ops;
extern qmi_cci_xport_ops_type ipc_router_ops, udp_ops;

/*===========================================================================
 *
 *                          FUNCTION DECLARATION
 *
 *==========================================================================*/

/** locClientGetSizeByRespIndId
 *  @brief this functions gets the size of the response
 *         indication structure, from a specified id
 *  @param [in]  respIndId
 *  @param [out] pRespIndSize
 *  @return true if resp ID was found; else false
*/

bool locClientGetSizeByRespIndId(uint32_t respIndId, size_t *pRespIndSize)
{
  size_t idx = 0, respIndTableSize = 0;
  respIndTableSize = (sizeof(locClientRespIndTable)/sizeof(locClientRespIndTableStructT));
  for(idx=0; idx<respIndTableSize; idx++ )
  {
    if(respIndId == locClientRespIndTable[idx].respIndId)
    {
      // found
      *pRespIndSize = locClientRespIndTable[idx].respIndSize;

      LOC_UTIL_LOGV("%s:%d]: resp ind Id %d size = %d\n", __func__, __LINE__,
                    respIndId, (uint32_t)*pRespIndSize);
      return true;
    }
  }

  //not found
  return false;
}

/** locClientGetSizeByEventIndId
 *  @brief this functions gets the size of the event indication
 *         structure, from a specified id
 *  @param [in]  eventIndId
 *  @param [out] pEventIndSize
 *  @return true if event ID was found; else false
*/
bool locClientGetSizeByEventIndId(uint32_t eventIndId, size_t *pEventIndSize)
{
  size_t idx = 0, eventIndTableSize = 0;

  // look in the event table
  eventIndTableSize =
    (sizeof(locClientEventIndTable)/sizeof(locClientEventIndTableStructT));

  for(idx=0; idx<eventIndTableSize; idx++ )
  {
    if(eventIndId == locClientEventIndTable[idx].eventId)
    {
      // found
      *pEventIndSize = locClientEventIndTable[idx].eventSize;

      LOC_UTIL_LOGV("%s:%d]: event ind Id %d size = %d\n", __func__, __LINE__,
                    eventIndId, (uint32_t)*pEventIndSize);
      return true;
    }
  }
  // not found
  return false;
}

/** locClientGetEventSizeByIndId*/

/** locClientGetSizeAndTypeByIndId
 *  @brief this function gets the size and the type (event,
 *         response)of the indication structure from its ID
 *  @param [in]  indId  ID of the indication
 *  @param [out] type   event or response indication
 *  @param [out] size   size of the indications
 *
 *  @return true if the ID was found, false otherwise */

static bool locClientGetSizeAndTypeByIndId (uint32_t indId, size_t *pIndSize,
                                         locClientIndEnumT *pIndType)
{
  // look in the event table
  if(true == locClientGetSizeByEventIndId(indId, pIndSize))
  {
    *pIndType = eventIndType;

    LOC_UTIL_LOGV("%s:%d]: indId %d is an event size = %d\n", __func__, __LINE__,
                  indId, (uint32_t)*pIndSize);
    return true;
  }

  //else look in response table
  if(true == locClientGetSizeByRespIndId(indId, pIndSize))
  {
    *pIndType = respIndType;

    LOC_UTIL_LOGV("%s:%d]: indId %d is a resp size = %d\n", __func__, __LINE__,
                  indId, (uint32_t)*pIndSize);
    return true;
  }

  // Id not found
  LOC_UTIL_LOGW("%s:%d]: indId %d not found\n", __func__, __LINE__, indId);
  return false;
}

/** isClientRegisteredForEvent
*  @brief checks the mask to identify if the client has
*         registered for the specified event Id
*  @param [in] eventIndId
*  @return true if client regstered for event; else false */

static bool isClientRegisteredForEvent(uint32_t eventIndId)
{
  size_t idx = 0, eventIndTableSize = 0;

  // look in the event table
  eventIndTableSize =
    (sizeof(locClientEventIndTable)/sizeof(locClientEventIndTableStructT));

  for(idx=0; idx<eventIndTableSize; idx++ )
  {
    if(eventIndId == locClientEventIndTable[idx].eventId)
    {
      LOC_UTIL_LOGV("%s:%d]: eventId %d registered mask = %llu, "
                    "eventMask = %llu\n", __func__, __LINE__,
                     eventIndId, gLocClientState.eventRegMask,
                     locClientEventIndTable[idx].eventMask);

      return((gLocClientState.eventRegMask &
              locClientEventIndTable[idx].eventMask)? true:false);
    }

  }
  LOC_UTIL_LOGW("%s:%d]: eventId %d not found\n", __func__, __LINE__,
                 eventIndId);
  return false;
}

/** locClienHandlePosReportInd
 *  @brief Validates a position report ind
 *  @param [in] msg_id
 *  @param [in] ind_buf
 *  @param [in] ind_buf_len
 *  @return true if pos report is valid, false otherwise

*/

static bool locClientHandlePosReportInd
(
 uint32_t        msg_id,
 const void*     *ind_buf,
 uint32_t        ind_buf_len
)
{
  // validate position report
  qmiLocEventPositionReportIndMsgT_v02 *posReport =
    (qmiLocEventPositionReportIndMsgT_v02 *)ind_buf;

  LOC_UTIL_LOGV ("%s:%d]: len = %d lat = %f, lon = %f, alt = %f\n",
                 __func__, __LINE__, ind_buf_len,
                 posReport->latitude, posReport->longitude,
                 posReport->altitudeWrtEllipsoid);

  return true;
}
//-----------------------------------------------------------------------------

/** locClientHandleSatReportInd
 *  @brief Validates a satellite report indication. Dk
 *  @param [in] msg_id
 *  @param [in] ind_buf
 *  @param [in] ind_buf_len
 *  @return true if sat report is valid, false otherwise
*/


static bool locClientHandleSatReportInd
(
 uint32_t        msg_id,
 const void*     *ind_buf,
 uint32_t        ind_buf_len
)
{
  // validate sat reports
   return true;
}

/** locClientHandleNmeaReportInd
 *  @brief Validate a NMEA report indication.
 *  @param [in] msg_id
 *  @param [in] ind_buf
 *  @param [in] ind_buf_len
 *  @return true if nmea report is valid, false otherwise
*/


static bool locClientHandleNmeaReportInd
(
 uint32_t        msg_id,
 const void*     *ind_buf,
 uint32_t        ind_buf_len
)
{

 // validate NMEA report
  return true;
}



/** locClientHandleGetServiceRevisionRespInd
 *  @brief Handles a Get Service Revision Rresponse indication.
 *  @param [in] msg_id
 *  @param [in] ind_buf
 *  @param [in] ind_buf_len
 *  @return true if service revision is valid, false otherwise
*/


static bool locClientHandleGetServiceRevisionRespInd
(
 uint32_t        msg_id,
 const void*     *ind_buf,
 uint32_t        ind_buf_len
)
{
  LOC_UTIL_LOGV("%s:%d] :\n", __func__, __LINE__);
  return true;
}


/** locClientHandleIndication
 *  @brief looks at each indication and calls the appropriate
 *         validation handler
 *  @param [in] indId
 *  @param [in] indBuffer
 *  @param [in] indSize
 *  @return true if indication was validated; else false */

static bool locClientHandleIndication(
  uint32_t        indId,
  void            *indBuffer,
  size_t          indSize
 )
{
  bool status = false;
  switch(indId)
  {
    // handle the event indications
    //-------------------------------------------------------------------------

    // handle position report
    case QMI_LOC_EVENT_POSITION_REPORT_IND_V02:
    {
      status = locClientHandlePosReportInd(indId, indBuffer, indSize);
      break;
    }
    // handle satellite report
    case QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02:
    {
      status = locClientHandleSatReportInd(indId, indBuffer, indSize);
      break;
    }

    // handle NMEA report
    case QMI_LOC_EVENT_NMEA_IND_V02:
    {
      status = locClientHandleNmeaReportInd(indId, indBuffer, indSize);
      break;
    }

    // handle NI Notify Verify Request Ind
    case QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND_V02:
    {
     // locClientHandleNiReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle Time Inject request Ind
    case QMI_LOC_EVENT_TIME_INJECT_REQ_IND_V02:
    {
     // locClientHandleTimeInjectReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle XTRA data Inject request Ind
    case QMI_LOC_EVENT_PREDICTED_ORBITS_INJECT_REQ_IND_V02:
    {
     // locClientHandleXtraInjectReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle position inject request Ind
    case QMI_LOC_EVENT_POSITION_INJECT_REQ_IND_V02:
    {
     // locClientHandlePositionInjectReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle engine state Ind
    case QMI_LOC_EVENT_ENGINE_STATE_IND_V02:
    {
     // locClientHandleEngineStateInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle fix session state Ind
    case QMI_LOC_EVENT_FIX_SESSION_STATE_IND_V02:
    {
     // locClientHandleFixSessionStateInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle Wifi request Ind
    case QMI_LOC_EVENT_WIFI_REQ_IND_V02:
    {
     // locClientHandleWifiReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle sensor streaming ready status Ind
    case QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND_V02:
    {
     // locClientHandleSensorStreamingReadyInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle time sync  Ind
    case QMI_LOC_EVENT_TIME_SYNC_REQ_IND_V02:
    {
     // locClientHandleTimeSyncReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    // handle set streaming report ind
    case QMI_LOC_EVENT_SET_SPI_STREAMING_REPORT_IND_V02:
    {
     // locClientHandleSetSpiStreamingInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    case QMI_LOC_EVENT_LOCATION_SERVER_CONNECTION_REQ_IND_V02:
    {
      //locClientHandleLocServerConnReqInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    //-------------------------------------------------------------------------

    // handle the response indications
    //-------------------------------------------------------------------------

    // Get service Revision response indication
    case QMI_LOC_GET_SERVICE_REVISION_IND_V02:
    {
      status = locClientHandleGetServiceRevisionRespInd(indId,
                                                        indBuffer, indSize);
      break;
    }
    // predicted orbits data response indication
    case QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_IND_V02:
    {
      //locClientHandleInjectPredictedOrbitsDataInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }

    case QMI_LOC_INJECT_SENSOR_DATA_IND_V02 :
    {
      //locClientHandleInjectSensorDataInd(user_handle, msg_id, ind_buf, ind_buf_len);
      status = true;
      break;
    }
    // for indications that only have a "status" field
    case QMI_LOC_NI_USER_RESPONSE_IND_V02:
    case QMI_LOC_INJECT_UTC_TIME_IND_V02:
    case QMI_LOC_INJECT_POSITION_IND_V02:
    case QMI_LOC_SET_ENGINE_LOCK_IND_V02:
    case QMI_LOC_SET_SBAS_CONFIG_IND_V02:
    case QMI_LOC_SET_NMEA_TYPES_IND_V02:
    case QMI_LOC_SET_LOW_POWER_MODE_IND_V02:
    case QMI_LOC_SET_SERVER_IND_V02:
    case QMI_LOC_DELETE_ASSIST_DATA_IND_V02:
    case QMI_LOC_SET_XTRA_T_SESSION_CONTROL_IND_V02:
    case QMI_LOC_INJECT_WIFI_POSITION_IND_V02:
    case QMI_LOC_NOTIFY_WIFI_STATUS_IND_V02:
    case QMI_LOC_SET_OPERATION_MODE_IND_V02:
    case QMI_LOC_SET_SPI_STATUS_IND_V02:
    case QMI_LOC_INJECT_TIME_SYNC_DATA_IND_V02:
    case QMI_LOC_SET_CRADLE_MOUNT_CONFIG_IND_V02:
    case QMI_LOC_SET_EXTERNAL_POWER_CONFIG_IND_V02:
    case QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02:
    {
      status = true;
      break;
    }
    default:
      LOC_UTIL_LOGW("%s:%d]: unknown ind id %d\n", __func__, __LINE__,
                   (uint32_t)indId);
      status = false;
      break;
  }
  return status;
}

/** locClientIndCb
 *  @brief handles the indications sent from the service, if a
 *         response indication was received then the it is sent
 *         to the response callback. If a event indication was
 *         received then it is sent to the event callback
 *  @param [in] user handle
 *  @param [in] msg_id
 *  @param [in] ind_buf
 *  @param [in] ind_buf_len
 *  @param [in] ind_cb_data */

static void locClientIndCb
(
 qmi_client_type                user_handle,
 unsigned long                  msg_id,
 unsigned char                  *ind_buf,
 int                            ind_buf_len,
 void                           *ind_cb_data
)
{
  locClientIndEnumT indType;
  size_t indSize = 0;
  qmi_client_error_type rc ;

  LOC_UTIL_LOGV("%s:%d]: Indication: msg_id=%d buf_len=%d\n",
                __func__, __LINE__, (uint32_t)msg_id, ind_buf_len);

  // if user handle is different from the
  // stored client handle; then drop the message

  if (0 != memcmp(&(gLocClientState.userHandle),&user_handle,
            sizeof(qmi_client_type)))
  {
    LOC_UTIL_LOGE("%s:%d]: handle doesn't match for ind ID %d\n",
                  __func__, __LINE__, (uint32_t)msg_id);
    return;
  }

  // client is not initialized drop the message

  if(false == gLocClientState.isInitialized)
  {
    LOC_UTIL_LOGE("%s:%d]: client not initialized ind ID %d\n",
                   __func__, __LINE__,(uint32_t)msg_id);

    // Get service revision may occur even before the client is
    // initialized
    if(QMI_LOC_GET_SERVICE_REVISION_IND_V02 == msg_id)
    {
      LOC_UTIL_LOGV("%s:%d]: got service rev ind\n", __func__, __LINE__);

      //response indication for get service revision during initialization
      qmiLocGetServiceRevisionIndMsgT_v02 getServiceInd;
      rc = qmi_client_message_decode(user_handle,QMI_IDL_INDICATION, msg_id,
                                     ind_buf, ind_buf_len, &getServiceInd,
                                     sizeof(getServiceInd));
      if( rc == QMI_NO_ERR )
      {
        loc_sync_process_ind(LOC_CLIENT_VALID_HANDLE,
                             msg_id, &getServiceInd);
      }
      else
      {
        LOC_UTIL_LOGE("%s:%d]: Error decoding service revision ind %d\n",
                      __func__, __LINE__, rc);
      }
    }
    return;
  }

  if( true == locClientGetSizeAndTypeByIndId(msg_id, &indSize, &indType))
  {
    void *indBuffer = NULL;

    // if type == event and no eventCallback registered or if the
    // client did not register for this event then just drop it
     if( (eventIndType == indType) &&
        ( (NULL == gLocClientState.eventCallback) ||
          (false == isClientRegisteredForEvent(msg_id)) ) )
    {
       LOC_UTIL_LOGW("%s:%d]: client is not registered for event %d\n",
                     __func__, __LINE__, (uint32_t)msg_id);
       return;
    }

    // decode the indication
    indBuffer = malloc(indSize);

    if(NULL == indBuffer)
    {
      LOC_UTIL_LOGE("%s:%d]: memory allocation failed\n", __func__, __LINE__);
      return;
    }

    rc = qmi_client_message_decode(user_handle,QMI_IDL_INDICATION, msg_id,
                                ind_buf, ind_buf_len, indBuffer, indSize);

    if( rc == QMI_NO_ERR )
    {
      //validate indication
      if (true == locClientHandleIndication(msg_id, indBuffer, indSize))
      {
        if(eventIndType == indType)
        {
          locClientEventIndUnionType eventIndUnion;
          // dummy event
          eventIndUnion.pPositionReportEvent =
            (qmiLocEventPositionReportIndMsgT_v02 *)indBuffer;

          // call the event callback
          gLocClientState.eventCallback(gLocClientState.clientHandle,
                                        msg_id, eventIndUnion);
        }
        else if(respIndType == indType)
        {
          locClientRespIndUnionType respIndUnion;

          // dummy to suppress compiler warnings
          respIndUnion.pDeleteAssistDataInd =
            (qmiLocDeleteAssistDataIndMsgT_v02 *)indBuffer;

          // call the response callback
          gLocClientState.respCallback(gLocClientState.clientHandle, msg_id,
                                       respIndUnion);
        }
      }
      else // error handling idication
      {
        LOC_UTIL_LOGE("%s:%d]: Error handling the indication %d\n",
                      __func__, __LINE__, (uint32_t)msg_id);
      }
    }
    else
    {
      LOC_UTIL_LOGE("%s:%d]: Error decoding indication %d\n",
                    __func__, __LINE__, rc);
    }
    if(indBuffer)
    {
      free (indBuffer);
    }
  }
  else // Id not found
  {
    LOC_UTIL_LOGE("%s:%d]: Error indication not found %d\n",
                  __func__, __LINE__,(uint32_t)msg_id);
  }
  return;
}


/** locClientExchangeRevision
 @brief  Inform the service of the client revision and get the
         service revision.
 @return false on failure true on success
*/

static bool locClientExchangeRevision()
{
  locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType reqUnion;
  qmiLocInformClientRevisionReqMsgT_v02 informClientRev;
  qmiLocGetServiceRevisionIndMsgT_v02 getServiceRevInd;


  memset (&informClientRev, 0, sizeof(informClientRev) );
  memset (&getServiceRevInd, 0, sizeof(getServiceRevInd));

  //TBD: define the revision in locClient global structure
  informClientRev.revision = 0;
  reqUnion.pInformClientRevisionReq = &informClientRev;

  status = locClientSendReq(LOC_CLIENT_VALID_HANDLE,
                            QMI_LOC_INFORM_CLIENT_REVISION_REQ_V02,
                            reqUnion);
  if(eLOC_CLIENT_SUCCESS != status)
  {
    LOC_UTIL_LOGE("%s:%d] inform client revision request error = %d\n",
                  __func__, __LINE__, status);
    return false;
  }


  // initialize the sync_req interface
  loc_sync_req_init();

  //GetServiceRevision request has no payload
  reqUnion.pDeleteAssistDataReq = NULL ;

  status = loc_sync_send_req(LOC_CLIENT_VALID_HANDLE,
                             QMI_LOC_GET_SERVICE_REVISION_REQ_V02,
                             reqUnion, LOC_CLIENT_SYNC_REQ_TIMEOUT,
                             QMI_LOC_GET_SERVICE_REVISION_IND_V02,
                             &getServiceRevInd );

  if (status != eLOC_CLIENT_SUCCESS ||
      eQMI_LOC_SUCCESS_V02 != getServiceRevInd.status)
  {
    LOC_UTIL_LOGW ("%s:%d] Error status = %d, ind.status = %d\n",
                   __func__, __LINE__, status, getServiceRevInd.status);
    // return false; // error
  }
  else
  {
    // got service revision
    LOC_UTIL_LOGD ("%s:%d] Revision Client = %d, Service = %d\n",
                __func__, __LINE__, 0, getServiceRevInd.revision );
  }

  return true;
}

static bool locClientRegisterEventMask(locClientEventMaskType eventRegMask)
{
  locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType reqUnion;
  qmiLocRegEventsReqMsgT_v02 regEventsReq;

  memset(&regEventsReq, 0, sizeof(regEventsReq));

  regEventsReq.eventRegMask = eventRegMask;
  reqUnion.pRegEventsReq = &regEventsReq;

  status = locClientSendReq(LOC_CLIENT_VALID_HANDLE, //use "default" client handle
                            QMI_LOC_REG_EVENTS_REQ_V02,
                            reqUnion);

  if(eLOC_CLIENT_SUCCESS != status )
  {
    LOC_UTIL_LOGE("%s:%d] status %d\n", __func__, __LINE__, status );
    return false;
  }

  return true;
}

/** convertResponseToStatus
 @brief converts a qmiLocGenRespMsgT to locClientStatusEnumType*
 @param [in] pResponse; pointer to the response received from
        QMI_LOC service.
 @return locClientStatusEnumType corresponding to the
         response.
*/

static locClientStatusEnumType convertResponseToStatus(
  qmiLocGenRespMsgT_v02 *pResponse)
{
  locClientStatusEnumType status =  eLOC_CLIENT_FAILURE_INTERNAL;

  // if result == SUCCESS don't look at error code
  if(pResponse->resp.result == QMI_RESULT_SUCCESS )
  {
    status  = eLOC_CLIENT_SUCCESS;
  }
  else
  {
    switch(pResponse->resp.error)
    {
      case QMI_ERR_MALFORMED_MSG:
        status = eLOC_CLIENT_FAILURE_INVALID_PARAMETER;
        break;

      case QMI_ERR_DEVICE_IN_USE:
        status = eLOC_CLIENT_FAILURE_ENGINE_BUSY;
        break;

      default:
        status = eLOC_CLIENT_FAILURE_INTERNAL;
        break;
    }
  }
  LOC_UTIL_LOGV("%s:%d]: result = %d, error = %d, status = %d\n",
                __func__, __LINE__, pResponse->resp.result,
                pResponse->resp.error, status);
  return status;
}

// validates the request
/**  validateRequest
  @brief validates the input request
  @param [in] reqId       request ID
  @param [in] reqPayload  Union of pointers to message payload
  @param [out] ppOutData  Pointer to void *data if successful
  @param [out] pOutLen    Pointer to length of data if succesful.
  @return false on failure, true on Success
*/

static bool validateRequest(
  uint32_t                    reqId,
  const locClientReqUnionType reqPayload,
  void                        **ppOutData,
  uint32_t                    *pOutLen )

{
  bool noPayloadFlag = false;

  LOC_UTIL_LOGV("%s:%d]: reqId = %d\n", __func__, __LINE__, reqId);
  switch(reqId)
  {
    case QMI_LOC_INFORM_CLIENT_REVISION_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInformClientRevisionReqMsgT_v02);
      break;
    }

    case QMI_LOC_REG_EVENTS_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocRegEventsReqMsgT_v02);
       break;
    }

    case QMI_LOC_START_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocStartReqMsgT_v02);
       break;
    }

    case QMI_LOC_STOP_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocStopReqMsgT_v02);
       break;
    }

    case QMI_LOC_NI_USER_RESPONSE_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocNiUserRespReqMsgT_v02);
       break;
    }

    case QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectPredictedOrbitsDataReqMsgT_v02);
      break;
    }

    case QMI_LOC_INJECT_UTC_TIME_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectUtcTimeReqMsgT_v02);
      break;
    }

    case QMI_LOC_INJECT_POSITION_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectPositionReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_ENGINE_LOCK_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetEngineLockReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_SBAS_CONFIG_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetSbasConfigReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_NMEA_TYPES_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetNmeaTypesReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_LOW_POWER_MODE_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetLowPowerModeReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_SERVER_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetServerReqMsgT_v02);
      break;
    }

    case QMI_LOC_DELETE_ASSIST_DATA_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocDeleteAssistDataReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_XTRA_T_SESSION_CONTROL_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetXtraTSessionControlReqMsgT_v02);
      break;
    }

    case QMI_LOC_INJECT_WIFI_POSITION_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectWifiPositionReqMsgT_v02);
      break;
    }

    case QMI_LOC_NOTIFY_WIFI_STATUS_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocNotifyWifiStatusReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_OPERATION_MODE_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetOperationModeReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_SPI_STATUS_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetSpiStatusReqMsgT_v02);
      break;
    }

    case QMI_LOC_INJECT_SENSOR_DATA_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectSensorDataReqMsgT_v02);
      break;
    }

    case QMI_LOC_INJECT_TIME_SYNC_DATA_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInjectTimeSyncDataReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_CRADLE_MOUNT_CONFIG_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetCradleMountConfigReqMsgT_v02);
      break;
    }

    case QMI_LOC_SET_EXTERNAL_POWER_CONFIG_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocSetExternalPowerConfigReqMsgT_v02);
      break;
    }

    case QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_REQ_V02:
    {
      *pOutLen = sizeof(qmiLocInformLocationServerConnStatusReqMsgT_v02);
      break;
    }

    // ALL requests with no payload
    case QMI_LOC_GET_SERVICE_REVISION_REQ_V02:
    case QMI_LOC_GET_FIX_CRITERIA_REQ_V02:
    case QMI_LOC_GET_PREDICTED_ORBITS_DATA_SOURCE_REQ_V02:
    case QMI_LOC_GET_PREDICTED_ORBITS_DATA_VALIDITY_REQ_V02:
    case QMI_LOC_GET_ENGINE_LOCK_REQ_V02:
    case QMI_LOC_GET_SBAS_CONFIG_REQ_V02:
    case QMI_LOC_GET_NMEA_TYPES_REQ_V02:
    case QMI_LOC_GET_LOW_POWER_MODE_REQ_V02:
    case QMI_LOC_GET_SERVER_REQ_V02:
    case QMI_LOC_GET_XTRA_T_SESSION_CONTROL_REQ_V02:
    case QMI_LOC_GET_REGISTERED_EVENTS_REQ_V02:
    case QMI_LOC_GET_OPERATION_MODE_REQ_V02:
    case QMI_LOC_GET_CRADLE_MOUNT_CONFIG_REQ_V02:
    case QMI_LOC_GET_EXTERNAL_POWER_CONFIG_REQ_V02:
    {
      noPayloadFlag = true;
      break;
    }

    default:
      LOC_UTIL_LOGW("%s:%d]: Error unknown reqId=%d\n", __func__, __LINE__,
                    reqId);
      return false;
  }
  if(true == noPayloadFlag)
  {
    *ppOutData = NULL;
    *pOutLen = 0;
  }
  else
  {
    //set dummy pointer for request union
    *ppOutData = (void*) reqPayload.pInformClientRevisionReq;
  }
  LOC_UTIL_LOGV("%s:%d]: reqId=%d, len = %d\n", __func__, __LINE__,
                reqId, *pOutLen);
  return true;
}

/** locClientInit
 @brief Initialize the internal state of the client.
 @param [in]              clnt; the qmi_client_Type is copied
                          for future reference
 @param [in] eventRegMask Mask of asynchronous events the
                          client is interested in receiving
 @param [in] eventIndCb   Function to be invoked to handle
                          an event.
 @param [in] respIndCb    Function to be invoked to handle a
                          response indication.
*/

static bool locClientInit( qmi_client_type clnt,
  locClientEventMaskType eventRegMask,
  locClientEventIndCbType eventIndCb,
  locClientRespIndCbType  respIndCb)
{
  LOC_UTIL_LOGV("%s:%d]: eventMask = %llu\n", __func__, __LINE__,
                eventRegMask);
  gLocClientState.isInitialized = true;
  memcpy(&(gLocClientState.userHandle),&clnt, sizeof(qmi_client_type));
  gLocClientState.clientHandle = LOC_CLIENT_VALID_HANDLE;
  gLocClientState.eventCallback = eventIndCb;
  gLocClientState.respCallback  = respIndCb;
  gLocClientState.eventRegMask  = eventRegMask;
  return true;
}

/** locClientDeInit
 @brief De-Initialize the internal state of the client*
*/

static bool locClientDeInit()
{
  qmi_client_error_type rc = QMI_NO_ERR; //No error
  LOC_UTIL_LOGV("%s:%d]: \n", __func__, __LINE__);
  if(gLocClientState.userHandle)
  {
    // release the handle
    rc = qmi_client_release(gLocClientState.userHandle);
    if( rc != QMI_NO_ERR)
    {
      LOC_UTIL_LOGW("%s:%d]: qmi_client_release error %d\n",
                    __func__, __LINE__, rc);
    }
  }
  gLocClientState.isInitialized = false;
  gLocClientState.clientHandle =  LOC_CLIENT_INVALID_HANDLE_VALUE;
  gLocClientState.userHandle =    NULL ;
  gLocClientState.eventCallback = NULL;
  gLocClientState.respCallback  = NULL;
  gLocClientState.eventRegMask  = 0;
  return (rc == QMI_NO_ERR ? true:false);
}

/** locClientQmiCtrlPointInit
 @brief wait for the service to come up or timeout; when the
        service comes up initialize the control point and set
        internal handle and indication callback.
 @param pQmiClient,
*/

static locClientStatusEnumType locClientQmiCtrlPointInit(qmi_client_type *pQmiClient)
{
  uint32_t num_services, num_entries = 10;
  qmi_client_type clnt, notifier;
  qmi_client_os_params os_params;
  qmi_service_info serviceInfo[5];// num instances of this service
  qmi_client_error_type rc = QMI_NO_ERR; //No error
  uint32_t timeout = 5000;  //5 seconds

  // Get the service object for the qmiLoc Service
  qmi_idl_service_object_type locClientServiceObject =
    qmiLoc_get_service_object_v02();

  // Verify that qmiLoc_get_service_object did not return NULL
  if (NULL == locClientServiceObject)
  {
      LOC_UTIL_LOGE("%s:%d]: qmiLoc_get_service_object_v02 failed\n" ,
                  __func__, __LINE__ );
    return(eLOC_CLIENT_FAILURE_INTERNAL);
  }

//TBD : qmi_cci stuff, need to remove
  LOC_UTIL_LOGV("%s:%d]: starting transport\n", __func__, __LINE__);
#ifdef UDP_XPORT
  qmi_cci_xport_start(&udp_ops, NULL);
#else
  qmi_cci_xport_start(&ipc_router_ops, NULL);
#endif

  // register for service notifications
  rc = qmi_client_notifier_init(locClientServiceObject, &os_params, &notifier);
  if(rc != QMI_NO_ERR)
  {
    LOC_UTIL_LOGE("%s:%d]: qmi_client_notifier_init failed\n",
                  __func__, __LINE__ );
    return(eLOC_CLIENT_FAILURE_INTERNAL);
  }

  // Get location service information, if service is not up wait on a signal
  //TBD: confirm if still need to call get_service_list or if
  //QMI_CCI_OS_SIGNAL returns immediately if the service is already up.

  rc = qmi_client_get_service_list( locClientServiceObject, NULL, NULL,
                                    &num_services);
  LOC_UTIL_LOGV("%s:%d]: qmi_client_get_service_list() returned %d "
                 "num_services = %d\n",  __func__, __LINE__,
                 rc, num_services);

  if(rc != QMI_NO_ERR)
  {
    LOC_UTIL_LOGD("%s:%d]: service not up, waiting for service to come up\n",
                  __func__, __LINE__);

    // wait for the server to come up */
    QMI_CCI_OS_SIGNAL_WAIT(&os_params, timeout);

    if(QMI_CCI_OS_SIGNAL_TIMED_OUT(&os_params))
    {
      // timed out, return with error
      LOC_UTIL_LOGE("%s:%d]: timed out waiting for service\n",
                    __func__, __LINE__);

      return(eLOC_CLIENT_FAILURE_TIMEOUT);
    }
    else
    {
      // get the service addressing information
      rc = qmi_client_get_service_list( locClientServiceObject, NULL, NULL,
                                      &num_services);
      LOC_UTIL_LOGV("%s:%d]: qmi_client_get_service_list() returned %d "
                    "num_services = %d\n", __func__, __LINE__, rc,
                    num_services);

      if(rc != QMI_NO_ERR)
      {
        LOC_UTIL_LOGE("%s:%d]: qmi_client_get_service_list failed even though"
                      "service is up !!!\n", __func__, __LINE__);
        return(eLOC_CLIENT_FAILURE_INTERNAL);
      }
    }
  }

  //num_entries = ??? , num_services = ???
  rc = qmi_client_get_service_list( locClientServiceObject, serviceInfo,
                                    &num_entries, &num_services);

  LOC_UTIL_LOGV("%s:%d]: qmi_client_get_service_list()"
                " returned %d num_entries = %d num_services = %d\n",
                __func__, __LINE__,
                 rc, num_entries, num_services);

  if(rc != QMI_NO_ERR)
  {
    LOC_UTIL_LOGE("%s:%d]: qmi_client_get_service_list Error %d \n",
                  __func__, __LINE__, rc);
    return(eLOC_CLIENT_FAILURE_INTERNAL);
  }

  // initialize the client
  rc = qmi_client_init( &serviceInfo[0], locClientServiceObject,
                         locClientIndCb, NULL, NULL, &clnt);

  if(rc != QMI_NO_ERR)
  {
    LOC_UTIL_LOGE("%s:%d]: qmi_client_init error %d\n",
                  __func__, __LINE__, rc);
    return(eLOC_CLIENT_FAILURE_INTERNAL);
  }

  // success copy the clnt handle returned in qmi_client_init
  memcpy(pQmiClient, &clnt, sizeof(qmi_client_type));
  return(eLOC_CLIENT_SUCCESS);

}
//----------------------- END INTERNAL FUNCTIONS ----------------------------------------

/** locClientOpen
  @brief Connects a location client to the location engine. If the connection
         is successful, returns a handle that the location client uses for
         future location operations.

  @param [in] eventRegMask     Mask of asynchronous events the client is
                               interested in receiving
  @param [in] eventIndCb       Function to be invoked to handle an event.
  @param [in] respIndCb        Function to be invoked to handle a response
                               indication.
  @param [out] locClientHandle Handle to be used by the client
                               for any subsequent requests.

  @return
  One of the following error codes:
  - eLOC_CLIENT_SUCCESS  -– If the connection is opened.
  - non-zero error code(see locClientStatusEnumType) -– On failure.
*/

locClientStatusEnumType locClientOpen (
  locClientEventMaskType       eventRegMask,
  locClientEventIndCbType      eventIndCb,
  locClientRespIndCbType       respIndCb,
  locClientHandleType*   pLocClientHandle)
{
  locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
  qmi_client_type clnt;

  LOC_UTIL_LOGV("%s:%d] \n", __func__, __LINE__);

  do
  {
    if (false == locClientDeInit())
    {
      status = eLOC_CLIENT_FAILURE_INTERNAL;
      break;
    }

    if( NULL == respIndCb || NULL == pLocClientHandle)
    {
      status = eLOC_CLIENT_FAILURE_INVALID_PARAMETER;
      break;
    }

    // Wait for the service or timeout,
    // when service comes up initialize the QMI control point
    status = locClientQmiCtrlPointInit(&clnt);
    LOC_UTIL_LOGV ("%s:%d] locClientQmiCtrlPointInit returned %d\n",
                    __func__, __LINE__, status);

    if(status != eLOC_CLIENT_SUCCESS)
    {
      break;
    }

    // initialize the client structure so that
    // revision and register event messages
    // can be exchanged

    locClientInit(clnt, eventRegMask, eventIndCb, respIndCb);

    // exchange revisions and register event mask
    if( true != locClientExchangeRevision() ||
       true != locClientRegisterEventMask(eventRegMask))
    {
      LOC_UTIL_LOGE("%s:%d]: Error sending initialization messages\n",
                  __func__, __LINE__);
      // release the client
      locClientDeInit();
      status = eLOC_CLIENT_FAILURE_INTERNAL;
      break;
    }

    *pLocClientHandle = gLocClientState.clientHandle;

  }while(0);

  if(eLOC_CLIENT_SUCCESS != status)
  {
    *pLocClientHandle = LOC_CLIENT_INVALID_HANDLE_VALUE;
  }
  LOC_UTIL_LOGD("%s:%d]: returning handle = %d, status = %d\n",
                __func__, __LINE__, *pLocClientHandle, status);
  return(status);
}

/** locClientClose
  @brief Disconnects a client from the location engine.
  @param [in] handle  Handle returned by the locClientOpen()
          function.
  @return
  One of the following error codes:
  - 0 (eLOC_CLIENT_SUCCESS) -– On success.
  - non-zero error code(see locClientStatusEnumType) -– On failure.
*/

locClientStatusEnumType locClientClose(
  locClientHandleType handle)
{
  LOC_UTIL_LOGD("%s:%d]:\n", __func__, __LINE__ );
  if(handle != gLocClientState.clientHandle )
  {
    LOC_UTIL_LOGE("%s:%d]: invalid handle \n",
                  __func__, __LINE__ );
    return(eLOC_CLIENT_FAILURE_INVALID_HANDLE);
  }
  if (false == locClientDeInit() )
  {
    LOC_UTIL_LOGE("%s:%d]: locClientDeInit failed \n",
                  __func__, __LINE__ );
    return eLOC_CLIENT_FAILURE_INTERNAL;
  }
  return eLOC_CLIENT_SUCCESS;
}

/** locClientSendReq
  @brief Sends a message to the location engine. If the locClientSendMsg()
         function is successful, the client should expect an indication
         (except start, stop, event reg and sensor injection messages),
         through the registered callback in the locOpen() function. The
         indication will contain the status of the request and if status is a
         success, indication also contains the payload
         associated with response.
  @param [in] handle Handle returned by the locClientOpen()
              function.
  @param [in] reqId         message ID of the request
  @param [in] reqPayload   Payload of the request, can be NULL
                            if request has no payload

  @return
  One of the following error codes:
  - 0 (eLOC_CLIENT_SUCCESS ) -– On success.
  - non-zero error code (see locClientStatusEnumType) -– On failure.
*/

locClientStatusEnumType locClientSendReq(
  locClientHandleType      handle,
  uint32_t                 reqId,
  locClientReqUnionType    reqPayload )
{
  locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
  qmi_client_error_type rc = QMI_NO_ERR; //No error
  qmiLocGenRespMsgT_v02 resp;
  uint32_t reqLen = 0;
  void *pReqData = NULL;

  // check if client was initialized
  if(false == gLocClientState.isInitialized)
  {
    LOC_UTIL_LOGE("%s:%d] error client not initialized\n", __func__,
                __LINE__);
    return(eLOC_CLIENT_FAILURE_NOT_INITIALIZED);
  }
  // check if handle is the same as is stored internally
  if(handle != gLocClientState.clientHandle)
  {

    LOC_UTIL_LOGE("%s:%d] error invalid handle\n", __func__,
                __LINE__);
    return(eLOC_CLIENT_FAILURE_INVALID_PARAMETER);
  }

  // validate that the request is correct
  if (validateRequest(reqId, reqPayload, &pReqData, &reqLen) == false)
  {

    LOC_UTIL_LOGE("%s:%d] error invalid request\n", __func__,
                __LINE__);
    return(eLOC_CLIENT_FAILURE_INVALID_PARAMETER);
  }

  LOC_UTIL_LOGV("%s:%d] sending reqId= %d, len = %d\n", __func__,
                __LINE__, reqId, reqLen);

  rc = qmi_client_send_msg_sync(gLocClientState.userHandle, reqId, pReqData,
                                reqLen, &resp, sizeof(resp), LOC_CLIENT_ACK_TIMEOUT);
  LOC_UTIL_LOGV("%s:%d] qmi_client_send_msg_sync returned %d\n", __func__,
                __LINE__, rc);

  if (rc != QMI_NO_ERR)
  {
    LOC_UTIL_LOGE("%s:%d]: send_msg_sync error: %d\n",__func__, __LINE__, rc);
    return(eLOC_CLIENT_FAILURE_INTERNAL);
  }
  status = convertResponseToStatus(&resp);
  return(status);
}
/*=============================================================================*/
