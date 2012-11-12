/**
 * @file
 *
 * Platform specific header files that defines time related functions
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <qcc/platform.h>

#include <time.h>

#include <qcc/time.h>

using namespace qcc;

static time_t s_clockOffset = 0;

uint32_t qcc::GetTimestamp(void)
{
    struct timespec ts;
    uint32_t ret_val;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (0 == s_clockOffset) {
        s_clockOffset = ts.tv_sec;
    }

    ret_val = ((uint32_t)(ts.tv_sec - s_clockOffset)) * 1000;
    ret_val += (uint32_t)ts.tv_nsec / 1000000;

    return ret_val;
}

void qcc::GetTimeNow(Timespec* ts)
{
    struct timespec _ts;
    clock_gettime(CLOCK_MONOTONIC, &_ts);
    ts->seconds = _ts.tv_sec;
    ts->mseconds = _ts.tv_nsec / 1000000;
}
