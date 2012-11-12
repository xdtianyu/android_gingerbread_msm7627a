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
 *
 */

#ifndef LOC_ENG_DBG_H
#define LOC_ENG_DBG_H

#ifndef DEBUG_X86

#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0
#define LOG_NVDEBUG 0

#define LOG_TAG "libloc"
#include <utils/Log.h>

#ifndef DEBUG_DMN_LOC_API

#include <rpc/rpc.h>
#include "loc_api_rpc_glue.h"
#include "loc_apicb_appinit.h"

#include <hardware/gps.h>
#include <cutils/properties.h>
#include <string.h>

#include <loc_eng_log.h>
#include <loc_eng_cfg.h>

/* LOGGING MACROS */
#define LOC_LOGE(...) \
if (gps_conf.DEBUG_LEVEL >= 1) { LOGE(__VA_ARGS__); }

#define LOC_LOGW(...) \
if (gps_conf.DEBUG_LEVEL >= 2) { LOGW(__VA_ARGS__); }

#define LOC_LOGI(...) \
if (gps_conf.DEBUG_LEVEL >= 3) { LOGI(__VA_ARGS__); }

#define LOC_LOGD(...) \
if (gps_conf.DEBUG_LEVEL >= 4) { LOGD(__VA_ARGS__); }

#define LOC_LOGV(...) \
if (gps_conf.DEBUG_LEVEL >= 5) { LOGV(__VA_ARGS__); }

#else /* DEBUG_DMN_LOC_API */

#define LOC_LOGE(...) LOGE(__VA_ARGS__)

#define LOC_LOGW(...) LOGW(__VA_ARGS__)

#define LOC_LOGI(...) LOGI(__VA_ARGS__)

#define LOC_LOGD(...) LOGD(__VA_ARGS__)

#define LOC_LOGV(...) LOGV(__VA_ARGS__)

#endif /* DEBUG_DMN_LOC_API */

#else /* DEBUG_X86 */

#include <stdio.h>

#define FPRINTF fprintf

#define LOC_LOGE(...) FPRINTF(stderr, __VA_ARGS__)

#define LOC_LOGW(...) FPRINTF(stderr, __VA_ARGS__)

#define LOC_LOGI(...) FPRINTF(stderr, __VA_ARGS__)

#define LOC_LOGD(...) FPRINTF(stderr, __VA_ARGS__)

#define LOC_LOGV(...) FPRINTF(stderr, __VA_ARGS__)

#endif /* DEBUG_X86 */

#endif // LOC_ENG_DBG_H
