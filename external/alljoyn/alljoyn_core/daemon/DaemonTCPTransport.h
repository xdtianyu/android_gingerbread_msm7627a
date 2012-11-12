/**
 * @file
 * TCPTransport is a specialization of class Transport for daemons talking over
 * TCP.
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

#ifndef _ALLJOYN_DAEMONTCPTRANSPORT_H
#define _ALLJOYN_DAEMONTCPTRANSPORT_H

#ifndef __cplusplus
#error Only include DaemonTCPTransport.h in C++ code.
#endif

#include <Status.h>

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Thread.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/time.h>

#include "Transport.h"
#include "RemoteEndpoint.h"

#include "NameService.h"

namespace ajn {

class DaemonTCPEndpoint;

/**
 * @brief A class for TCP Transports used in daemons.
 *
 * The TCPTransport class has different incarnations depending on whether or not
 * an instantiated endpoint using the transport resides in a daemon, or in the
 * case of Windows, on a service or client.  The differences between these
 * versions revolves around routing and discovery. This class provides a
 * specialization of class Transport for use by daemons.
 */
class DaemonTCPTransport : public Transport, public RemoteEndpoint::EndpointListener, public qcc::Thread {
    friend class DaemonTCPEndpoint;

  public:
    /**
     * Create a TCP based transport for use by daemons.
     *
     * @param bus The BusAttachment associated with this endpoint
     */
    DaemonTCPTransport(BusAttachment& bus);

    /**
     * Destructor
     */
    virtual ~DaemonTCPTransport();

    /**
     * Start the transport and associate it with a router.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     *
     * @return
     *      - ER_OK if successful
     *      - an error status otherwise.
     */
    QStatus Join();

    /**
     * Determine if this transport is running. Running means Start() has been called.
     *
     * @return  Returns true if the transport is running.
     */
    bool IsRunning() { return Thread::IsRunning(); }

    /**
     * @internal
     * @brief Normalize a transport specification.
     *
     * Given a transport specification, convert it into a form which is guaranteed to
     * have a one-to-one relationship with a connection instance.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, std::map<qcc::String, qcc::String>& argMap) const;

    /**
     * Connect to a specified remote AllJoyn/DBus address.
     *
     * @param connectSpec    Transport specific key/value args used to configure the client-side endpoint.
     *                       The form of this string is @c "<transport>:<key1>=<val1>,<key2>=<val2>..."
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Connect(const char* connectSpec, RemoteEndpoint** newep);

    /**
     * Disconnect from a specified AllJoyn/DBus address.
     *
     * @param connectSpec    The connectSpec used in Connect.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Disconnect(const char* connectSpec);

    /**
     * Start listening for incomming connections on a specified bus address.
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *                    - Valid transport is @c "tcp". All others ignored.
     *                    - Valid keys are:
     *                        - @c addr = IP address of server to connect to.
     *                        - @c port = Port number of server to connect to.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus StartListen(const char* listenSpec);

    /**
     * @brief Stop listening for incomming connections on a specified bus address.
     *
     * This method cancels a StartListen request. Therefore, the listenSpec must
     * match previous call to StartListen().
     *
     * @param listenSpec  Transport specific key/value arguments that specify the physical interface to listen on.
     *                    - Valid transport is @c "tcp". All others ignored.
     *                    - Valid keys are:
     *                        - @c addr = IP address of server to connect to.
     *                        - @c port = Port number of server to connect to.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus StopListen(const char* listenSpec);

    /**
     * Set a listener for transport related events.  There can only be one
     * listener set at a time. Setting a listener implicitly removes any
     * previously set listener.
     *
     * @param listener  Listener for transport related events.
     */
    void SetListener(TransportListener* listener) { m_listener = listener; }

    /**
     * @internal
     * @brief Start discovering busses.
     */
    void EnableDiscovery(const char* namePrefix);

    /**
     * @internal
     * @brief Stop discovering busses to connect to.  No action needs to be
     * taken in the IP-based name service we use.
     */
    void DisableDiscovery(const char* namePrefix) { }

    /**
     * Start advertising a well-known name with the given quality of service.
     *
     * @param advertiseName   Well-known name to add to list of advertised names.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus EnableAdvertisement(const qcc::String& advertiseName);

    /**
     * Stop advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to remove from list of advertised names.
     * @param nameListEmpty   Indicates whether advertise name list is completely empty (safe to disable OTA advertising).
     */
    void DisableAdvertisement(const qcc::String& advertiseName, bool nameListEmpty);

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName(); }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_WLAN; }

    /**
     * Get a list of the possible listen specs of the current Transport for a
     * given set of session options.
     *
     * Session options specify high-level characteristics of session, such as
     * whether or not the underlying transport carries data encapsulated in
     * AllJoyn messages, and whether or not delivery is reliable.
     *
     * It is possible that there is more than one answer to the question: what
     * abstract address should I use when talking to another endpoint.  Each
     * Transports is equipped to understand how many answers there are and also
     * which answers are better than the others.  This method fills in the
     * provided vector with a list of currently available busAddresses ordered
     * according to which the transport thinks would be best.
     *
     * If there are no addresses appropriate to the given session options the
     * provided vector of String is left unchanged.  If there are addresses,
     * they are added at the end of the provided vector.
     *
     * @param opts Session options describing the desired characteristics of
     *             an underlying session
     * @param busAddrs A vector of String to which bus addresses corresponding
     *                 to IFF_UP interfaces matching the desired characteristics
     *                 are added.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus GetListenAddresses(const SessionOpts& opts, std::vector<qcc::String>& busAddrs) const;

    /**
     * Indicates whether this transport may be used for a connection between
     * an application and the daemon on the same machine or not.
     *
     * @return  true indicates this transport may be used for local connections.
     */
    bool LocallyConnectable() const { return true; }

    /**
     * Indicates whether this transport may be used for a connection between
     * an application and the daemon on a different machine or not.
     *
     * @return  true indicates this transport may be used for external connections.
     */
    bool ExternallyConnectable() const { return true; }

    /**
     * Name of transport used in transport specs.
     *
     * @return name of transport: @c "tcp".
     */
    static const char* TransportName() { return "tcp"; }

    /**
     * Callback for TCPEndpoint exit.
     *
     * @param endpoint   TCPEndpoint instance that has exited.
     */
    void EndpointExit(RemoteEndpoint* endpoint);

  private:
    BusAttachment& m_bus;                                          /**< The message bus for this transport */
    NameService* m_ns;                                             /**< The name service used for bus name discovery */
    bool m_stopping;                                               /**< True if Stop() has been called but endpoints still exist */
    TransportListener* m_listener;                                 /**< Registered TransportListener */
    std::list<DaemonTCPEndpoint*> m_authList;                      /**< List of authenticating endpoints */
    std::list<DaemonTCPEndpoint*> m_endpointList;                  /**< List of active endpoints */
    qcc::Mutex m_endpointListLock;                                 /**< Mutex that protects the endpoint and auth lists */

    std::list<std::pair<qcc::String, qcc::SocketFd> > m_listenFds; /**< file descriptors the transport is listening on */
    qcc::Mutex m_listenFdsLock;                                    /**< Mutex that protects m_listenFds */

    /**
     * @internal
     * @brief Thread entry point.
     *
     * @param arg  Unused thread entry arg.
     */
    qcc::ThreadReturn STDCALL Run(void* arg);

    /**
     * @internal
     * @brief Authentication complete notificiation.
     *
     * @param conn Pointer to the DaemonTCPEndpoint that completed authentication.
     */
    void Authenticated(DaemonTCPEndpoint* conn);

    /**
     * @internal
     * @brief Normalize a listen specification.
     *
     * Given a listen specification (which is the same as a transport
     * specification but with relaxed semantics allowing defaults), convert
     * it into a form which is guaranteed to have a one-to-one relationship
     * with a listener instance.
     *
     * @param inSpec    Input transport connect spec.
     * @param outSpec   Output transport connect spec.
     * @param argMap    Parsed parameter map.
     *
     * @return ER_OK if successful.
     */
    QStatus NormalizeListenSpec(const char* inSpec, qcc::String& outSpec, std::map<qcc::String, qcc::String>& argMap) const;

    class FoundCallback {
      public:
        FoundCallback(TransportListener*& listener) : m_listener(listener) { }
        void Found(const qcc::String& busAddr, const qcc::String& guid, std::vector<qcc::String>& nameList, uint8_t timer);
      private:
        TransportListener*& m_listener;
    };

    FoundCallback m_foundCallback;  /**< Called by NameService when new busses are discovered */

    /**
     * @brief The default timeout for in-process authentications.
     *
     * The authentication process can be used as the basis of a denial of
     * service attack by simply stopping in mid-authentication.  If an
     * authentication takes longer than this number of milliseconds, it may be
     * summarily aborted if another connection comes in.  This value can be
     * overridden in the config file by setting "auth_timeout".  The 30 second
     * number comes from the smaller of two common DBus auth_timeout settings:
     * 30 sec or 240 sec.
     */
    static const uint32_t ALLJOYN_AUTH_TIMEOUT_DEFAULT = 30000;

    /**
     * @brief The default value for the maximum number of authenticating
     * connections.
     *
     * This corresponds to the configuration item "max_incomplete_connections"
     * in the DBus configuration, but it applies only to the TCP transport.  To
     * override this value, change the limit, "max_incomplete_connections_tcp".
     * Typically, DBus sets this value to 10,000 which is essentially infinite
     * from the perspective of a phone.  Since this represents a transient state
     * in connection establishment, there should be few connections in this
     * state, so we default to a quite low number.
     */
    static const uint32_t ALLJOYN_MAX_INCOMPLETE_CONNECTIONS_TCP_DEFAULT = 10;

    /**
     * @brief The default value for the maximum number of TCP connections
     * (remote endpoints).
     *
     * This corresponds to the configuration item "max_completed_connections"
     * in the DBus configuration, but it applies only to the TCP transport.
     * To override this value, change the limit, "max_completed_connections_tcp".
     * Typically, DBus sets this value to 100,000 which is essentially infinite
     * from the perspective of a phone.  Since we expect bus topologies to be
     * relatively small, we default to a quite low number.
     *
     * @warning This maximum is enforced on incoming connections only.  An
     * AllJoyn daemon is free to form as many outbound connections as it pleases
     * but if the total number of connections exceeds this value, no inbound
     * connections will be accepted.  This is because we are defending against
     * attacks from "abroad" and trust ourselves implicitly.
     */
    static const uint32_t ALLJOYN_MAX_COMPLETED_CONNECTIONS_TCP_DEFAULT = 50;
};

} // namespace ajn

#endif // _ALLJOYN_DAEMONTCPTRANSPORT_H
