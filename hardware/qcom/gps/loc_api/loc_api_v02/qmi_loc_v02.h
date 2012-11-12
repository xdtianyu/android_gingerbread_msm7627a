/* Copyright (c) 2011 Code Aurora Forum. All rights reserved.
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
#ifndef QMILOC_SERVICE_H
#define QMILOC_SERVICE_H

/** @defgroup qmiLoc_qmi_consts Constant values defined in the IDL */
/** @defgroup qmiLoc_qmi_msg_ids Constant values for QMI message IDs */
/** @defgroup qmiLoc_qmi_enums Enumerated types used in QMI messages */
/** @defgroup qmiLoc_qmi_messages Structures sent as QMI messages */
/** @defgroup qmiLoc_qmi_aggregates Aggregate types used in QMI messages */
/** @defgroup qmiLoc_qmi_accessor Accessor for QMI service object */
/** @defgroup qmiLoc_qmi_version Constant values for versioning information */

#include <stdint.h>
#include "qmi_idl_lib.h"
#include "common_v01.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup qmiLoc_qmi_version
    @{
  */
/** Major Version Number of the IDL used to generate this file */
#define QMILOC_V02_IDL_MAJOR_VERS 0x02
/** Revision Number of the IDL used to generate this file */
#define QMILOC_V02_IDL_MINOR_VERS 0x00
/** Major Version Number of the qmi_idl_compiler used to generate this file */
#define QMILOC_V02_IDL_TOOL_VERS 0x02
/** Maximum Defined Message ID */
#define QMILOC_V02_MAX_MESSAGE_ID 0x0034;
/**
    @}
  */


/** @addtogroup qmiLoc_qmi_consts
    @{
  */

/**  Client must enable this mask to receive position report
     event indications.  */
#define QMI_LOC_EVENT_POSITION_REPORT_V02 0x00000001

/**  Client must enable this mask to receive satellite report
     event indications. These reports are sent at a 1 Hz rate.  */
#define QMI_LOC_EVENT_SATELLITE_REPORT_V02 0x00000002

/**  Client must enable this mask to receive NMEA reports for
     position and satellites in view. The report is at a 1 Hz rate.  */
#define QMI_LOC_EVENT_NMEA_POSITION_REPORT_V02 0x00000004

/**  Client must enable this mask to receive NI notify verify request
     event indications.  */
#define QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQUEST_V02 0x00000008

/**  Client must enable this mask to receive position injection request
     event indications.  */
#define QMI_LOC_EVENT_POSITION_INJECTION_REQUEST_V02 0x00000010

/**  Client must enable this mask to receive time injection request
     event indications.  */
#define QMI_LOC_EVENT_TIME_INJECTION_REQUEST_V02 0x00000020

/**  Client must enable this mask to receive predicted orbits request
     event indications.  */
#define QMI_LOC_EVENT_PREDICTED_ORBITS_REQUEST_V02 0x00000040

/**  Client must enable this mask to receive engine state report
     event indications.  */
#define QMI_LOC_EVENT_ENGINE_STATE_REPORT_V02 0x00000080

/**  Client must enable this mask to receive fix session status report
     event indications.  */
#define QMI_LOC_EVENT_FIX_SESSION_STATUS_REPORT_V02 0x00000100

/**  Client must enable this mask to receive WiFi position request
     event indications.  */
#define QMI_LOC_EVENT_WIFI_REQUEST_V02 0x00000200

/**  Client must enable this mask to receive notifications from the
     GPS engine indicating its readiness to accept data from the
     accelerometer (sensor).  */
#define QMI_LOC_EVENT_ACCEL_STREAMING_READY_STATUS_V02 0x00000400

/**  Client must enable this mask to receive notifications from the
     GPS engine indicating its readiness to accept data from the
     gyrometer (sensor).  */
#define QMI_LOC_EVENT_GYRO_STREAMING_READY_STATUS_V02 0x00000800

/**  Client must enable this mask to receive time-sync requests from
     the GPS engine. Time sync enables the GPS engine to synchronize
     its clock with the sensor processor's clock.  */
#define QMI_LOC_EVENT_TIME_SYNC_REQUEST_V02 0x00001000

/**  Client must enable this mask to receive the stationary position
     indicator (SPI) streaming report indications.  */
#define QMI_LOC_EVENT_SPI_STREAMING_REQUEST_V02 0x00002000

/**  Client must enable this mask to receive location server request.
     These requests are generated when the service wishes to establish a
     connection with Location server. */
#define QMI_LOC_EVENT_LOCATION_SERVER_REQUEST_V02 0x00004000

/**  Satellites were used to generate the fix.  */
#define QMI_LOC_POS_TECH_SATELLITE_V02 0x00000001

/**  Cell towers were used to generate the fix.  */
#define QMI_LOC_POS_TECH_CELLID_V02 0x00000002

/**  WiFi access points were used to generate the fix.  */
#define QMI_LOC_POS_TECH_WIFI_V02 0x00000004

/**  Bitmask to specify if an accelerometer was used.  */
#define QMI_LOC_SENSOR_USED_ACCEL_V02 0x00000001

/**  Bitmask to specify if a gyrometer was used.  */
#define QMI_LOC_SENSOR_USED_GYRO_V02 0x00000002

/**  Bitmask to specify if a sensor was used to calculate heading.  */
#define QMI_LOC_SENSOR_AIDING_MASK_HEADING_V02 0x00000001

/**  Bitmask to specify if a sensor was used to calculate speed.  */
#define QMI_LOC_SENSOR_AIDING_MASK_SPEED_V02 0x00000002

/**  Bitmask to specify if a sensor was used to calculate position.  */
#define QMI_LOC_SENSOR_AIDING_MASK_POSITION_V02 0x00000004

/**  Bitmask to specify if a sensor was used to calculate velocity.  */
#define QMI_LOC_SENSOR_AIDING_MASK_VELOCITY_V02 0x00000008

/**  System field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_SYSTEM_V02 0x00000001

/**  PRN field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_PRN_V02 0x00000002

/**  healthStatus field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_HEALTH_STATUS_V02 0x00000004

/**  processStatus field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_PROCESS_STATUS_V02 0x00000008

/**  svInfoMask field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_SVINFO_MASK_V02 0x00000010

/**  Elevation field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_ELEVATION_V02 0x00000020

/**  Azimuth field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_AZIMUTH_V02 0x00000040

/**  SNR field is valid in SV information.  */
#define QMI_LOC_SV_INFO_VALID_SNR_V02 0x00000080

/**  Maximum number of satellites in the satellite report.  */
#define QMI_LOC_SV_INFO_LIST_MAX_SIZE_V02 80

/**  Ephemeris is available for this SV.  */
#define QMI_LOC_SVINFO_MASK_HAS_EPHEMERIS_V02 0x01

/**  Almanac is available for this SV.     */
#define QMI_LOC_SVINFO_MAKS_HAS_ALMANAC_V02 0x02

/**  Maximum NMEA string length.  */
#define QMI_LOC_NMEA_STRING_MAX_LENGTH_V02 200

/**  Maximum length of the requestor ID string.  */
#define QMI_LOC_NI_MAX_REQUESTOR_ID_LENGTH_V02 200

/**  Session ID byte length.  */
#define QMI_LOC_NI_SUPL_SLP_SESSION_ID_BYTE_LENGTH_V02 4

/**  Maximum client name length allowed.  */
#define QMI_LOC_NI_MAX_CLIENT_NAME_LENGTH_V02 64

/**  Horizontal accuracy is valid in the Quality of Position (QoP).  */
#define QMI_LOC_NI_SUPL_QOP_HORZ_ACC_VALID_V02 0x01

/**  Vertical accuracy is valid in the QoP.  */
#define QMI_LOC_NI_SUPL_QOP_VER_ACC_VALID_V02 0x02

/**  Vertical accuracy is valid in the QoP.  */
#define QMI_LOC_NI_SUPL_QOP_MAXAGE_VALID_V02 0x04

/**  Vertical accuracy is valid in the QoP.  */
#define QMI_LOC_NI_SUPL_QOP_DELAY_VALID_V02 0x08

/**  Maximum URL length accepted by location engine.  */
#define QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02 256

/**  IPV6 address length in bytes.  */
#define QMI_LOC_IPV6_ADDR_LENGTH_V02 8

/**  IPV4 server address type.  */
#define QMI_LOC_SERVER_ADDR_TYPE_IPV4_MASK_V02 0x0001

/**  IPV6 server address type.  */
#define QMI_LOC_SERVER_ADDR_TYPE_IPV6_MASK_V02 0x0002

/**  URL server address type.  */
#define QMI_LOC_SERVER_ADDR_TYPE_URL_MASK_V02 0x0004

/**  SUPL hash length.  */
#define QMI_LOC_NI_SUPL_HASH_LENGTH_V02 8

/**  Mask to denote that the server information
    is present in a NI SUPL notify verify request event. This mask is set in
    the "valid_flags" field of a notify verify structure.  */
#define QMI_LOC_SUPL_SERVER_INFO_MASK_V02 0x0001

/**  Mask to denote that the SUPL session id
     is present in a NI SUPL notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.   */
#define QMI_LOC_SUPL_SESSION_ID_MASK_V02 0x0002

/**  Mask to denote that supl hash is present
     in a NI notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.   */
#define QMI_LOC_SUPL_HASH_MASK_V02 0x0004

/**  Mask to denote that position method is present
     in a NI SUPL notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_POS_METHOD_MASK_V02 0x0008

/**  Mask to denote that data coding scheme
     is present in a NI SUPL notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_DATA_CODING_SCHEME_MASK_V02 0x0010

/**  Mask to denote that requestor id
     is present in a NI notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_REQUESTOR_ID_MASK_V02 0x0020

/**  Mask to denote that requestor id
     is present in a NI notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_CLIENT_NAME_MASK_V02 0x0040

/**  Mask to denote that the quality of position
     is present in a NI notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_QOP_MASK_V02 0x0080

/**  Mask to denote that the user response timer
     is present in a NI notify verify request event.
     This mask is set in the "valid_flags" field of a
     notify verify structure.  */
#define QMI_LOC_SUPL_USER_RESP_TIMER_MASK_V02 0x0100

/**  Maximum client address length allowed.  */
#define QMI_LOC_NI_MAX_EXT_CLIENT_ADDRESS_V02 20

/**  Maximum codeword length allowed.  */
#define QMI_LOC_NI_CODEWORD_MAX_LENGTH_V02 20

/**  Mask to denote that invoke id
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_INVOKE_ID_MASK_V02 0x0001

/**  Mask to denote that data coding scheme
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_DATA_CODING_SCHEME_MASK_V02 0x0002

/**  Mask to denote that notification text
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_NOTIFICATION_TEXT_MASK_V02 0x0004

/**  Mask to denote that client address
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_CLIENT_ADDRESS_MASK_V02 0x0008

/**  Mask to denote that location type
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_LOCATION_TYPE_MASK_V02 0x0010

/**  Mask to denote that requestor id
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_REQUESTOR_ID_MASK_V02 0x0020

/**  Mask to denote that codeword string
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_CODEWORD_STRING_MASK_V02 0x0040

/**  Mask to denote that service type
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_SERVICE_TYPE_MASK_V02 0x0080

/**  Mask to denote that the user response timer
     is present in a NI notify verify request event.
     This mask is set in the "valid flags" field of a
     notify verify structure.  */
#define QMI_LOC_UMTS_CP_USER_RESP_TIMER_MASK_V02 0x0100

/**  Maximum number of NTP Servers sent out with this event. */
#define QMI_LOC_MAX_NTP_SERVERS_V02 3

/**  Maximum number of predicted orbits servers supported in the location
     engine.  */
#define QMI_LOC_MAX_PREDICTED_ORBITS_SERVERS_V02 3

/**  Maximum part length that can be injected. The client should
     also look at the maxPartSize field in the predicted orbits injection
     request indication and pick the minimum of the two.   */
#define QMI_LOC_MAX_PREDICTED_ORBITS_PART_LEN_V02 1024

/**  Enable all types. */
#define QMI_LOC_NMEA_MASK_ALL_V02 0xffff

/**  Enable GGA type.  */
#define QMI_LOC_NMEA_MASK_GGA_V02 0x0001

/**  Enable RMC type.  */
#define QMI_LOC_NMEA_MASK_RMC_V02 0x0002

/**  Enable GSV type.  */
#define QMI_LOC_NMEA_MASK_GSV_V02 0x0004

/**  Enable GSA type.  */
#define QMI_LOC_NMEA_MASK_GSA_V02 0x0008

/**  Enable VTG type.  */
#define QMI_LOC_NMEA_MASK_VTG_V02 0x0010

/**  All assistance data is to be deleted.  */
#define QMI_LOC_ASSIST_DATA_MASK_ALL_V02 0xFFFFFFFF

/**  MAC address length in bytes.  */
#define QMI_LOC_WIFI_MAC_ADDR_LENGTH_V02 6

/**  Access point is being used by the WPS.  */
#define QMI_LOC_WIFI_AP_QUALIFIER_BEING_USED_V02 0x1

/**  AP does not broadcast SSID.  */
#define QMI_LOC_WIFI_AP_QUALIFIER_HIDDEN_SSID_V02 0x2

/**  AP has encryption turned on.  */
#define QMI_LOC_WIFI_AP_QUALIFIER_PRIVATE_V02 0x4

/**  AP is in infrastructure mode and not in ad-hoc/unknown mode.  */
#define QMI_LOC_WIFI_AP_QUALIFIER_INFRASTRUCTURE_MODE_V02 0x8

/**  Maximum number of APs that the sender can report.  */
#define QMI_LOC_WIFI_MAX_REPORTED_APS_PER_MSG_V02 50

/**  Bitmask to specify that a sign reversal is required while interpreting
     the sensor data.  */
#define QMI_LOC_SENSOR_DATA_FLAG_SIGN_REVERSAL_V02 0x00000001

/**  Maximum number of samples that can be injected in a TLV.  */
#define QMI_LOC_SENSOR_DATA_MAX_SAMPLES_V02 50

/**  Maximum APN string length allowed.  */
#define QMI_LOC_MAX_APN_NAME_LENGTH_V02 100

/**  Maximum APN profiles supported. */
#define QMI_LOC_MAX_APN_PROFILES_V02 6
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Response Message; Sends a general response (accept or reject). */
typedef struct {

  /* Mandatory */
  /*  Result Code */
  qmi_response_type_v01 resp;
}qmiLocGenRespMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Informs the service of the revision that the client
                    supports. */
typedef struct {

  /* Mandatory */
  uint32_t revision;
  /**<   Revision that the client is using, monotonically increasing.   */
}qmiLocInformClientRevisionReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Registers for asynchronous events generated from the GPS
                    engine. */
typedef struct {

  /* Mandatory */
  uint64_t eventRegMask;
  /**<   Event registration mask (see bitmask fields above prefixed with
       QMI_LOC_EVENT_*). Multiple events can be registered by ORing
       the individual masks and sending them in this TLV. All unused bits in
       this mask must be set to 0.  */
}qmiLocRegEventsReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCFIXRECURRENCEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_RECURRENCE_PERIODIC_V02 = 1, /**<  Request periodic fixes.
 Request just one fix.  */
  eQMI_LOC_RECURRENCE_SINGLE_V02 = 2,
  QMILOCFIXRECURRENCEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocFixRecurrenceEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCACCURACYLEVELENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_ACCURACY_LOW_V02 = 1, /**<  Low accuracy.  */
  eQMI_LOC_ACCURACY_MED_V02 = 2, /**<  Medium accuracy.
 High accuracy.  */
  eQMI_LOC_ACCURACY_HIGH_V02 = 3,
  QMILOCACCURACYLEVELENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocAccuracyLevelEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCPOWERCRITERIAENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_POWER_REQ_LOW_V02 = 1, /**<  Low power is required.
 Low power is not required.  */
  eQMI_LOC_POWER_REQ_HIGH_V02 = 2,
  QMILOCPOWERCRITERIAENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocPowerCriteriaEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCINTERMEDIATEREPORTSTATEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_INTERMEDIATE_REPORTS_ON_V02 = 1, /**<  Intermediate reports are turned on.
 Intermediate reports are turned off.   */
  eQMI_LOC_INTERMEDIATE_REPORTS_OFF_V02 = 2,
  QMILOCINTERMEDIATEREPORTSTATEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocIntermediateReportStateEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Starts a positioning session for this client. */
typedef struct {

  /* Mandatory */
  uint8_t sessionId;
  /**<   ID of the session. This must be reported back in the position
       reports and specified in the stop request. Range: 0..255.  */

  /* Optional */
  uint8_t fixRecurrence_valid;  /**< Must be set to true if fixRecurrence is being passed */
  qmiLocFixRecurrenceEnumT_v02 fixRecurrence;
  /**<   Fix recurrence (SINGLE, PERIODIC). If not specified, recurrence
       defaults to SINGLE.  */

  /* Optional */
  uint8_t horizontalAccuracyLevel_valid;  /**< Must be set to true if horizontalAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 horizontalAccuracyLevel;
  /**<   Horizontal accuracy level:\n
   - HIGH -- Client requires high horizontal accuracy.\n
   - MED  -- Client requires medium horizontal accuracy.\n
   - LOW  -- Client requires low horizontal accuracy.\n
   - None -- If not specified, accuracy defaults to LOW.   */

  /* Optional */
  uint8_t altitudeAccuracyLevel_valid;  /**< Must be set to true if altitudeAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 altitudeAccuracyLevel;
  /**<   Altitude accuracy level:\n
       - HIGH -- Client requires high accuracy altitude fixes\n
       - LOW -- Client does not require high accuracy altitude fixes\n
       - None -- If not specified, altitude accuracy defaults to LOW  */

  /* Optional */
  uint8_t bearingAccuracyLevel_valid;  /**< Must be set to true if bearingAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 bearingAccuracyLevel;
  /**<   Bearing accuracy level:\n
       - HIGH -- Client requires high accuracy bearing \n
       - LOW -- Client does not require high accuracy bearing\n
       - None -- If not specified, bearing accuracy defaults to LOW   */

  /* Optional */
  uint8_t speedAccuracyLevel_valid;  /**< Must be set to true if speedAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 speedAccuracyLevel;
  /**<   Speed accuracy level:\n
       - HIGH -- Client requires high accuracy speed \n
       - LOW -- Client does not require high accuracy speed\n
       - None -- If not specified, speed accuracy defaults to LOW   */

  /* Optional */
  uint8_t powerLevel_valid;  /**< Must be set to true if powerLevel is being passed */
  qmiLocPowerCriteriaEnumT_v02 powerLevel;
  /**<   Power level:\n
       - HIGH -- Client does not require a low power fix\n
       - LOW -- Client requires a low power fix\n
       - None -- If not specified, power level defaults to HIGH   */

  /* Optional */
  uint8_t intermediateReportState_valid;  /**< Must be set to true if intermediateReportState is being passed */
  qmiLocIntermediateReportStateEnumT_v02 intermediateReportState;
  /**<   Intermediate Reports State (ON, OFF).
     Client should explicitly set this field to OFF to stop receiving
     intermediate position reports. Intermediate position reports are
     generated at 1Hz and are ON by default.  If intermediate reports
     are turned ON, the client receives position reports even if the
     accuracy criteria is not met. The status in the position report is
     set to IN_PROGRESS for intermediate reports.  */

  /* Optional */
  uint8_t minInterval_valid;  /**< Must be set to true if minInterval is being passed */
  uint32_t minInterval;
  /**<   Time that must elapse before alerting the client. \n
       Units: milliseconds; Default: 1000 ms  */
}qmiLocStartReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Stops this client's positioning session. */
typedef struct {

  /* Mandatory */
  uint8_t sessionId;
  /**<   ID of the session that was specified in the start request.\n
       Range: 0..255.  */
}qmiLocStopReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSESSIONSTATUSENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SESS_STATUS_SUCCESS_V02 = 0, /**<  Session was successful.       */
  eQMI_LOC_SESS_STATUS_IN_PROGESS_V02 = 1, /**<  Session still in progress. Further position reports will be
       generated until either the fix criteria specified by the client
       are met or the client response timeout occurs.    */
  eQMI_LOC_SESS_STATUS_GENERAL_FAILURE_V02 = 2, /**<  Session failed.   */
  eQMI_LOC_SESS_STATUS_TIMEOUT_V02 = 3, /**<  Fix request failed because the session timed out.       */
  eQMI_LOC_SESS_STATUS_USER_END_V02 = 4, /**<  Fix request failed because the session was ended by the user.       */
  eQMI_LOC_SESS_STATUS_BAD_PARAMETER_V02 = 5, /**<  Fix request failed due to bad parameters in the request.  */
  eQMI_LOC_SESS_STATUS_PHONE_OFFLINE_V02 = 6, /**<  Fix request failed because the phone is offline.
 Fix request failed because the engine is locked.  */
  eQMI_LOC_SESS_STATUS_ENGINE_LOCKED_V02 = 7,
  QMILOCSESSIONSTATUSENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocSessionStatusEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint16_t gpsWeek;
  /**<   Current GPS week as calculated from midnight, Jan. 6, 1980.  */

  uint32_t gpsTimeOfWeekMs;
  /**<   Milliseconds into the current GPS week.  */
}qmiLocGPSTimeStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  float PDOP;
  /**<   Position dilution of position.\n
       Range: 1 (highest accuracy) .. 50 (lowest accuracy)\n
       PDOP = sqrt(HDOP^2 +VDOP^2)  */

  float HDOP;
  /**<   Horizontal dilution of position.\n
       Range: 1 (highest accuracy) .. 50 (lowest accuracy)  */

  float VDOP;
  /**<   Vertical dilution of position.\n
       Range: 1 (highest accuracy) .. 50 (lowest accuracy)  */
}qmiLocDOPStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t usageMask;
  /**<   Specifies which sensors are used.
       Valid masks are specified by the following constants:\n
       - QMI_LOC_SENSOR_USED_ACCEL\n
       - QMI_LOC_SENSOR_USED_GYRO  */

  uint32_t aidingIndicatorMask;
  /**<   Specifies which results are aided by sensors.
       Valid masks are specified by the following constants:\n
       - QMI_LOC_SENSOR_AIDING_MASK_HEADING\n
       - QMI_LOC_SENSOR_AIDING_MASK_SPEED\n
       - QMI_LOC_SENSOR_AIDING_MASK_POSITION\n
       - QMI_LOC_SENSOR_AIDING_MASK_VELOCITY  */
}qmiLocSensorUsageIndicatorStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCTIMESOURCEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_TIME_SRC_INVALID_V02 = 0, /**<  Invalid time.  */
  eQMI_LOC_TIME_SRC_NETWORK_TIME_TRANSFER_V02 = 1, /**<  Time is set by the 1x system.  */
  eQMI_LOC_TIME_SRC_NETWORK_TIME_TAGGING_V02 = 2, /**<  Time is set by WCDMA/GSM time tagging (i.e.,
       associating network time with GPS time).  */
  eQMI_LOC_TIME_SRC_EXTERNAL_INPUT_V02 = 3, /**<  Time is set by an external injection.  */
  eQMI_LOC_TIME_SRC_TOW_DECODE_V02 = 4, /**<  Time is set after decoding over-the-air GPS navigation data
       from one GPS satellite.  */
  eQMI_LOC_TIME_SRC_TOW_CONFIRMED_V02 = 5, /**<  Time is set after decoding over-the-air GPS navigation data
       from multiple satellites.  */
  eQMI_LOC_TIME_SRC_TOW_AND_WEEK_CONFIRMED_V02 = 6, /**<  Both time of the week and the GPS week number are known.  */
  eQMI_LOC_TIME_SRC_NAV_SOLUTION_V02 = 7, /**<  Time is set by the position engine after the fix is obtained.
 Time is set by the position engine after performing SFT.
       This is done when the clock time uncertainty is large.  */
  eQMI_LOC_TIME_SRC_SOLVE_FOR_TIME_V02 = 8,
  QMILOCTIMESOURCEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocTimeSourceEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCRELIABILITYENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_RELIABILITY_NOT_SET_V02 = 0, /**<  Location reliability is not set  */
  eQMI_LOC_RELIABILITY_VERY_LOW_V02 = 1, /**<  Location reliability is very low; use it at your own risk  */
  eQMI_LOC_RELIABILITY_LOW_V02 = 2, /**<  Location reliability is low; little or no cross-checking is possible  */
  eQMI_LOC_RELIABILITY_MEDIUM_V02 = 3, /**<  Location reliability is medium; limited cross-check passed
 Location reliability is high; strong cross-check passed  */
  eQMI_LOC_RELIABILITY_HIGH_V02 = 4,
  QMILOCRELIABILITYENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocReliabilityEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used to send a position report to a client. */
typedef struct {

  /* Mandatory */
  qmiLocSessionStatusEnumT_v02 sessionStatus;
  /**<   Session status   */

  /* Mandatory */
  uint8_t sessionId;
  /**<   ID of the session that was specified in the start request.\n
       Range: 0..255  */

  /* Optional */
  uint8_t latitude_valid;  /**< Must be set to true if latitude is being passed */
  double latitude;
  /**<   Latitude.\n
       Units: degrees; Range: -90.0 .. 90.0\n
       Positive values indicate northern latitude.\n
       Negative values indicate southern latitude.   */

  /* Optional */
  uint8_t longitude_valid;  /**< Must be set to true if longitude is being passed */
  double longitude;
  /**<   Longitude (specified in WGS84 datum).\n
       Units: degrees; Range: -180.0 .. 180.0\n
       Positive values indicate eastern longitude.\n
       Negative values indicate western longitude.  */

  /* Optional */
  uint8_t horUncCircular_valid;  /**< Must be set to true if horUncCircular is being passed */
  float horUncCircular;
  /**<   Horizontal position uncertainty (circular).\n
        Units: meters  */

  /* Optional */
  uint8_t horUncEllipseSemiMinor_valid;  /**< Must be set to true if horUncEllipseSemiMinor is being passed */
  float horUncEllipseSemiMinor;
  /**<   Semi-minor axis of horizontal elliptical uncertainty.\n
        Units: meters  */

  /* Optional */
  uint8_t horUncEllipseSemiMajor_valid;  /**< Must be set to true if horUncEllipseSemiMajor is being passed */
  float horUncEllipseSemiMajor;
  /**<   Semi-major axis of horizontal elliptical uncertainty.\n
        Units: meters  */

  /* Optional */
  uint8_t horUncEllipseOrientAzimuth_valid;  /**< Must be set to true if horUncEllipseOrientAzimuth is being passed */
  float horUncEllipseOrientAzimuth;
  /**<   Elliptical horizontal uncertainty azimuth of orientation.\n
        Units: decimal degrees; Range: 0..180  */

  /* Optional */
  uint8_t horConfidence_valid;  /**< Must be set to true if horConfidence is being passed */
  uint8_t horConfidence;
  /**<   Horizontal uncertainty confidence.\n
        Units: percent; Range: 0..99  */

  /* Optional */
  uint8_t horReliability_valid;  /**< Must be set to true if horReliability is being passed */
  qmiLocReliabilityEnumT_v02 horReliability;
  /**<   Specifies the reliability of the horizontal position.    */

  /* Optional */
  uint8_t speedHorizontal_valid;  /**< Must be set to true if speedHorizontal is being passed */
  float speedHorizontal;
  /**<   Horizontal speed.\n
       Units: meters/second  */

  /* Optional */
  uint8_t speedUnc_valid;  /**< Must be set to true if speedUnc is being passed */
  float speedUnc;
  /**<   Speed uncertainty.\n
       Units: meters/second  */

  /* Optional */
  uint8_t altitudeWrtEllipsoid_valid;  /**< Must be set to true if altitudeWrtEllipsoid is being passed */
  float altitudeWrtEllipsoid;
  /**<   Altitude with respect to the WGS84 ellipsoid.\n
       Units: meters; Range: -500 .. 15883  */

  /* Optional */
  uint8_t altitudeWrtMeanSeaLevel_valid;  /**< Must be set to true if altitudeWrtMeanSeaLevel is being passed */
  float altitudeWrtMeanSeaLevel;
  /**<   Altitude with respect to mean sea level.\n
       Units: meters  */

  /* Optional */
  uint8_t vertUnc_valid;  /**< Must be set to true if vertUnc is being passed */
  float vertUnc;
  /**<   Vertical uncertainty.\n
       Units: meters  */

  /* Optional */
  uint8_t vertConfidence_valid;  /**< Must be set to true if vertConfidence is being passed */
  uint8_t vertConfidence;
  /**<   Vertical uncertainty confidence.\n
       Units: percent; Range: 0..99  */

  /* Optional */
  uint8_t vertReliability_valid;  /**< Must be set to true if vertReliability is being passed */
  qmiLocReliabilityEnumT_v02 vertReliability;
  /**<   Specifies the reliability of the vertical position.   */

  /* Optional */
  uint8_t speedVertical_valid;  /**< Must be set to true if speedVertical is being passed */
  float speedVertical;
  /**<   Vertical speed.\n
         Units: meters/second  */

  /* Optional */
  uint8_t heading_valid;  /**< Must be set to true if heading is being passed */
  float heading;
  /**<   Heading.\n
         Units: degrees; Range: 0..359.999   */

  /* Optional */
  uint8_t headingUnc_valid;  /**< Must be set to true if headingUnc is being passed */
  float headingUnc;
  /**<   Heading uncertainty.\n
       Units: degrees; Range: 0..359.999  */

  /* Optional */
  uint8_t magneticDeviation_valid;  /**< Must be set to true if magneticDeviation is being passed */
  float magneticDeviation;
  /**<   Difference between the bearing to true north and the bearing shown
      on a magnetic compass. The deviation is positive when the magnetic
      north is east of true north.  */

  /* Optional */
  uint8_t technologyMask_valid;  /**< Must be set to true if technologyMask is being passed */
  uint32_t technologyMask;
  /**<   Technologies used in computing this fix. The masks are specified
       through the constants QMI_LOC_POS_TECH_*. Set all unused bits in
       this mask to 0.  */

  /* Optional */
  uint8_t gpsSatelliteUsedMask_valid;  /**< Must be set to true if gpsSatelliteUsedMask is being passed */
  uint32_t gpsSatelliteUsedMask;
  /**<   Bitmask of the GPS satellites used in this fix.  */

  /* Optional */
  uint8_t glonassSatelliteUsedMask_valid;  /**< Must be set to true if glonassSatelliteUsedMask is being passed */
  uint32_t glonassSatelliteUsedMask;
  /**<   Bitmask of the GLONASS satellites used in this fix.  */

  /* Optional */
  uint8_t DOP_valid;  /**< Must be set to true if DOP is being passed */
  qmiLocDOPStructT_v02 DOP;
  /**<   Dilution of precision associated with this position.  */

  /* Optional */
  uint8_t timestampUtc_valid;  /**< Must be set to true if timestampUtc is being passed */
  uint64_t timestampUtc;
  /**<   ETC timeshare. \n
       Units: milliseconds since Jan. 1, 1970  */

  /* Optional */
  uint8_t leapSeconds_valid;  /**< Must be set to true if leapSeconds is being passed */
  uint8_t leapSeconds;
  /**<   Leap second information. If leapSeconds is not available,
         timestamp_utc is calculated based on a hard-coded value
         for leap seconds.
         Units: seconds  */

  /* Optional */
  uint8_t gpsTime_valid;  /**< Must be set to true if gpsTime is being passed */
  qmiLocGPSTimeStructT_v02 gpsTime;
  /**<   GPS time, i.e., the number of weeks since Jan. 5, 1980, and
       milliseconds into the current week.  */

  /* Optional */
  uint8_t timeUnc_valid;  /**< Must be set to true if timeUnc is being passed */
  float timeUnc;
  /**<   Time uncertainty.\n
       Units: milliseconds   */

  /* Optional */
  uint8_t timeSrc_valid;  /**< Must be set to true if timeSrc is being passed */
  qmiLocTimeSourceEnumT_v02 timeSrc;
  /**<   Time source.  */

  /* Optional */
  uint8_t sensorDataUsage_valid;  /**< Must be set to true if sensorDataUsage is being passed */
  qmiLocSensorUsageIndicatorStructT_v02 sensorDataUsage;
  /**<   Whether sensor data was used in computing the position in this
       position report.  */
}qmiLocEventPositionReportIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSVSYSTEMENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SV_SYSTEM_GPS_V02 = 1, /**<  GPS satellite.  */
  eQMI_LOC_SV_SYSTEM_GALILEO_V02 = 2, /**<  GALILEO satellite.  */
  eQMI_LOC_SV_SYSTEM_SBAS_V02 = 3, /**<  SBAS satellite.  */
  eQMI_LOC_SV_SYSTEM_COMPASS_V02 = 4, /**<  COMPASS satellite.
 GLONASS satellite.  */
  eQMI_LOC_SV_SYSTEM_GLONASS_V02 = 5,
  QMILOCSVSYSTEMENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocSvSystemEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSVSTATUSENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SV_STATUS_IDLE_V02 = 1, /**<  SV is not being actively processed.  */
  eQMI_LOC_SV_STATUS_SEARCH_V02 = 2, /**<  The system is searching for this SV.
 SV is being tracked.  */
  eQMI_LOC_SV_STATUS_TRACK_V02 = 3,
  QMILOCSVSTATUSENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocSvStatusEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t validMask;
  /**<   Bitmask of field validities.  */

  qmiLocSvSystemEnumT_v02 system;
  /**<   Indicates which constellation this SV belongs to.  */

  uint8_t prn;
  /**<   SV ID.\n
         Range: 0..255  */

  uint8_t healthStatus;
  /**<   Health status. \n
         Range: 0 == Unhealthy; 1 == Healthy  */

  qmiLocSvStatusEnumT_v02 svStatus;
  /**<   SV processing status: IDLE, SEARCHING, or TRACKING.  */

  uint8_t svInfoMask;
  /**<   Whether almanac and ephemeris information are available.  */

  float elevation;
  /**<   SV elevation angle.\n
         Units: degrees; Range: 0..90  */

  float azimuth;
  /**<   SV azimuth angle.\n
          Units: degrees; Range: 0..360  */

  float snr;
  /**<   SV Signal to Noise Ratio\n
         Units: dB-Hz  */
}qmiLocSvInfoStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used to send a satellite report. */
typedef struct {

  /* Mandatory */
  uint8_t altitudeAssumed;
  /**<   Altitude assumed or calculated:\n
         - TRUE -- Valid altitude is assumed; there may not be enough
                   satellites to determine precise altitude\n
         - FALSE -- Valid altitude is calculated   */

  /* Optional */
  uint8_t svList_valid;  /**< Must be set to true if svList is being passed */
  uint32_t svList_len;  /**< Must be set to # of elements in svList */
  qmiLocSvInfoStructT_v02 svList[QMI_LOC_SV_INFO_LIST_MAX_SIZE_V02];
  /**<   SV information list.  */
}qmiLocEventGnssSvInfoIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used to send NMEA strings to the client. */
typedef struct {

  /* Mandatory */
  char nmea[QMI_LOC_NMEA_STRING_MAX_LENGTH_V02 + 1];
  /**<   NMEA string.  */
}qmiLocEventNmeaIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNINOTIFYVERIFYENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_USER_NO_NOTIFY_NO_VERIFY_V02 = 1, /**<  No notification and no verification required.  */
  eQMI_LOC_NI_USER_NOTIFY_ONLY_V02 = 2, /**<  Notify only; no verification required.  */
  eQMI_LOC_NI_USER_NOTIFY_VERIFY_ALLOW_NO_RESP_V02 = 3, /**<  Notify and verify, but no response required.  */
  eQMI_LOC_NI_USER_NOTIFY_VERIFY_NOT_ALLOW_NO_RESP_V02 = 4, /**<  Notify and verify, and require a response.
 Notify and Verify, and require a response.  */
  eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02 = 5,
  QMILOCNINOTIFYVERIFYENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiNotifyVerifyEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNIVXPOSMODEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_VX_MS_ASSISTED_ONLY_V02 = 1, /**<  MS-assisted only allowed.  */
  eQMI_LOC_NI_VX_MS_BASED_ONLY_V02 = 2, /**<  MS-based only allowed.  */
  eQMI_LOC_NI_VX_MS_ASSISTED_PREFERRED_MS_BASED_ALLOWED_V02 = 3, /**<  MS-assisted preferred, but MS-based allowed.
 MS-based preferred, but MS-assisted allowed.  */
  eQMI_LOC_NI_VX_MS_BASED_PREFERRED_MS_ASSISTED_ALLOWED_V02 = 4,
  QMILOCNIVXPOSMODEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiVxPosModeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNIVXREQUESTORIDENCODINGSCHEMEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_VX_OCTET_V02 = 0, /**<  Encoding is OCTET.  */
  eQMI_LOC_NI_VX_EXN_PROTOCOL_MSG_V02 = 1, /**<  Encoding is EXN PROTOCOL MSG.  */
  eQMI_LOC_NI_VX_ASCII_V02 = 2, /**<  Encoding is ASCII.  */
  eQMI_LOC_NI_VX_IA5_V02 = 3, /**<  Encoding is IA5.  */
  eQMI_LOC_NI_VX_UNICODE_V02 = 4, /**<  Encoding is UNICODE.  */
  eQMI_LOC_NI_VX_SHIFT_JIS_V02 = 5, /**<  Encoding is SHIFT JIS.  */
  eQMI_LOC_NI_VX_KOREAN_V02 = 6, /**<  Encoding is KOREAN.  */
  eQMI_LOC_NI_VX_LATIN_HEBREW_V02 = 7, /**<  Encoding is LATIN HEBREW.  */
  eQMI_LOC_NI_VX_LATIN_V02 = 8, /**<  Encoding is LATIN.
 Encoding is GSM.  */
  eQMI_LOC_NI_VX_GSM_V02 = 9,
  QMILOCNIVXREQUESTORIDENCODINGSCHEMEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiVxRequestorIdEncodingSchemeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint8_t posQosIncl;
  /**<   Whether quality of service is included:\n
         - TRUE --  QoS included\n
       - FALSE -- QoS not included  */

  uint8_t posQos;
  /**<   Position QoS timeout. \n
         Units: seconds  */

  uint32_t numFixes;
  /**<   Number of fixes allowed.  */

  uint32_t timeBetweenFixes;
  /**<   Time between fixes.\n
         Units: seconds  */

  qmiLocNiVxPosModeEnumT_v02 posMode;
  /**<   Position mode.  */

  qmiLocNiVxRequestorIdEncodingSchemeEnumT_v02 encodingScheme;
  /**<   VX encoding scheme.  */

  char requestorId[QMI_LOC_NI_MAX_REQUESTOR_ID_LENGTH_V02 + 1];

  uint16_t userRespTimerInSeconds;
  /**<   Number of seconds to wait for the user to respond.  */
}qmiLocNiVxNotifyVerifyStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNISUPLPOSMETHODENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_SUPL_POSMETHOD_AGPS_SETASSISTED_V02 = 1, /**<  Set assisted.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_AGPS_SETBASED_V02 = 2, /**<  Set based.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_AGPS_SETASSISTED_PREF_V02 = 3, /**<  Set assisted preferred.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_AGPS_SETBASED_PREF_V02 = 4, /**<  Set based preferred.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_AUTONOMOUS_GPS_V02 = 5, /**<  Standalone GPS.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_AFLT_V02 = 6, /**<  Advanced forward link trilateration.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_ECID_V02 = 7, /**<  Exclusive chip ID.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_EOTD_V02 = 8, /**<  Enhnaced observed time difference.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_OTDOA_V02 = 9, /**<  Observed time delay of arrival.
 No position.  */
  eQMI_LOC_NI_SUPL_POSMETHOD_NO_POSITION_V02 = 10,
  QMILOCNISUPLPOSMETHODENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiSuplPosMethodEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNIDATACODINGSCHEMEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_SS_GERMAN_V02 = 12, /**<  Language is German.  */
  eQMI_LOC_NI_SS_ENGLISH_V02 = 13, /**<  Language is English.  */
  eQMI_LOC_NI_SS_ITALIAN_V02 = 14, /**<  Language is Italian.  */
  eQMI_LOC_NI_SS_FRENCH_V02 = 15, /**<  Language is French.  */
  eQMI_LOC_NI_SS_SPANISH_V02 = 16, /**<  Language is Spanish.  */
  eQMI_LOC_NI_SS_DUTCH_V02 = 17, /**<  Language is Dutch.   */
  eQMI_LOC_NI_SS_SWEDISH_V02 = 18, /**<  Language is Swedish.  */
  eQMI_LOC_NI_SS_DANISH_V02 = 19, /**<  Language is Danish.  */
  eQMI_LOC_NI_SS_PORTUGUESE_V02 = 20, /**<  Language is Portuguese.  */
  eQMI_LOC_NI_SS_FINNISH_V02 = 21, /**<  Language is Finnish.  */
  eQMI_LOC_NI_SS_NORWEGIAN_V02 = 22, /**<  Language is Norwegian.  */
  eQMI_LOC_NI_SS_GREEK_V02 = 23, /**<  Language is Greek.  */
  eQMI_LOC_NI_SS_TURKISH_V02 = 24, /**<  Language is Turkish.  */
  eQMI_LOC_NI_SS_HUNGARIAN_V02 = 25, /**<  Language is Hungarian.  */
  eQMI_LOC_NI_SS_POLISH_V02 = 26, /**<  Language is Polish.  */
  eQMI_LOC_NI_SS_LANGUAGE_UNSPEC_V02 = 27, /**<  Language is unspecified.  */
  eQMI_LOC_NI_SUPL_UTF8_V02 = 28, /**<  Encoding is UTF 8.  */
  eQMI_LOC_NI_SUPL_UCS2_V02 = 29, /**<  Encoding is UCS 2.
 Encoding is GSM default.  */
  eQMI_LOC_NI_SUPL_GSM_DEFAULT_V02 = 30,
  QMILOCNIDATACODINGSCHEMEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiDataCodingSchemeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNISUPLFORMATENUMTYPE_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_SUPL_FORMAT_LOGICAL_NAME_V02 = 0, /**<  SUPL logical name format.  */
  eQMI_LOC_NI_SUPL_FORMAT_EMAIL_ADDRESS_V02 = 1, /**<  SUPL email address format.  */
  eQMI_LOC_NI_SUPL_FORMAT_MSISDN_V02 = 2, /**<  SUPL logical name format.  */
  eQMI_LOC_NI_SUPL_FORMAT_URL_V02 = 3, /**<  SUPL URL format.  */
  eQMI_LOC_NI_SUPL_FORMAT_SIP_URL_V02 = 4, /**<  SUPL SIP URL format.  */
  eQMI_LOC_NI_SUPL_FORMAT_MIN_V02 = 5, /**<  SUPL MIN format.  */
  eQMI_LOC_NI_SUPL_FORMAT_MDN_V02 = 6, /**<  SUPL MDN format.
 SUPL unknown format.  */
  eQMI_LOC_NI_SUPL_FORMAT_OSS_UNKNOWN_V02 = 2147483647,
  QMILOCNISUPLFORMATENUMTYPE_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiSuplFormatEnumType_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocNiSuplFormatEnumType_v02 formatType;
  /**<   Format of the stringVal. The list of applicable formats is specified
        in constants QMI_LOC_NI_SUPL_FORMAT_*.   */

  char formattedString[QMI_LOC_NI_MAX_CLIENT_NAME_LENGTH_V02 + 1];
}qmiLocNiSuplFormattedStringStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint8_t validMask;
  /**<   Bit field indicating which fields are valid in this struct.  */

  uint8_t horizontalAccuracy;
  /**<   Horizontal accuracy in meters.  */

  uint8_t verticalAccuracy;
  /**<   Vertical accuracy in meters.  */

  uint16_t maxLocAge;
  /**<   Maximum age of the location if the engine sends a previously
        computed position.  */

  uint8_t delay;
  /**<   Delay the server is willing to tolerate for the fix.
        Units: seconds  */
}qmiLocNiSuplQopStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSERVERTYPEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SERVER_TYPE_CDMA_PDE_V02 = 1, /**<  Server type is CDMA PDE.  */
  eQMI_LOC_SERVER_TYPE_CDMA_MPC_V02 = 2, /**<  Server type is CDMA MPC.  */
  eQMI_LOC_SERVER_TYPE_UMTS_SLP_V02 = 3, /**<  Server type is UMTS SLP.
 Server type is custom PDE.  */
  eQMI_LOC_SERVER_TYPE_CUSTOM_PDE_V02 = 4,
  QMILOCSERVERTYPEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocServerTypeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t addr;
  /**<   IPV4 address.  */

  uint16_t port;
  /**<   IPV4 Port.  */
}qmiLocIpV4AddrStructType_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint16_t addr[QMI_LOC_IPV6_ADDR_LENGTH_V02];
  /**<   IPV6 address */

  uint32_t port;
  /**<   IPV6 port  */
}qmiLocIpV6AddrStructType_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocServerTypeEnumT_v02 serverType;
  /**<   Type of server, as defined in qmiLocServerTypeEnumT.  */

  uint8_t suplServerAddrTypeMask;
  /**<   IPV4, IPV6, URL.  */

  qmiLocIpV4AddrStructType_v02 ipv4Addr;
  /**<   IPV4 address and port.  */

  qmiLocIpV6AddrStructType_v02 ipv6Addr;
  /**<   IPV6 address and port.  */

  char urlAddr[QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02 + 1];
  /**<   URL.  */
}qmiLocNiSuplServerInfoStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t valid_flags;
  /**<   Indicates which of the following are present.  */

  qmiLocNiSuplServerInfoStructT_v02 suplServerInfo;
  /**<   SUPL Server Information.  */

  uint8_t suplSessionId[QMI_LOC_NI_SUPL_SLP_SESSION_ID_BYTE_LENGTH_V02];
  /**<   SUPL session ID.  */

  uint8_t suplHash[QMI_LOC_NI_SUPL_HASH_LENGTH_V02];
  /**<   Hash for SUPL_INIT; used to validate that the message was not
       corrupted.  */

  qmiLocNiSuplPosMethodEnumT_v02 posMethod;
  /**<   The GPS mode to be used for the fix.  */

  qmiLocNiDataCodingSchemeEnumT_v02 dataCodingScheme;
  /**<   dataCoding_scheme applies to both requestor_id and client_name.   */

  qmiLocNiSuplFormattedStringStructT_v02 requestorId;
  /**<   Requestor ID. The encoding scheme for requestor_id is specified in
       the dataCodingScheme field.  */

  qmiLocNiSuplFormattedStringStructT_v02 clientName;
  /**<   Client name. The encoding scheme for client_name is specified in
       the dataCodingScheme field.  */

  qmiLocNiSuplQopStructT_v02 suplQop;
  /**<   SUPL QoP.  */

  uint16_t userResponseTimer;
  /**<   Number of seconds to wait for the user to respond.  */
}qmiLocNiSuplNotifyVerifyStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNILOCATIONTYPEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_LOCATIONTYPE_CURRENT_LOCATION_V02 = 1, /**<  Current location.  */
  eQMI_LOC_NI_LOCATIONTYPE_CURRENT_OR_LAST_KNOWN_LOCATION_V02 = 2, /**<  Last known location; may be current location.
 Initial location.  */
  eQMI_LOC_NI_LOCATIONTYPE_INITIAL_LOCATION_V02 = 3,
  QMILOCNILOCATIONTYPEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiLocationTypeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocNiDataCodingSchemeEnumT_v02 dataCodingScheme;
  /**<   One of the values in qmiLocNiDataCodingSchemeEnumT.  */

  char codedString[QMI_LOC_NI_CODEWORD_MAX_LENGTH_V02 + 1];
  /**<   Coded string.  */
}qmiLocNiUmtsCpCodedStringStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint16_t valid_flags;
  /**<   Which of the following fields are valid.  */

  uint8_t invokeId;
  /**<   Supplementary Services invoke ID.  */

  qmiLocNiDataCodingSchemeEnumT_v02 dataCodingScheme;
  /**<   Type of data encoding scheme for the text.
       Applies to both notification_text and client address.   */

  char notificationText[QMI_LOC_NI_MAX_CLIENT_NAME_LENGTH_V02 + 1];
  /**<   Notification text; the encoding method is specified in
       dataCoding_scheme.  */

  char clientAddress[QMI_LOC_NI_MAX_EXT_CLIENT_ADDRESS_V02 + 1];
  /**<   Client address; the encoding method is specified in
       dataCoding_scheme.  */

  qmiLocNiLocationTypeEnumT_v02 locationType;
  /**<   Location type.  */

  qmiLocNiUmtsCpCodedStringStructT_v02 requestorId;
  /**<   Requestor ID; the encoding method is specified in the
       qmiLocNiUmtsCpCodedStringStructT.dataCodingScheme field.  */

  qmiLocNiUmtsCpCodedStringStructT_v02 codewordString;
  /**<   Codeword string; the encoding method is specified in the
       qmiLocNiUmtsCpCodedStringStructT.dataCodingScheme field.  */

  uint8_t lcsServiceTypeId;
  /**<   Service type ID.  */

  uint16_t userResponseTimer;
  /**<   Number of seconds to wait for the user to respond.  */
}qmiLocNiUmtsCpNotifyVerifyStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNISERVICEINTERACTIONENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_SERVICE_INTERACTION_ONGOING_NI_INCOMING_MO_V02 = 1, /**<  Service interaction between ongoing NI and incoming MO sessions.  */
  QMILOCNISERVICEINTERACTIONENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiServiceInteractionEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocNiVxNotifyVerifyStructT_v02 niVxReq;
  /**<   Ongoing NI session request; this information is currently not filled.  */

  qmiLocNiServiceInteractionEnumT_v02 serviceInteractionType;
  /**<   Service interaction type specified in qmiLocNiServiceInteractionEnumT.  */
}qmiLocNiVxServiceInteractionStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Indicates an NI notify/verify request to the client. */
typedef struct {

  /* Mandatory */
  qmiLocNiNotifyVerifyEnumT_v02 notificationType;
  /**<   Type of notification/verification to be performed.  */

  /* Optional */
  uint8_t NiVxInd_valid;  /**< Must be set to true if NiVxInd is being passed */
  qmiLocNiVxNotifyVerifyStructT_v02 NiVxInd;
  /**<   Optional NI Vx request payload.  */

  /* Optional */
  uint8_t NiSuplInd_valid;  /**< Must be set to true if NiSuplInd is being passed */
  qmiLocNiSuplNotifyVerifyStructT_v02 NiSuplInd;
  /**<   Optional NI SUPL request payload.  */

  /* Optional */
  uint8_t NiUmtsCpInd_valid;  /**< Must be set to true if NiUmtsCpInd is being passed */
  qmiLocNiUmtsCpNotifyVerifyStructT_v02 NiUmtsCpInd;
  /**<   Optional NI UMTS-CP request payload.  */

  /* Optional */
  uint8_t NiVxServiceInteractionInd_valid;  /**< Must be set to true if NiVxServiceInteractionInd is being passed */
  qmiLocNiVxServiceInteractionStructT_v02 NiVxServiceInteractionInd;
  /**<   Optional NI service interaction payload.  */
}qmiLocEventNiNotifyVerifyReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  char serverUrl[QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02 + 1];
  /**<   Assistance Server URL  */
}qmiLocAssistanceServerUrlStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t delayThreshold;
  /**<   The time server should be skipped if one way delay to the server
       exceeds this threshold. \n
       Units: milliseconds  */

  uint32_t timeServerList_len;  /**< Must be set to # of elements in timeServerList */
  qmiLocAssistanceServerUrlStructT_v02 timeServerList[QMI_LOC_MAX_NTP_SERVERS_V02];
  /**<   List of Time Server URL's that are recommended by the service for time
       information, the list is ordered, the client should use the first
       server specified in the list as the primary URL to fetch NTP time,
       the 2nd one as secondary, and so on. */
}qmiLocTimeServerListStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Requests the client to inject time information. */
typedef struct {

  /* Optional */
  uint8_t timeUtc_valid;  /**< Must be set to true if timeUtc is being passed */
  uint64_t timeUtc;
  /**<   Engine's estimate of UTC time. \n
       Units: milliseconds  */

  /* Optional */
  uint8_t timeUnc_valid;  /**< Must be set to true if timeUnc is being passed */
  uint32_t timeUnc;
  /**<   Engine's time uncertainty.\n
       Units: milliseconds  */

  /* Optional */
  uint8_t timeServerInfo_valid;  /**< Must be set to true if timeServerInfo is being passed */
  qmiLocTimeServerListStructT_v02 timeServerInfo;
  /**<   Contains information about the time servers recommended by the
       location service for NTP time.  */
}qmiLocEventTimeInjectReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t maxFileSizeInBytes;
  /**<   Maximum allowable predicted orbits file size (in bytes).  */

  uint32_t maxPartSize;
  /**<   Maximum allowable predicted orbits file chunk size (in bytes).  */
}qmiLocPredictedOrbitsAllowedSizesStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t serverList_len;  /**< Must be set to # of elements in serverList */
  qmiLocAssistanceServerUrlStructT_v02 serverList[QMI_LOC_MAX_PREDICTED_ORBITS_SERVERS_V02];
  /**<   List of predicted orbits URLs. The list is ordered, so the client
       must use the first server specified in the list as the primary URL
       from which to download predicted orbits data, the second one as
       secondary, and so on.  */
}qmiLocPredictedOrbitsServerListStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Requests the client to inject predicted orbits data. */
typedef struct {

  /* Mandatory */
  qmiLocPredictedOrbitsAllowedSizesStructT_v02 allowedSizes;
  /**<   Maximum part and file size allowed to be injected in the engine.  */

  /* Optional */
  uint8_t serverList_valid;  /**< Must be set to true if serverList is being passed */
  qmiLocPredictedOrbitsServerListStructT_v02 serverList;
  /**<   List of servers that can be used by the client to download
       predicted orbits data.  */
}qmiLocEventPredictedOrbitsInjectReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Requests the client to inject a position. */
typedef struct {

  /* Mandatory */
  double latitude;
  /**<   Latitude.\n
       Units: degrees; Range: -90.0 .. 90.0\n
       Positive values indicate northern latitude.\n
       Negative values indicate southern latitude.  */

  /* Mandatory */
  double longitude;
  /**<   Longitude (specified in WGS84 datum).\n
       Units: degrees; Range: -180.0 .. 180.0\n
       Positive values indicate eastern longitude.\n
       Negative values indicate western longitude.  */

  /* Mandatory */
  float horUncCircular;
  /**<   Horizontal position uncertainty (circular).\n
       Units: meters  */

  /* Mandatory */
  uint64_t timestampUtc;
  /**<   UTC timestamp.\n
       Units: milliseconds since Jan. 1, 1970  */

  /* Optional */
  uint8_t leapSeconds_valid;  /**< Must be set to true if leapSeconds is being passed */
  uint8_t leapSeconds;
  /**<   Leap second information. If leapSeconds is not available,
       timestamp_utc is calculated based on the hard-coded
       value for leap seconds.\n
       Units: seconds  */

  /* Optional */
  uint8_t gpsTime_valid;  /**< Must be set to true if gpsTime is being passed */
  qmiLocGPSTimeStructT_v02 gpsTime;
  /**<   GPS time, i.e., the number of weeks since Jan. 5, 1980, and
       milliseconds into the current week.  */
}qmiLocEventPositionInjectionReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCENGINESTATEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_ENGINE_STATE_ON_V02 = 1, /**<  Location engine is on.
 Location engine is off.  */
  eQMI_LOC_ENGINE_STATE_OFF_V02 = 2,
  QMILOCENGINESTATEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocEngineStateEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sends the engine state to the client. */
typedef struct {

  /* Mandatory */
  qmiLocEngineStateEnumT_v02 engineState;
  /**<   Location engine state.  */
}qmiLocEventEngineStateIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCFIXSESSIONSTATEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_FIX_SESSION_STARTED_V02 = 1, /**<  Location fix session has started.
 Location fix session has ended.  */
  eQMI_LOC_FIX_SESSION_FINISHED_V02 = 2,
  QMILOCFIXSESSIONSTATEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocFixSessionStateEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sends the fix session state to the client. */
typedef struct {

  /* Mandatory */
  qmiLocFixSessionStateEnumT_v02 sessionState;
  /**<   LOC fix session state.  */

  /* Optional */
  uint8_t sessionId_valid;  /**< Must be set to true if sessionId is being passed */
  uint8_t sessionId;
  /**<   ID of the session that was specified in the start request.
    This may not be specified for a fix session corresponding to
    a Network initiated request.
    Range 0..255  */
}qmiLocEventFixSessionStateIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCWIFIREQUESTENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_WIFI_START_PERIODIC_HI_FREQ_FIXES_V02 = 0, /**<  Start periodic fixes with high frequency.  */
  eQMI_LOC_WIFI_START_PERIODIC_KEEP_WARM_V02 = 1, /**<  Keep warm for low frequency fixes without data downloads.
 Stop periodic fixes request.  */
  eQMI_LOC_WIFI_STOP_PERIODIC_FIXES_V02 = 2,
  QMILOCWIFIREQUESTENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocWifiRequestEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sends a WiFi request to the client. */
typedef struct {

  /* Mandatory */
  qmiLocWifiRequestEnumT_v02 requestType;
  /**<   Request type as specified in qmiWifiRequestEnumT.  */

  /* Optional */
  uint8_t tbfInMs_valid;  /**< Must be set to true if tbfInMs is being passed */
  uint16_t tbfInMs;
  /**<   Time between fixes for a periodic request.\n
        Units: milliseconds  */
}qmiLocEventWifiRequestIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Notifies clients if the GPS engine is
                    ready to accept sensor data. */
typedef struct {

  /* Optional */
  uint8_t accelReady_valid;  /**< Must be set to true if accelReady is being passed */
  uint8_t accelReady;
  /**<   Whether the GPS engine is ready to accept accelerometer sensor data:\n
         - TRUE -- GPS engine is ready to accept accelerometer sensor data.\n
         - FALSE -- GPS engine is not ready to accept accelerometer sensor data.  */

  /* Optional */
  uint8_t gyroReady_valid;  /**< Must be set to true if gyroReady is being passed */
  uint8_t gyroReady;
  /**<   Whether the GPS engine is ready to accept gyrometer sensor data:\n
         - TRUE -- GPS engine is ready to accept gyrometer sensor data.\n
         - FALSE -- GPS engine is not ready to accept gyrometer sensor data.  */
}qmiLocEventSensorStreamingReadyStatusIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Notifies clients to inject time synchronization data.  */
typedef struct {

  /* Mandatory */
  uint32_t refCounter;
  /**<   This message is sent to registered control points. It is sent by
        the GPS engine when it needs to synchronize GPS and control
        point (sensor processor) times.
        This TLV must be echoed back in the Time Sync Inject message.  */
}qmiLocEventTimeSyncReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Requests the client to enable SPI streaming reports.  */
typedef struct {

  /* Mandatory */
  uint8_t enable;
  /**<   Whether the client is to start or stop sending an SPI status stream.\n
       - TRUE -- Client is to start sending an SPI status stream.\n
       - FALSE -- Client is to stop sending an SPI status stream.  */
}qmiLocEventSetSpiStreamingReportIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSERVERPROTOCOLENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SERVER_PROTOCOL_DEFAULT_V02 = 0, /**<  Use default location server protocol  */
  eQMI_LOC_SERVER_PROTOCOL_SUPL_V02 = 1, /**<  Use SUPL Location server  */
  eQMI_LOC_SERVER_PROTOCOL_VX_MPC_V02 = 2, /**<  Use VX MPC server for untrusted apps
 Use VX PDE server for trusted apps  */
  eQMI_LOC_SERVER_PROTOCOL_VX_PDE_V02 = 3,
  QMILOCSERVERPROTOCOLENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocServerProtocolEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSERVERREQUESTENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SERVER_REQUEST_OPEN_V02 = 1, /**<  Open a connection to the location server
 Close a connection to the location server  */
  eQMI_LOC_SERVER_REQUEST_CLOSE_V02 = 2,
  QMILOCSERVERREQUESTENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocServerRequestEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Requests the client to open or to close a connection
                    to the assisted GPS location server  */
typedef struct {

  /* Mandatory */
  uint32_t connHandle;
  /**<   Identifies a connection across open and close request events. */

  /* Mandatory */
  qmiLocServerRequestEnumT_v02 requestType;
  /**<   Open or close a connection to the location server. */

  /* Optional */
  uint8_t serverProtocol_valid;  /**< Must be set to true if serverProtocol is being passed */
  qmiLocServerProtocolEnumT_v02 serverProtocol;
  /**<   Identifies the location server protocol. If not specified
  eQMI_LOC_SERVER_PROTOCOL_DEFAULT is assumed.  */
}qmiLocEventLocationServerConnectionReqIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSTATUSENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SUCCESS_V02 = 0, /**<  Request was completed successfully.       */
  eQMI_LOC_GENERAL_FAILURE_V02 = 1, /**<  Request failed because of a general failure.  */
  eQMI_LOC_UNSUPPORTED_V02 = 2, /**<  Request failed because it is unsupported.  */
  eQMI_LOC_INVALID_PARAMETER_V02 = 3, /**<  Request failed because it contained invalid parameters.    */
  eQMI_LOC_ENGINE_BUSY_V02 = 4, /**<  Request failed because the engine is busy.  */
  eQMI_LOC_PHONE_OFFLINE_V02 = 5, /**<  Request failed because the phone is offline.
 Request failed because it timed out.  */
  eQMI_LOC_TIMEOUT_V02 = 6,
  QMILOCSTATUSENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocStatusEnumT_v02;
/**
    @}
  */

/*
 * qmiLocGetServiceRevisionReqMsgT is empty
 * typedef struct {
 * }qmiLocGetServiceRevisionReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Client can query the service revision using this message. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get revision request.  */

  /* Mandatory */
  uint32_t revision;
  /**<   Revision of the service. This is the minor revision of the service.
       Minor revision updates of the service are always backward compatible.  */
}qmiLocGetServiceRevisionIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetFixCriteriaReqMsgT is empty
 * typedef struct {
 * }qmiLocGetFixCriteriaReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the fix criteria from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get fix criteria request.  */

  /* Optional */
  uint8_t horizontalAccuracyLevel_valid;  /**< Must be set to true if horizontalAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 horizontalAccuracyLevel;
  /**<   Horizontal accuracy level: \n
       - HIGH -- High horizontal accuracy  is required. \n
       - MED -- Medium horizontal accuracy is required. \n
       - LOW -- Low horizontal accuracy is required.   */

  /* Optional */
  uint8_t altitudeAccuracyLevel_valid;  /**< Must be set to true if altitudeAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 altitudeAccuracyLevel;
  /**<   Altitude accuracy level:\n
       - HIGH -- Client requires high accuracy altitude fixes \n
       - LOW -- Client does not require high accuracy altitude fixes  */

  /* Optional */
  uint8_t bearingAccuracyLevel_valid;  /**< Must be set to true if bearingAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 bearingAccuracyLevel;
  /**<   Bearing accuracy level:\n
       - HIGH -- Client requires a high accuracy bearing\n
       - LOW -- Client does not require a high accuracy bearing   */

  /* Optional */
  uint8_t speedAccuracyLevel_valid;  /**< Must be set to true if speedAccuracyLevel is being passed */
  qmiLocAccuracyLevelEnumT_v02 speedAccuracyLevel;
  /**<   Speed accuracy level:\n
       - HIGH -- Client requires high accuracy speed\n
       - LOW -- Client does not require high accuracy speed\n
       - None -- If not specified, speed accuracy defaults to LOW   */

  /* Optional */
  uint8_t powerLevel_valid;  /**< Must be set to true if powerLevel is being passed */
  qmiLocPowerCriteriaEnumT_v02 powerLevel;
  /**<   Power level:\n
       - HIGH -- Client does not require a low power fix (default)\n
       - LOW -- Client requires a low power fix   */

  /* Optional */
  uint8_t intermediateReportState_valid;  /**< Must be set to true if intermediateReportState is being passed */
  qmiLocIntermediateReportStateEnumT_v02 intermediateReportState;
  /**<   Intermediate Reports State (ON, OFF).\n
     Client should explicitly set this field to OFF to stop receiving
     intermediate position reports. Intermediate position reports are
     generated at 1Hz and are ON by default.  If intermediate reports
     are turned ON the client receives position reports even if the
     accuracy criteria is not met. The status in the position report is
     set to IN_PROGRESS for intermediate reports  */

  /* Optional */
  uint8_t minInterval_valid;  /**< Must be set to true if minInterval is being passed */
  uint32_t minInterval;
  /**<   Time that must elapse before alerting the client. \n
       Units: milliseconds  */
}qmiLocGetFixCriteriaIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCNIUSERRESPENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02 = 1, /**<  User accepted notify verify request.  */
  eQMI_LOC_NI_LCS_NOTIFY_VERIFY_DENY_V02 = 2, /**<  User denied notify verify request.
 User did not respond to notify verify request.  */
  eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02 = 3,
  QMILOCNIUSERRESPENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocNiUserRespEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Sends the NI user response back to the engine; success or
                    failure is reported in a separate indication. */
typedef struct {

  /* Mandatory */
  qmiLocNiUserRespEnumT_v02 userResp;
  /**<   User accepted or denied.  */

  /* Mandatory */
  qmiLocNiNotifyVerifyEnumT_v02 notificationType;
  /**<   Type of notification/verification performed.  */

  /* Optional */
  uint8_t NiVxPayload_valid;  /**< Must be set to true if NiVxPayload is being passed */
  qmiLocNiVxNotifyVerifyStructT_v02 NiVxPayload;
  /**<   Optional NI VX request payload.  */

  /* Optional */
  uint8_t NiSuplPayload_valid;  /**< Must be set to true if NiSuplPayload is being passed */
  qmiLocNiSuplNotifyVerifyStructT_v02 NiSuplPayload;
  /**<   Optional NI SUPL request payload.  */

  /* Optional */
  uint8_t NiUmtsCpPayload_valid;  /**< Must be set to true if NiUmtsCpPayload is being passed */
  qmiLocNiUmtsCpNotifyVerifyStructT_v02 NiUmtsCpPayload;
  /**<   Optional NI UMTS-CP request payload.  */

  /* Optional */
  uint8_t NiVxServiceInteractionPayload_valid;  /**< Must be set to true if NiVxServiceInteractionPayload is being passed */
  qmiLocNiVxServiceInteractionStructT_v02 NiVxServiceInteractionPayload;
  /**<   Optional NI service interaction payload.          */
}qmiLocNiUserRespReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sends the NI user response back to the engine; success or
                    failure is reported in a separate indication. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the NI user response request.  */
}qmiLocNiUserRespIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCPREDICTEDORBITSDATAFORMATENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_PREDICTED_ORBITS_XTRA_V02 = 0, /**<  Default is QCOM-XTRA format.  */
  QMILOCPREDICTEDORBITSDATAFORMATENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocPredictedOrbitsDataFormatEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Injects predicted orbits data.  */
typedef struct {

  /* Mandatory */
  uint32_t totalSize;
  /**<   Total size of the predicted orbits data to be injected. \n
        Units: bytes  */

  /* Mandatory */
  uint16_t totalParts;
  /**<   Total number of parts the predicted orbits data is
        divided into.  */

  /* Mandatory */
  uint16_t partNum;
  /**<   Number of the current predicted orbits data part; starts at 1.  */

  /* Mandatory */
  uint32_t partData_len;  /**< Must be set to # of elements in partData */
  char partData[QMI_LOC_MAX_PREDICTED_ORBITS_PART_LEN_V02];
  /**<   Predicted orbits data.  */

  /* Optional */
  uint8_t formatType_valid;  /**< Must be set to true if formatType is being passed */
  qmiLocPredictedOrbitsDataFormatEnumT_v02 formatType;
  /**<   Predicted orbits data format.  */
}qmiLocInjectPredictedOrbitsDataReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Injects predicted orbits data.  */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the inject data, which will be sent
       only after all data parts have been injected.  */

  /* Optional */
  uint8_t partNum_valid;  /**< Must be set to true if partNum is being passed */
  uint16_t partNum;
  /**<   Number of the predicted orbits data part for which this indication
      is sent; starts at 1.  */
}qmiLocInjectPredictedOrbitsDataIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetPredictedOrbitsDataSourceReqMsgT is empty
 * typedef struct {
 * }qmiLocGetPredictedOrbitsDataSourceReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the predicted orbits data source. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the query request for a predicted orbits data source.  */

  /* Optional */
  uint8_t allowedSizes_valid;  /**< Must be set to true if allowedSizes is being passed */
  qmiLocPredictedOrbitsAllowedSizesStructT_v02 allowedSizes;
  /**<   Maximum part and file size allowed to be injected in the engine.  */

  /* Optional */
  uint8_t serverList_valid;  /**< Must be set to true if serverList is being passed */
  qmiLocPredictedOrbitsServerListStructT_v02 serverList;
  /**<   List of servers that can be used by the client to download
       predicted orbits data.  */
}qmiLocGetPredictedOrbitsDataSourceIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetPredictedOrbitsDataValidityReqMsgT is empty
 * typedef struct {
 * }qmiLocGetPredictedOrbitsDataValidityReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint64_t startTimeInUTC;
  /**<   The predicted orbits data  is valid starting from this time. \n
       Units: seconds (since Jan. 1, 1970)  */

  uint16_t durationHours;
  /**<   The duration from the start time for which the data is valid.\n
       Units: hours  */
}qmiLocPredictedOrbitsDataValidityStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the predicted orbits data validity. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the query request for predicted orbits data validity.  */

  /* Optional */
  uint8_t validityInfo_valid;  /**< Must be set to true if validityInfo is being passed */
  qmiLocPredictedOrbitsDataValidityStructT_v02 validityInfo;
}qmiLocGetPredictedOrbitsDataValidityIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Injects UTC time in the location engine.  */
typedef struct {

  /* Mandatory */
  uint64_t timeUtc;
  /**<   UTC time since Jan 1st, 1970.\n
       Units: milliseconds  */

  /* Mandatory */
  uint32_t timeUnc;
  /**<   Time uncertainty.\n
       Units: milliseconds   */
}qmiLocInjectUtcTimeReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Injects UTC time in the location engine.  */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the UTC time injection request.  */
}qmiLocInjectUtcTimeIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCALTSRCENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_ALT_SRC_UNKNOWN_V02 = 0, /**<  Source is unknown.  */
  eQMI_LOC_ALT_SRC_GPS_V02 = 1, /**<  GPS is the source.  */
  eQMI_LOC_ALT_SRC_CELL_ID_V02 = 2, /**<  Cell ID provided the source.  */
  eQMI_LOC_ALT_SRC_ENHANCED_CELL_ID_V02 = 3, /**<  Source is enhanced cell ID.  */
  eQMI_LOC_ALT_SRC_WIFI_V02 = 4, /**<  WiFi is the source.  */
  eQMI_LOC_ALT_SRC_TERRESTRIAL_V02 = 5, /**<  Terrestrial source.  */
  eQMI_LOC_ALT_SRC_TERRESTRIAL_HYBRID_V02 = 6, /**<  Hybrid terrestrial source.  */
  eQMI_LOC_ALT_SRC_ALTITUDE_DATABASE_V02 = 7, /**<  Altitude database is the source.  */
  eQMI_LOC_ALT_SRC_BAROMETRIC_ALTIMETER_V02 = 8, /**<  Barometric altimeter is the source.
 Other sources.  */
  eQMI_LOC_ALT_SRC_OTHER_V02 = 9,
  QMILOCALTSRCENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocAltSrcEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCALTSRCLINKAGEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_ALT_SRC_LINKAGE_NOT_SPECIFIED_V02 = 0, /**<  Not specified.  */
  eQMI_LOC_ALT_SRC_LINKAGE_FULLY_INTERDEPENDENT_V02 = 1, /**<  Fully interdependent.  */
  eQMI_LOC_ALT_SRC_LINKAGE_DEPENDS_ON_LAT_LONG_V02 = 2, /**<  Depends on latitude and longitude.
 Fully independent.  */
  eQMI_LOC_ALT_SRC_LINKAGE_FULLY_INDEPENDENT_V02 = 3,
  QMILOCALTSRCLINKAGEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocAltSrcLinkageEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCALTSRCUNCERTAINTYCOVERAGEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_ALT_UNCERTAINTY_NOT_SPECIFIED_V02 = 0, /**<  Not specified.  */
  eQMI_LOC_ALT_UNCERTAINTY_POINT_V02 = 1, /**<  Altitude uncertainty is valid at the injected horizontal
       position coordinates only.
 Altitude uncertainty applies to the position of the device
       regardless of horizontal position (within the horizontal
       uncertainty region, if provided).  */
  eQMI_LOC_ALT_UNCERTAINTY_FULL_V02 = 2,
  QMILOCALTSRCUNCERTAINTYCOVERAGEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocAltSrcUncertaintyCoverageEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocAltSrcEnumT_v02 source;
  /**<   Specifies the source of the altitude.  */

  qmiLocAltSrcLinkageEnumT_v02 linkage;
  /**<   Specifies the dependency between the horizontal and
       altitude position components.  */

  qmiLocAltSrcUncertaintyCoverageEnumT_v02 coverage;
  /**<   Specifies the region of uncertainty.  */
}qmiLocAltitudeSrcInfoStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Injects a position to the location engine. */
typedef struct {

  /* Mandatory */
  double latitude;
  /**<   Latitude.\n
       Units: degrees; Range: -90.0 .. 90.0\n
       Positive values indicate northern latitude.\n
       Negative values indicate southern latitude.   */

  /* Mandatory */
  double longitude;
  /**<   Longitude (specified in WBS84 datum).\n
       Units: degrees; Range: -180.0 .. 180.0\n
       Positive values indicate eastern longitude.\n
       Negative values indicate western longitude.  */

  /* Optional */
  uint8_t horUncCircular_valid;  /**< Must be set to true if horUncCircular is being passed */
  float horUncCircular;
  /**<   Horizontal position uncertainty (circular).\n
        Units: meters  */

  /* Optional */
  uint8_t horConfidence_valid;  /**< Must be set to true if horConfidence is being passed */
  uint8_t horConfidence;
  /**<   Horizontal confidence, as defined by  ETSI TS 101 109.\n
        Units: percentage [0-99]\n
        - 0 -- invalid value\n
        - 100 to 256 -- not used.\n
        - If 100 is received, re-interpret to 99.\n
        This field must be specified together with horizontal uncertainty.
        If not specified, the default value will be 50.  */

  /* Optional */
  uint8_t horReliability_valid;  /**< Must be set to true if horReliability is being passed */
  qmiLocReliabilityEnumT_v02 horReliability;
  /**<   Specifies the reliability of the horizontal position.   */

  /* Optional */
  uint8_t altitudeWrtEllipsoid_valid;  /**< Must be set to true if altitudeWrtEllipsoid is being passed */
  float altitudeWrtEllipsoid;
  /**<   Altitude with respect to the WGS84 ellipsoid.\n
       Units: meters; positive = height, negative = depth   */

  /* Optional */
  uint8_t altitudeWrtMeanSeaLevel_valid;  /**< Must be set to true if altitudeWrtMeanSeaLevel is being passed */
  float altitudeWrtMeanSeaLevel;
  /**<   Altitude with respect to mean sea level.\n
       Units: meters  */

  /* Optional */
  uint8_t vertUnc_valid;  /**< Must be set to true if vertUnc is being passed */
  float vertUnc;
  /**<   Vertical uncertainty. This is mandatory if either altitudeWrtEllipsoid
        or altitudeWrtMeanSeaLevel is specified.\n
        Units: meters  */

  /* Optional */
  uint8_t vertConfidence_valid;  /**< Must be set to true if vertConfidence is being passed */
  uint8_t vertConfidence;
  /**<   Vertical confidence, as defined by  ETSI TS 101 109.\n
        Units: percentage [0-99]\n
        - 0 -- invalid value\n
        - 100 to 256 -- not used\n
        - If 100 is received, re-interpret to 99.\n
        This field must be specified together with the field of vertUnc.
        If not specified, the default value will be 50.  */

  /* Optional */
  uint8_t vertReliability_valid;  /**< Must be set to true if vertReliability is being passed */
  qmiLocReliabilityEnumT_v02 vertReliability;
  /**<   Specifies the reliability of the vertical position.   */

  /* Optional */
  uint8_t altSourceInfo_valid;  /**< Must be set to true if altSourceInfo is being passed */
  qmiLocAltitudeSrcInfoStructT_v02 altSourceInfo;
  /**<   Specifies information regarding the altitude source.  */

  /* Optional */
  uint8_t timestampUtc_valid;  /**< Must be set to true if timestampUtc is being passed */
  uint64_t timestampUtc;
  /**<   UTC timestamp. \n
        Units: milliseconds (since Jan. 1, 1970)  */

  /* Optional */
  uint8_t timestampAge_valid;  /**< Must be set to true if timestampAge is being passed */
  int32_t timestampAge;
  /**<   Position age, which is an estimate of how long ago this fix was made. \n
        Units: milliseconds  */
}qmiLocInjectPositionReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Injects a position to the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the UTC position injection request.  */
}qmiLocInjectPositionIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCLOCKENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_LOCK_NONE_V02 = 1,
  eQMI_LOC_LOCK_MI_V02 = 2,
  eQMI_LOC_LOCK_MT_V02 = 3,
  eQMI_LOC_LOCK_ALL_V02 = 4,
  QMILOCLOCKENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocLockEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Sets the location engine lock. */
typedef struct {

  /* Mandatory */
  qmiLocLockEnumT_v02 lockType;
  /**<   Type of lock.   */
}qmiLocSetEngineLockReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sets the location engine lock. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set engine lock request.  */
}qmiLocSetEngineLockIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetEngineLockReqMsgT is empty
 * typedef struct {
 * }qmiLocGetEngineLockReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the location engine lock. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get engine lock request.  */

  /* Optional */
  uint8_t lockType_valid;  /**< Must be set to true if lockType is being passed */
  qmiLocLockEnumT_v02 lockType;
  /**<   Type of lock.  */
}qmiLocGetEngineLockIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Sets the SBAS configuration. */
typedef struct {

  /* Mandatory */
  uint8_t sbasConfig;
  /**<   Whether SBAS configuration is enabled.
       TRUE == enabled; FALSE == disabled  */
}qmiLocSetSbasConfigReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sets the SBAS configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set SBAS configuration request.  */
}qmiLocSetSbasConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetSbasConfigReqMsgT is empty
 * typedef struct {
 * }qmiLocGetSbasConfigReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the SBAS configuration from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get SBAS configuration request.  */

  /* Optional */
  uint8_t sbasConfig_valid;  /**< Must be set to true if sbasConfig is being passed */
  uint8_t sbasConfig;
  /**<   Whether SBAS configuration is enabled:\n
       - TRUE -- enabled\n
       - FALSE -- disabled  */
}qmiLocGetSbasConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Sets the NMEA types. */
typedef struct {

  /* Mandatory */
  uint32_t nmeaSentenceType;
  /**<   NMEA types to enable; defined in QMI_LOC_NMEA_MASK_*.  */
}qmiLocSetNmeaTypesReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Sets the NMEA types. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of set NMEA types operation.  */
}qmiLocSetNmeaTypesIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetNmeaTypesReqMsgT is empty
 * typedef struct {
 * }qmiLocGetNmeaTypesReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the NMEA types from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get NMEA types operation.  */

  /* Optional */
  uint8_t nmeaSentenceType_valid;  /**< Must be set to true if nmeaSentenceType is being passed */
  uint32_t nmeaSentenceType;
  /**<   NMEA types to enable; defined in QMI_LOC_NMEA_MASK_*.  */
}qmiLocGetNmeaTypesIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Enables/disables low power mode configuration. */
typedef struct {

  /* Mandatory */
  uint8_t lowPowerMode;
  /**<   Whether to enable low power mode:\n
       - TRUE -- enable LPM\n
       - FALSE -- disable LPM  */
}qmiLocSetLowPowerModeReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Enables/disables low power mode configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set low power mode request.  */
}qmiLocSetLowPowerModeIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetLowPowerModeReqMsgT is empty
 * typedef struct {
 * }qmiLocGetLowPowerModeReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the low power mode from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Whether the get LPM operation was successful.  */

  /* Optional */
  uint8_t lowPowerMode_valid;  /**< Must be set to true if lowPowerMode is being passed */
  uint8_t lowPowerMode;
  /**<   Whether LPM is enabled:\n
       - TRUE -- LPM enabled\n
       - FALSE -- LPM disabled  */
}qmiLocGetLowPowerModeIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Specifies the A-GPS server type and address. */
typedef struct {

  /* Mandatory */
  qmiLocServerTypeEnumT_v02 serverType;
  /**<   Type of server; as defined in qmiLocServerTypeEnumT.  */

  /* Optional */
  uint8_t ipv4Addr_valid;  /**< Must be set to true if ipv4Addr is being passed */
  qmiLocIpV4AddrStructType_v02 ipv4Addr;
  /**<   IPV4 address and port.  */

  /* Optional */
  uint8_t ipv6Addr_valid;  /**< Must be set to true if ipv6Addr is being passed */
  qmiLocIpV6AddrStructType_v02 ipv6Addr;

  /* Optional */
  uint8_t urlAddr_valid;  /**< Must be set to true if urlAddr is being passed */
  char urlAddr[QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02 + 1];
  /**<   URL address. If both ipv4Addr and urlAddr fields are present, the
       ipv4Addr field is the selected.  */
}qmiLocSetServerReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Specifies the A-GPS server type and address. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set server request.  */
}qmiLocSetServerIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Gets the location server from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocServerTypeEnumT_v02 serverType;
  /**<   Type of server, as defined in qmiLocServerTypeEnumT.  */

  /* Optional */
  uint8_t serverAddrTypeMask_valid;  /**< Must be set to true if serverAddrTypeMask is being passed */
  uint8_t serverAddrTypeMask;
  /**<   Type of address the client wants, if unspecified. The
       indication will contain all the types of addresses
       it has for the specified server type  */
}qmiLocGetServerReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the location server from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get server request.  */

  /* Mandatory */
  qmiLocServerTypeEnumT_v02 serverType;
  /**<   Type of server, as defined in qmiLocServerTypeEnumT.  */

  /* Optional */
  uint8_t ipv4Addr_valid;  /**< Must be set to true if ipv4Addr is being passed */
  qmiLocIpV4AddrStructType_v02 ipv4Addr;
  /**<   IPV4 address and port.  */

  /* Optional */
  uint8_t ipv6Addr_valid;  /**< Must be set to true if ipv6Addr is being passed */
  qmiLocIpV6AddrStructType_v02 ipv6Addr;
  /**<   IPV6 address and port.  */

  /* Optional */
  uint8_t urlAddr_valid;  /**< Must be set to true if urlAddr is being passed */
  char urlAddr[QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02 + 1];
  /**<   URL.  */
}qmiLocGetServerIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; This command is used to delete the location engine
                    assistance data  */
typedef struct {

  /* Mandatory */
  uint32_t deleteMask;
  /**<   Assistance data to be deleted.  */
}qmiLocDeleteAssistDataReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; This command is used to delete the location engine
                    assistance data  */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the delete assist data request.  */
}qmiLocDeleteAssistDataIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Enables/disables XTRA-T session control. */
typedef struct {

  /* Mandatory */
  uint8_t xtraTSessionControl;
  /**<   Whether to enable XTRA-T:\n
       - TRUE -- enable XTRA-T\n
       - FALSE -- disable XTRA-T  */
}qmiLocSetXtraTSessionControlReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Enables/disables XTRA-T session control. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set XTRA-T session control request.  */
}qmiLocSetXtraTSessionControlIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetXtraTSessionControlReqMsgT is empty
 * typedef struct {
 * }qmiLocGetXtraTSessionControlReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the XTRA-T session control value from the location
                    engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get XTRA-T session control request.  */

  /* Optional */
  uint8_t xtraTSessionControl_valid;  /**< Must be set to true if xtraTSessionControl is being passed */
  uint8_t xtraTSessionControl;
  /**<   Whether XTRA-T is enabled:\n
       - TRUE -- XTRA-T enabled\n
       - FALSE -- XTRA-T disabled  */
}qmiLocGetXtraTSessionControlIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t wifiPositionTime;
  /**<   Common counter (typically, the number of milliseconds since bootup).
        This field should only be provided if modem and host processors are
        synchronized.  */
}qmiLocWifiFixTimeStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  double lat;
  /**<   WiFi position latitude. \n
        Units: degrees  */

  double lon;
  /**<   WiFi position longitude. \n
        Units: degrees  */

  uint16_t hepe;
  /**<   WiFi position HEPE.\n
        Units: meters  */

  uint8_t numApsUsed;
  /**<   Number of Access Points (AP) used to generate a fix.  */

  uint8_t fixErrorCode;
  /**<   WiFi position error code; set to 0 if fix succeeds. This position is
        only used by a module if the value is 0. If there was a failure, the
            error code provided by the WiFi positioning system can be provided
            here.  */
}qmiLocWifiFixPosStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint8_t macAddr[QMI_LOC_WIFI_MAC_ADDR_LENGTH_V02];
  /**<   Associated MAC address of the AP.  */

  int32_t rssi;
  /**<   Receive signal strength indicator.\n
        Units: dBm (offset with +100 dB).  */

  uint16_t channel;
  /**<   WiFi channel on which a beacon was received.  */

  uint8_t apQualifier;
  /**<   A bitmask of Boolean qualifiers for AP, as specified
        via QMI_LOC_WIFI_AP_QUALIFIER_*. All unused bits in
        this mask must be set to 0.  */
}qmiLocWifiApInfoStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Injects the WiFi position. */
typedef struct {

  /* Optional */
  uint8_t wifiFixTime_valid;  /**< Must be set to true if wifiFixTime is being passed */
  qmiLocWifiFixTimeStructT_v02 wifiFixTime;
  /**<   Time of WiFi position fix.  */

  /* Optional */
  uint8_t wifiFixPosition_valid;  /**< Must be set to true if wifiFixPosition is being passed */
  qmiLocWifiFixPosStructT_v02 wifiFixPosition;
  /**<   WiFi position fix.  */

  /* Optional */
  uint8_t apInfo_valid;  /**< Must be set to true if apInfo is being passed */
  uint32_t apInfo_len;  /**< Must be set to # of elements in apInfo */
  qmiLocWifiApInfoStructT_v02 apInfo[QMI_LOC_WIFI_MAX_REPORTED_APS_PER_MSG_V02];
  /**<   AP scan list.  */

  /* Optional */
  uint8_t horizontalReliability_valid;  /**< Must be set to true if horizontalReliability is being passed */
  qmiLocReliabilityEnumT_v02 horizontalReliability;
  /**<   Specifies the reliability of the horizontal position.    */
}qmiLocInjectWifiPositionReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Injects the WiFi position. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the inject WiFi position request.  */
}qmiLocInjectWifiPositionIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCWIFISTATUSENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_WIFI_STATUS_AVAILABLE_V02 = 1, /**<  WiFi is available.
 WiFi is not available.  */
  eQMI_LOC_WIFI_STATUS_UNAVAILABLE_V02 = 2,
  QMILOCWIFISTATUSENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocWifiStatusEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Gets the low power mode from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocWifiStatusEnumT_v02 wifiStatus;
  /**<   WiFi status information.  */
}qmiLocNotifyWifiStatusReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the low power mode from the location engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the notify WiFi status request.  */
}qmiLocNotifyWifiStatusIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetRegisteredEventsReqMsgT is empty
 * typedef struct {
 * }qmiLocGetRegisteredEventsReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the mask of the events for which a client has
                    registered. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get registered events request.  */

  /* Optional */
  uint8_t eventRegMask_valid;  /**< Must be set to true if eventRegMask is being passed */
  uint64_t eventRegMask;
  /**<   Event registration mask (see bitmask fields prefixed with
      QMI_LOC_EVENT_*).  */
}qmiLocGetRegisteredEventsIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCOPERATIONMODEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_OPER_MODE_DEFAULT_V02 = 1, /**<  Use the default engine mode.  */
  eQMI_LOC_OPER_MODE_MSB_V02 = 2, /**<  Use the MS-based mode.  */
  eQMI_LOC_OPER_MODE_MSA_V02 = 3, /**<  Use the MS-assisted mode.  */
  eQMI_LOC_OPER_MODE_STANDALONE_V02 = 4, /**<  Use Standalone mode.  */
  eQMI_LOC_OPER_MODE_SPEED_OPTIMAL_V02 = 5, /**<  Optimize for speed.  */
  eQMI_LOC_OPER_MODE_ACCURACY_OPTIMAL_V02 = 6, /**<  Optimize for accuracy.  */
  eQMI_LOC_OPER_MODE_DATA_OPTIMAL_V02 = 7, /**<  Optimize for data.
 Use cell ID. For 1x, this mode corresponds to
       AFLT.  */
  eQMI_LOC_OPER_MODE_CELL_ID_V02 = 8,
  QMILOCOPERATIONMODEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocOperationModeEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Tells the engine to use the specified operation mode while
                    making the position fixes. */
typedef struct {

  /* Mandatory */
  qmiLocOperationModeEnumT_v02 operationMode;
  /**<   Preferred operation mode.  */
}qmiLocSetOperationModeReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Tells the engine to use the specified operation mode while
                    making the position fixes. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set operation mode request.  */
}qmiLocSetOperationModeIndMsgT_v02;  /* Message */
/**
    @}
  */

/*
 * qmiLocGetOperationModeReqMsgT is empty
 * typedef struct {
 * }qmiLocGetOperationModeReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Gets the current operation mode from the engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get operation mode request.  */

  /* Optional */
  uint8_t operationMode_valid;  /**< Must be set to true if operationMode is being passed */
  qmiLocOperationModeEnumT_v02 operationMode;
  /**<   Current operation mode.  */
}qmiLocGetOperationModeIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint8_t stationary;
  /**<   Whether the device is stationary:\n
       - 0x00 -- Device is not stationary\n
       - 0x01 -- Device is stationary  */

  uint8_t confidence;
  /**<   Confidence expressed as a percentage.\n
       Range: 0..100  */
}qmiLocSpiStatusStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to set the Stationary Position
                    Indidcator (SPI) status, which indicates whether the
                    device is stationary. */
typedef struct {

  /* Optional */
  uint8_t status_valid;  /**< Must be set to true if status is being passed */
  qmiLocSpiStatusStructT_v02 status;
  /**<   Specifies the status of SPI.  */
}qmiLocSetSpiStatusReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to set the Stationary Position
                    Indidcator (SPI) status, which indicates whether the
                    device is stationary. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the SPI status request.  */
}qmiLocSetSpiStatusIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint16_t timeOffset;
  /**<   Sample time offset. This time offset must be
       relative to the sensor time of the first sample.\n
       Units: milliseconds  */

  float xAxis;
  /**<   Accelerometer x-axis sample.\n
       Units: meter/(sec^2), in IEEE single float format  */

  float yAxis;
  /**<   Accelerometer y-axis sample.\n
       Units: meter/(sec^2), in IEEE single float format  */

  float zAxis;
  /**<   Accelerometer z-axis sample. \n
       Units: meter/(sec^2), in IEEE single float format  */
}qmiLoc3AxisSensorSampleStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  uint32_t timeOfFirstSample;
  /**<   Denotes a full 32-bit time tag of the first (oldest) sample in this
       message in units of milliseconds  */

  uint8_t flags;
  /**<   Flags to indicate any deviation from the default measurement
       assumptions. All unused bits in this field must be set to 0.  */

  uint32_t sensorData_len;  /**< Must be set to # of elements in sensorData */
  qmiLoc3AxisSensorSampleStructT_v02 sensorData[QMI_LOC_SENSOR_DATA_MAX_SAMPLES_V02];
  /**<   Variable length array to specify sensor samples.                              */
}qmiLoc3AxisSensorSampleListStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to inject sensor data into the
                    GPS engine. */
typedef struct {

  /* Optional */
  uint8_t opaqueIdentifier_valid;  /**< Must be set to true if opaqueIdentifier is being passed */
  uint32_t opaqueIdentifier;
  /**<   An opaque identifier that is sent in by the client that will be echoed
       in the indication so the client can relate the indication to the
       request.  */

  /* Optional */
  uint8_t threeAxisAccelData_valid;  /**< Must be set to true if threeAxisAccelData is being passed */
  qmiLoc3AxisSensorSampleListStructT_v02 threeAxisAccelData;
  /**<   Accelerometer sensor samples.  */

  /* Optional */
  uint8_t threeAxisGyroData_valid;  /**< Must be set to true if threeAxisGyroData is being passed */
  qmiLoc3AxisSensorSampleListStructT_v02 threeAxisGyroData;
  /**<   Gyrometer sensor samples.  */
}qmiLocInjectSensorDataReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to inject sensor data into the
                    GPS engine. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the inject sensor data request.  */

  /* Optional */
  uint8_t opaqueIdentifier_valid;  /**< Must be set to true if opaqueIdentifier is being passed */
  uint32_t opaqueIdentifier;
  /**<   An opaque identifier that was sent in by the client echoed
       so the client can relate the indication to the request.  */

  /* Optional */
  uint8_t threeAxisAccelSamplesAccepted_valid;  /**< Must be set to true if threeAxisAccelSamplesAccepted is being passed */
  uint8_t threeAxisAccelSamplesAccepted;
  /**<   This field lets the client know how many 3-axis accelerometer samples
       were accepted.  */

  /* Optional */
  uint8_t threeAxisGyroSamplesAccepted_valid;  /**< Must be set to true if threeAxisGyroSamplesAccepted is being passed */
  uint8_t threeAxisGyroSamplesAccepted;
  /**<   This field lets the client know how many 3-axis gyrometer samples were
       accepted.  */
}qmiLocInjectSensorDataIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to inject time sync data. */
typedef struct {

  /* Mandatory */
  uint32_t refCounter;
  /**<   Must be set to the value that was sent to the control point when the
       GPS engine requested time sync injection.  */

  /* Mandatory */
  uint32_t sensorProcRxTime;
  /**<   The value of the sensor time when the control point received the
       time sync injection request from the GPS engine.

       Must be monotonically increasing, jitter @latexonly $\leq$ @endlatexonly 1
       millisecond, never stopping until the process is rebooted.\n
       Units: milliseconds  */

  /* Mandatory */
  uint32_t sensorProcTxTime;
  /**<   The value of the sensor time when the control point injects this message
       for use by the GPS engine.

       Must be monotonically increasing, jitter @latexonly $\leq$ @endlatexonly 1
       millisecond, never stopping until the process is rebooted.\n
       Units: milliseconds  */
}qmiLocInjectTimeSyncDataReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to inject time sync data. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the inject time sync data request.  */
}qmiLocInjectTimeSyncDataIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCCRADLEMOUNTSTATEENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_CRADLE_STATE_NOT_MOUNTED_V02 = 0, /**<  Device is mounted on the cradle */
  eQMI_LOC_CRADLE_STATE_MOUNTED_V02 = 1, /**<  Device is not mounted on the cradle
 Unknown cradle mount state */
  eQMI_LOC_CRADLE_STATE_UNKNOWN_V02 = 2,
  QMILOCCRADLEMOUNTSTATEENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocCradleMountStateEnumT_v02;
/**
    @}
  */

/*
 * qmiLocGetCradleMountConfigReqMsgT is empty
 * typedef struct {
 * }qmiLocGetCradleMountConfigReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocCradleMountStateEnumT_v02 cradleMountState;
  /**<   Cradle mount state set by the control point. \n
       Range : NOT_MOUNTED, MOUNTED, UNKNOWN */

  uint8_t confidence;
  /**<   Confidence of state expressed in percent.\n
       Range: 0..100  */
}qmiLocCradleMountInfoStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to get the current
                    cradle mount configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get cradle mount configuration request.  */

  /* Optional */
  uint8_t cradleState_valid;  /**< Must be set to true if cradleState is being passed */
  qmiLocCradleMountInfoStructT_v02 cradleState;
  /**<   Echo back the cradle state that was last specified by the control point.  */
}qmiLocGetCradleMountConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to set the current
                    cradle mount configuration. */
typedef struct {

  /* Optional */
  uint8_t cradleState_valid;  /**< Must be set to true if cradleState is being passed */
  qmiLocCradleMountInfoStructT_v02 cradleState;
  /**<   Echo back the cradle state that was last specified by control point.  */
}qmiLocSetCradleMountConfigReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to set the current
                    cradle mount configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set cradle mount configuration request.  */
}qmiLocSetCradleMountConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCEXTERNALPOWERCONFIGENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_EXTERNAL_POWER_NOT_CONNECTED_V02 = 0, /**<  Device is not connected to external power source.  */
  eQMI_LOC_EXTERNAL_POWER_CONNECTED_V02 = 1, /**<  Device is connected to external power source.
 Unknown external power state.  */
  eQMI_LOC_EXTERNAL_POWER_UNKNOWN_V02 = 2,
  QMILOCEXTERNALPOWERCONFIGENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocExternalPowerConfigEnumT_v02;
/**
    @}
  */

/*
 * qmiLocGetExternalPowerConfigReqMsgT is empty
 * typedef struct {
 * }qmiLocGetExternalPowerConfigReqMsgT_v02;
 */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to get the current
                    external power configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the get external power configuration request.  */

  /* Optional */
  uint8_t externalPowerState_valid;  /**< Must be set to true if externalPowerState is being passed */
  qmiLocExternalPowerConfigEnumT_v02 externalPowerState;
  /**<   Power state; injected by the control point \n
    Range : NOT_CONNECTED, CONNECTED, UNKNOWN  */
}qmiLocGetExternalPowerConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to set the current
                    external power configuration. */
typedef struct {

  /* Optional */
  uint8_t externalPowerState_valid;  /**< Must be set to true if externalPowerState is being passed */
  qmiLocExternalPowerConfigEnumT_v02 externalPowerState;
  /**<   Power state; injected by the control point
    Range : NOT_CONNECTED, CONNECTED, UNKNOWN  */
}qmiLocSetExternalPowerConfigReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to set the current
                    external power configuration. */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the set external power configuration request.  */
}qmiLocSetExternalPowerConfigIndMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSERVERPDNENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4_V02 = 0x01, /**<  IPV4 PDN type  */
  eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV6_V02 = 0x02, /**<  IPV6 PDN type  */
  eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4V6_V02 = 0x03, /**<  IPV4V6 PDN type
 PPP PDN type  */
  eQMI_LOC_APN_PROFILE_PDN_TYPE_PPP_V02 = 0x04,
  QMILOCSERVERPDNENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocServerPDNEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_aggregates
    @{
  */
typedef struct {

  qmiLocServerPDNEnumT_v02 pdnType;
  /**<   Pdp type of the APN profile */

  char apnName[QMI_LOC_MAX_APN_NAME_LENGTH_V02 + 1];
  /**<   APN name  */
}qmiLocApnProfilesStructT_v02;  /* Type */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_enums
    @{
  */
typedef enum {
  QMILOCSERVERREQSTATUSENUMT_MIN_ENUM_VAL_V02 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  eQMI_LOC_SERVER_REQ_STATUS_SUCCESS_V02 = 1, /**<  The location server request was successful.
 The location server request failed. */
  eQMI_LOC_SERVER_REQ_STATUS_FAILURE_V02 = 2,
  QMILOCSERVERREQSTATUSENUMT_MAX_ENUM_VAL_V02 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}qmiLocServerReqStatusEnumT_v02;
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Request Message; Used by the control point to inform the service about the
                    status of the location server connection request that the
                    service may have sent via the
                    QMI_LOC_EVENT_LOCATION_SERVER_REQ_IND event */
typedef struct {

  /* Mandatory */
  uint32_t connHandle;
  /**<   connHandle that the service had specified in the
       location server connection request event.  */

  /* Mandatory */
  qmiLocServerRequestEnumT_v02 requestType;
  /**<   Specifies the connection request (OPEN, CLOSE) that
       this status is about.  */

  /* Mandatory */
  qmiLocServerReqStatusEnumT_v02 statusType;
  /**<   Status (SUCCESS, FAIL) of the connection request. */

  /* Optional */
  uint8_t serverProtocol_valid;  /**< Must be set to true if serverProtocol is being passed */
  qmiLocServerProtocolEnumT_v02 serverProtocol;
  /**<   Protocol of the server; should be the same as one specified
       in the location server connection request event. */

  /* Optional */
  uint8_t apnProfile_valid;  /**< Must be set to true if apnProfile is being passed */
  qmiLocApnProfilesStructT_v02 apnProfile;
  /**<   APN profile information; may only be present when requestType
       is OPEN and statusType is SUCCESS.  */
}qmiLocInformLocationServerConnStatusReqMsgT_v02;  /* Message */
/**
    @}
  */

/** @addtogroup qmiLoc_qmi_messages
    @{
  */
/** Indication Message; Used by the control point to inform the service about the
                    status of the location server connection request that the
                    service may have sent via the
                    QMI_LOC_EVENT_LOCATION_SERVER_REQ_IND event */
typedef struct {

  /* Mandatory */
  qmiLocStatusEnumT_v02 status;
  /**<   Status of the inform location server connection status request.  */
}qmiLocInformLocationServerConnStatusIndMsgT_v02;  /* Message */
/**
    @}
  */

/*Service Message Definition*/
/** @addtogroup qmiLoc_qmi_msg_ids
    @{
  */
#define QMI_LOC_INFORM_CLIENT_REVISION_REQ_V02 0x0001
#define QMI_LOC_INFORM_CLIENT_REVISION_RESP_V02 0x0001
#define QMI_LOC_REG_EVENTS_REQ_V02 0x0002
#define QMI_LOC_REG_EVENTS_RESP_V02 0x0002
#define QMI_LOC_START_REQ_V02 0x0003
#define QMI_LOC_START_RESP_V02 0x0003
#define QMI_LOC_STOP_REQ_V02 0x0004
#define QMI_LOC_STOP_RESP_V02 0x0004
#define QMI_LOC_EVENT_POSITION_REPORT_IND_V02 0x0005
#define QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02 0x0006
#define QMI_LOC_EVENT_NMEA_IND_V02 0x0007
#define QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND_V02 0x0008
#define QMI_LOC_EVENT_TIME_INJECT_REQ_IND_V02 0x0009
#define QMI_LOC_EVENT_PREDICTED_ORBITS_INJECT_REQ_IND_V02 0x000A
#define QMI_LOC_EVENT_POSITION_INJECT_REQ_IND_V02 0x000B
#define QMI_LOC_EVENT_ENGINE_STATE_IND_V02 0x000C
#define QMI_LOC_EVENT_FIX_SESSION_STATE_IND_V02 0x000D
#define QMI_LOC_EVENT_WIFI_REQ_IND_V02 0x000E
#define QMI_LOC_EVENT_SENSOR_STREAMING_READY_STATUS_IND_V02 0x000F
#define QMI_LOC_EVENT_TIME_SYNC_REQ_IND_V02 0x0010
#define QMI_LOC_EVENT_SET_SPI_STREAMING_REPORT_IND_V02 0x0011
#define QMI_LOC_EVENT_LOCATION_SERVER_CONNECTION_REQ_IND_V02 0x0012
#define QMI_LOC_GET_SERVICE_REVISION_REQ_V02 0x0013
#define QMI_LOC_GET_SERVICE_REVISION_RESP_V02 0x0013
#define QMI_LOC_GET_SERVICE_REVISION_IND_V02 0x0013
#define QMI_LOC_GET_FIX_CRITERIA_REQ_V02 0x0014
#define QMI_LOC_GET_FIX_CRITERIA_RESP_V02 0x0014
#define QMI_LOC_GET_FIX_CRITERIA_IND_V02 0x0014
#define QMI_LOC_NI_USER_RESPONSE_REQ_V02 0x0015
#define QMI_LOC_NI_USER_RESPONSE_RESP_V02 0x0015
#define QMI_LOC_NI_USER_RESPONSE_IND_V02 0x0015
#define QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_REQ_V02 0x0016
#define QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_RESP_V02 0x0016
#define QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_IND_V02 0x0016
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_SOURCE_REQ_V02 0x0017
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_SOURCE_RESP_V02 0x0017
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_SOURCE_IND_V02 0x0017
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_VALIDITY_REQ_V02 0x0018
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_VALIDITY_RESP_V02 0x0018
#define QMI_LOC_GET_PREDICTED_ORBITS_DATA_VALIDITY_IND_V02 0x0018
#define QMI_LOC_INJECT_UTC_TIME_REQ_V02 0x0019
#define QMI_LOC_INJECT_UTC_TIME_RESP_V02 0x0019
#define QMI_LOC_INJECT_UTC_TIME_IND_V02 0x0019
#define QMI_LOC_INJECT_POSITION_REQ_V02 0x001A
#define QMI_LOC_INJECT_POSITION_RESP_V02 0x001A
#define QMI_LOC_INJECT_POSITION_IND_V02 0x001A
#define QMI_LOC_SET_ENGINE_LOCK_REQ_V02 0x001B
#define QMI_LOC_SET_ENGINE_LOCK_RESP_V02 0x001B
#define QMI_LOC_SET_ENGINE_LOCK_IND_V02 0x001B
#define QMI_LOC_GET_ENGINE_LOCK_REQ_V02 0x001C
#define QMI_LOC_GET_ENGINE_LOCK_RESP_V02 0x001C
#define QMI_LOC_GET_ENGINE_LOCK_IND_V02 0x001C
#define QMI_LOC_SET_SBAS_CONFIG_REQ_V02 0x001D
#define QMI_LOC_SET_SBAS_CONFIG_RESP_V02 0x001D
#define QMI_LOC_SET_SBAS_CONFIG_IND_V02 0x001D
#define QMI_LOC_GET_SBAS_CONFIG_REQ_V02 0x001E
#define QMI_LOC_GET_SBAS_CONFIG_RESP_V02 0x001E
#define QMI_LOC_GET_SBAS_CONFIG_IND_V02 0x001E
#define QMI_LOC_SET_NMEA_TYPES_REQ_V02 0x001F
#define QMI_LOC_SET_NMEA_TYPES_RESP_V02 0x001F
#define QMI_LOC_SET_NMEA_TYPES_IND_V02 0x001F
#define QMI_LOC_GET_NMEA_TYPES_REQ_V02 0x0020
#define QMI_LOC_GET_NMEA_TYPES_RESP_V02 0x0020
#define QMI_LOC_GET_NMEA_TYPES_IND_V02 0x0020
#define QMI_LOC_SET_LOW_POWER_MODE_REQ_V02 0x0021
#define QMI_LOC_SET_LOW_POWER_MODE_RESP_V02 0x0021
#define QMI_LOC_SET_LOW_POWER_MODE_IND_V02 0x0021
#define QMI_LOC_GET_LOW_POWER_MODE_REQ_V02 0x0022
#define QMI_LOC_GET_LOW_POWER_MODE_RESP_V02 0x0022
#define QMI_LOC_GET_LOW_POWER_MODE_IND_V02 0x0022
#define QMI_LOC_SET_SERVER_REQ_V02 0x0023
#define QMI_LOC_SET_SERVER_RESP_V02 0x0023
#define QMI_LOC_SET_SERVER_IND_V02 0x0023
#define QMI_LOC_GET_SERVER_REQ_V02 0x0024
#define QMI_LOC_GET_SERVER_RESP_V02 0x0024
#define QMI_LOC_GET_SERVER_IND_V02 0x0024
#define QMI_LOC_DELETE_ASSIST_DATA_REQ_V02 0x0025
#define QMI_LOC_DELETE_ASSIST_DATA_RESP_V02 0x0025
#define QMI_LOC_DELETE_ASSIST_DATA_IND_V02 0x0025
#define QMI_LOC_SET_XTRA_T_SESSION_CONTROL_REQ_V02 0x0026
#define QMI_LOC_SET_XTRA_T_SESSION_CONTROL_RESP_V02 0x0026
#define QMI_LOC_SET_XTRA_T_SESSION_CONTROL_IND_V02 0x0026
#define QMI_LOC_GET_XTRA_T_SESSION_CONTROL_REQ_V02 0x0027
#define QMI_LOC_GET_XTRA_T_SESSION_CONTROL_RESP_V02 0x0027
#define QMI_LOC_GET_XTRA_T_SESSION_CONTROL_IND_V02 0x0027
#define QMI_LOC_INJECT_WIFI_POSITION_REQ_V02 0x0028
#define QMI_LOC_INJECT_WIFI_POSITION_RESP_V02 0x0028
#define QMI_LOC_INJECT_WIFI_POSITION_IND_V02 0x0028
#define QMI_LOC_NOTIFY_WIFI_STATUS_REQ_V02 0x0029
#define QMI_LOC_NOTIFY_WIFI_STATUS_RESP_V02 0x0029
#define QMI_LOC_NOTIFY_WIFI_STATUS_IND_V02 0x0029
#define QMI_LOC_GET_REGISTERED_EVENTS_REQ_V02 0x002A
#define QMI_LOC_GET_REGISTERED_EVENTS_RESP_V02 0x002A
#define QMI_LOC_GET_REGISTERED_EVENTS_IND_V02 0x002A
#define QMI_LOC_SET_OPERATION_MODE_REQ_V02 0x002B
#define QMI_LOC_SET_OPERATION_MODE_RESP_V02 0x002B
#define QMI_LOC_SET_OPERATION_MODE_IND_V02 0x002B
#define QMI_LOC_GET_OPERATION_MODE_REQ_V02 0x002C
#define QMI_LOC_GET_OPERATION_MODE_RESP_V02 0x002C
#define QMI_LOC_GET_OPERATION_MODE_IND_V02 0x002C
#define QMI_LOC_SET_SPI_STATUS_REQ_V02 0x002D
#define QMI_LOC_SET_SPI_STATUS_RESP_V02 0x002D
#define QMI_LOC_SET_SPI_STATUS_IND_V02 0x002D
#define QMI_LOC_INJECT_SENSOR_DATA_REQ_V02 0x002E
#define QMI_LOC_INJECT_SENSOR_DATA_RESP_V02 0x002E
#define QMI_LOC_INJECT_SENSOR_DATA_IND_V02 0x002E
#define QMI_LOC_INJECT_TIME_SYNC_DATA_REQ_V02 0x002F
#define QMI_LOC_INJECT_TIME_SYNC_DATA_RESP_V02 0x002F
#define QMI_LOC_INJECT_TIME_SYNC_DATA_IND_V02 0x002F
#define QMI_LOC_SET_CRADLE_MOUNT_CONFIG_REQ_V02 0x0030
#define QMI_LOC_SET_CRADLE_MOUNT_CONFIG_RESP_V02 0x0030
#define QMI_LOC_SET_CRADLE_MOUNT_CONFIG_IND_V02 0x0030
#define QMI_LOC_GET_CRADLE_MOUNT_CONFIG_REQ_V02 0x0031
#define QMI_LOC_GET_CRADLE_MOUNT_CONFIG_RESP_V02 0x0031
#define QMI_LOC_GET_CRADLE_MOUNT_CONFIG_IND_V02 0x0031
#define QMI_LOC_SET_EXTERNAL_POWER_CONFIG_REQ_V02 0x0032
#define QMI_LOC_SET_EXTERNAL_POWER_CONFIG_RESP_V02 0x0032
#define QMI_LOC_SET_EXTERNAL_POWER_CONFIG_IND_V02 0x0032
#define QMI_LOC_GET_EXTERNAL_POWER_CONFIG_REQ_V02 0x0033
#define QMI_LOC_GET_EXTERNAL_POWER_CONFIG_RESP_V02 0x0033
#define QMI_LOC_GET_EXTERNAL_POWER_CONFIG_IND_V02 0x0033
#define QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_REQ_V02 0x0034
#define QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_RESP_V02 0x0034
#define QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02 0x0034
/**
    @}
  */

/* Service Object Accessor */
/** @addtogroup wms_qmi_accessor
    @{
  */
/** This function is used internally by the autogenerated code.  Clients should use the
   macro qmiLoc_get_service_object_v02( ) that takes in no arguments. */
qmi_idl_service_object_type qmiLoc_get_service_object_internal_v02
 ( int32_t idl_maj_version, int32_t idl_min_version, int32_t library_version );

/** This macro should be used to get the service object */
#define qmiLoc_get_service_object_v02( ) \
          qmiLoc_get_service_object_internal_v02( \
            QMILOC_V02_IDL_MAJOR_VERS, QMILOC_V02_IDL_MINOR_VERS, \
            QMILOC_V02_IDL_TOOL_VERS )
/**
    @}
  */


#ifdef __cplusplus
}
#endif
#endif

