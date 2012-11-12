/**
 * @file
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

#include <assert.h>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/atomic.h>
#include <qcc/Thread.h>
#include <qcc/SocketStream.h>
#include <qcc/atomic.h>

#include <alljoyn/BusAttachment.h>

#include "Router.h"
#include "RemoteEndpoint.h"
#include "LocalTransport.h"
#include "AllJoynPeerObj.h"
#include "BusInternal.h"

#ifndef NDEBUG
#include <qcc/time.h>
#endif

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

#define ENDPOINT_IS_DEAD_ALERTCODE  1


/* Endpoint constructor */
RemoteEndpoint::RemoteEndpoint(BusAttachment& bus,
                               bool incoming,
                               const qcc::String& connectSpec,
                               Stream& stream,
                               const char* threadName,
                               bool isSocket) :
    BusEndpoint(BusEndpoint::ENDPOINT_TYPE_REMOTE),
    bus(bus),
    stream(stream),
    auth(bus, *this, incoming),
    txQueue(),
    txWaitQueue(),
    txQueueLock(),
    exitCount(0),
    rxThread(bus, (qcc::String(incoming ? "rx-srv-" : "rx-cli-") + threadName).c_str(), incoming),
    txThread(bus, (qcc::String(incoming ? "tx-srv-" : "tx-cli-") + threadName).c_str(), txQueue, txWaitQueue, txQueueLock),
    connSpec(connectSpec),
    incoming(incoming),
    processId(-1),
    refCount(0),
    isSocket(isSocket),
    armRxPause(false),
    numWaiters(0)
{
}

RemoteEndpoint::~RemoteEndpoint()
{
    /* Request Stop */
    Stop();

    /* Wait for thread to shutdown */
    Join();
}

QStatus RemoteEndpoint::Start()
{
    QCC_DbgTrace(("RemoteEndpoint::Start(isBusToBus = %s, allowRemote = %s)",
                  features.isBusToBus ? "true" : "false",
                  features.allowRemote ? "true" : "false"));
    QStatus status;
    Router& router = bus.GetInternal().GetRouter();
    bool isTxStarted = false;
    bool isRxStarted = false;

    if (features.isBusToBus) {
        endpointType = BusEndpoint::ENDPOINT_TYPE_BUS2BUS;
    }

    /* Start the TX thread */
    status = txThread.Start(this, this);
    isTxStarted = (ER_OK == status);

    /* Register endpoint */
    if (ER_OK == status) {
        status = router.RegisterEndpoint(*this, false);
    }

    /* Start the Rx thread */
    if (ER_OK == status) {
        status = rxThread.Start(this, this);
        isRxStarted = (ER_OK == status);
    }

    /* If thread failed to start, then unregister. */
    if (ER_OK != status) {
        if (isTxStarted) {
            txThread.Stop();
            txThread.Join();
        }
        if (isRxStarted) {
            rxThread.Stop();
            rxThread.Join();
        }
        router.UnregisterEndpoint(*this);
        QCC_LogError(status, ("AllJoynRemoteEndoint::Start failed"));
    }

    return status;
}

void RemoteEndpoint::SetListener(EndpointListener* listener)
{
    this->listener = listener;
}

QStatus RemoteEndpoint::Stop(void)
{
    /* Alert any threads that are on the wait queue */
    txQueueLock.Lock();
    deque<Thread*>::iterator it = txWaitQueue.begin();
    while (it != txWaitQueue.end()) {
        (*it++)->Alert(ENDPOINT_IS_DEAD_ALERTCODE);
    }
    txQueueLock.Unlock();

    /*
     * Don't call txThread.Stop() here; the logic in RemoteEndpoint::ThreadExit() takes care of
     * stopping the txThread.
     *
     * See also the comment in UnixTransport::Disconnect() which says that once this function is
     * called, the endpoint must be considered dead (i.e. may have been destroyed).  That comment
     * applied here means that once rxThread.Stop() above is called, this may have been destroyed.
     */
    return rxThread.Stop();
}

QStatus RemoteEndpoint::StopAfterTxEmpty()
{
    QStatus status;

    /* Wait for txqueue to empty before triggering stop */
    txQueueLock.Lock();
    while (true) {
        if (txQueue.empty()) {
            status = Stop();
            break;
        } else {
            txQueueLock.Unlock();
            qcc::Sleep(5);
            txQueueLock.Lock();
        }
    }
    txQueueLock.Unlock();
    return status;
}

QStatus RemoteEndpoint::PauseAfterRxReply()
{

    armRxPause = true;
    return ER_OK;
}

QStatus RemoteEndpoint::Join(void)
{
    /* Wait for any threads blocked in PushMessage to exit */
    while (numWaiters > 0) {
        qcc::Sleep(10);
    }

    /*
     * Note that we don't join txThread and rxThread, rather we let the thread destructors handle
     * this when the RemoteEndpoint destructor is called. The reason for this is tied up in the
     * ThreadExit logic that coordinates the stopping of both rx and tx threads.
     */
    return ER_OK;
}

void RemoteEndpoint::ThreadExit(Thread* thread)
{
    /* If one thread stops, the other must too */
    if ((&rxThread == thread) && txThread.IsRunning()) {
        txThread.Stop();
    } else if (rxThread.IsRunning()) {
        rxThread.Stop();
    }

    /* Unregister endpoint when both rx and tx exit */
    if (2 == IncrementAndFetch(&exitCount)) {
        /* De-register this remote endpoint */
        bus.GetInternal().GetRouter().UnregisterEndpoint(*this);
        if (NULL != listener) {
            listener->EndpointExit(this);
        }
    }
}

static inline bool IsControlMessage(Message& msg)
{
    if (strcmp("org.freedesktop.DBus", msg->GetInterface()) == 0) {
        return true;
    }
    if (strcmp("org.alljoyn.Daemon", msg->GetInterface()) == 0) {
        return true;
    }
    return false;
}

void* RemoteEndpoint::RxThread::Run(void* arg)
{
    QStatus status = ER_OK;
    RemoteEndpoint* ep = reinterpret_cast<RemoteEndpoint*>(arg);
    const bool bus2bus = BusEndpoint::ENDPOINT_TYPE_BUS2BUS == ep->GetEndpointType();

    Router& router = bus.GetInternal().GetRouter();
    qcc::Event& ev = ep->GetSource().GetSourceEvent();
    /* Receive messages until the socket is disconnected */
    while (!IsStopping() && (ER_OK == status)) {
        status = Event::Wait(ev);
        if (ER_OK == status) {
            Message msg(bus);
            status = msg->Unmarshal(*ep, (validateSender && !bus2bus));
            switch (status) {
            case ER_OK :
                status = router.PushMessage(msg, *ep);
                if (status != ER_OK) {
                    /*
                     * There are three cases where a failure to push a message to the router is ok:
                     *
                     * 1) The message received did not match the expected signature.
                     * 2) The message was a method reply that did not match up to a method call.
                     * 3) A daemon is pushing the message to a connected client or service.
                     *
                     */
                    if ((router.IsDaemon() && !bus2bus) || (status == ER_BUS_SIGNATURE_MISMATCH) || (status == ER_BUS_UNMATCHED_REPLY_SERIAL)) {
                        QCC_DbgHLPrintf(("Discarding %s: %s", msg->Description().c_str(), QCC_StatusText(status)));
                        status = ER_OK;
                    }
                }
                break;

            case ER_BUS_CANNOT_EXPAND_MESSAGE :
                /*
                 * The message could not be expanded so pass it the peer object to request the expansion
                 * rule from the endpoint that sent it.
                 */
                status = bus.GetInternal().GetLocalEndpoint().GetPeerObj()->RequestHeaderExpansion(msg, ep);
                if ((status != ER_OK) && router.IsDaemon()) {
                    QCC_LogError(status, ("Discarding %s", msg->Description().c_str()));
                    status = ER_OK;
                }
                break;

            case ER_BUS_TIME_TO_LIVE_EXPIRED:
                QCC_DbgHLPrintf(("TTL expired discarding %s", msg->Description().c_str()));
                status = ER_OK;
                break;

            case ER_BUS_INVALID_HEADER_SERIAL:
                /*
                 * Ignore invalid serial numbers for unreliable messages or broadcast messages that come from
                 * bus2bus endpoints as these can be delivered out-of-order or repeated.
                 *
                 * Ignore control messages (i.e. messages targeted at the bus controller)
                 * TODO - need explanation why this is neccessary.
                 *
                 * In all other cases an invalid serial number cause the connection to be dropped.
                 */
                if (msg->IsUnreliable() || (bus2bus && msg->IsBroadcastSignal()) || IsControlMessage(msg)) {
                    QCC_DbgHLPrintf(("Invalid serial discarding %s", msg->Description().c_str()));
                    status = ER_OK;
                } else {
                    QCC_LogError(status, ("Invalid serial %s", msg->Description().c_str()));
                }
                break;

            default:
                break;
            }

            /* Check pause condition. Block until stopped */
            if (ep->armRxPause && !IsStopping() && (msg->GetType() == MESSAGE_METHOD_RET)) {
                status = Event::Wait(Event::neverSet);
            }
        }
    }
    if ((status != ER_OK) && (status != ER_STOPPING_THREAD) && (status != ER_SOCK_OTHER_END_CLOSED) && (status != ER_BUS_STOPPING)) {
        QCC_LogError(status, ("Endpoint Rx thread (%s) exiting", GetName().c_str()));
    }

    /* On an unexpected disconnect save the status that cause the thread exit */
    if (ep->disconnectStatus == ER_OK) {
        ep->disconnectStatus = (status == ER_STOPPING_THREAD) ? ER_OK : status;
    }

    /* Inform transport of endpoint exit */
    return (void*) status;
}

void* RemoteEndpoint::TxThread::Run(void* arg)
{
    QStatus status = ER_OK;
    RemoteEndpoint* ep = reinterpret_cast<RemoteEndpoint*>(arg);

    /* Wait for queue to be non-empty */
    while (!IsStopping() && (ER_OK == status)) {

        status = Event::Wait(Event::neverSet);

        if (!IsStopping() && (ER_ALERTED_THREAD == status)) {
            stopEvent.ResetEvent();
            status = ER_OK;
            queueLock.Lock();
            while (!queue.empty() && !IsStopping()) {

                /* Get next message */
                Message msg = queue.back();

                /* Alert next thread on wait queue */
                if (0 < waitQueue.size()) {
                    Thread* wakeMe = waitQueue.back();
                    waitQueue.pop_back();
                    status = wakeMe->Alert();
                    if (ER_OK != status) {
                        QCC_LogError(status, ("Failed to alert thread blocked on full tx queue"));
                    }
                }

                queueLock.Unlock();

                /* Deliver message */
                status = msg->Deliver(*ep);
                queueLock.Lock();
                queue.pop_back();
            }
            queueLock.Unlock();
        }
    }
    /* Wake any thread waiting on tx queue availability */
    queueLock.Lock();
    while (0 < waitQueue.size()) {
        Thread* wakeMe = waitQueue.back();
        QStatus status = wakeMe->Alert();
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to clear tx wait queue"));
        }
        waitQueue.pop_back();
    }
    queueLock.Unlock();

    /* On an unexpected disconnect save the status that cause the thread exit */
    if (ep->disconnectStatus == ER_OK) {
        ep->disconnectStatus = (status == ER_STOPPING_THREAD) ? ER_OK : status;
    }

    /* Inform transport of endpoint exit */
    return (void*) status;
}

QStatus RemoteEndpoint::PushMessage(Message& msg)
{
    static const size_t MAX_TX_QUEUE_SIZE = 10;

    QStatus status = ER_OK;

    /*
     * Don't continue if this endpoint is in the process of being closed
     * Otherwise we risk deadlock when sending NameOwnerChanged signal to
     * this dying endpoint
     */
    if (rxThread.IsStopping() || txThread.IsStopping()) {
        return ER_BUS_ENDPOINT_CLOSING;
    }
    IncrementAndFetch(&numWaiters);
    txQueueLock.Lock();
    size_t count = txQueue.size();
    bool wasEmpty = (count == 0);
    if (MAX_TX_QUEUE_SIZE > count) {
        txQueue.push_front(msg);
    } else {
        while (true) {
            /* Remove a queue entry whose TTLs is expired if possible */
            deque<Message>::iterator it = txQueue.begin();
            uint32_t maxWait = 20 * 1000;
            while (it != txQueue.end()) {
                uint32_t expMs;
                if ((*it)->IsExpired(&expMs)) {
                    txQueue.erase(it);
                    break;
                } else {
                    ++it;
                }
                maxWait = (std::min)(maxWait, expMs);
            }
            if (txQueue.size() < MAX_TX_QUEUE_SIZE) {
                /* Check queue wasn't drained while we were waiting */
                if (txQueue.size() == 0) {
                    wasEmpty = true;
                }
                txQueue.push_front(msg);
                status = ER_OK;
                break;
            } else {
                Thread* thread = Thread::GetThread();
                assert(thread);

                /* This thread will have to wait for room in the queue */
                txWaitQueue.push_front(thread);
                txQueueLock.Unlock();
                status = Event::Wait(Event::neverSet, maxWait);
                txQueueLock.Lock();
                if (ER_ALERTED_THREAD == status) {
                    if (thread->GetAlertCode() == ENDPOINT_IS_DEAD_ALERTCODE) {
                        status = ER_BUS_ENDPOINT_CLOSING;
                    } else {
                        thread->GetStopEvent().ResetEvent();
                    }
                } else {
                    /* There was a timeout or some other non-expected exit from wait. Remove thread from wait queue. */
                    /* If thread isn't on queue, this means there is an alert in progress that we must clear */
                    bool foundThread = false;
                    deque<Thread*>::iterator eit = txWaitQueue.begin();
                    while (eit != txWaitQueue.end()) {
                        if (*eit == thread) {
                            txWaitQueue.erase(eit);
                            foundThread = true;
                            break;
                        }
                        ++eit;
                    }
                    if (!foundThread) {
                        thread->GetStopEvent().ResetEvent();
                    }
                }
                if ((ER_OK != status) && (ER_ALERTED_THREAD != status) && (ER_TIMEOUT != status)) {
                    break;
                }
            }
        }
    }
    txQueueLock.Unlock();

    if (wasEmpty) {
        status = txThread.Alert();
    }

#ifndef NDEBUG
#undef QCC_MODULE
#define QCC_MODULE "TXSTATS"
    static uint32_t lastTime = 0;
    uint32_t now = GetTimestamp();
    if ((now - lastTime) > 1000) {
        QCC_DbgPrintf(("Tx queue size (%s - %x) = %d", txThread.GetName().c_str(), txThread.GetHandle(), count));
        lastTime = now;
    }
#undef QCC_MODULE
#define QCC_MODULE "ALLJOYN"
#endif

    DecrementAndFetch(&numWaiters);
    return status;
}

void RemoteEndpoint::IncrementRef()
{
    IncrementAndFetch(&refCount);
}

void RemoteEndpoint::DecrementRef()
{
    int refs = DecrementAndFetch(&refCount);
    if (refs <= 0) {
        Stop();
    }
}

SocketFd RemoteEndpoint::GetSocketFd()
{
    if (isSocket) {
        SocketStream& ss = static_cast<SocketStream&>(stream);
        ss.DetachSocketFd();
        return ss.GetSocketFd();
    } else {
        return -1;
    }
}

}
