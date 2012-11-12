/**
 * @file
 * TCPTransportBase is a partial specialization of Transport that listens
 * on a TCP socket.
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

#include <list>

#include <qcc/IPAddress.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "TCPTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

class TCPEndpoint : public RemoteEndpoint {
  public:
    TCPEndpoint(TCPTransport* transport, BusAttachment& bus, const qcc::String connectSpec,
                qcc::SocketFd sock, const qcc::IPAddress& ipAddr, uint16_t port)
        :
        RemoteEndpoint(bus, false, connectSpec, m_stream, "tcp"),
        m_transport(transport),
        m_stream(sock),
        m_ipAddr(ipAddr),
        m_port(port),
        m_wasSuddenDisconnect(true) { }

    ~TCPEndpoint() { }

    const qcc::IPAddress& GetIPAddress() { return m_ipAddr; }
    uint16_t GetPort() { return m_port; }
    bool IsSuddenDisconnect() { return m_wasSuddenDisconnect; }
    void SetSuddenDisconnect(bool val) { m_wasSuddenDisconnect = val; }

  protected:
    TCPTransport* m_transport;
    qcc::SocketStream m_stream;
    qcc::IPAddress m_ipAddr;
    uint16_t m_port;
    bool m_wasSuddenDisconnect;
};

TCPTransport::TCPTransport(BusAttachment& bus)
    : m_bus(bus), m_running(false), m_stopping(false), m_listener(0)
{
}

TCPTransport::~TCPTransport()
{
    Stop();
    Join();
}

QStatus TCPTransport::Start()
{
    /*
     * Start() is defined in the underlying class Transport as a placeholder
     * for a method used to crank up a server accept loop.  We have no need
     * for such a loop, so we don't need to do anything but make a couple of
     * notes to ourselves that this method has been called.
     */
    m_running = true;
    m_stopping = false;

    return ER_OK;
}

QStatus TCPTransport::Stop(void)
{
    m_running = false;

    /*
     * Ask any running endpoints to shut down and exit their threads.
     */
    m_endpointListLock.Lock();

    /*
     * Stop() and Join() may be called any number of times, but at least one
     * call to Stop() must preceed any call to Join() if there are active
     * endpoints.  The member variable m_stopping indicates we are in the
     * transitional state where Stop() has been called but active endpoints
     * still exist (have not exited yet).
     */
    if (m_endpointList.size()) {
        m_stopping = true;
    } else {
        m_stopping = false;
    }

    for (vector<TCPEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        (*i)->Stop();
    }

    m_endpointListLock.Unlock();

    return ER_OK;
}

QStatus TCPTransport::Join(void)
{
    /*
     * Must have called Stop() to arrange for endpoints to exist or have never
     * started any endpoints before doing this.
     */
    assert(m_running == false);

    /*
     * A call to Stop() above will ask all of the endpoints to stop.  We still
     * need to wait here until all of the threads running in those endpoints
     * actually stop running.  When the underlying remote endpoint stops it will
     * call back into EndpointExit() and remove itself from the list.  We poll
     * for the all-exited condition, yielding the CPU to let them all wake and
     * exit.
     */
    m_endpointListLock.Lock();

    /*
     * Stop() or Join() can be called multiple times, but at least one call to
     * Stop() must preceed any call to Join().  If there are endpoints that we
     * expect to exit as a result of a previous Stop(), then m_stopping will
     * be set to true.  If there are endpoints, and m_stopping is not true
     * we have a programming error (calling Join() before calling Stop()).
     */
    if (m_endpointList.size()) {
        assert(m_stopping == true);
    }

    /*
     * A call to Stop() above will ask all of the endpoints to stop.  We still
     * need to wait here until all of the threads running in those endpoints
     * actually stop running.  When the underlying remote endpoint stops it will
     * call back into EndpointExit() and remove itself from the list.  We poll
     * for the all-exited condition, yielding the CPU to let them all wake and
     * exit.
     */
    while (m_endpointList.size() > 0) {
        m_endpointListLock.Unlock();
        qcc::Sleep(50);
        m_endpointListLock.Lock();
    }

    m_endpointListLock.Unlock();

    m_stopping = false;

    return ER_OK;
}

void TCPTransport::EndpointExit(RemoteEndpoint* ep)
{
    TCPEndpoint* tep = static_cast<TCPEndpoint*>(ep);
    assert(tep);

    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our TCPEndpoint inherits from class RemoteEndpoint and so when either of
     * the threads (transmit or receive) of one of our endpoints exits for some
     * reason, we get called back here.
     */
    QCC_DbgTrace(("TCPTransport::EndpointExit()"));

    /* Remove the dead endpoint from the live endpoint list */
    m_endpointListLock.Lock();
    vector<TCPEndpoint*>::iterator i = find(m_endpointList.begin(), m_endpointList.end(), tep);
    if (i != m_endpointList.end()) {
        m_endpointList.erase(i);
    }
    m_endpointListLock.Unlock();

    /*
     * The endpoint can exit if it was asked to by us in response to a Disconnect()
     * from higher level code, or if it got an error from the underlying transport.
     * We need to notify upper level code if the disconnect is due to an event from
     * the transport.
     */
    if (m_listener && tep->IsSuddenDisconnect()) {
        m_listener->BusConnectionLost(tep->GetConnectSpec());
    }

    delete tep;
}

QStatus TCPTransport::NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, map<qcc::String, qcc::String>& argMap) const
{
    /*
     * Take the string in inSpec, which must start with "tcp:" and parse it,
     * looking for comma-separated "key=value" pairs and initialize the
     * argMap with those pairs.
     */
    QStatus status = ParseArguments("tcp", inSpec, argMap);
    if (status != ER_OK) {
        return status;
    }

    /*
     * We need to return a map with all of the configuration items set to
     * valid values and a normalized string with the same.  For a client or
     * service TCP, we need a valid "addr" key.
     */
    map<qcc::String, qcc::String>::iterator i = argMap.find("addr");
    if (i == argMap.end()) {
        return ER_FAIL;
    } else {
        /*
         * We have a value associated with the "addr" key.  Run it through
         * a conversion function to make sure it's a valid value.
         */
        IPAddress addr;
        status = addr.SetAddress(i->second);
        if (status == ER_OK) {
            i->second = addr.ToString();
            outSpec = "tcp:addr=" + i->second;
        } else {
            return ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    /*
     * For a client or service TCP, we need a valid "port" key.
     */
    i = argMap.find("port");
    if (i == argMap.end()) {
        return ER_FAIL;
    } else {
        /*
         * We have a value associated with the "port" key.  Run it through
         * a conversion function to make sure it's a valid value.
         */
        uint32_t port = StringToU32(i->second);
        if (port > 0 && port <= 0xffff) {
            i->second = U32ToString(port);
            outSpec += ",port=" + i->second;
        } else {
            return ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    return ER_OK;
}

QStatus TCPTransport::Connect(const char* connectSpec, RemoteEndpoint** newep)
{
    QCC_DbgHLPrintf(("TCPTransport::Connect(): %s", connectSpec));

    /*
     * Don't bother trying to create new endpoints if the state precludes them.
     */
    if (m_running == false || m_stopping == true) {
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    /*
     * Parse and normalize the connectArgs.  For a client or service, there are
     * no reasonable defaults and so the addr and port keys MUST be present or
     * an error is returned.
     */
    QStatus status;
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("TCPTransport::Connect(): Invalid TCP connect spec \"%s\"", connectSpec));
        return status;
    }

    IPAddress ipAddr(argMap.find("addr")->second); // Guaranteed to be there.
    uint16_t port = StringToU32(argMap["port"]);   // Guaranteed to be there.

    /*
     * Check to see if we are already connected to a remote endpoint identified
     * by the address and port.  If we are we never duplicate the connection.
     */
    m_endpointListLock.Lock();
    for (vector<TCPEndpoint*>::const_iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        if ((port == (*i)->GetPort()) && (((*i)->GetIPAddress() == ipAddr))) {
            m_endpointListLock.Unlock();
            return ER_BUS_ALREADY_CONNECTED;
        }
    }
    m_endpointListLock.Unlock();

    /*
     * This is a new not previously satisfied connection request, so attempt
     * to connect to the remote TCP address and port specified in the connectSpec.
     */
    SocketFd sockFd = -1;
    status = Socket(QCC_AF_INET, QCC_SOCK_STREAM, sockFd);
    if (status != ER_OK) {
        QCC_LogError(status, ("TCPTransport(): socket Create() failed"));
        return status;
    }

    /*
     * Got a socket, now Connect() to the remote address and port.
     */
    bool isConnected = false;
    status = qcc::Connect(sockFd, ipAddr, port);
    if (status != ER_OK) {
        QCC_LogError(status, ("TCPTransport(): socket Connect() failed"));
        qcc::Close(sockFd);
        return status;
    }

    isConnected = true;

    /*
     * We have a TCP connection established, but DBus (the wire protocol
     * of which we are using) requires that every connection,
     * irrespective of transport, start with a single zero byte.  This
     * is so that the Unix-domain socket transport used by DBus can pass
     * SCM_RIGHTS out-of-band when that byte is sent.
     */
    uint8_t nul = 0;
    size_t sent;

    status = Send(sockFd, &nul, 1, sent);
    if (status != ER_OK) {
        QCC_LogError(status, ("TCPTransport::Connect(): Failed to send initial NUL byte"));
        qcc::Close(sockFd);
        return status;
    }

    /*
     * The underlying transport mechanism is started, but we need to create a
     * TCPEndpoint object that will orchestrate the movement of data across the
     * transport.
     */
    TCPEndpoint* conn = NULL;
    if (status == ER_OK) {
        conn = new TCPEndpoint(this, m_bus, normSpec, sockFd, ipAddr, port);
        m_endpointListLock.Lock();
        m_endpointList.push_back(conn);
        m_endpointListLock.Unlock();

        /* Initialized the features for this endpoint */
        conn->GetFeatures().isBusToBus = false;
        conn->GetFeatures().allowRemote = m_bus.GetInternal().AllowRemoteMessages();
        conn->GetFeatures().handlePassing = true;

        qcc::String authName;
        status = conn->Establish("ANONYMOUS", authName);
        if (status == ER_OK) {
            conn->SetListener(this);
            status = conn->Start();
        }

        /*
         * We put the endpoint into our list of active endpoints to make life
         * easier reporting problems up the chain of command behind the scenes
         * if we got an error during the authentincation process and the endpoint
         * startup.  If we did get an error, we need to remove the endpoint if it
         * is still there and the endpoint exit callback didn't kill it.
         */
        if (status != ER_OK) {
            QCC_LogError(status, ("TCPTransport::Connect(): Start TCPEndpoint failed"));
            m_endpointListLock.Lock();
            vector<TCPEndpoint*>::iterator i = find(m_endpointList.begin(), m_endpointList.end(), conn);
            if (i != m_endpointList.end()) {
                m_endpointList.erase(i);
            }
            m_endpointListLock.Unlock();
            delete conn;
            conn = NULL;
        }
    }

    /*
     * If we got an error, we need to cleanup the socket and zero out the
     * returned endpoint.  If we got this done without a problem, we return
     * a pointer to the new endpoint.
     */
    if (status != ER_OK) {
        if (isConnected) {
            qcc::Shutdown(sockFd);
        }
        if (sockFd >= 0) {
            qcc::Close(sockFd);
        }
        if (newep) {
            *newep = NULL;
        }
    } else {
        if (newep) {
            *newep = conn;
        }
    }

    return status;
}

QStatus TCPTransport::Disconnect(const char* connectSpec)
{
    QCC_DbgHLPrintf(("TCPTransport::Disconnect(): %s", connectSpec));

    /*
     * Higher level code tells us which connection is refers to by giving us the
     * same connect spec it used in the Connect() call.  We have to determine the
     * address and port in exactly the same way
     */
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    QStatus status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("TCPTransport::Disconnect(): Invalid TCP connect spec \"%s\"", connectSpec));
        return status;
    }

    IPAddress ipAddr(argMap.find("addr")->second); // Guaranteed to be there.
    uint16_t port = StringToU32(argMap["port"]);   // Guaranteed to be there.

    /*
     * Stop the remote endpoint.  Be careful here since calling Stop() on the
     * TCPEndpoint is going to cause the transmit and receive threads of the
     * underlying RemoteEndpoint to exit, which will cause our EndpointExit()
     * to be called, which will walk the list of endpoints and delete the one
     * we are stopping.  Once we poke ep->Stop(), the pointer to ep must be
     * considered dead.
     */
    status = ER_BUS_BAD_TRANSPORT_ARGS;
    m_endpointListLock.Lock();
    for (vector<TCPEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        if ((*i)->GetPort() == port && (*i)->GetIPAddress() == ipAddr) {
            TCPEndpoint* ep = *i;
            ep->SetSuddenDisconnect(false);
            m_endpointListLock.Unlock();
            return ep->Stop();
        }
    }
    m_endpointListLock.Unlock();
    return status;
}

} // namespace ajn
