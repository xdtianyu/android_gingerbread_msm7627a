/**
 * @file
 *
 * Windows implementation of event
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

#include <map>
#include <windows.h>
#include <winsock2.h>
#include <errno.h>
#include <time.h>

#include <qcc/Debug.h>
#include <qcc/Event.h>
#include <qcc/Mutex.h>
#include <qcc/Stream.h>
#include <qcc/Thread.h>
#include <qcc/time.h>

using namespace std;

/** @internal */
#define QCC_MODULE "EVENT"

namespace qcc {

/** Mutext that protects rdwrCounts map */
static Mutex countLock;

/** Track number of threads & events that are simultaneously waiting for READ or WRITE I/O events on a given fd */
static map<int, pair<uint16_t, uint16_t> > rdwrCounts;


Event Event::alwaysSet(0, 0);

Event Event::neverSet(WAIT_FOREVER, 0);

void Event::SetIOMask(Event& evt)
{
    long mask = 0;
    bool doEventSelect = false;

    countLock.Lock();
    pair<uint16_t, uint16_t> counts = rdwrCounts[evt.ioFd];

    if (IO_READ == evt.eventType) {
        doEventSelect = (0 == counts.first);
        ++counts.first;
    } else if (IO_WRITE == evt.eventType) {
        doEventSelect = (0 == counts.second);
        ++counts.second;
    }

    if (0 < counts.first) {
        mask |= FD_READ | FD_CLOSE | FD_ACCEPT;
    }
    if (0 < counts.second) {
        mask |= FD_WRITE | FD_CLOSE | FD_CONNECT;
    }
    rdwrCounts[evt.ioFd] = counts;
    countLock.Unlock();

    if (doEventSelect) {
        WSAResetEvent(evt.ioHandle);
        int ret = WSAEventSelect(evt.ioFd, evt.ioHandle, mask);
        if (0 != ret) {
            QCC_LogError(ER_OS_ERROR, ("WSAEventSelect failed. lastError=%d", WSAGetLastError()));
        }
    }
}

void Event::ReleaseIOMask(Event& evt)
{
    countLock.Lock();
    pair<uint16_t, uint16_t> counts = rdwrCounts[evt.ioFd];
    if (IO_READ == evt.eventType) {
        --counts.first;
    } else if (IO_WRITE == evt.eventType) {
        --counts.second;
    }
    if ((0 == counts.first) && (0 == counts.second)) {
        rdwrCounts.erase(evt.ioFd);
    } else {
        rdwrCounts[evt.ioFd] = counts;
    }
    countLock.Unlock();
}

QStatus Event::Wait(Event& evt, uint32_t maxWaitMs)
{
    HANDLE handles[3];
    int numHandles = 0;

    Thread* thread = Thread::GetThread();

    Event* stopEvent = &thread->GetStopEvent();
    handles[numHandles++] = stopEvent->handle;

    if (INVALID_HANDLE_VALUE != evt.handle) {
        handles[numHandles++] = evt.handle;
    }
    if (INVALID_HANDLE_VALUE != evt.ioHandle) {
        handles[numHandles++] = evt.ioHandle;
        SetIOMask(evt);
    }
    if (TIMED == evt.eventType) {
        uint32_t now = GetTimestamp();
        if (evt.timestamp <= now) {
            if (0 < evt.period) {
                evt.timestamp += (((now - evt.timestamp) / evt.period) + 1) * evt.period;
            }
            return ER_OK;
        } else if (WAIT_FOREVER == maxWaitMs || ((evt.timestamp - now) < maxWaitMs)) {
            maxWaitMs = evt.timestamp - now;
        }
    }

    evt.IncrementNumThreads();

    DWORD ret = WaitForMultipleObjectsEx(numHandles, handles, FALSE, maxWaitMs, FALSE);

    evt.DecrementNumThreads();

    QStatus status;

    if (INVALID_HANDLE_VALUE != evt.ioHandle) {
        ReleaseIOMask(evt);
    }
    if ((WAIT_OBJECT_0 <= ret) && ((WAIT_OBJECT_0 + numHandles) > ret)) {
        if (WAIT_OBJECT_0  == ret) {
            /* StopEvent fired */
            status = (thread && thread->IsStopping()) ? ER_STOPPING_THREAD : ER_ALERTED_THREAD;
        } else {
            status = ER_OK;
        }
    } else if (WAIT_TIMEOUT == ret) {
        if (TIMED == evt.eventType) {
            uint32_t now = GetTimestamp();
            if (now >= evt.timestamp) {
                if (0 < evt.period) {
                    evt.timestamp += (((now - evt.timestamp) / evt.period) + 1) * evt.period;
                }
                status = ER_OK;
            } else {
                status = ER_TIMEOUT;
            }
        } else {
            status = ER_TIMEOUT;
        }
    } else {
        status = ER_FAIL;
        QCC_LogError(status, ("WaitForMultipleObjectsEx returned 0x%x.", ret));
        if (WAIT_FAILED == ret) {
            QCC_LogError(status, ("GetLastError=%d", GetLastError()));
            QCC_LogError(status, ("numHandles=%d, maxWaitMs=%d, Handles: ", numHandles, maxWaitMs));
            for (int i = 0; i < numHandles; ++i) {
                QCC_LogError(status, ("  0x%x", handles[i]));
            }
        }
    }
    return status;
}


QStatus Event::Wait(const vector<Event*>& checkEvents, vector<Event*>& signaledEvents, uint32_t maxWaitMs)
{
    const int MAX_HANDLES = 16;

    int numHandles = 0;
    HANDLE handles[MAX_HANDLES];
    vector<Event*>::const_iterator it;

    for (it = checkEvents.begin(); it != checkEvents.end(); ++it) {
        Event* evt = *it;
        evt->IncrementNumThreads();
        if (INVALID_HANDLE_VALUE != evt->handle) {
            handles[numHandles++] = evt->handle;
            if (MAX_HANDLES <= numHandles) {
                break;
            }
        }
        if (INVALID_HANDLE_VALUE != evt->ioHandle) {
            handles[numHandles++] = evt->ioHandle;
            if (MAX_HANDLES <= numHandles) {
                break;
            }
            SetIOMask(*evt);
        }
        if (evt->eventType == TIMED) {
            uint32_t now = GetTimestamp();
            if (evt->timestamp <= now) {
                maxWaitMs = 0;
            } else if ((WAIT_FOREVER == maxWaitMs) || ((evt->timestamp - now) < maxWaitMs)) {
                maxWaitMs = evt->timestamp - now;
            }
        }
    }
    /* Restore thread counts if we are not going to block */
    if (numHandles >= MAX_HANDLES) {
        while (true) {
            (*it)->DecrementNumThreads();
            if (it == checkEvents.begin()) {
                break;
            }
            --it;
        }
        QCC_LogError(ER_FAIL, ("EREvent::Wait: Maximum number of HANDLES reached"));
        return ER_FAIL;
    }

    DWORD ret = WaitForMultipleObjectsEx(numHandles, handles, FALSE, maxWaitMs, FALSE);

    for (it = checkEvents.begin(); it != checkEvents.end(); ++it) {
        Event* evt = *it;
        (*it)->DecrementNumThreads();
        if (INVALID_HANDLE_VALUE != evt->ioHandle) {
            ReleaseIOMask(*evt);
        }
        if ((IO_READ == evt->eventType) || (IO_WRITE == evt->eventType) || (GEN_PURPOSE == evt->eventType)) {
            if ((WAIT_OBJECT_0 <= ret) && ((WAIT_OBJECT_0 + numHandles) > ret)) {
                if ((INVALID_HANDLE_VALUE != evt->handle) || (INVALID_HANDLE_VALUE != evt->ioHandle)) {
                    if (evt->IsSet()) {
                        signaledEvents.push_back(evt);
                    }
                }
            }
        } else if (TIMED == evt->eventType) {
            uint32_t now = GetTimestamp();
            if (now >= evt->timestamp) {
                if (0 < evt->period) {
                    evt->timestamp += (((now - evt->timestamp) / evt->period) + 1) * evt->period;
                }
                signaledEvents.push_back(evt);
            }
        }
    }


    QStatus status;
    if (((WAIT_OBJECT_0 <= ret) && ((WAIT_OBJECT_0 + numHandles) > ret)) || (WAIT_TIMEOUT == ret)) {
        status = signaledEvents.empty() ? ER_TIMEOUT : ER_OK;
    } else {
        status = ER_FAIL;
        QCC_LogError(status, ("WaitForMultipleObjectsEx(2) returned 0x%x.", ret));
        if (WAIT_FAILED == ret) {
            QCC_LogError(status, ("GetLastError=%d", GetLastError()));
            QCC_LogError(status, ("numHandles=%d, maxWaitMs=%d, Handles: ", numHandles, maxWaitMs));
            for (int i = 0; i < numHandles; ++i) {
                QCC_LogError(status, ("  0x%x", handles[i]));
            }
        }
    }
    return status;
}

Event::Event() : handle(CreateEvent(NULL, TRUE, FALSE, NULL)),
    ioHandle(INVALID_HANDLE_VALUE),
    eventType(GEN_PURPOSE),
    timestamp(0),
    period(0),
    ioFd(-1),
    ownsIoHandle(false),
    numThreads(0)
{
}

Event::Event(Event& event, EventType eventType, bool genPurpose)
    : handle(INVALID_HANDLE_VALUE),
    ioHandle(event.ioHandle),
    eventType(eventType),
    timestamp(0),
    period(0),
    ioFd(event.ioFd),
    ownsIoHandle(false),
    numThreads(0)
{
    if (genPurpose) {
        handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
}

Event::Event(int ioFd, EventType eventType, bool genPurpose)
    : handle(INVALID_HANDLE_VALUE),
    ioHandle(INVALID_HANDLE_VALUE),
    eventType(eventType),
    timestamp(0),
    period(0),
    ioFd(ioFd),
    ownsIoHandle(true),
    numThreads(0)
{
    /* Create an event for the socket fd */
    if (0 <= ioFd) {
        ioHandle = WSACreateEvent();
    }

    if (genPurpose) {
        handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
}

Event::Event(uint32_t timestamp, uint32_t period)
    : handle(INVALID_HANDLE_VALUE),
    ioHandle(INVALID_HANDLE_VALUE),
    eventType(TIMED),
    timestamp(WAIT_FOREVER == timestamp ? WAIT_FOREVER : GetTimestamp() + timestamp),
    period(period),
    ioFd(-1),
    ownsIoHandle(false),
    numThreads(0)
{
}

Event::~Event()
{
    /* Threads should not be waiting on this event */
    if ((INVALID_HANDLE_VALUE != handle) && !::SetEvent(handle)) {
        QCC_LogError(ER_FAIL, ("SetEvent failed with %d (%s)", GetLastError()));
    }
    if (TIMED == eventType) {
        timestamp = 0;
    }

    /* Close the handles */
    if (INVALID_HANDLE_VALUE != handle) {
        CloseHandle(handle);
    }
    if (ownsIoHandle && (INVALID_HANDLE_VALUE != ioHandle)) {
        WSAEventSelect(ioFd, ioHandle, 0);
        WSACloseEvent(ioHandle);
    }
}

QStatus Event::SetEvent()
{
    QStatus status = ER_OK;

    if (INVALID_HANDLE_VALUE != handle) {
        if (!::SetEvent(handle)) {
            status = ER_FAIL;
            QCC_LogError(status, ("SetEvent failed with %d", GetLastError()));
        }
    } else if (TIMED == eventType) {
        uint32_t now = GetTimestamp();
        if (now < timestamp) {
            if (0 < period) {
                timestamp -= (((now - timestamp) / period) + 1) * period;
            } else {
                timestamp = now;
            }
        }
        status = ER_OK;
    } else {
        /* Not a general purpose or timed event */
        status = ER_FAIL;
        QCC_LogError(status, ("Attempt to manually set an I/O event"));
    }
    return status;
}

QStatus Event::ResetEvent()
{
    QStatus status = ER_OK;

    if (INVALID_HANDLE_VALUE != handle) {
        if (!::ResetEvent(handle)) {
            status = ER_FAIL;
            QCC_LogError(status, ("ResetEvent failed with %d", GetLastError()));
        }
    } else if (TIMED == eventType) {
        if (0 < period) {
            uint32_t now = GetTimestamp();
            timestamp += (((now - timestamp) / period) + 1) * period;
        } else {
            timestamp = static_cast<uint32_t>(-1);
        }
    } else {
        /* Not a general purpose or timed event */
        status = ER_FAIL;
        QCC_LogError(status, ("Attempt to manually reset an I/O event"));
    }
    return status;
}

bool Event::IsSet()
{
    return (ER_TIMEOUT == Wait(*this, 0)) ? false : true;
}

void Event::ResetTime(uint32_t delay, uint32_t period)
{
    if (delay == WAIT_FOREVER) {
        this->timestamp = WAIT_FOREVER;
    } else {
        this->timestamp = GetTimestamp() + delay;
    }
    this->period = period;
}

void Event::ReplaceIO(Event& event)
{
    /* Check event type */
    if ((IO_READ != eventType) && (IO_WRITE != eventType)) {
        QCC_LogError(ER_FAIL, ("Attempt to replaceIO on non-io event"));
        return;
    }

    /* Remove existing ioHandle if we own it */
    if (ownsIoHandle && (INVALID_HANDLE_VALUE != ioHandle)) {
        WSACloseEvent(ioHandle);
    }

    /* Replace I/O */
    ownsIoHandle = false;
    ioHandle = event.ioHandle;
    ioFd = event.ioFd;
}

}  /* namespace */

