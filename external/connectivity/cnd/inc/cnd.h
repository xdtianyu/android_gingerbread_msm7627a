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
#ifndef CND_H
#define CND_H

#include <stdlib.h>
#include <sys/time.h>
#include <utils/Log.h>
#include "cne.h"

#define CNE_DEBUG_LOGGING 0

#define CNE_LOGE(...) LOGE(__VA_ARGS__)
#define CNE_LOGW(...) LOGW(__VA_ARGS__)
#define CNE_LOGI(...) \
    ( (CONDITION(CNE_DEBUG_LOGGING)) \
    ? (void)LOG(LOG_INFO, LOCAL_TAG, __VA_ARGS__) \
    : (void)0 )
#define CNE_LOGD(...) \
    ( (CONDITION(CNE_DEBUG_LOGGING)) \
    ? (void)LOG(LOG_DEBUG, LOCAL_TAG, __VA_ARGS__) \
    : (void)0 )
#define CNE_LOGV(...) \
    ( (CONDITION(CNE_DEBUG_LOGGING)) \
    ? (void)LOG(LOG_VERBOSE, LOCAL_TAG, __VA_ARGS__) \
    : (void)0 )

#ifdef __cplusplus
extern "C" {
#endif


typedef void * CND_Token;

typedef enum {
    CND_E_SUCCESS = 0,
    CND_E_RADIO_NOT_AVAILABLE = 1,     /* If radio did not start or is resetting */
    CND_E_GENERIC_FAILURE = 2,
    CND_E_INVALID_RESPONSE

} CND_Errno;




void cnd_init(void);
void cnd_startEventLoop(void);



#ifdef __cplusplus
}
#endif

#endif /* CND_H */

