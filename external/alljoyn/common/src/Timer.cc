/**
 * @file
 *
 * Timer thread
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

#include <qcc/Debug.h>
#include <qcc/Timer.h>

#define QCC_MODULE  "TIMER"

#define FALLBEHIND_WARNING_MS 500

using namespace std;
using namespace qcc;

QStatus Timer::AddAlarm(const Alarm& alarm)
{
    lock.Lock();
    if (!IsStopping()) {
        bool alertThread = alarms.empty() || (alarm < *alarms.begin());
        alarms.insert(alarm);

        bool inUserCallback = (currentAlarm != NULL);
        lock.Unlock();
        if (alertThread && !inUserCallback) {
            Alert();
        }
        return ER_OK;
    } else {
        lock.Unlock();
        return ER_TIMER_EXITING;
    }
}

void Timer::RemoveAlarm(const Alarm& alarm)
{
    lock.Lock();
    alarms.erase(alarm);
    /*
     * There might be a call in progress to the alarm that is being removed.
     */
    if (currentAlarm && (alarm == *currentAlarm) && (Thread::GetThread() != this)) {
        do {
            lock.Unlock();
            qcc::Sleep(1);
            lock.Lock();
        } while (currentAlarm && (alarm == *currentAlarm));
    }
    lock.Unlock();
}

bool Timer::RemoveAlarm(AlarmListener* listener, Alarm& alarm)
{
    bool removedOne = false;
    lock.Lock();
    for (set<Alarm>::iterator it = alarms.begin(); it != alarms.end(); ++it) {
        if (it->listener == listener) {
            alarm = *it;
            alarms.erase(it);
            removedOne = true;
            break;
        }
    }
    /*
     * This function is most likely being called because the listener is about to be freed. If there
     * are no alarms remaining check that we are not currently servicing an alarm for this listener.
     * If we are, wait until the listener returns.
     */
    if (!removedOne && currentAlarm && (currentAlarm->listener == listener) && (Thread::GetThread() != this)) {
        do {
            lock.Lock();
            qcc::Sleep(1);
            lock.Unlock();
        } while (currentAlarm && (currentAlarm->listener == listener));
    }
    lock.Unlock();
    return removedOne;
}

bool Timer::HasAlarm(const Alarm& alarm)
{
    lock.Lock();
    bool hasAlarm = alarms.count(alarm) != 0;
    lock.Unlock();
    return hasAlarm;
}

ThreadReturn STDCALL Timer::Run(void* arg)
{
    /* Wait for first entry on (sorted) alarm list to expire */
    lock.Lock();
    while (!IsStopping()) {
        Timespec now;
        GetTimeNow(&now);
        if (!alarms.empty()) {
            const Alarm& topAlarm = *alarms.begin();
            int64_t delay = topAlarm.alarmTime - now;
            if (0 < delay) {
                lock.Unlock();
                Event evt(static_cast<uint32_t>(delay), 0);
                Event::Wait(evt);
                stopEvent.ResetEvent();

            } else {
                set<Alarm>::iterator it = alarms.begin();
                Alarm top = *it;
                alarms.erase(it);
                currentAlarm = &top;
                stopEvent.ResetEvent();
                lock.Unlock();
                if (FALLBEHIND_WARNING_MS < -delay) {
                    QCC_LogError(ER_TIMER_FALLBEHIND, ("Timer has fallen behind by %d ms", -delay));
                }
                (top.listener->AlarmTriggered)(top, ER_OK);
                currentAlarm = NULL;
                if (0 != top.periodMs) {
                    top.alarmTime += top.periodMs;
                    if (top.alarmTime < now) {
                        top.alarmTime = now;
                    }
                    AddAlarm(top);
                }
            }
        } else {
            lock.Unlock();
            Event evt(Event::WAIT_FOREVER, 0);
            Event::Wait(evt);
            stopEvent.ResetEvent();
        }
        lock.Lock();
    }
    if (expireOnExit) {
        /* Call all alarms */
        while (!alarms.empty()) {
            /*
             * Note it is possible that the callback will call RemoveAlarm()
             */
            set<Alarm>::iterator it = alarms.begin();
            Alarm alarm = *it;
            alarms.erase(it);
            lock.Unlock();
            alarm.listener->AlarmTriggered(alarm, ER_TIMER_EXITING);
            lock.Lock();
        }
    }
    lock.Unlock();

    return (ThreadReturn) 0;
}



