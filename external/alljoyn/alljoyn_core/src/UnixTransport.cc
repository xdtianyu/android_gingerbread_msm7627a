/**
 * @file
 * UnixTransport is an implementation of Transport that listens
 * on an AF_UNIX socket.
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

#include <errno.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "UnixTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

class UnixEndpoint : public RemoteEndpoint {
  public:
    /* Unix endpoint constructor */
    UnixEndpoint(BusAttachment& bus, bool incoming, const qcc::String connectSpec, SocketFd sock) :
        RemoteEndpoint(bus, incoming, connectSpec, stream, "unix"),
        userId(-1),
        groupId(-1),
        processId(-1),
        stream(sock)
    {
    }

    /**
     * Set the user id of the endpoint.
     *
     * @param   userId      User ID number.
     */
    void SetUserId(uint32_t userId) { this->userId = userId; }

    /**
     * Set the group id of the endpoint.
     *
     * @param   groupId     Group ID number.
     */
    void SetGroupId(uint32_t groupId) { this->groupId = groupId; }

    /**
     * Set the process id of the endpoint.
     *
     * @param   processId   Process ID number.
     */
    void SetProcessId(uint32_t processId) { this->processId = processId; }

    /**
     * Return the user id of the endpoint.
     *
     * @return  User ID number.
     */
    uint32_t GetUserId() const { return userId; }

    /**
     * Return the group id of the endpoint.
     *
     * @return  Group ID number.
     */
    uint32_t GetGroupId() const { return groupId; }

    /**
     * Return the process id of the endpoint.
     *
     * @return  Process ID number.
     */
    uint32_t GetProcessId() const { return processId; }

    /**
     * Indicates if the endpoint supports reporting UNIX style user, group, and process IDs.
     *
     * @return  'true' if UNIX IDs supported, 'false' if not supported.
     */
    bool SupportsUnixIDs() const { return true; }


  private:
    uint32_t userId;
    uint32_t groupId;
    uint32_t processId;
    SocketStream stream;
};

UnixTransport::UnixTransport(BusAttachment& bus) : m_bus(bus), m_running(false), m_stopping(false), m_listener(0)
{
}

UnixTransport::~UnixTransport()
{
    Stop();
    Join();
}

QStatus UnixTransport::Start()
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

QStatus UnixTransport::Stop(void)
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

    for (vector<UnixEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        (*i)->Stop();
    }

    m_endpointListLock.Unlock();

    return ER_OK;
}

QStatus UnixTransport::Join(void)
{
    /*
     * Must have called Stop() to arrange for endpoints to exist or have never
     * started any endpoints before doing this.
     */
    assert(m_running == false);

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

void UnixTransport::EndpointExit(RemoteEndpoint* ep)
{
    UnixEndpoint* uep = static_cast<UnixEndpoint*>(ep);
    assert(uep);

    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our UnixEndpoint inherits from class RemoteEndpoint and so when either of
     * the threads (transmit or receive) of one of our endpoints exits for some
     * reason, we get called back here.
     */
    QCC_DbgTrace(("UnixTransport::EndpointExit()"));

    /* Remove thread from thread list */
    m_endpointListLock.Lock();
    vector<UnixEndpoint*>::iterator i = find(m_endpointList.begin(), m_endpointList.end(), uep);
    if (i != m_endpointList.end()) {
        m_endpointList.erase(i);
    }
    m_endpointListLock.Unlock();

    delete uep;
}



QStatus UnixTransport::NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, map<qcc::String, qcc::String>& argMap) const
{
    /*
     * Take the string in inSpec, which must start with "unix:" and parse it,
     * looking for comma-separated "key=value" pairs and initialize the
     * argMap with those pairs.
     */
    QStatus status = ParseArguments("unix", inSpec, argMap);
    if (status != ER_OK) {
        return status;
    }

    qcc::String path = Trim(argMap["path"]);
    qcc::String abstract = Trim(argMap["abstract"]);
    if (ER_OK == status) {
        // @@ TODO: Path normalization?
        outSpec = "unix:";
        if (!path.empty()) {
            outSpec.append("path=");
            outSpec.append(path);
            argMap["_spec"] = path;
        } else if (!abstract.empty()) {
            outSpec.append("abstract=");
            outSpec.append(abstract);
            argMap["_spec"] = qcc::String("@") + abstract;
        } else {
            status = ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    return status;
}

QStatus UnixTransport::Connect(const char* connectArgs, RemoteEndpoint** newep)
{
    /*
     * Parse and normalize the connectArgs.  For a client or service, there are
     * no reasonable defaults and so the addr and port keys MUST be present or
     * an error is returned.
     */
    QStatus status;
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    status = NormalizeTransportSpec(connectArgs, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("UnixTransport::Connect(): Invalid Unix connect spec \"%s\"", connectArgs));
        return status;
    }

    /*
     * Check to see if we are already connected to a remote endpoint identified
     * by the address and port.  If we are we never duplicate the connection.
     */
    qcc::String& connectAddr = argMap["_spec"];
    m_endpointListLock.Lock();
    for (vector<UnixEndpoint*>::const_iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        if (normSpec == (*i)->GetConnectSpec()) {
            m_endpointListLock.Unlock();
            return ER_BUS_ALREADY_CONNECTED;
        }
    }
    m_endpointListLock.Unlock();

    /*
     * This is a new not previously satisfied connection request, so attempt
     * to connect to the endpoint specified in the connectSpec.
     */
    SocketFd sockFd = -1;
    status = Socket(QCC_AF_UNIX, QCC_SOCK_STREAM, sockFd);
    if (status != ER_OK) {
        QCC_LogError(status, ("UnixTransport(): socket Create() failed"));
        return status;
    }

    /*
     * Got a socket, now Connect() to the remote address and port.
     */
    bool isConnected = false;
    status = qcc::Connect(sockFd, connectAddr.c_str());
    if (status != ER_OK) {
        QCC_LogError(status, ("UnixTransport(): socket Connect() failed"));
        qcc::Close(sockFd);
        return status;
    }

    isConnected = true;

    int enableCred = 1;
    int rc = setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &enableCred, sizeof(enableCred));
    if (rc == -1) {
        QCC_LogError(status, ("UnixTransport(): setsockopt(SO_PASSCRED) failed"));
        qcc::Close(sockFd);
        return ER_OS_ERROR;
    }

    /*
     * Compose a header that includes the local user credentials and a single NUL byte.
     */
    ssize_t ret;
    char nulbuf = 0;
    struct cmsghdr* cmsg;
    struct ucred* cred;
    struct iovec iov[] = { { &nulbuf, sizeof(nulbuf) } };
    char cbuf[CMSG_SPACE(sizeof(struct ucred))];
    ::memset(cbuf, 0, sizeof(cbuf));
    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = ArraySize(iov);
    msg.msg_control = cbuf;
    msg.msg_controllen = ArraySize(cbuf);
    msg.msg_flags = 0;

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
    cred = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg));
    cred->uid = GetUid();
    cred->gid = GetGid();
    cred->pid = GetPid();

    QCC_DbgHLPrintf(("Sending UID: %u  GID: %u  PID %u", cred->uid, cred->gid, cred->pid));

    UnixEndpoint* conn = NULL;
    ret = sendmsg(sockFd, &msg, 0);
    if (ret == 1) {
        conn = new UnixEndpoint(m_bus, false, normSpec, sockFd);
        m_endpointListLock.Lock();
        m_endpointList.push_back(conn);
        m_endpointListLock.Unlock();

        /* Initialized the features for this endpoint */
        conn->GetFeatures().isBusToBus = false;
        conn->GetFeatures().allowRemote = m_bus.GetInternal().AllowRemoteMessages();
        conn->GetFeatures().handlePassing = true;

        qcc::String authName;
        status = conn->Establish("EXTERNAL", authName);
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
            QCC_LogError(status, ("UnixTransport::Connect(): Start UnixEndpoint failed"));
            m_endpointListLock.Lock();
            vector<UnixEndpoint*>::iterator i = find(m_endpointList.begin(), m_endpointList.end(), conn);
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
        /*
         * If we don't disable this every read will have credentials which adds overhead if have
         * enabled unix file descriptor passing.
         */
        int disableCred = 0;
        rc = setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &disableCred, sizeof(disableCred));
        if (rc == -1) {
            QCC_LogError(status, ("UnixTransport(): setsockopt(SO_PASSCRED) failed"));
        }
        if (newep) {
            *newep = conn;
        }
    }

    return status;
}

QStatus UnixTransport::Disconnect(const char* connectSpec)
{
    QCC_DbgHLPrintf(("UnixTransport::Disconnect(): %s", connectSpec));

    /*
     * Higher level code tells us which connection is refers to by giving us the
     * same connect spec it used in the Connect() call.  We have to determine the
     * address and port in exactly the same way
     */
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    QStatus status = NormalizeTransportSpec(connectSpec, normSpec, argMap);
    if (ER_OK != status) {
        QCC_LogError(status, ("UnixTransport::Disconnect(): Invalid Unix connect spec \"%s\"", connectSpec));
        return status;
    }

    /*
     * Stop the remote endpoint.  Be careful here since calling Stop() on the
     * UnixEndpoint is going to cause the transmit and receive threads of the
     * underlying RemoteEndpoint to exit, which will cause our EndpointExit()
     * to be called, which will walk the list of endpoints and delete the one
     * we are stopping.  Once we poke ep->Stop(), the pointer to ep must be
     * considered dead.
     */
    status = ER_BUS_BAD_TRANSPORT_ARGS;

    m_endpointListLock.Lock();
    for (vector<UnixEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        if (normSpec == (*i)->GetConnectSpec()) {
            UnixEndpoint* ep = *i;
            m_endpointListLock.Unlock();
            return ep->Stop();
        }
    }
    m_endpointListLock.Unlock();
    return status;
}

} // namespace ajn
