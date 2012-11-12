/**
 * @file
 *
 * Define a class that abstracts Windows process/threads.
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

#include <algorithm>
#include <assert.h>
#include <process.h>
#include <map>

#include <qcc/String.h>
#include <qcc/Debug.h>
#include <qcc/Thread.h>
#include <qcc/Mutex.h>

#include <Status.h>

using namespace std;

/** @internal */
#define QCC_MODULE "THREAD"

namespace qcc {

static uint32_t started = 0;
static uint32_t running = 0;
static uint32_t stopped = 0;


/** Maximum number of milliseconds to wait between calls to select to check for thread death */
static const uint32_t MAX_SELECT_WAIT_MS = 10000;

/** Lock that protects global list of Threads and their handles */
static Mutex threadListLock;

/** Thread list */
static map<unsigned int, Thread*> threadList;

QStatus Sleep(uint32_t ms) {
    ::sleep(ms);
    return ER_OK;
}

Thread* Thread::GetThread()
{
    Thread* ret = NULL;
    unsigned int id = GetCurrentThreadId();

    /* Find thread on threadList */
    threadListLock.Lock();
    map<unsigned int, Thread*>::const_iterator iter = threadList.find(id);
    if (iter != threadList.end()) {
        ret = iter->second;
    }
    threadListLock.Unlock();
    /*
     * If the current thread isn't on the list, then create an external (wrapper) thread
     */
    if (NULL == ret) {
        char name[32];
        snprintf(name, sizeof(name), "external%d", id);
        /* TODO @@ Memory leak */
        ret = new Thread(name, NULL, true);
    }

    return ret;
}


Thread::Thread(qcc::String funcName, Thread::ThreadFunction func, bool isExternal) :
    state(isExternal ? RUNNING : DEAD),
    isStopping(false),
    funcName(funcName),
    function(isExternal ? NULL : func),
    handle(isExternal ? GetCurrentThread() : 0),
    exitValue(NULL),
    arg(NULL),
    threadId(isExternal ? GetCurrentThreadId() : 0),
    listener(NULL),
    isExternal(isExternal),
    noBlockResource(NULL),
    alertCode(0)
{
    /*
     * External threads are already running so just add them to the thread list.
     */
    if (isExternal) {
        threadListLock.Lock();
        threadList[threadId] = this;
        threadListLock.Unlock();
    }
    QCC_DbgHLPrintf(("Thread::Thread() [%s,%x]", GetName().c_str(), this));
}

Thread::~Thread(void)
{
    if (IsRunning()) {
        Stop();
        Join();
    } else if (!isExternal && handle) {
        CloseHandle(handle);
        ++stopped;
    }
    QCC_DbgHLPrintf(("Thread::~Thread() [%s,%x] started:%d running:%d stopped:%d", GetName().c_str(), this, started, running, stopped));
}


ThreadInternalReturn STDCALL Thread::RunInternal(void* threadArg)
{
    Thread* thread(reinterpret_cast<Thread*>(threadArg));

    assert(thread != NULL);
    assert(thread->state = STARTED);
    assert(!thread->isExternal);

    ++started;

    QCC_DbgTrace(("Thread::RunInternal() [%s]", thread->GetName().c_str()));

    /* Add this Thread to list of running threads */
    threadListLock.Lock();
    threadList[thread->threadId] = thread;
    thread->state = RUNNING;
    threadListLock.Unlock();

    /* Start the thread if it hasn't been stopped */
    if (!thread->isStopping) {
        QCC_DbgPrintf(("Starting thread: %s", thread->funcName.c_str()));
        ++running;
        thread->exitValue  = thread->Run(thread->arg);
        --running;
        QCC_DbgPrintf(("Thread function exited: %s --> %p", thread->funcName.c_str(), thread->exitValue));
    }

    /*
     * Call thread exit callback if specified. Note that ThreadExit may dellocate the thread so the
     * members of thread may not be accessed after this call
     */
    unsigned retVal = (unsigned)thread->exitValue;
    uint32_t threadId = thread->threadId;

    thread->state = STOPPING;
    thread->stopEvent.ResetEvent();

    if (thread->listener) {
        thread->listener->ThreadExit(thread);
    }

    /* Remove this Thread from list of running threads */
    QCC_DbgPrintf(("Removing %x", threadId));
    threadListLock.Lock();
    threadList.erase(threadId);
    threadListLock.Unlock();

    _endthreadex(retVal);
    return retVal;
}

QStatus Thread::Start(void* arg, ThreadListener* listener)
{
    QStatus status = ER_OK;

    /* Check that thread can be started */
    if (isExternal) {
        status = ER_EXTERNAL_THREAD;
    } else if (isStopping) {
        status = ER_THREAD_STOPPING;
    } else if (IsRunning()) {
        status = ER_THREAD_RUNNING;
    }

    if (status != ER_OK) {
        QCC_LogError(status, ("Thread::Start() [%s]", funcName.c_str()));
    } else {
        QCC_DbgTrace(("Thread::Start() [%s]", funcName.c_str()));
        /*  Reset the stop event so the thread doesn't start out alerted. */
        stopEvent.ResetEvent();
        /* Create OS thread */
        this->arg = arg;
        this->listener = listener;

        state = STARTED;
        handle = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, RunInternal, this, 0, &threadId));
        if (handle == 0) {
            state = DEAD;
            isStopping = false;
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Creating thread"));
        }
    }
    return status;
}


QStatus Thread::Stop(void)
{
    /* Cannot stop external threads */
    if (isExternal) {
        QCC_LogError(ER_EXTERNAL_THREAD, ("Cannot stop an external thread"));
        return ER_EXTERNAL_THREAD;
    } else if (state == DEAD) {
        QCC_DbgPrintf(("Thread::Stop() thread is dead [%s]", funcName.c_str()));
        return ER_DEAD_THREAD;
    } else {
        QCC_DbgTrace(("Thread::Stop() %x [%s]", handle, funcName.c_str()));
        isStopping = true;
        return stopEvent.SetEvent();
    }
}

QStatus Thread::Alert()
{
    if (state == DEAD) {
        return ER_DEAD_THREAD;
    }
    QCC_DbgTrace(("Thread::Alert() [%s:%srunning]", funcName.c_str(), IsRunning() ? " " : " not "));
    return stopEvent.SetEvent();
}

QStatus Thread::Alert(uint32_t alertCode)
{
    this->alertCode = alertCode;
    if (state == DEAD) {
        return ER_DEAD_THREAD;
    }
    QCC_DbgTrace(("Thread::Alert() [%s run: %s]", funcName.c_str(), IsRunning() ? "true" : "false"));
    return stopEvent.SetEvent();
}

QStatus Thread::Kill(void)
{
    QStatus status = ER_OK;

    /* Cannot kill external threads */
    if (isExternal) {
        status = ER_EXTERNAL_THREAD;
        QCC_LogError(status, ("Cannot kill an external thread"));
        return status;
    }
    QCC_DbgTrace(("Thread::Kill() [%s run: %s]", funcName.c_str(), IsRunning() ? "true" : "false"));
    threadListLock.Lock();
    if (IsRunning()) {
        TerminateThread(handle, 0);
        CloseHandle(handle);
        handle = 0;
        state = DEAD;
        isStopping = false;
        /* Remove this Thread from list of running threads */
        threadList.erase(threadId);
    }
    threadListLock.Unlock();

    return status;
}


QStatus Thread::Join(void)
{
    QStatus status = ER_OK;
    bool self = (threadId == GetCurrentThreadId());

    QCC_DbgTrace(("Thread::Join() [%s run: %s]", funcName.c_str(), IsRunning() ? "true" : "false"));

    /*
     * Nothing to join if the thread is dead
     */
    if (state == DEAD) {
        QCC_DbgPrintf(("Thread::Join() thread is dead [%s]", funcName.c_str()));
        return ER_DEAD_THREAD;
    }
    /*
     * There is a race condition where the underlying OS thread has not yet started to run. We need
     * to wait until the thread is actually running before we can join it.
     */
    while (state == STARTED) {
        ::sleep(5);
    }

    QCC_DbgPrintf(("[%s - %x] %s thread %x [%s - %x]",
                   self ? funcName.c_str() : GetThread()->funcName.c_str(),
                   self ? threadId : GetThread()->threadId,
                   self ? "Closing" : "Joining",
                   threadId, funcName.c_str(), threadId));

    if (handle) {
        DWORD ret;
        if (self) {
            ret = WAIT_OBJECT_0;
        } else {
            ret = WaitForSingleObject(handle, INFINITE);
        }
        if (ret != WAIT_OBJECT_0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Joining thread: %d", ret));
        }
        CloseHandle(handle);
        handle = 0;
        ++stopped;
    }
    isStopping = false;
    state = DEAD;
    QCC_DbgPrintf(("%s thread %s", self ? "Closed" : "Joined", funcName.c_str()));
    return status;
}

ThreadReturn STDCALL Thread::Run(void* arg)
{
    assert(NULL != function);
    return (*function)(arg);
}

}    /* namespace */
