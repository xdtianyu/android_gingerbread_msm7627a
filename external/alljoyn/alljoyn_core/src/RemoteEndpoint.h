/**
 * @file
 * RemoteEndpoint provides a remote endpoint rx and tx threading.
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
#ifndef _ALLJOYN_REMOTEENDPOINT_H
#define _ALLJOYN_REMOTEENDPOINT_H

#include <qcc/platform.h>

#include <deque>

#include <qcc/String.h>
#include <qcc/GUID.h>
#include <qcc/Mutex.h>
#include <qcc/Stream.h>
#include <qcc/Thread.h>

#include "BusEndpoint.h"
#include "EndpointAuth.h"

#include <Status.h>

namespace ajn {

/**
 * %RemoteEndpoint handles incoming and outgoing messages
 * over a stream interface
 */
class RemoteEndpoint : public BusEndpoint, public qcc::ThreadListener {

    friend class EndpointAuth;

  public:

    /**
     * RemoteEndpoint::Features type.
     */
    class Features {

      public:

        Features() : isBusToBus(false), allowRemote(false), handlePassing(false)
        { }

        bool isBusToBus;       /**< When initiating connection this is an input value indicating if this is a bus-to-bus connection.
                                    When accepting a connection this is an output value indicicating if this is bus-to-bus connection. */

        bool allowRemote;      /**< When initiating a connection this input value tells the local daemon whether it wants to receive
                                    messages from remote busses. When accepting a connection, this output indicates whether the connected
                                    endpoint is willing to receive messages from remote busses. */

        bool handlePassing;    /**< Indicates if support for handle passing is enabled for this the endpoint. This is only
                                    enabled for endpoints that connect applications on the same device. */

    };

    /**
     * Listener called when endpoint changes state.
     */
    class EndpointListener {
      public:
        /**
         * Virtual destructor for derivable class.
         */
        virtual ~EndpointListener() { }

        /**
         * Called when endpoint is about to exit.
         *
         * @param ep   Endpoint that is exiting.
         */
        virtual void EndpointExit(RemoteEndpoint* ep) = 0;
    };

    /**
     * Constructor
     *
     * @param bus            Message bus associated with transport.
     * @param connectSpec    AllJoyn connection specification for this endpoint.
     * @param stream         Socket Stream used to communicate with media.
     * @param type           Base name for thread.
     * @param isSocket       true iff stream is actually a socketStream.
     */
    RemoteEndpoint(BusAttachment& bus,
                   bool incoming,
                   const qcc::String& connectSpec,
                   qcc::Stream& stream,
                   const char* type = "endpoint",
                   bool isSocket = true);

    /**
     * Destructor
     */
    virtual ~RemoteEndpoint();

    /**
     * Send an outgoing message.
     *
     * @param msg   Message to be sent.
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    virtual QStatus PushMessage(Message& msg);

    /**
     * Start the endpoint.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    virtual QStatus Start();

    /**
     * Request the endpoint to stop executing.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    virtual QStatus Stop();

    /**
     * Request endpoint to stop AFTER the endpoint's txqueue empties out.
     *
     * @return
     *     - ER_OK if successful.
     */
    QStatus StopAfterTxEmpty();

    /**
     * Request endpoint to pause receiving (wihtout stopping) AFTER next METHOD_REPLY is received.
     *
     * @return
     *     - ER_OK if successful.
     */
    QStatus PauseAfterRxReply();

    /**
     * Join the endpoint.
     * Block the caller until the endpoint is stopped.
     *
     * @return ER_OK if successful.
     */
    virtual QStatus Join(void);

    /**
     * Set the listener for this endpoint.
     *
     * @param listener   Endpoint listener.
     */
    void SetListener(EndpointListener* listener);

    /**
     * Get the unique bus name assigned by the bus for this endpoint.
     *
     * @return
     *      - The unique bus name for this endpoint
     *      - An empty string if called before the endpoint has been established.
     */
    const qcc::String& GetUniqueName() const { return auth.GetUniqueName(); }

    /**
     * Get the bus name for the peer at the remote end of this endpoint.
     *
     * @return  - The bus name of the remote side.
     *          - An empty string if called before the endpoint has been established.
     */
    const qcc::String& GetRemoteName() const { return auth.GetRemoteName(); }

    /**
     * Get the protocol version used by the remote end of this endpoint.
     *
     * @return  - The protocol version use by the remote side
     *          - 0 if called before the endpoint has been established
     */
    uint32_t GetRemoteProtocolVersion() const { return auth.GetRemoteProtocolVersion(); }

    /**
     * Establish a connection.
     *
     * @param authMechanisms  The authentication mechanism(s) to use.
     * @param authUsed        [OUT]    Returns the name of the authentication method
     *                                 that was used to establish the connection.
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Establish(const qcc::String& authMechanisms, qcc::String& authUsed) {
        return auth.Establish(authMechanisms, authUsed);
    }

    /**
     * Get the GUID of the remote side of a bus-to-bus endpoint.
     *
     * @return GUID of the remote side of a bus-to-bus endpoint.
     */
    const qcc::GUID& GetRemoteGUID() const { return auth.GetRemoteGUID(); }

    /**
     * Get the connect spec for this endpoint.
     *
     * @return The connect spec string (may be empty).
     */
    const qcc::String& GetConnectSpec() const { return connSpec; }

    /**
     * Return the user id of the endpoint.
     *
     * @return  User ID number.
     */
    uint32_t GetUserId() const { return -1; }

    /**
     * Return the group id of the endpoint.
     *
     * @return  Group ID number.
     */
    uint32_t GetGroupId() const { return -1; }

    /**
     * Return the process id of the remote end of this endpoint.
     *
     * @return  Process ID number or -1 if the process id is unknown.
     */
    uint32_t GetProcessId() const { return processId; }

    /**
     * Indicates if the endpoint supports reporting UNIX style user, group, and process IDs.
     *
     * @return  'true' if UNIX IDs supported, 'false' if not supported.
     */
    bool SupportsUnixIDs() const { return false; }

    /**
     * Indicate whether this endpoint can receive messages from other devices.
     *
     * @return true iff this endpoint can receive messages from other devices.
     */
    bool AllowRemoteMessages() { return features.allowRemote; }

    /**
     * Indicate if this endpoint is for an incoming connection or an outgoing connection.
     *
     * @return 'true' if incoming, 'false' if outgoing.
     */
    bool IsIncomingConnection() const { return incoming; }

    /**
     * Get the data source for this endpoint
     *
     * @return  The data source for this endpoint.
     */
    qcc::Source& GetSource() { return stream; };

    /**
     * Get the data sink for this endpoint
     *
     * @return  The data sink for this endpoint.
     */
    qcc::Sink& GetSink() { return stream; };

    /**
     * Get the SocketFd from this endpoint and detach it from the endpoint.
     *
     * @return Underlying socket file descriptor for this endpoint.
     */
    qcc::SocketFd GetSocketFd();

    /**
     * Return the features for this BusEndpoint
     *
     * @return   Returns the features for this BusEndpoint.
     */
    Features& GetFeatures() { return features; }

    /**
     * Increment the reference count for this remote endpoint.
     * RemoteEndpoints are destroyed when the number of references reaches zero.
     */
    void IncrementRef();

    /**
     * Decremeent the reference count for this remote endpoing.
     * RemoteEndpoints are destroyed when the number of refereneces reaches zero.
     */
    void DecrementRef();

  private:

    /**
     * Assignment operator is private - LocalEndpoints cannot be assigned.
     */
    RemoteEndpoint& operator=(const RemoteEndpoint& other) { return *this; }

    /**
     * Thread used to receive endpoint data.
     */
    class RxThread : public qcc::Thread {
      public:
        RxThread(BusAttachment& bus,
                 const char* name,
                 bool validateSender)
            : qcc::Thread(name), bus(bus), validateSender(validateSender) { }

      protected:
        qcc::ThreadReturn STDCALL Run(void* arg);

      private:
        BusAttachment& bus;       /**< Bus associated with transport */
        bool validateSender;      /**< If true, the sender field on incomming messages will be overwritten with actual endpoint name */
    };

    /**
     * Thread used to send endpoint data.
     */
    class TxThread : public qcc::Thread {
      public:
        TxThread(BusAttachment& bus,
                 const char* name,
                 std::deque<Message>& queue,
                 std::deque<Thread*>& waitQueue,
                 qcc::Mutex& queueLock)
            : qcc::Thread(name), bus(bus), queue(queue), waitQueue(waitQueue), queueLock(queueLock) { }

      protected:
        qcc::ThreadReturn STDCALL Run(void* arg);

      private:
        BusAttachment& bus;
        std::deque<Message>& queue;
        std::deque<Thread*>& waitQueue;
        qcc::Mutex& queueLock;
    };

    /**
     * Internal callback used to indicate that one of the internal threads (rx or tx) has exited.
     * RemoteEndpoint users should not call this method.
     *
     * @param thread   Thread that exited.
     */
    void ThreadExit(qcc::Thread* thread);

    BusAttachment& bus;                      /**< Message bus associated with this endpoint */
    qcc::Stream& stream;                     /**< Stream for this endpoint */
    EndpointAuth auth;                       /**< Endpoint AllJoynAuthentication */

    std::deque<Message> txQueue;             /**< Transmit message queue */
    std::deque<qcc::Thread*> txWaitQueue;    /**< Threads waiting for txQueue to become not-full */
    qcc::Mutex txQueueLock;                  /**< Transmit message queue mutex */
    int32_t exitCount;                       /**< Number of sub-threads (rx and tx) that have exited (atomically incremented) */

    RxThread rxThread;                       /**< Thread used to receive messages from the media */
    TxThread txThread;                       /**< Thread used to send messages to the media */

    EndpointListener* listener;              /**< Listener for thread exit notifications */

    qcc::String connSpec;                    /**< Connection specification for out-going connections */
    bool incoming;                           /**< Indicates if connection is incoming (true) or outgoing (false) */

    Features features;                       /**< Requested and negotiated features of this endpoint */
    uint32_t processId;                      /**< Process id of the process at the remote end of this endpoint */
    int32_t refCount;                        /**< Number of active users of this remote endpoint */
    bool isSocket;                           /**< True iff this endpoint contains a SockStream as its 'stream' member */
    bool armRxPause;                         /**< Pause Rx after receiving next METHOD_REPLY message */
    int32_t numWaiters;                      /**< Number of threads currently running in PushMessage */
};

}

#endif
