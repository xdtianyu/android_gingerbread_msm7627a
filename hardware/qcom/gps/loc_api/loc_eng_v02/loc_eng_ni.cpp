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
#define LOG_NIDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>

#include <loc_eng.h>
#include "loc_eng_ni.h"
#include "loc_api_sync_req.h"

//logging
#define LOG_TAG "loc_eng_v02"
#include "loc_util_log.h"


/*=============================================================================
 *
 *                             DATA DECLARATION
 *
 *============================================================================*/

void loc_eng_ni_init(GpsNiCallbacks *callbacks);


const GpsNiInterface sLocEngNiInterface =
{
   sizeof(GpsNiInterface),
   loc_eng_ni_init,
   loc_eng_ni_respond,
};

bool loc_eng_ni_data_init = false;
loc_eng_ni_data_s_type loc_eng_ni_data;

extern loc_eng_data_s_type loc_eng_data;
/* User response callback waiting conditional variable */
pthread_cond_t             user_cb_arrived_cond = PTHREAD_COND_INITIALIZER;

/* User callback waiting data block, protected by user_cb_data_mutex */
pthread_mutex_t            user_cb_data_mutex   = PTHREAD_MUTEX_INITIALIZER;

/*=============================================================================
 *
 *                             FUNCTION DECLARATIONS
 *
 *============================================================================*/
static void* loc_ni_thread_proc(void *threadid);
/*===========================================================================

FUNCTION respond_from_enum

DESCRIPTION
   Returns the name of the response

RETURN VALUE
   response name string

===========================================================================*/
static const char* respond_from_enum(qmiLocNiUserRespEnumT_v02 resp)
{
   switch (resp)
   {
   case eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02:
      return "accept";
   case eQMI_LOC_NI_LCS_NOTIFY_VERIFY_DENY_V02:
      return "deny";
   case eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02:
      return "no response";
   default:
      return NULL;
   }
}

/*===========================================================================

FUNCTION loc_ni_respond

DESCRIPTION
   Displays the NI request and awaits user input. If a previous request is
   in session, the new one is handled using sys.ni_default_response (if exists);
   otherwise, it is denied.

DEPENDENCY
   Do not lock the data by mutex loc_ni_lock

RETURN VALUE
   none

===========================================================================*/
static void loc_ni_respond
(
      qmiLocNiUserRespEnumT_v02 resp,
      const qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *request_pass_back
)
{
   locClientReqUnionType req_union;
   locClientStatusEnumType status;
   LOC_UTIL_LOGD("Sending NI response: %s\n", respond_from_enum(resp));

   qmiLocNiUserRespReqMsgT_v02 ni_resp;
   qmiLocNiUserRespIndMsgT_v02 ni_resp_ind;

   memset(&ni_resp,0, sizeof(ni_resp));

   ni_resp.userResp = resp;
   ni_resp.notificationType = request_pass_back->notificationType;

   // copy SUPL payload from request
   if(request_pass_back->NiSuplInd_valid == 1)
   {
      ni_resp.NiSuplPayload_valid = 1;
      memcpy(&(ni_resp.NiSuplPayload), &(request_pass_back->NiSuplInd),
             sizeof(qmiLocNiSuplNotifyVerifyStructT_v02));

   }
   // should this be an "else if"?? we don't need to decide

   // copy UMTS-CP payload from request
   if( request_pass_back->NiUmtsCpInd_valid == 1 )
   {
      ni_resp.NiUmtsCpPayload_valid = 1;
      memcpy(&(ni_resp.NiUmtsCpPayload), &(request_pass_back->NiUmtsCpInd),
             sizeof(qmiLocNiUmtsCpNotifyVerifyStructT_v02));
   }

   //copy Vx payload from the request
   if( request_pass_back->NiVxInd_valid == 1)
   {
      ni_resp.NiVxPayload_valid = 1;
      memcpy(&(ni_resp.NiVxPayload), &(request_pass_back->NiVxInd),
             sizeof(qmiLocNiVxNotifyVerifyStructT_v02));
   }

   // copy Vx service interaction payload from the request
   if(request_pass_back->NiVxServiceInteractionInd_valid == 1)
   {
      ni_resp.NiVxServiceInteractionPayload_valid = 1;
      memcpy(&(ni_resp.NiVxServiceInteractionPayload),
             &(request_pass_back->NiVxServiceInteractionInd),
             sizeof(qmiLocNiVxServiceInteractionStructT_v02));
   }

   req_union.pNiUserRespReq = &ni_resp;

   status = loc_sync_send_req (
      loc_eng_data.client_handle, QMI_LOC_NI_USER_RESPONSE_REQ_V02,
      req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
      QMI_LOC_NI_USER_RESPONSE_IND_V02, &ni_resp_ind);

   LOC_UTIL_LOGD("loc_ni_respond: status = %d, resp_status = %d\n",
                 status, ni_resp_ind.status);

}

/*===========================================================================

FUNCTION loc_ni_fill_notif_verify_type

DESCRIPTION
   Fills need_notify, need_verify, etc.

RETURN VALUE
   none

===========================================================================*/
static bool loc_ni_fill_notif_verify_type(GpsNiNotification *notif,
      qmiLocNiNotifyVerifyEnumT_v02 notif_priv)
{
   notif->notify_flags       = 0;
   notif->default_response   = GPS_NI_RESPONSE_NORESP;

   switch (notif_priv)
   {
   case eQMI_LOC_NI_USER_NO_NOTIFY_NO_VERIFY_V02:
      notif->notify_flags = 0;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_ONLY_V02:
      notif->notify_flags = GPS_NI_NEED_NOTIFY;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_ALLOW_NO_RESP_V02:
      notif->notify_flags = GPS_NI_NEED_NOTIFY | GPS_NI_NEED_VERIFY;
      notif->default_response = GPS_NI_RESPONSE_ACCEPT;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_NOT_ALLOW_NO_RESP_V02:
      notif->notify_flags = GPS_NI_NEED_NOTIFY | GPS_NI_NEED_VERIFY;
      notif->default_response = GPS_NI_RESPONSE_DENY;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02:
      notif->notify_flags = GPS_NI_PRIVACY_OVERRIDE;
      break;

   default:
      return false;
   }

   return true;
}

/*===========================================================================

FUNCTION hexcode

DESCRIPTION
   Converts a binary array into a Hex string. E.g., 1F 00 3F --> "1F003F"

RETURN VALUE
   bytes encoded

===========================================================================*/
static int hexcode(char *hexstring, int string_size, const char *data, int data_size)
{
   int i;
   for (i = 0; i < data_size; i++)
   {
      char ch = data[i];
      if (i*2 + 3 <= string_size)
      {
         snprintf(&hexstring[i*2], 3, "%02X", ch);
      }
      else {
         break;
      }
   }
   return i;
}

/*===========================================================================

FUNCTION decode_address

DESCRIPTION
   Converts a binary encoding into an address string. E.g., 91 21 F3 --> "123".
The 91 is a prefix, hex digits are reversed in order. 0xF means the absence of
a digit.

RETURN VALUE
   number of bytes encoded

===========================================================================*/
static int decode_address(char *addr_string, int string_size, const char *data, int data_size)
{
   const char addr_prefix = 0x91;
   int i, idxOutput = 0;

   if (!data || !addr_string) { return 0; }

   if (data[0] != addr_prefix)
   {
      LOC_UTIL_LOGW("decode_address: address prefix is not 0x%x but 0x%x", addr_prefix, data[0]);
      addr_string[0] = '\0';
      return 0; // prefix not correct
   }

   for (i = 1; i < data_size; i++)
   {
      unsigned char ch = data[i], low = ch & 0x0F, hi = ch >> 4;
      if (low <= 9 && idxOutput < string_size - 1) { addr_string[idxOutput++] = low + '0'; }
      if (hi <= 9 && idxOutput < string_size - 1) { addr_string[idxOutput++] = hi + '0'; }
   }

   addr_string[idxOutput] = '\0'; // Terminates the string

   return idxOutput;
}

static GpsNiEncodingType convert_encoding_type(qmiLocNiDataCodingSchemeEnumT_v02 loc_encoding)
{
   GpsNiEncodingType enc = GPS_ENC_UNKNOWN;

   switch (loc_encoding)
   {
   case eQMI_LOC_NI_SUPL_UTF8_V02:
      enc = GPS_ENC_SUPL_UTF8;
      break;
   case eQMI_LOC_NI_SUPL_UCS2_V02:
      enc = GPS_ENC_SUPL_UCS2;
      break;
   case eQMI_LOC_NI_SUPL_GSM_DEFAULT_V02:
      enc = GPS_ENC_SUPL_GSM_DEFAULT;
      break;
   case eQMI_LOC_NI_SS_LANGUAGE_UNSPEC_V02:
      enc = GPS_ENC_SUPL_GSM_DEFAULT; // SS_LANGUAGE_UNSPEC = GSM
      break;
   default:
      break;
   }

   return enc;
}

/*===========================================================================

FUNCTION loc_ni_request_handler

DESCRIPTION
   Displays the NI request and awaits user input. If a previous request is
   in session, it is ignored.

RETURN VALUE
   none

===========================================================================*/
static void loc_ni_request_handler(
   const char *msg, const qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *ni_req_ptr)
{
   GpsNiNotification notif;
   char lcs_addr[32]; // Decoded LCS address for UMTS CP NI

   notif.size = sizeof(notif);
   strlcpy(notif.text, "[text]", sizeof notif.text);    // defaults
   strlcpy(notif.requestor_id, "[requestor id]", sizeof notif.requestor_id);

   /* If busy, use default or deny */
   if (loc_eng_ni_data.notif_in_progress)
   {
      /* XXX Consider sending a NO RESPONSE reply or queue the request */
      LOC_UTIL_LOGW("loc_ni_request_handler, notification in progress, "
                    "new NI request ignored %s\n", msg);
   }
   else
   {
      /* Print notification */
      LOC_UTIL_LOGD("NI Notification: %s \n", msg );

      pthread_mutex_lock(&loc_eng_ni_data.loc_ni_lock);

      /* Save request */
      memcpy(&loc_eng_ni_data.loc_ni_request, ni_req_ptr, sizeof loc_eng_ni_data.loc_ni_request);

      /* Set up NI response waiting */
      loc_eng_ni_data.notif_in_progress = true;
      loc_eng_ni_data.current_notif_id = abs(rand());

      /* Fill in notification */
      notif.notification_id = loc_eng_ni_data.current_notif_id;

      const qmiLocNiVxNotifyVerifyStructT_v02 *vx_req;
      const qmiLocNiSuplNotifyVerifyStructT_v02 *supl_req;
      const qmiLocNiUmtsCpNotifyVerifyStructT_v02 *umts_cp_req;

      if(ni_req_ptr->NiVxInd_valid == 1)
      {
         vx_req = &(ni_req_ptr->NiVxInd);
         notif.ni_type     = GPS_NI_TYPE_VOICE;
         notif.timeout     = LOC_NI_NO_RESPONSE_TIME;
         memset(notif.extras, 0, sizeof notif.extras);
         memset(notif.text, 0, sizeof notif.text);
         memset(notif.requestor_id, 0, sizeof notif.requestor_id);

         // Requestor ID, the requestor id recieved is NULL terminated
         hexcode(notif.requestor_id, sizeof notif.requestor_id,
                 vx_req->requestorId, strlen(vx_req->requestorId));

         notif.text_encoding = 0; // No text and no encoding

         // ??? makes little sense as vx encoding scheme is very different
         // from SUPL which notif.requestor_id_encoding uses. commenting
         // and using unknown directly here

         // notif.requestor_id_encoding = convert_encoding_type(vx_req->encodingScheme);
         notif.requestor_id_encoding = GPS_ENC_UNKNOWN;

         // Set default_response & notify_flags
         loc_ni_fill_notif_verify_type(&notif, ni_req_ptr->notificationType);

         // Privacy override handling
         if (ni_req_ptr->notificationType ==
             eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02)
         {
            loc_eng_mute_one_session();
         }
      } // end if ni_req_ptr->NiVxInd_valid == 1

      else if(ni_req_ptr->NiUmtsCpInd_valid == 1)
      {
         umts_cp_req = &ni_req_ptr->NiUmtsCpInd;
         notif.ni_type     = GPS_NI_TYPE_UMTS_CTRL_PLANE;
         notif.timeout     = LOC_NI_NO_RESPONSE_TIME; // umts_cp_req->user_response_timer;
         memset(notif.extras, 0, sizeof notif.extras);
         memset(notif.text, 0, sizeof notif.text);
         memset(notif.requestor_id, 0, sizeof notif.requestor_id);

           // notificationText should always be a NULL terminated string
         hexcode(notif.text, sizeof notif.text, umts_cp_req->notificationText,
                 strlen(umts_cp_req->notificationText));

        // Stores requestor ID

         hexcode(notif.requestor_id, sizeof notif.requestor_id,
                 umts_cp_req->requestorId.codedString,
                 strlen(umts_cp_req->requestorId.codedString));

         // Encodings
         notif.text_encoding = convert_encoding_type(umts_cp_req->dataCodingScheme);
         notif.requestor_id_encoding =
            convert_encoding_type(umts_cp_req->requestorId.dataCodingScheme);

         // LCS address (using extras field)
         if ( strlen(umts_cp_req->clientAddress) != 0)
         {
            // Copy LCS Address into notif.extras in the format: Address = 012345
            strlcat(notif.extras, LOC_NI_NOTIF_KEY_ADDRESS, sizeof notif.extras);
            strlcat(notif.extras, " = ", sizeof notif.extras);
            int addr_len = 0;
            const char *address_source = NULL;
            address_source = umts_cp_req->clientAddress;
            // client Address is always NULL terminated
            addr_len = decode_address(lcs_addr, sizeof lcs_addr, address_source,
                                     strlen(umts_cp_req->clientAddress));

           // The address is ASCII string
            if (addr_len)
            {
               strlcat(notif.extras, lcs_addr, sizeof notif.extras);
            }
         }

         // Set default_response & notify_flags
         loc_ni_fill_notif_verify_type(&notif, ni_req_ptr->notificationType);

         // Privacy override handling
         if (ni_req_ptr->notificationType ==
             eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02)
         {
            loc_eng_mute_one_session();
         }

      }//if(ni_req_ptr->NiUmtsCpInd_valid == 1)

      else if(ni_req_ptr->NiSuplInd_valid == 1)
      {
         supl_req = &ni_req_ptr->NiSuplInd;
         notif.ni_type     = GPS_NI_TYPE_UMTS_SUPL;
         notif.timeout     = LOC_NI_NO_RESPONSE_TIME; // supl_req->user_response_timer;
         memset(notif.extras, 0, sizeof notif.extras);
         memset(notif.text, 0, sizeof notif.text);
         memset(notif.requestor_id, 0, sizeof notif.requestor_id);
         // Client name
         if (supl_req->valid_flags & QMI_LOC_SUPL_CLIENT_NAME_MASK_V02)
         {
            hexcode(notif.text, sizeof notif.text,
                   supl_req->clientName.formattedString,   /* buffer */
                   strlen(supl_req->clientName.formattedString) /* length */
               );
           LOC_UTIL_LOGV("SUPL NI: client_name: %s \n", notif.text);
         }
         else
         {
           LOC_UTIL_LOGV("SUPL NI: client_name not present.");
         }

         // Requestor ID
         if (supl_req->valid_flags & QMI_LOC_SUPL_REQUESTOR_ID_MASK_V02)
         {
           hexcode(notif.requestor_id, sizeof notif.requestor_id,
                   supl_req->requestorId.formattedString,  /* buffer */
                   strlen(supl_req->requestorId.formattedString)           /* length */
               );
           LOC_UTIL_LOGV("SUPL NI: requestor_id: %s \n", notif.requestor_id);
         }
         else
         {
           LOC_UTIL_LOGV("SUPL NI: requestor_id not present.");
         }

        // Encoding type
         if (supl_req->valid_flags & QMI_LOC_SUPL_DATA_CODING_SCHEME_MASK_V02)
         {
           notif.text_encoding = convert_encoding_type(supl_req->dataCodingScheme);
           notif.requestor_id_encoding = convert_encoding_type(supl_req->dataCodingScheme);
         }
         else
         {
           notif.text_encoding = notif.requestor_id_encoding = GPS_ENC_UNKNOWN;
         }

         // Set default_response & notify_flags
         loc_ni_fill_notif_verify_type(&notif, ni_req_ptr->notificationType);

         // Privacy override handling
         if (ni_req_ptr->notificationType ==
            eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02)
         {
           loc_eng_mute_one_session();
         }

      } //ni_req_ptr->NiSuplInd_valid == 1
      else
      {
         LOC_UTIL_LOGE("loc_ni_request_handler, unknown request event \n");
         return;
      }

    /* Log requestor ID and text for debugging */
      LOC_UTIL_LOGI("Notification: notif_type: %d, timeout: %d, default_resp: %d", notif.ni_type, notif.timeout, notif.default_response);
      LOC_UTIL_LOGI("              requestor_id: %s (encoding: %d)", notif.requestor_id, notif.requestor_id_encoding);
      LOC_UTIL_LOGI("              text: %s text (encoding: %d)", notif.text, notif.text_encoding);
      if (notif.extras[0])
      {
         LOC_UTIL_LOGI("              extras: %s", notif.extras);
      }

      /* For robustness, spawn a thread at this point to timeout to clear up the notification status, even though
     * the OEM layer in java does not do so.
     **/
      loc_eng_ni_data.response_time_left = 5 + (notif.timeout != 0 ? notif.timeout : LOC_NI_NO_RESPONSE_TIME);
      LOC_UTIL_LOGI("Automatically sends 'no response' in %d seconds (to clear status)\n", loc_eng_ni_data.response_time_left);

      /* @todo may required when android framework issue is fixed
     * loc_eng_ni_data.callbacks_ref->create_thread_cb("loc_api_ni", loc_ni_thread_proc, NULL);
     */

      int rc = 0;
      rc = pthread_create(&loc_eng_ni_data.loc_ni_thread, NULL, loc_ni_thread_proc, NULL);
      if (rc)
      {
         LOC_UTIL_LOGE("Loc NI thread is not created.\n");
      }
      rc = pthread_detach(loc_eng_ni_data.loc_ni_thread);
      if (rc)
      {
         LOC_UTIL_LOGE("Loc NI thread is not detached.\n");
      }
      pthread_mutex_unlock(&loc_eng_ni_data.loc_ni_lock);

    /* Notify callback */
      if (loc_eng_data.ni_notify_cb != NULL)
      {
         loc_eng_data.ni_notify_cb(&notif);
      }
   }
}

/*===========================================================================

FUNCTION loc_ni_process_user_response

DESCRIPTION
   Handles user input from the UI

RETURN VALUE
   error code (0 for successful, -1 for error)

===========================================================================*/
int loc_ni_process_user_response(GpsUserResponseType userResponse)
{
   int rc=0;
   LOC_UTIL_LOGD("NI response from UI: %d", userResponse);

   qmiLocNiUserRespEnumT_v02 resp;

   switch (userResponse)
   {
   case GPS_NI_RESPONSE_ACCEPT:
      resp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02;
      break;
   case GPS_NI_RESPONSE_DENY:
      resp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_DENY_V02;
      break;
   case GPS_NI_RESPONSE_NORESP:
      resp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02;
      break;
   default:
      return -1;
   }
   /* Turn of the timeout*/
   pthread_mutex_lock(&user_cb_data_mutex);
   // copy the response to loc_eng_ni_data
   loc_eng_ni_data.loc_ni_resp.userResp = resp;
   loc_eng_ni_data.user_response_received = true;
   rc = pthread_cond_signal(&user_cb_arrived_cond);
   pthread_mutex_unlock(&user_cb_data_mutex);
   return 0;
}

/*===========================================================================

FUNCTION loc_eng_ni_callback

DESCRIPTION
   Loc API callback handler

RETURN VALUE
   error code (0 for success)

===========================================================================*/
int loc_eng_ni_callback (
     const qmiLocEventNiNotifyVerifyReqIndMsgT_v02  *ni_req_ptr
)
{
   int rc = 0;

   if (ni_req_ptr->NiVxInd_valid == 1)
   {
      LOC_UTIL_LOGD("VX Notification");
      loc_ni_request_handler("VX Notify", ni_req_ptr);
   }

   else if(ni_req_ptr->NiUmtsCpInd_valid == 1)
   {
      LOC_UTIL_LOGD("UMTS CP Notification\n");
      loc_ni_request_handler("UMTS CP Notify", ni_req_ptr);
   }

   else if(ni_req_ptr->NiSuplInd_valid == 1)
   {
      LOC_UTIL_LOGD("SUPL Notification\n");
      loc_ni_request_handler("SUPL Notify", ni_req_ptr);
   }

   else
   {
      LOC_UTIL_LOGE("Invalid NI event \n");
   }

   return rc;
}

/*===========================================================================

FUNCTION loc_ni_thread_proc

===========================================================================*/
static void* loc_ni_thread_proc(void *threadid)
{
   int rc = 0;          /* return code from pthread calls */

   struct timeval present_time;
   struct timespec expire_time;

   LOC_UTIL_LOGD("Starting Loc NI thread...\n");
   pthread_mutex_lock(&user_cb_data_mutex);
   /* Calculate absolute expire time */
   gettimeofday(&present_time, NULL);
   expire_time.tv_sec  = present_time.tv_sec;
   expire_time.tv_nsec = present_time.tv_usec * 1000;
   expire_time.tv_sec += loc_eng_ni_data.response_time_left;
   LOC_UTIL_LOGD("loc_ni_thread_proc-Time out set for abs time %ld\n", (long) expire_time.tv_sec );

   while (!loc_eng_ni_data.user_response_received)
   {
      rc = pthread_cond_timedwait(&user_cb_arrived_cond, &user_cb_data_mutex, &expire_time);
      if (rc == ETIMEDOUT)
      {
         loc_eng_ni_data.loc_ni_resp.userResp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02;
         LOC_UTIL_LOGD("loc_ni_thread_proc-Thread time out after waiting"
                       " for specified time. Ret Val %d\n",rc );
         break;
      }
   }
   if (loc_eng_ni_data.user_response_received == true)
   {
      LOC_UTIL_LOGD("loc_ni_thread_proc-Java layer has sent us a user response and return value from "
               "pthread_cond_timedwait = %d\n",rc );
      loc_eng_ni_data.user_response_received = false; /* Reset the user response flag for the next session*/
   }
   loc_ni_respond(loc_eng_ni_data.loc_ni_resp.userResp, &loc_eng_ni_data.loc_ni_request);
   pthread_mutex_unlock(&user_cb_data_mutex);
   pthread_mutex_lock(&loc_eng_ni_data.loc_ni_lock);
   loc_eng_ni_data.notif_in_progress = false;
   loc_eng_ni_data.response_time_left = 0;
   loc_eng_ni_data.current_notif_id = -1;
   pthread_mutex_unlock(&loc_eng_ni_data.loc_ni_lock);
   return NULL;
}

/*===========================================================================
FUNCTION    loc_eng_ni_init

DESCRIPTION
   This function initializes the NI interface

DEPENDENCIES
   NONE

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_ni_init(GpsNiCallbacks *callbacks)
{
   LOC_UTIL_LOGD("loc_eng_ni_init: entered.");

   if (!loc_eng_ni_data_init)
   {
      pthread_mutex_init(&loc_eng_ni_data.loc_ni_lock, NULL);
      loc_eng_ni_data_init = true;
   }

   loc_eng_ni_data.notif_in_progress = false;
   loc_eng_ni_data.current_notif_id = -1;
   loc_eng_ni_data.response_time_left = 0;
   loc_eng_ni_data.user_response_received = false;

   srand(time(NULL));
   loc_eng_data.ni_notify_cb = callbacks->notify_cb;
}

/*===========================================================================
FUNCTION    loc_eng_ni_respond

DESCRIPTION
   This function sends an NI respond to the modem processor

DEPENDENCIES
   NONE

RETURN VALUE
   None

SIDE EFFECTS
   N/A

===========================================================================*/
void loc_eng_ni_respond(int notif_id, GpsUserResponseType user_response)
{
   if (notif_id == loc_eng_ni_data.current_notif_id &&
         loc_eng_ni_data.notif_in_progress)
   {
      LOC_UTIL_LOGI("loc_eng_ni_respond: send user response %d for notif %d", user_response, notif_id);
      loc_ni_process_user_response(user_response);
   }
   else {
      LOC_UTIL_LOGE("loc_eng_ni_respond: notif_id %d mismatch or notification not in progress, response: %d",
            notif_id, user_response);
   }
}
