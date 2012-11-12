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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include <loc_eng.h>
#include <loc_api_v02_client.h>
#include <qmi_loc_v02.h>
#include <loc_api_sync_req.h>

#define LOG_TAG "loc_eng_v02"
#include "loc_util_log.h"

#define LOC_XTRA_INJECT_DEFAULT_TIMEOUT (3100)
#define XTRA_BLOCK_SIZE                 (1024)

static int qct_loc_eng_xtra_init (GpsXtraCallbacks* callbacks);
static int qct_loc_eng_inject_xtra_data(char* data, int length);
static int qct_loc_eng_inject_xtra_data_proxy(char* data, int length);

const GpsXtraInterface sLocEngXTRAInterface =
{
    sizeof(GpsXtraInterface),
    qct_loc_eng_xtra_init,
    qct_loc_eng_inject_xtra_data,
    /* qct_loc_eng_inject_xtra_data_proxy, */ // This func buffers xtra data if GPS is in-session
};

/*===========================================================================
FUNCTION    qct_loc_eng_xtra_init

DESCRIPTION
   Initialize XTRA module.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
static int qct_loc_eng_xtra_init (GpsXtraCallbacks* callbacks)
{
   loc_eng_xtra_data_s_type *xtra_module_data_ptr;

   LOC_UTIL_LOGD("qct_loc_eng_inject_xtra_init: cb_ptr =%lu \n",
                 (unsigned long) callbacks->download_request_cb);

   xtra_module_data_ptr = &loc_eng_data.xtra_module_data;
   xtra_module_data_ptr->download_request_cb = callbacks->download_request_cb;

   //??? Set xtra_data_for_injection to NULL in the initialization
   // otherwise it may get injected twice.
   xtra_module_data_ptr->xtra_data_for_injection = NULL;

   return 0;
}

/*===========================================================================
FUNCTION    qct_loc_eng_inject_xtra_data

DESCRIPTION
   Injects XTRA file into the engine.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success
   error code > 0

SIDE EFFECTS
   N/A

===========================================================================*/
static int qct_loc_eng_inject_xtra_data(char* data, int length)
{
   int     ret_val = 0;
   locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
   int     total_parts;
   uint8_t   part;
   uint16_t  len_injected;

   locClientReqUnionType req_union;
   qmiLocInjectPredictedOrbitsDataReqMsgT_v02 inject_xtra;
   qmiLocInjectPredictedOrbitsDataIndMsgT_v02 inject_xtra_ind;

   req_union.pInjectPredictedOrbitsDataReq = &inject_xtra;

   LOC_UTIL_LOGD("qct_loc_eng_inject_xtra_data, xtra size = %d\n", length);

   inject_xtra.formatType_valid = 1;
   inject_xtra.formatType = eQMI_LOC_PREDICTED_ORBITS_XTRA_V02;
   inject_xtra.totalSize = length;

   total_parts = ((length - 1) / XTRA_BLOCK_SIZE) + 1;

   inject_xtra.totalParts = total_parts;

   len_injected = 0; // O bytes injected

   // XTRA injection starts with part 1
   for (part = 1; part <= total_parts; part++)
   {
      inject_xtra.partNum = part;

      if (XTRA_BLOCK_SIZE > (length - len_injected))
      {
         inject_xtra.partData_len = length - len_injected;
      }
      else
      {
          inject_xtra.partData_len = XTRA_BLOCK_SIZE;
      }

      // copy data into the message
      memcpy(inject_xtra.partData, data+len_injected, inject_xtra.partData_len);

 //???     predicted_orbits_data_ptr->data_ptr.data_ptr_val = data + len_injected;

      LOC_UTIL_LOGD("qct_loc_eng_inject_xtra_data, part %d/%d, len = %d, total injected = %d\n",
            inject_xtra.partNum, total_parts, inject_xtra.partData_len,
            len_injected);

      status = loc_sync_send_req( loc_eng_data.client_handle,
                                  QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_REQ_V02,
                                  req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                                  QMI_LOC_INJECT_PREDICTED_ORBITS_DATA_IND_V02,
                                  &inject_xtra_ind);

      if (status != eLOC_CLIENT_SUCCESS ||
          eQMI_LOC_SUCCESS_V02 != inject_xtra_ind.status ||
          inject_xtra.partNum != inject_xtra_ind.partNum)
      {
         LOC_UTIL_LOGE ("qct_loc_eng_inject_xtra_data failed  status = %d, "
                        "inject_pos_ind.status = %d, part num = %d, ind.partNum = %d\n",
                        status, inject_xtra_ind.status, inject_xtra.partNum,
                        inject_xtra_ind.partNum);

         ret_val = EIO;
         break;
      }


      len_injected += inject_xtra.partData_len;
      LOC_UTIL_LOGD("loc_ioctl XTRA injected length: %d\n", len_injected);
   }

   return ret_val;
}

/*===========================================================================
FUNCTION    loc_eng_inject_xtra_data_in_buffer

DESCRIPTION
   Injects buffered XTRA file into the engine and clears the buffer.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success
   >0: failure

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_inject_xtra_data_in_buffer()
{
   int rc = 0;

   pthread_mutex_lock(&loc_eng_data.xtra_module_data.lock);

   if (loc_eng_data.xtra_module_data.xtra_data_for_injection)
   {
      if (qct_loc_eng_inject_xtra_data(
            loc_eng_data.xtra_module_data.xtra_data_for_injection,
            loc_eng_data.xtra_module_data.xtra_data_len))
      {
         // FIXME gracefully handle injection error
         LOC_UTIL_LOGE("XTRA injection failed.");
         rc = -1;
      }

      // Clears buffered data
      free(loc_eng_data.xtra_module_data.xtra_data_for_injection);
      loc_eng_data.xtra_module_data.xtra_data_for_injection = NULL;
      loc_eng_data.xtra_module_data.xtra_data_len = 0;
   }

   pthread_mutex_unlock(&loc_eng_data.xtra_module_data.lock);

   return rc;
}

/*===========================================================================
FUNCTION    qct_loc_eng_inject_xtra_data_proxy

DESCRIPTION
   Injects XTRA file into the engine but buffers the data if engine is busy.

DEPENDENCIES
   N/A

RETURN VALUE
   0: success
   >0: failure

SIDE EFFECTS
   N/A

===========================================================================*/
static int qct_loc_eng_inject_xtra_data_proxy(char* data, int length)
{
   if (!data || !length) return -1;

   pthread_mutex_lock(&loc_eng_data.xtra_module_data.lock);

   char *buf = (char*) malloc(length);
   if (buf)
   {
      memcpy(buf, data, length);
      loc_eng_data.xtra_module_data.xtra_data_for_injection = buf;
      loc_eng_data.xtra_module_data.xtra_data_len = length;
   }

   pthread_mutex_unlock(&loc_eng_data.xtra_module_data.lock);

   //??? Why check this condition, shouldn't the signal be sent
   // irrespective? the thread will check if gps status is not on

   if (loc_eng_data.engine_status != GPS_STATUS_ENGINE_ON)
   {

      pthread_cond_signal(&loc_eng_data.deferred_action_cond);
   }

   return 0;
}
