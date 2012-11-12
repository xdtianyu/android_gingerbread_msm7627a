/**
 * @file
 *
 * Define a class that abstracts Linux threads.
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
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <map>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Thread.h>

#include <Status.h>

using namespace std;

/** @internal */
#define QCC_MODULE "THREAD"

namespace qcc {


static uint32_t started = 0;
static uint32_t running = 0;
static uint32_t joined = 0;

/** Mutex that protects global thread list */
static Mutex threadListLock;

/** Global thread list */
static map<ThreadHandle, Thread*> threadList;

void Thread::SigHandler(int signal)
{
    QCC_DbgPrintf(("Signal handler pid=%x", (unsigned long)pthread_self()));
    /* Remove thread from thread list */
    threadListLock.Lock();
    threadList.erase(pthread_self());
    threadListLock.Unlock();
    pthread_exit(0);
}

QStatus Sleep(uint32_t ms) {
    usleep(1000 * ms);
    return ER_OK;
}

Thread* Thread::GetThread()
{
    Thread* ret = NULL;

    /* Find thread on threadList */
    threadListLock.Lock();
    map<ThreadHandle, Thread*>::const_iterator iter = threadList.find(pthread_self());
    if (iter != threadList.end()) {
        ret = iter->second;
    }
    threadListLock.Unlock();

    /* If the current thread isn't on the list, then create an external (wrapper) thread */
    if (NULL == ret) {
        ret = new Thread("external", NULL, true);
    }

    return ret;
}

void Thread::CleanExternalThreads()
{
    threadListLock.Lock();
    map<ThreadHandle, Thread*>::iterator it = threadList.begin();
    while (it != threadList.end()) {
        if (it->second->isExternal) {
            delete it->second;
            threadList.erase(it++);
        } else {
            ++it;
        }
    }
    threadListLock.Unlock();
}

Thread::Thread(qcc::String funcName, Thread::ThreadFunction func, bool isExternal) :
    stopEvent(),
    state(isExternal ? RUNNING : INITIAL),
    isStopping(false),
    funcName(funcName),
    function(isExternal ? NULL : func),
    handle(isExternal ? pthread_self() : 0),
    exitValue(NULL),
    listener(NULL),
    isExternal(isExternal),
    noBlockResource(NULL),
    alertCode(0),
    joinCtx(NULL),
    threadOwnsJoinCtx(true)
{
    /* If this is an external thread, add it to the thread list here since Run will not be called */
    QCC_DbgHLPrintf(("Thread::Thread() created %s - %x -- started:%d running:%d joined:%d", funcName.c_str(), handle, started, running, joined));
    if (isExternal) {
        assert(func == NULL);
        threadListLock.Lock();
        threadList[handle] = this;
        threadListLock.Unlock();
    }
}


Thread::~Thread(void)
{
    QCC_DbgHLPrintf(("Thread::~Thread() destroying %s - %x", funcName.c_str(), handle));

    if (!isExternal) {
        Stop();
        if (handle != pthread_self()) {
            Join();
        } else if (threadOwnsJoinCtx) {
            JoinContext* jc = joinCtx;
            joinCtx = NULL;
            delete jc;
            if (handle) {
                pthread_detach(handle);
            }
            ++joined;
        } else {
            QCC_LogError(ER_FAIL, ("Thread destructor (%p) exiting with pending joined threads", this));
        }
    }
    QCC_DbgHLPrintf(("Thread::~Thread() destroyed %s - %x -- started:%d running:%d joined:%d", funcName.c_str(), handle, started, running, joined));
}


ThreadInternalReturn Thread::RunInternal(void* threadArg)
{
    Thread* thread(reinterpret_cast<Thread*>(threadArg));
    sigset_t newmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGUSR1);

    assert(thread != NULL);
    assert(thread->state == STARTED);

    /* Plug race condition between Start and Run. (pthread_create may not write handle before run is called) */
    thread->handle = pthread_self();

    ++started;

    QCC_DbgPrintf(("Thread::RunInternal: %s (pid=%x)", thread->funcName.c_str(), (unsigned long) thread->handle));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &SigHandler;
    sa.sa_flags = 0;
    int ret = sigaction(SIGUSR1, &sa, NULL);
    if (0 != ret) {
        QCC_LogError(ER_OS_ERROR, ("Thread:Run() [%s] Failed to set SIGUSR1 handler", thread->funcName.c_str()));
        thread->exitValue = reinterpret_cast<ThreadInternalReturn>(0);
    } else {

        /* Add this Thread to list of running threads */
        threadListLock.Lock();
        threadList[thread->handle] = thread;
        thread->state = RUNNING;
        threadListLock.Unlock();

        pthread_sigmask(SIG_UNBLOCK, &newmask, NULL);

        /* Start the thread if it hasn't been stopped */
        if (!thread->isStopping) {
            QCC_DbgPrintf(("Starting thread: %s", thread->funcName.c_str()));
            ++running;
            thread->exitValue = thread->Run(thread->arg);
            --running;
            QCC_DbgPrintf(("Thread function exited: %s --> %p", thread->funcName.c_str(), thread->exitValue));
        }
    }

    thread->state = STOPPING;
    thread->stopEvent.ResetEvent();

    /*
     * Call thread exit callback if specified. Note that ThreadExit may dellocate the thread so the
     * members of thread may not be accessed after this call
     */
    void* retVal = thread->exitValue;
    ThreadHandle handle = thread->handle;


    if (thread->listener) {
        thread->listener->ThreadExit(thread);
    }

    if (ret == 0) {
        /* Remove this Thread from list of running threads */
        QCC_DbgPrintf(("Removing %x", handle));
        threadListLock.Lock();
        threadList.erase(handle);
        threadListLock.Unlock();
    }

    return reinterpret_cast<ThreadInternalReturn>(retVal);
}

static const uint32_t stacksize = 80 * 1024;

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
        QCC_LogError(status, ("Thread::Start"));
    } else {
        int ret;

        /* Clear/initialize the join context */
        while (joinCtx) {
            Join();
        }
        joinCtx = new JoinContext();
        threadOwnsJoinCtx = true;

        /*  Reset the stop event so the thread doesn't start out alerted. */
        stopEvent.ResetEvent();
        /* Create OS thread */
        this->arg = arg;
        this->listener = listener;

        state = STARTED;
        pthread_attr_t attr;
        ret = pthread_attr_init(&attr);
        if (ret != 0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Initializing thread attr: %s", strerror(ret)));
        }
        ret = pthread_attr_setstacksize(&attr, stacksize);
        if (ret != 0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Setting stack size: %s", strerror(ret)));
        }
        ret = pthread_create(&handle, &attr, RunInternal, this);
        QCC_DbgTrace(("Thread::Start() [%s] pid = %x", funcName.c_str(), handle));
        if (ret != 0) {
            state = DEAD;
            isStopping = false;
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Creating thread %s: %s", funcName.c_str(), strerror(ret)));
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
    QCC_DbgTrace(("Thread::Alert(%u) [%s:%srunning]", alertCode, funcName.c_str(), IsRunning() ? " " : " not "));
    return stopEvent.SetEvent();
}

QStatus Thread::Kill(void)
{
    QStatus status = ER_OK;
    QCC_DbgTrace(("Thread::Kill() [%s:%srunning]", funcName.c_str(), IsRunning() ? " " : " not "));

    /* Cannot kill external threads */
    if (isExternal) {
        status = ER_EXTERNAL_THREAD;
        QCC_LogError(status, ("Cannot kill an external thread"));
        return status;
    }
    QCC_DbgTrace(("Thread::Kill() [%s run: %s]", funcName.c_str(), IsRunning() ? "true" : "false"));
    threadListLock.Lock();
    if (IsRunning()) {
        threadListLock.Unlock();

        QCC_DbgPrintf(("Killing thread: %s", funcName.c_str()));

        int ret = pthread_kill(handle, SIGUSR1);
        if (ret == 0) {
            handle = 0;
            state = DEAD;
            isStopping = false;
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Killing thread: %s", strerror(ret)));
        }
    } else {
        threadListLock.Unlock();
    }

    return status;
}

QStatus Thread::Join(void)
{
    QStatus status = ER_OK;

    QCC_DbgTrace(("Thread::Join() [%s - %x :%srunning]", funcName.c_str(), handle, IsRunning() ? " " : " not "));

    QCC_DbgPrintf(("[%s - %x] Joining thread [%s - %x]",
                   GetThread()->funcName.c_str(), GetThread()->handle,
                   funcName.c_str(), handle));

    /*
     * Nothing to join if the thread is dead
     */
    if (state == DEAD) {
        QCC_DbgPrintf(("Thread::Join() thread is dead [%s]", funcName.c_str()));
        return ER_DEAD_THREAD;
    }
    /*
     * There is a race condition where the underlying OS thread has not yet started to run. We need
     * to wait until the thread is actually running before we can free it.
     */
    while (state == STARTED) {
        usleep(1000 * 5);
    }

    assert(handle != pthread_self());

    /*
     * Unfortunately, POSIX pthread_join can only be called once for a given thread. This is quite
     * inconvenient in a system of multiple threads that need to synchronize with each other.
     * This ugly looking code allows multiple threads to Join a thread. All but one thread block
     * in a Mutex. The first thread to obtain the mutex is the one that is allowed to call pthread_join.
     * All other threads wait for that join to complete. Then they are released.
     */
    if (handle) {
        int ret = 0;
        JoinContext* jc = joinCtx;
        if (jc) {
            IncrementAndFetch(&jc->count);
            threadOwnsJoinCtx = false;
            jc->lock.Lock();
            if (!jc->hasBeenJoined) {
                ret = pthread_join(handle, NULL);
                jc->hasBeenJoined = true;
                ++joined;
            }
            jc->lock.Unlock();
            if (0 == DecrementAndFetch(&jc->count)) {
                joinCtx = NULL;
                delete jc;
            }
        }

        if (ret != 0) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Joining thread: %d - %s", ret, strerror(ret)));
        }
        handle = 0;
        isStopping = false;
    }
    state = DEAD;
    QCC_DbgPrintf(("Joined thread %s", funcName.c_str()));
    return status;
}

ThreadReturn STDCALL Thread::Run(void* arg)
{
    QCC_DbgTrace(("Thread::Run() [%s:%srunning]", funcName.c_str(), IsRunning() ? " " : " not "));
    assert(NULL != function);
    assert(!isExternal);
    return (*function)(arg);
}

} /* namespace */
