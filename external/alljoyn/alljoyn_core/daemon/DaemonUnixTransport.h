/**
 * @file
 * UnixTransport is a specialization of class Transport for daemons talking over
 * Unix domain sockets.
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
#ifndef _ALLJOYN_DAEMONUNIXTRANSPORT_H
#define _ALLJOYN_DAEMONUNIXTRANSPORT_H

#ifndef __cplusplus
#error Only include DaemonUnixTransport.h in C++ code.
#endif

#include <Status.h>

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Mutex.h>
#include <qcc/Socket.h>
#include <qcc/Thread.h>
#include <qcc/SocketStream.h>
#include <qcc/time.h>

#include "Transport.h"
#include "RemoteEndpoint.h"

namespace ajn {

class DaemonUnixEndpoint;

/**
 * @brief A class for Unix Transports used in daemons.
 *
 * The UnixTransport class has different incarnations depending on whether or not
 * an instantiated endpoint using the transport resides in a daemon, or in the
 * case of Windows, on a service or client.  The differences between these
 * versions revolves around routing and discovery. This class provides a
 * specialization of class Transport for use by daemons.
 */
class DaemonUnixTransport : public Transport, public RemoteEndpoint::EndpointListener, public qcc::Thread {
    friend class DaemonUnixEndpoint;

  public:
    /**
     * Create a Unix domain socket based Transport for use by daemons.
     *
     * @param bus  The bus associated with this transport.
     */
    DaemonUnixTransport(BusAttachment& bus);

    /**
     * Destructor
     */
    ~DaemonUnixTransport();

    /**
     * Start the transport and associate it with a router.
     *
     * @return ER_OK if successful.
     */
    QStatus Start();

    /**
     * Stop the transport.
     *
     * @return ER_OK if successful.
     */
    QStatus Stop();

    /**
     * Pend the caller until the transport stops.
     * @return ER_OK if successful.
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
     *                             - Valid transport is @c "unix". All others ignored.
     *                             - Valid keys are:
     *                                 - @c path = Filesystem path name for AF_UNIX socket
     *                                 - @c abstract = Abstract (unadvertised) filesystem path for AF_UNIX socket.
     *
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus Connect(const char* connectSpec, RemoteEndpoint** newep);

    /**
     * Disconnect from a given unix endpoint.
     *
     * @param connectSpec   The connect spec used in connect
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
     *                    - Valid transport is @c "unix". All others ignored.
     *                    - Valid keys are:
     *                        - @c path = Filesystem path name for AF_UNIX socket
     *                        - @c abstract = Abstract (unadvertised) filesystem path for AF_UNIX socket.
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
     *                    - Valid transport is @c "unix". All others ignored.
     *                    - Valid keys are:
     *                        - @c path = Filesystem path name for AF_UNIX socket
     *                        - @c abstract = Abstract (unadvertised) filesystem path for AF_UNIX socket.
     *
     * @return ER_OK if successful.
     */
    QStatus StopListen(const char* listenSpec);

    /**
     * @brief Provide an empty implementation of a function not used in the
     * case of a Unix transport.
     *
     * @param listener Unused parameter.
     */
    void SetListener(TransportListener* listener) { };

    /**
     * @brief Provide an empty implementation of a discovery function not used
     * by the Unix daemon transport.
     *
     * @param namePrefix Unused parameter.
     */
    void EnableDiscovery(const char* namePrefix) { }

    /**
     * @brief Provide an empty implementation of a discovery function not used
     * by the Unix daemon transport.
     *
     * @param namePrefix Unused parameter.
     */
    void DisableDiscovery(const char* namePrefix) { }

    /**
     * Start advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to add to list of advertised names.
     * @return
     *      - ER_OK if successful.
     *      - an error status otherwise.
     */
    QStatus EnableAdvertisement(const qcc::String& advertiseName) { return ER_FAIL; }

    /**
     * Stop advertising a well-known name with a given quality of service.
     *
     * @param advertiseName   Well-known name to remove from list of advertised names.
     * @param nameListEmpty   Indicates whether advertise name list is completely empty (safe to disable OTA advertising).
     */
    void DisableAdvertisement(const qcc::String& advertiseName, bool nameListEmpty) { }

    /**
     * Get the transport mask for this transport
     *
     * @return the TransportMask for this transport.
     */
    TransportMask GetTransportMask() const { return TRANSPORT_NONE; }

    /**
     * Get a list of the possible listen specs for a given set of session options.
     * @param[IN]    opts      Session options.
     * @param[OUT]   busAddrs  Set of listen addresses. Always empty for this transport.
     * @return ER_OK if successful.
     */
    QStatus GetListenAddresses(const SessionOpts& opts, std::vector<qcc::String>& busAddrs) const { return ER_OK; }

    /**
     * Returns the name of this transport
     */
    const char* GetTransportName() const { return TransportName(); }

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
    bool ExternallyConnectable() const { return false; }

    /**
     * Name of transport used in transport specs.
     *
     * @return name of transport: @c "unix".
     */
    static const char* TransportName() { return "unix"; }

    /**
     * Callback for UnixEndpoint exit.
     *
     * @param endpoint   UnixEndpoint instance that has exited.
     */
    void EndpointExit(RemoteEndpoint* endpoint);

  private:
    BusAttachment& m_bus;                                          /**< The message bus for this transport */
    bool m_stopping;                                               /**< True if Stop() has been called but endpoints still exist */
    std::list<DaemonUnixEndpoint*> m_endpointList;                 /**< List of active endpoints */
    qcc::Mutex m_endpointListLock;                                 /**< Mutex that protects the endpoint list */

    std::list<std::pair<qcc::String, qcc::SocketFd> > m_listenFds; /**< file descriptors the transport is listening on */
    qcc::Mutex m_listenFdsLock;                                    /**< Mutex that protects m_listenFds */

    /**
     * @internal
     * @brief Thread entry point.
     *
     * @param arg  Unused thread entry arg.
     */
    qcc::ThreadReturn STDCALL Run(void* arg);

    static const int CRED_TIMEOUT = 5000;  /**< Times out credentials exchange to avoid denial of service attack */
};

} // namespace ajn

#endif // _ALLJOYN_DAEMONUNIXTRANSPORT_H
