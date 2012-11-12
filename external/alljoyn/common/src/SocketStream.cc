/**
 * @file SocketStream.cc
 *
 * Sink/Source wrapper for Socket.
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

#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/Stream.h>
#include <qcc/String.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "NETWORK"


static int MakeSock(AddressFamily family, SocketType type)
{
    SocketFd sock = static_cast<SocketFd>(-1);
    QStatus status = Socket(family, type, sock);
    if (ER_OK != status) {
        QCC_LogError(status, ("Socket failed"));
        sock = -1;
    }
    return sock;
}

SocketStream::SocketStream(SocketFd sock) :
    isConnected(true),
    sock(sock),
    sourceEvent(sock, Event::IO_READ, false),
    sinkEvent(sock, Event::IO_WRITE, false),
    isDetached(false)
{
}


SocketStream::SocketStream(AddressFamily family, SocketType type) :
    isConnected(false),
    sock(MakeSock(family, type)),
    sourceEvent(sock, Event::IO_READ, false),
    sinkEvent(sourceEvent, Event::IO_WRITE, false),
    isDetached(false)
{
}

SocketStream::~SocketStream()
{
    Close();
}

QStatus SocketStream::Connect(qcc::String& host, uint16_t port)
{
    IPAddress ipAddr(host);
    QStatus status = qcc::Connect(sock, ipAddr, port);

    if (ER_WOULDBLOCK == status) {
        status = Event::Wait(sourceEvent, Event::WAIT_FOREVER);
        if (ER_OK == status) {
            status = qcc::Connect(sock, ipAddr, port);
        }
    }

    isConnected = (ER_OK == status);
    return status;
}

QStatus SocketStream::Connect(qcc::String& path)
{
    QStatus status = qcc::Connect(sock, path.c_str());
    if (ER_WOULDBLOCK == status) {
        status = Event::Wait(sourceEvent, Event::WAIT_FOREVER);
        if (ER_OK == status) {
            status = qcc::Connect(sock, path.c_str());
        }
    }
    isConnected = (ER_OK == status);
    return status;
}


void SocketStream::Close()
{
    if (isConnected) {
        if (!isDetached) {
            Shutdown(sock);
        }
        isConnected = false;
    }
    if (SOCKET_ERROR != sock) {
        qcc::Close(sock);
        sock = SOCKET_ERROR;
    }
}

QStatus SocketStream::PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    if (!isConnected) {
        return ER_FAIL;
    }
    if (reqBytes == 0) {
        actualBytes = 0;
        return ER_OK;
    }
    QStatus status;
    while (true) {
        status = Recv(sock, buf, reqBytes, actualBytes);
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(sourceEvent, timeout);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }

    if ((ER_OK == status) && (0 == actualBytes)) {
        /* Other end has closed */
        Close();
        status = ER_SOCK_OTHER_END_CLOSED;
    }
    return status;
}

QStatus SocketStream::PullBytesAndFds(void* buf, size_t reqBytes, size_t& actualBytes, SocketFd* fdList, size_t& numFds, uint32_t timeout)
{
    QStatus status;
    size_t recvdFds = 0;
    while (true) {
        /*
         * There will only be one set of file descriptors read in each call to this function
         * so once we have received file descriptors we revert to the standard Recv call.
         */
        if (recvdFds) {
            status = Recv(sock, buf, reqBytes, actualBytes);
        } else {
            status = RecvWithFds(sock, buf, reqBytes, actualBytes, fdList, numFds, recvdFds);
        }
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(sourceEvent, timeout);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }
    numFds = recvdFds;
    return status;
}

QStatus SocketStream::PushBytes(const void* buf, size_t numBytes, size_t& numSent)
{
    if (!isConnected) {
        return ER_FAIL;
    }
    if (numBytes == 0) {
        numSent = 0;
        return ER_OK;
    }
    QStatus status;
    while (true) {
        status = qcc::Send(sock, buf, numBytes, numSent);
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(sinkEvent);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }

    return status;
}

QStatus SocketStream::PushBytesAndFds(const void* buf, size_t numBytes, size_t& numSent, SocketFd* fdList, size_t numFds, uint32_t pid)
{
    if (!isConnected) {
        return ER_FAIL;
    }
    if (numBytes == 0) {
        return ER_BAD_ARG_2;
    }
    if (numFds == 0) {
        return ER_BAD_ARG_5;
    }
    QStatus status;
    while (true) {
        status = qcc::SendWithFds(sock, buf, numBytes, numSent, fdList, numFds, pid);
        if (ER_WOULDBLOCK == status) {
            status = Event::Wait(sinkEvent);
            if (ER_OK != status) {
                break;
            }
        } else {
            break;
        }
    }
    return status;
}

