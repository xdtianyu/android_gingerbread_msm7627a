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

#ifndef __LOG_UTIL_H__
#define __LOG_UTIL_H__

#ifdef LOG_UTIL_OFF_TARGET

#include <stdio.h>

//error logs
#define UTIL_LOGE(...) printf(__VA_ARGS__)
//warning logs
#define UTIL_LOGW(...) printf(__VA_ARGS__)
// debug logs
#define UTIL_LOGD(...) printf(__VA_ARGS__)
//info logs
#define UTIL_LOGI(...) printf(__VA_ARGS__)
//verbose logs
#define UTIL_LOGV(...) printf(__VA_ARGS__)

// get around strl*: not found in glibc
// TBD:look for presence of eglibc other libraries
// with strlcpy supported.
#define strlcpy(X,Y,Z) strcpy(X,Y)
#define strlcat(X,Y,Z) strcat(X,Y)

#elif defined(_ANDROID_)

#include <utils/Log.h>

//error logs
#define UTIL_LOGE(...)   LOGE(__VA_ARGS__)
//warning logs
#define UTIL_LOGW(...)   LOGW(__VA_ARGS__)
// debug logs
#define UTIL_LOGD(...)   LOGD(__VA_ARGS__)
// info logs
#define UTIL_LOGI(...)   LOGI(__VA_ARGS__)
// verbose logs
#define UTIL_LOGV(...)   LOGV(__VA_ARGS__)

#endif //LOG_UTIL_OFF_TARGET


#endif // __LOG_UTIL_H__
