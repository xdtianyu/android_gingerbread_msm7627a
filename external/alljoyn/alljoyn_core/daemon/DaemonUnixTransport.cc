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

#include <errno.h>

#include <qcc/platform.h>
#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>

#include "BusInternal.h"
#include "RemoteEndpoint.h"
#include "Router.h"
#include "DaemonUnixTransport.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

/*
 * An endpoint class to handle the details of authenticating a connection in
 * the Unix Domain Sockets way.
 */
class DaemonUnixEndpoint : public RemoteEndpoint {

  public:

    DaemonUnixEndpoint(BusAttachment& bus, bool incoming, const qcc::String connectSpec, SocketFd sock) :
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

DaemonUnixTransport::DaemonUnixTransport(BusAttachment& bus)
    : Thread("DaemonUnixTransport"), m_bus(bus), m_stopping(false)
{
    /*
     * We know we are daemon code, so we'd better be running with a daemon
     * router.  This is assumed elsewhere.
     */
    assert(m_bus.GetInternal().GetRouter().IsDaemon());
}

DaemonUnixTransport::~DaemonUnixTransport()
{
    Stop();
    Join();
}

QStatus DaemonUnixTransport::Start()
{
    m_stopping = false;
    return Thread::Start();
}

QStatus DaemonUnixTransport::Stop(void)
{
    m_stopping = true;

    /*
     * Tell the server accept loop thread to shut down through the thead
     * base class.
     */
    QStatus status = Thread::Stop();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonDaemonUnixTransport::Stop(): Failed to Stop() server thread"));
        return status;
    }

    m_endpointListLock.Lock();

    /*
     * Ask any running endpoints to shut down and exit their threads.
     */
    for (list<DaemonUnixEndpoint*>::iterator i = m_endpointList.begin(); i != m_endpointList.end(); ++i) {
        (*i)->Stop();
    }

    m_endpointListLock.Unlock();

    return ER_OK;
}

QStatus DaemonUnixTransport::Join(void)
{
    /*
     * Wait for the server accept loop thread to exit.
     */
    QStatus status = Thread::Join();
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonUnixTransport::Join(): Failed to Join() server thread"));
        return status;
    }

    /*
     * A call to Stop() above will ask all of the endpoints to stop.  We still
     * need to wait here until all of the threads running in those endpoints
     * actually stop running.  When a remote endpoint thead exits the endpoint
     * will call back into our EndpointExit() and have itself removed from the
     * list.  We poll for the all-exited condition, yielding the CPU to let
     * the endpoint therad wake and exit.
     */
    m_endpointListLock.Lock();
    while (m_endpointList.size() > 0) {
        m_endpointListLock.Unlock();
        qcc::Sleep(50);
        m_endpointListLock.Lock();
    }
    m_endpointListLock.Unlock();

    m_stopping = false;

    return ER_OK;
}

void DaemonUnixTransport::EndpointExit(RemoteEndpoint* ep)
{
    DaemonUnixEndpoint* uep = static_cast<DaemonUnixEndpoint*>(ep);
    assert(uep);

    /*
     * This is a callback driven from the remote endpoint thread exit function.
     * Our DaemonUnixEndpoint inherits from class RemoteEndpoint and so when
     * either of the threads (transmit or receive) of one of our endpoints exits
     * for some reason, we get called back here.
     */
    QCC_DbgTrace(("DaemonUnixTransport::EndpointExit()"));

    /* Remove the dead endpoint from the live endpoint list */
    m_endpointListLock.Lock();
    list<DaemonUnixEndpoint*>::iterator i = find(m_endpointList.begin(), m_endpointList.end(), uep);
    if (i != m_endpointList.end()) {
        m_endpointList.erase(i);
    }
    m_endpointListLock.Unlock();

    delete uep;
}

void* DaemonUnixTransport::Run(void* arg)
{
    QStatus status = ER_OK;

    while (!IsStopping()) {

        /*
         * Each time through the loop we create a set of events to wait on.
         * We need to wait on the stop event and all of the SocketFds of the
         * addresses and ports we are listening on.  If the list changes, the
         * code that does the change Alert()s this thread and we wake up and
         * re-evaluate the list of SocketFds.
         */
        m_listenFdsLock.Lock();
        vector<Event*> checkEvents, signaledEvents;
        checkEvents.push_back(&stopEvent);
        for (list<pair<qcc::String, SocketFd> >::const_iterator i = m_listenFds.begin(); i != m_listenFds.end(); ++i) {
            checkEvents.push_back(new Event(i->second, Event::IO_READ, false));
        }
        m_listenFdsLock.Unlock();

        /*
         * We have our list of events, so now wait for something to happen
         * on that list (or get alerted).
         */
        signaledEvents.clear();

        status = Event::Wait(checkEvents, signaledEvents);
        if (ER_OK != status) {
            QCC_LogError(status, ("Event::Wait failed"));
            break;
        }

        /*
         * We're back from our Wait() so something has happened.  Iterate over
         * the vector of signaled events to find out which event(s) got
         * bugged
         */
        for (vector<Event*>::iterator i = signaledEvents.begin(); i != signaledEvents.end(); ++i) {
            /*
             * Reset an Alert() (or Stop())
             */
            if (*i == &stopEvent) {
                stopEvent.ResetEvent();
                continue;
            }

            /*
             * If the current event is not the stop event, it reflects one of
             * the SocketFds we are waiting on for incoming connections.  Go
             * ahead and Accept() the new connection on the current SocketFd.
             */
            SocketFd newSock;

            status = Accept((*i)->GetFD(), newSock);

            if (status == ER_OK) {
                int enableCred = 1;
                int ret = setsockopt(newSock, SOL_SOCKET, SO_PASSCRED, &enableCred, sizeof(enableCred));
                if (ret == -1) {
                    status = ER_OS_ERROR;
                    qcc::Close(newSock);
                }
            }

            if (status == ER_OK) {
                qcc::String authName;
                DaemonUnixEndpoint* conn;
                ssize_t ret;
                char nulbuf = 255;
                struct cmsghdr* cmsg;
                struct iovec iov[] = { { &nulbuf, sizeof(nulbuf) } };
                struct msghdr msg;
                char cbuf[CMSG_SPACE(sizeof(struct ucred))];
                msg.msg_name = NULL;
                msg.msg_namelen = 0;
                msg.msg_iov = iov;
                msg.msg_iovlen = ArraySize(iov);
                msg.msg_flags = 0;
                msg.msg_control = cbuf;
                msg.msg_controllen = CMSG_LEN(sizeof(struct ucred));

                while (true) {
                    ret = recvmsg(newSock, &msg, 0);
                    if (ret == -1) {
                        if (errno == EWOULDBLOCK) {
                            qcc::Event event(newSock, qcc::Event::IO_READ, false);
                            status = Event::Wait(event, CRED_TIMEOUT);
                            if (status != ER_OK) {
                                QCC_LogError(status, ("Credentials exhange timeout"));
                                break;
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }

                if ((ret != 1) && (nulbuf != 0)) {
                    qcc::Close(newSock);
                    continue;
                }

                conn = new DaemonUnixEndpoint(m_bus, true, "", newSock);

                for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                    if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SCM_CREDENTIALS)) {
                        struct ucred* cred = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg));
                        conn->SetUserId(cred->uid);
                        conn->SetGroupId(cred->gid);
                        conn->SetProcessId(cred->pid);
                        QCC_DbgHLPrintf(("Received UID: %u  GID: %u  PID %u", cred->uid, cred->gid, cred->pid));
                    }
                }

                /* Initialized the features for this endpoint */
                conn->GetFeatures().isBusToBus = false;
                conn->GetFeatures().allowRemote = false;
                conn->GetFeatures().handlePassing = true;

                m_endpointListLock.Lock();
                m_endpointList.push_back(conn);
                m_endpointListLock.Unlock();
                status = conn->Establish("EXTERNAL", authName);
                if (ER_OK == status) {
                    conn->SetListener(this);
                    status = conn->Start();
                }
                if (ER_OK != status) {
                    QCC_LogError(status, ("Error starting RemoteEndpoint"));
                    m_endpointListLock.Lock();
                    list<DaemonUnixEndpoint*>::iterator ei = find(m_endpointList.begin(), m_endpointList.end(), conn);
                    if (ei != m_endpointList.end()) {
                        m_endpointList.erase(ei);
                    }
                    m_endpointListLock.Unlock();
                    delete conn;
                    conn = NULL;
                }
            } else if (ER_WOULDBLOCK == status) {
                status = ER_OK;
            }

            if (ER_OK != status) {
                QCC_LogError(status, ("Error accepting new connection. Ignoring..."));
            }
        }

        /*
         * We're going to loop back and create a new list of checkEvents that
         * reflect the current state, so we need to delete the checkEvents we
         * created on this iteration.
         */
        for (vector<Event*>::iterator i = checkEvents.begin(); i != checkEvents.end(); ++i) {
            if (*i != &stopEvent) {
                delete *i;
            }
        }
    }

    QCC_DbgPrintf(("DaemonUnixTransport::Run is exiting status=%s\n", QCC_StatusText(status)));
    return (void*) status;
}

QStatus DaemonUnixTransport::NormalizeTransportSpec(const char* inSpec, qcc::String& outSpec, map<qcc::String, qcc::String>& argMap) const
{
    QStatus status = ParseArguments("unix", inSpec, argMap);
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

QStatus DaemonUnixTransport::Connect(const char* connectArgs, RemoteEndpoint** newep)
{
    /* The daemon doesn't make outgoing connections over this transport */
    return ER_NOT_IMPLEMENTED;
}

QStatus DaemonUnixTransport::Disconnect(const char* connectSpec)
{
    /* The daemon doesn't make outgoing connections over this transport */
    return ER_NOT_IMPLEMENTED;
}

QStatus DaemonUnixTransport::StartListen(const char* listenSpec)
{
    /*
     * Don't bother trying to create new listeners if we're stopped or shutting
     * down.
     */
    if (IsRunning() == false || m_stopping == true) {
        return ER_BUS_TRANSPORT_NOT_STARTED;
    }

    /* Normalize the listen spec. */
    QStatus status;
    qcc::String normSpec;
    map<qcc::String, qcc::String> serverArgs;
    status = NormalizeTransportSpec(listenSpec, normSpec, serverArgs);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonUnixTransport::StartListen(): Invalid Unix listen spec \"%s\"", listenSpec));
        return status;
    }

    m_listenFdsLock.Lock();

    /*
     * Check to see if the requested address is already being listened to.  The
     * normalized listen spec is saved to define the instance of the listener.
     */
    for (list<pair<qcc::String, SocketFd> >::iterator i = m_listenFds.begin(); i != m_listenFds.end(); ++i) {
        if (i->first == normSpec) {
            m_listenFdsLock.Unlock();
            return ER_BUS_ALREADY_LISTENING;
        }
    }

    SocketFd listenFd = -1;
    status = Socket(QCC_AF_UNIX, QCC_SOCK_STREAM, listenFd);
    if (status != ER_OK) {
        m_listenFdsLock.Unlock();
        QCC_LogError(status, ("DaemonUnixTransport::StartListen(): Socket() failed"));
        return status;
    }

    /* Calculate bindAddr */
    qcc::String bindAddr;
    if (status == ER_OK) {
        if (!serverArgs["path"].empty()) {
            bindAddr = serverArgs["path"];
        } else if (!serverArgs["abstract"].empty()) {
            bindAddr = qcc::String("@") + serverArgs["abstract"];
        } else {
            m_listenFdsLock.Unlock();
            QCC_LogError(status, ("DaemonUnixTransport::StartListen(): Invalid listen spec for unix transport"));
            return ER_BUS_BAD_TRANSPORT_ARGS;
        }
    }

    /*
     * Bind the socket to the listen address and start listening for incoming
     * connections on it.
     */
    status = Bind(listenFd, bindAddr.c_str());
    if (ER_OK == status) {
        status = qcc::Listen(listenFd, 0);
        if (status == ER_OK) {
            QCC_DbgPrintf(("DaemonUnixTransport::StartListen(): Listening on %s", bindAddr.c_str()));
            m_listenFds.push_back(pair<qcc::String, SocketFd>(normSpec, listenFd));
        } else {
            QCC_LogError(status, ("DaemonUnixTransport::StartListen(): Listen failed"));
        }
    } else {
        QCC_LogError(status, ("DaemonUnixTransport::StartListen(): Failed to bind to %s", bindAddr.c_str()));
    }

    m_listenFdsLock.Unlock();

    /*
     * Signal the (probably) waiting run thread so it will wake up and add this
     * new socket to its list of sockets it is waiting for connections on.
     */
    if (ER_OK == status) {
        Alert();
    }

    return status;
}

QStatus DaemonUnixTransport::StopListen(const char* listenSpec)
{
    /*
     * Normalize the listen spec.  We are going to use the name string that was
     * put together for the StartListen call to find the listener instance to
     * stop, so we need to do it exactly the same way.
     */
    qcc::String normSpec;
    map<qcc::String, qcc::String> argMap;
    QStatus status = NormalizeTransportSpec(listenSpec, normSpec, argMap);
    if (status != ER_OK) {
        QCC_LogError(status, ("DaemonUnixTransport::StopListen(): Invalid Unix listen spec \"%s\"", listenSpec));
        return status;
    }

    /*
     * Find the (single) listen spec and remove it from the list of active FDs
     * used by the server accept loop (run thread).
     */
    m_listenFdsLock.Lock();
    status = ER_BUS_BAD_TRANSPORT_ARGS;
    qcc::SocketFd stopFd = -1;
    for (list<pair<qcc::String, SocketFd> >::iterator i = m_listenFds.begin(); i != m_listenFds.end(); ++i) {
        if (i->first == normSpec) {
            stopFd = i->second;
            m_listenFds.erase(i);
            status = ER_OK;
            break;
        }
    }
    m_listenFdsLock.Unlock();

    /*
     * If we took a socketFD off of the list of active FDs, we need to tear it
     * down and alert the server accept loop that the list of FDs on which it
     * is listening has changed.
     */
    if (status == ER_OK) {
        qcc::Shutdown(stopFd);
        qcc::Close(stopFd);

        Alert();
    }

    return status;
}

} // namespace ajn
