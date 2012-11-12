#ifndef REF_CNE_DEFS_H
#define REF_CNE_DEFS_H

/**----------------------------------------------------------------------------
  @file REFCNE_Defs.h

  This file holds various definations that get used across, different CNE
  modules.
-----------------------------------------------------------------------------*/


/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
 #include <utils/Log.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

  #define RCNE_LOG_TAG "RefCnE"

  #define RCNE_MSG_WARN(...) LOG(LOG_WARN,RCNE_LOG_TAG,__VA_ARGS__)
  #define RCNE_MSG_WARN1 RCNE_MSG_WARN
  #define RCNE_MSG_WARN2 RCNE_MSG_WARN
  #define RCNE_MSG_WARN3 RCNE_MSG_WARN
  #define RCNE_MSG_WARN4 RCNE_MSG_WARN
  #define RCNE_MSG_WARN5 RCNE_MSG_WARN

  #define RCNE_MSG_DEBUG(...) LOG(LOG_DEBUG,RCNE_LOG_TAG,__VA_ARGS__)
  #define RCNE_MSG_DEBUG1 RCNE_MSG_DEBUG
  #define RCNE_MSG_DEBUG2 RCNE_MSG_DEBUG
  #define RCNE_MSG_DEBUG3 RCNE_MSG_DEBUG
  #define RCNE_MSG_DEBUG4 RCNE_MSG_DEBUG
  #define RCNE_MSG_DEBUG5 RCNE_MSG_DEBUG

  #define RCNE_MSG_ERROR(...) LOG(LOG_ERROR,RCNE_LOG_TAG,__VA_ARGS__)
  #define RCNE_MSG_ERROR1 RCNE_MSG_ERROR
  #define RCNE_MSG_ERROR2 RCNE_MSG_ERROR
  #define RCNE_MSG_ERROR3 RCNE_MSG_ERROR
  #define RCNE_MSG_ERROR4 RCNE_MSG_ERROR
  #define RCNE_MSG_ERROR5 RCNE_MSG_ERROR

  #define RCNE_MSG_VERBOSE(...) LOG(LOG_VERBOSE,RCNE_LOG_TAG,__VA_ARGS__)
  #define RCNE_MSG_VERBOSE1 RCNE_MSG_VERBOSE
  #define RCNE_MSG_VERBOSE2 RCNE_MSG_VERBOSE
  #define RCNE_MSG_VERBOSE3 RCNE_MSG_VERBOSE
  #define RCNE_MSG_VERBOSE4 RCNE_MSG_VERBOSE
  #define RCNE_MSG_VERBOSE5 RCNE_MSG_VERBOSE

  #define RCNE_MSG_INFO(...) LOG(LOG_INFO,RCNE_LOG_TAG,__VA_ARGS__)
  #define RCNE_MSG_INFO1 RCNE_MSG_INFO
  #define RCNE_MSG_INFO2 RCNE_MSG_INFO
  #define RCNE_MSG_INFO3 RCNE_MSG_INFO
  #define RCNE_MSG_INFO4 RCNE_MSG_INFO
  #define RCNE_MSG_INFO5 RCNE_MSG_INFO

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/** Possible return codes */
typedef enum
{
  /* ADD other new error codes here */
  REF_CNE_RET_ERROR = -1,

  REF_CNE_RET_OK = 1,
} ref_cne_ret_enum_type;

typedef enum
{
  REF_CNE_NET_STATE_DISCONNECTED=0,
  REF_CNE_NET_STATE_CONNECTED,
  REF_CNE_NET_STATE_UNINITIALIZED,
} ref_cne_net_con_status_enum_type;

typedef enum
{
  REF_CNE_NET_PENDING_CONNECT=0,
  REF_CNE_NET_PENDING_DISCONNECT,
  REF_CNE_NET_NOT_PENDING,
} ref_cne_net_con_req_enum_type;

#ifndef TRUE
  #define TRUE   1   /* Boolean true value. */
#endif /* TRUE */

#ifndef FALSE
  #define FALSE  0   /* Boolean false value. */
#endif /* FALSE */

#ifndef NULL
  #define NULL  0
#endif /* NULL */

#ifndef MAX
   #define  MAX( x, y ) ( ((x) > (y)) ? (x) : (y) )
#endif /* MAX */

#ifndef MIN
   #define  MIN( x, y ) ( ((x) < (y)) ? (x) : (y) )
#endif /* MIN */

#endif /* REF_CNE_DEFS_H */
