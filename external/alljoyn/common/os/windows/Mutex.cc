/**
 * @file
 *
 * Define a class that abstracts Windows mutexs.
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

#include <windows.h>
#include <stdio.h>

#include <qcc/Thread.h>
#include <qcc/Mutex.h>

/** @internal */
#define QCC_MODULE "MUTEX"

using namespace qcc;

void Mutex::Init()
{
    /* default security attributes, initially not owned, unnamed mutex */
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (!mutex) {
        char buf[80];
        uint32_t ret = GetLastError();
        strerror_r(ret, buf, sizeof(buf));
        // Can't use ER_LogError() since it uses mutexs under the hood.
        printf("**** Mutex attribute initialization failure: %u - %s", ret, buf);
    }

}

Mutex::~Mutex()
{
    if (mutex && !CloseHandle(mutex)) {
        char buf[80];
        uint32_t ret = GetLastError();
        strerror_r(ret, buf, sizeof(buf));
        // Can't use ER_LogError() since it uses mutexs under the hood.
        printf("***** Mutex(0x%x) destruction failure: %u - %s", mutex, ret, buf);
    }
}

QStatus Mutex::Lock(void)
{
    if (!mutex) {
        return ER_INIT_FAILED;
    }
    uint32_t waitResult = WaitForSingleObject(mutex, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        char buf[80];
        uint32_t ret = GetLastError();
        strerror_r(ret, buf, sizeof(buf));
        // Can't use ER_LogError() since it uses mutexs under the hood.
        printf("***** Thread %d Mutex(0x%x) lock failure: %u - %s", GetCurrentThreadId(), mutex, ret, buf);
        return ER_OS_ERROR;
    }
    return ER_OK;
}

QStatus Mutex::Unlock(void)
{
    if (!mutex) {
        return ER_INIT_FAILED;
    }
    if (!ReleaseMutex(mutex)) {
        char buf[80];
        uint32_t ret = GetLastError();
        strerror_r(ret, buf, sizeof(buf));
        // Can't use ER_LogError() since it uses mutexs under the hood.
        printf("***** Thread %d Mutex(0x%x) unlock failure: %u - %s", GetCurrentThreadId(), mutex, ret, buf);
        return ER_OS_ERROR;
    }
    return ER_OK;
}
