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

#include <sys/timeb.h>
#include <stdlib.h>
#include <time.h>

#include <qcc/time.h>


namespace qcc {

uint32_t qcc::GetTimestamp(void)
{
    static uint32_t base = 0;
    struct _timeb timebuffer;
    uint32_t ret_val;

    _ftime(&timebuffer);

    ret_val = ((uint32_t)timebuffer.time) * 1000;
    ret_val += timebuffer.millitm;

#ifdef RANDOM_TIMESTAMPS
    /*
     * Randomize time base
     */
    while (!base) {
        srand(ret_val);
        base = rand() | (rand() << 16);
    }
#endif

    return ret_val + base;
}

void qcc::GetTimeNow(Timespec* ts)
{
    struct _timeb timebuffer;

    _ftime(&timebuffer);

    ts->seconds = timebuffer.time;
    ts->mseconds = timebuffer.millitm;
}

}  /* namespace */
