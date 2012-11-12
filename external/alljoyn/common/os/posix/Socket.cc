/**
 * @file
 *
 * Define the abstracted socket interface for Linux
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
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <qcc/IPAddress.h>
#include <qcc/ScatterGatherList.h>
#include <qcc/Util.h>
#include <qcc/Thread.h>

#include <Status.h>

#define QCC_MODULE "NETWORK"

namespace qcc {

const SocketFd INVALID_SOCKET_FD = -1;

static void MakeSockAddr(const char* path,
                         struct sockaddr_storage* addrBuf, socklen_t& addrSize)
{
    size_t pathLen = strlen(path);
    struct sockaddr_un sa;
    assert((size_t)addrSize >= sizeof(sa));
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, path, (std::min)(pathLen, sizeof(sa.sun_path) - 1));
    /*
     * We use an @ in the first character position to indicate an abstract socket. Abstract sockets
     * start with a NUL character.
     */
    if (sa.sun_path[0] == '@') {
        sa.sun_path[0] = 0;
        addrSize = offsetof(struct sockaddr_un, sun_path) + pathLen;
    } else {
        addrSize = sizeof(sa);
    }
    memcpy(addrBuf, &sa, sizeof(sa));
}


static void MakeSockAddr(const IPAddress& addr, uint16_t port,
                         struct sockaddr_storage* addrBuf, socklen_t addrSize)
{
    if (addr.IsIPv4()) {
        struct sockaddr_in sa;
        assert((size_t)addrSize >= sizeof(sa));
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = addr.GetIPv4AddressNetOrder();
        memcpy(addrBuf, &sa, sizeof(sa));
    } else {
        struct sockaddr_in6 sa;
        assert((size_t)addrSize >= sizeof(sa));
        memset(&sa, 0, sizeof(sa));
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(port);
        sa.sin6_flowinfo = 0;  // TODO: What should go here???
        addr.RenderIPv6Binary(sa.sin6_addr.s6_addr, sizeof(sa.sin6_addr.s6_addr));
        sa.sin6_scope_id = 0;  // TODO: What should go here???
        memcpy(addrBuf, &sa, sizeof(sa));
    }
}


static QStatus GetSockAddr(const sockaddr_storage* addrBuf, socklen_t addrSize,
                           IPAddress& addr, uint16_t& port)
{
    QStatus status = ER_OK;
    char hostname[NI_MAXHOST];
    char servInfo[NI_MAXSERV];

    int s = getnameinfo((struct sockaddr*) addrBuf,
                        addrSize,
                        hostname,
                        NI_MAXHOST, servInfo,
                        NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

    if (s != 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("GetSockAddr: %d - %s", s, gai_strerror(s)));
    } else {
        /*
         * In the case of IPv6, the hostname will have the interface name
         * tacked on the end, as in "fe80::20c:29ff:fe7b:6f10%eth1".  We
         * need to chop that off since nobody expects either the Spanish
         * Inquisition or the interface.
         */
        char* p = strchr(hostname, '%');
        if (p) *p = 0;
        addr = IPAddress(hostname);
        port = atoi(servInfo);
    }

    return status;
}



QStatus Socket(AddressFamily addrFamily, SocketType type, SocketFd& sockfd)
{
    QStatus status = ER_OK;
    int ret;

    QCC_DbgTrace(("Socket(addrFamily = %d, type = %d, sockfd = <>)", addrFamily, type));

    ret = socket(static_cast<int>(addrFamily), static_cast<int>(type), 0);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Opening socket: %d - %s", errno, strerror(errno)));
    } else {
        sockfd = static_cast<SocketFd>(ret);
    }
    return status;
}


QStatus Connect(SocketFd sockfd, const IPAddress& remoteAddr, uint16_t remotePort)
{
    QStatus status = ER_OK;
    int ret;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("Connect(sockfd = %d, remoteAddr = %s, remotePort = %hu)",
                  sockfd, remoteAddr.ToString().c_str(), remotePort));

    MakeSockAddr(remoteAddr, remotePort, &addr, addrLen);
    ret = connect(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (ret == -1) {
        if ((errno == EINPROGRESS) || (errno == EALREADY)) {
            status = ER_WOULDBLOCK;
        } else if (errno == EISCONN) {
            status = ER_OK;
        } else if (errno == ECONNREFUSED) {
            status = ER_CONN_REFUSED;
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Connecting (sockfd = %u) to %s %d: %d - %s", sockfd,
                                  remoteAddr.ToString().c_str(), remotePort,
                                  errno, strerror(errno)));
        }
    } else {
        int flags = fcntl(sockfd, F_GETFL, 0);
        ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        if (ret == -1) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Connect fcntl (sockfd = %u) to O_NONBLOCK: %d - %s", sockfd, errno, strerror(errno)));
            /* better to close and error out than to leave in unexpected state */
            close(sockfd);
        }
    }

    return status;
}

QStatus Connect(SocketFd sockfd, const char* pathName)
{
    QStatus status = ER_OK;
    int ret;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("Connect(sockfd = %u, path = %s)", sockfd, pathName));

    MakeSockAddr(pathName, &addr, addrLen);
    ret = connect(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Connecting (sockfd = %u) to %s : %d - %s", sockfd,
                              pathName,
                              errno, strerror(errno)));
    } else {
        int flags = fcntl(sockfd, F_GETFL, 0);
        ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        if (ret == -1) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Connect fcntl (sockfd = %u) to O_NONBLOCK: %d - %s", sockfd, errno, strerror(errno)));
            /* better to close and error out than to leave in unexpected state */
            close(sockfd);
        }
    }
    return status;
}


QStatus Bind(SocketFd sockfd, const IPAddress& localAddr, uint16_t localPort)
{
    QStatus status = ER_OK;
    int ret;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("Bind(sockfd = %d, localAddr = %s, localPort = %hu)",
                  sockfd, localAddr.ToString().c_str(), localPort));

    MakeSockAddr(localAddr, localPort, &addr, addrLen);
    ret = bind(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (ret != 0) {
        status = (errno == EADDRNOTAVAIL ? ER_SOCKET_BIND_ERROR : ER_OS_ERROR);
        QCC_LogError(status, ("Binding (sockfd = %u) to %s %d: %d - %s", sockfd,
                              localAddr.ToString().c_str(), localPort, errno, strerror(errno)));
    }
    return status;
}


QStatus Bind(SocketFd sockfd, const char* pathName)
{
    QStatus status = ER_OK;
    int ret;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("Bind(sockfd = %d, pathName = %s)", sockfd, pathName));

    MakeSockAddr(pathName, &addr, addrLen);
    ret = bind(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (ret != 0) {
        status = (errno == EADDRNOTAVAIL ? ER_SOCKET_BIND_ERROR : ER_OS_ERROR);
        QCC_LogError(status, ("Binding (sockfd = %u) to %s: %d - %s", sockfd,
                              pathName, errno, strerror(errno)));
    }
    return status;
}


QStatus Listen(SocketFd sockfd, int backlog)
{
    QStatus status = ER_OK;
    int ret;

    QCC_DbgTrace(("Listen(sockfd = %d, backlog = %d)", sockfd, backlog));

    ret = listen(static_cast<int>(sockfd), backlog);
    if (ret != 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Listening (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    }
    return status;
}


QStatus Accept(SocketFd sockfd, IPAddress& remoteAddr, uint16_t& remotePort, SocketFd& newSockfd)
{
    QStatus status = ER_OK;
    int ret;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("Accept(sockfd = %d, remoteAddr = <>, remotePort = <>)", sockfd));

    ret = accept(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
    if (ret == -1) {
        if (errno == EWOULDBLOCK) {
            status = ER_WOULDBLOCK;
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Accept (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
        }
    } else {
        if (addr.ss_family == AF_INET) {
            struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&addr);
            uint8_t* portBuf = reinterpret_cast<uint8_t*>(&sa->sin_port);
            remoteAddr = IPAddress(reinterpret_cast<uint8_t*>(&sa->sin_addr.s_addr),
                                   IPAddress::IPv4_SIZE);
            remotePort = (static_cast<uint16_t>(portBuf[0]) << 8) | static_cast<uint16_t>(portBuf[1]);
        } else if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6* sa = reinterpret_cast<struct sockaddr_in6*>(&addr);
            uint8_t* portBuf = reinterpret_cast<uint8_t*>(&sa->sin6_port);
            remoteAddr = IPAddress(reinterpret_cast<uint8_t*>(&sa->sin6_addr.s6_addr),
                                   IPAddress::IPv6_SIZE);
            remotePort = (static_cast<uint16_t>(portBuf[0]) << 8) | static_cast<uint16_t>(portBuf[1]);
        } else {
            remotePort = 0;
        }
        newSockfd = static_cast<SocketFd>(ret);
        QCC_DbgPrintf(("New socket FD: %d", newSockfd));

        int flags = fcntl(newSockfd, F_GETFL, 0);
        ret = fcntl(newSockfd, F_SETFL, flags | O_NONBLOCK);

        if (ret == -1) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Accept fcntl (newSockfd = %u) to O_NONBLOCK: %d - %s", newSockfd, errno, strerror(errno)));
            /* better to close and error out than to leave in unexpected state */
            close(newSockfd);
        }
    }
    return status;
}


QStatus Accept(SocketFd sockfd, SocketFd& newSockfd)
{
    IPAddress addr;
    uint16_t port;
    return Accept(sockfd, addr, port, newSockfd);
}


QStatus Shutdown(SocketFd sockfd)
{
    QStatus status = ER_OK;
    int ret;

    QCC_DbgTrace(("Shutdown(sockfd = %d)", sockfd));

    ret = shutdown(static_cast<int>(sockfd), SHUT_RDWR);
    if (ret != 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Shutdown socket (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    }
    return status;
}


void Close(SocketFd sockfd)
{

    close(static_cast<int>(sockfd));
}

QStatus SocketDup(SocketFd sockfd, SocketFd& dupSock)
{
    QStatus status = ER_OK;

    dupSock = dup(sockfd);
    if (dupSock < 0) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("SocketDup of %d failed %d - %s", sockfd, errno, strerror(errno)));
    }
    return status;
}

QStatus GetLocalAddress(SocketFd sockfd, IPAddress& addr, uint16_t& port)
{
    QStatus status = ER_OK;
    struct sockaddr_storage addrBuf;
    socklen_t addrLen = sizeof(addrBuf);
    int ret;

    QCC_DbgTrace(("GetLocalAddress(sockfd = %d, addr = <>, port = <>)", sockfd));

    memset(&addrBuf, 0, addrLen);

    ret = getsockname(static_cast<int>(sockfd), reinterpret_cast<struct sockaddr*>(&addrBuf), &addrLen);

    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Geting Local Address (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    } else {
        if (addrBuf.ss_family == AF_INET) {
            struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&addrBuf);
            uint8_t* portBuf = reinterpret_cast<uint8_t*>(&sa->sin_port);
            QCC_DbgLocalData(&addrBuf, sizeof(*sa));
            addr = IPAddress(reinterpret_cast<uint8_t*>(&sa->sin_addr.s_addr), IPAddress::IPv4_SIZE);
            port = (static_cast<uint16_t>(portBuf[0]) << 8) | static_cast<uint16_t>(portBuf[1]);
        } else {
            struct sockaddr_in6* sa = reinterpret_cast<struct sockaddr_in6*>(&addrBuf);
            uint8_t* portBuf = reinterpret_cast<uint8_t*>(&sa->sin6_port);
            addr = IPAddress(reinterpret_cast<uint8_t*>(&sa->sin6_addr.s6_addr), IPAddress::IPv6_SIZE);
            port = (static_cast<uint16_t>(portBuf[0]) << 8) | static_cast<uint16_t>(portBuf[1]);
        }
        QCC_DbgPrintf(("Local Address (sockfd = %u): %s - %u", sockfd, addr.ToString().c_str(), port));
    }

    return status;
}


QStatus Send(SocketFd sockfd, const void* buf, size_t len, size_t& sent)
{
    QStatus status = ER_OK;
    ssize_t ret;

    QCC_DbgTrace(("Send(sockfd = %d, *buf = <>, len = %lu, sent = <>)",
                  sockfd, len));
    assert(buf != NULL);

    QCC_DbgLocalData(buf, len);

    while (status == ER_OK) {
        ret = send(static_cast<int>(sockfd), buf, len, MSG_NOSIGNAL);
        if (ret == -1) {
            if (errno == EAGAIN) {
                fd_set set;
                FD_ZERO(&set);
                FD_SET(sockfd, &set);
                ret = select(sockfd + 1, NULL, &set, NULL, NULL);
                if (ret == -1) {
                    status = ER_OS_ERROR;
                    QCC_LogError(status, ("Send (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
                    break;
                }
                continue;
            }
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Send (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
        } else {
            sent = static_cast<size_t>(ret);
        }
        break;
    }
    return status;
}


QStatus SendTo(SocketFd sockfd, IPAddress& remoteAddr, uint16_t remotePort,
               const void* buf, size_t len, size_t& sent)
{
    QStatus status = ER_OK;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    ssize_t ret;

    QCC_DbgTrace(("SendTo(sockfd = %d, remoteAddr = %s, remotePort = %u, *buf = <>, len = %lu, sent = <>)",
                  sockfd, remoteAddr.ToString().c_str(), remotePort, len));
    assert(buf != NULL);

    QCC_DbgLocalData(buf, len);

    MakeSockAddr(remoteAddr, remotePort, &addr, addrLen);
    ret = sendto(static_cast<int>(sockfd), buf, len, MSG_NOSIGNAL,
                 reinterpret_cast<struct sockaddr*>(&addr), addrLen);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("SendTo (sockfd = %u  addr = %s  port = %u): %d - %s",
                              sockfd, remoteAddr.ToString().c_str(), remotePort, errno, strerror(errno)));
    } else {
        sent = static_cast<size_t>(ret);
    }
    return status;
}


static QStatus SendSGCommon(SocketFd sockfd, struct sockaddr_storage* addr, socklen_t addrLen,
                            const ScatterGatherList& sg, size_t& sent)
{
    QStatus status = ER_OK;
    ssize_t ret;
    struct msghdr msg;
    size_t index;
    struct iovec* iov;
    ScatterGatherList::const_iterator iter;
    QCC_DbgTrace(("SendSGCommon(sockfd = %d, *addr, addrLen, sg[%u:%u/%u], sent = <>)",
                  sockfd, sg.Size(), sg.DataSize(), sg.MaxDataSize()));

    iov = new struct iovec[sg.Size()];
    for (index = 0, iter = sg.Begin(); iter != sg.End(); ++index, ++iter) {
        iov[index].iov_base = iter->buf;
        iov[index].iov_len = iter->len;
        QCC_DbgLocalData(iov[index].iov_base, iov[index].iov_len);
    }

    msg.msg_name = addr;
    msg.msg_namelen = addrLen;
    msg.msg_iov = iov;
    msg.msg_iovlen = sg.Size();
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    ret = sendmsg(static_cast<int>(sockfd), &msg, MSG_NOSIGNAL);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("SendSGCommon (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    } else {
        sent = static_cast<size_t>(ret);
    }
    delete[] iov;
    return status;
}

QStatus SendSG(SocketFd sockfd, const ScatterGatherList& sg, size_t& sent)
{
    QCC_DbgTrace(("SendSG(sockfd = %d, sg[%u:%u/%u], sent = <>)",
                  sockfd, sg.Size(), sg.DataSize(), sg.MaxDataSize()));

    return SendSGCommon(sockfd, NULL, 0, sg, sent);
}

QStatus SendToSG(SocketFd sockfd, IPAddress& remoteAddr, uint16_t remotePort,
                 const ScatterGatherList& sg, size_t& sent)
{
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    QCC_DbgTrace(("SendToSG(sockfd = %d, remoteAddr = %s, remotePort = %u, sg[%u:%u/%u], sent = <>)",
                  sockfd, remoteAddr.ToString().c_str(), remotePort,
                  sg.Size(), sg.DataSize(), sg.MaxDataSize()));

    MakeSockAddr(remoteAddr, remotePort, &addr, addrLen);
    return SendSGCommon(sockfd, &addr, addrLen, sg, sent);
}



QStatus Recv(SocketFd sockfd, void* buf, size_t len, size_t& received)
{
    QStatus status = ER_OK;
    ssize_t ret;

    QCC_DbgTrace(("Recv(sockfd = %d, buf = <>, len = %lu, received = <>)", sockfd, len));
    assert(buf != NULL);

    ret = recv(static_cast<int>(sockfd), buf, len, 0);
    if ((ret == -1) && (errno == EWOULDBLOCK)) {
        return ER_WOULDBLOCK;
    }

    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Recv (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    } else {
        received = static_cast<size_t>(ret);
    }

    QCC_DbgRemoteData(buf, received);

    return status;
}


QStatus RecvFrom(SocketFd sockfd, IPAddress& remoteAddr, uint16_t& remotePort,
                 void* buf, size_t len, size_t& received)
{
    QStatus status = ER_OK;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    ssize_t ret;
    received = 0;

    QCC_DbgTrace(("RecvFrom(sockfd = %d, remoteAddr = %s, remotePort = %u, buf = <>, len = %lu, received = <>)",
                  sockfd, remoteAddr.ToString().c_str(), remotePort, len));
    assert(buf != NULL);

    ret = recvfrom(static_cast<int>(sockfd), buf, len, 0,
                   reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("RecvFrom (sockfd = %u): %d - %s",
                              sockfd, errno, strerror(errno)));
    } else {
        received = static_cast<size_t>(ret);
        GetSockAddr(&addr, addrLen, remoteAddr, remotePort);
        QCC_DbgPrintf(("Received %u bytes, remoteAddr = %s, remotePort = %u",
                       received, remoteAddr.ToString().c_str(), remotePort));
    }

    QCC_DbgRemoteData(buf, received);

    return status;
}


static QStatus RecvSGCommon(SocketFd sockfd, struct sockaddr_storage* addr, socklen_t* addrLen,
                            ScatterGatherList& sg, size_t& received)
{
    QStatus status = ER_OK;
    ssize_t ret;
    struct msghdr msg;
    size_t index;
    struct iovec* iov;
    ScatterGatherList::const_iterator iter;
    QCC_DbgTrace(("RecvSGCommon(sockfd = &d, addr, addrLen, sg = <>, received = <>)",
                  sockfd));

    iov = new struct iovec[sg.Size()];
    for (index = 0, iter = sg.Begin(); iter != sg.End(); ++index, ++iter) {
        iov[index].iov_base = iter->buf;
        iov[index].iov_len = iter->len;
    }

    msg.msg_name = addr;
    msg.msg_namelen = *addrLen;
    msg.msg_iov = iov;
    msg.msg_iovlen = sg.Size();
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    ret = recvmsg(static_cast<int>(sockfd), &msg, 0);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("RecvSGCommon (sockfd = %u): %d - %s", sockfd, errno, strerror(errno)));
    } else {
        received = static_cast<size_t>(ret);
        sg.SetDataSize(static_cast<size_t>(ret));
        *addrLen = msg.msg_namelen;
    }
    delete[] iov;

#if !defined(NDEBUG)
    if (1) {
        size_t rxcnt = received;
        QCC_DbgPrintf(("Received %u bytes", received));
        for (iter = sg.Begin(); rxcnt > 0 && iter != sg.End(); ++iter) {
            QCC_DbgRemoteData(iter->buf, std::min(rxcnt, iter->len));
            rxcnt -= std::min(rxcnt, iter->len);
        }
    }
#endif

    return status;
}


QStatus RecvSG(SocketFd sockfd, ScatterGatherList& sg, size_t& received)
{
    socklen_t addrLen = 0;
    QCC_DbgTrace(("RecvSG(sockfd = %d, sg = <>, received = <>)", sockfd));

    return RecvSGCommon(sockfd, NULL, &addrLen, sg, received);
}


QStatus RecvFromSG(SocketFd sockfd, IPAddress& remoteAddr, uint16_t& remotePort,
                   ScatterGatherList& sg, size_t& received)
{
    QStatus status;
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    status = RecvSGCommon(sockfd, &addr, &addrLen, sg, received);
    if (ER_OK == status) {
        GetSockAddr(&addr, addrLen, remoteAddr, remotePort);
        QCC_DbgTrace(("RecvFromSG(sockfd = %d, remoteAddr = %s, remotePort = %u, sg = <>, sent = <>)",
                      sockfd, remoteAddr.ToString().c_str(), remotePort));
    }
    return status;
}

QStatus RecvWithFds(SocketFd sockfd, void* buf, size_t len, size_t& received, SocketFd* fdList, size_t maxFds, size_t& recvdFds)
{
    QStatus status = ER_OK;

    if (!fdList) {
        return ER_BAD_ARG_5;
    }
    if (!maxFds) {
        return ER_BAD_ARG_6;
    }
    QCC_DbgHLPrintf(("RecvWithFds"));

    recvdFds = 0;
    maxFds = std::min(maxFds, SOCKET_MAX_FILE_DESCRIPTORS);

    struct iovec iov[] = { { buf, len } };

    static const size_t sz = CMSG_SPACE(sizeof(struct ucred)) + CMSG_SPACE(SOCKET_MAX_FILE_DESCRIPTORS * sizeof(SocketFd));
    char cbuf[sz];

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    msg.msg_iov = iov;
    msg.msg_iovlen = ArraySize(iov);
    msg.msg_control = cbuf;
    msg.msg_controllen = CMSG_LEN(sz);

    ssize_t ret = recvmsg(sockfd, &msg, 0);
    if (ret == -1) {
        if (errno == EWOULDBLOCK) {
            status = ER_WOULDBLOCK;
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Receiving file descriptors: %d - %s", errno, strerror(errno)));
        }
    } else {
        struct cmsghdr* cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if ((cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SCM_RIGHTS)) {
                recvdFds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(SocketFd);
                /*
                 * Check we have enough room to return the file descriptors.
                 */
                if (recvdFds > maxFds) {
                    status = ER_OS_ERROR;
                    QCC_LogError(status, ("Too many handles: %d implementation limit is %d", recvdFds, maxFds));
                } else {
                    memcpy(fdList, CMSG_DATA(cmsg), recvdFds * sizeof(SocketFd));
                    QCC_DbgHLPrintf(("Received %d handles %d...", recvdFds, fdList[0]));
                }
                break;
            }
        }
        received = static_cast<size_t>(ret);
    }
    return status;
}

QStatus SendWithFds(SocketFd sockfd, const void* buf, size_t len, size_t& sent, SocketFd* fdList, size_t numFds, uint32_t pid)
{
    QStatus status = ER_OK;

    if (!fdList) {
        return ER_BAD_ARG_5;
    }
    if (!numFds || (numFds > SOCKET_MAX_FILE_DESCRIPTORS)) {
        return ER_BAD_ARG_6;
    }

    QCC_DbgHLPrintf(("SendWithFds"));

    struct iovec iov[] = { { const_cast<void*>(buf), len } };
    size_t sz = numFds * sizeof(SocketFd);
    char* cbuf = new char[CMSG_SPACE(sz)];

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));

    msg.msg_iov = iov;
    msg.msg_iovlen = ArraySize(iov);
    msg.msg_control = cbuf;
    msg.msg_controllen = CMSG_SPACE(sz);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sz);

    QCC_DbgHLPrintf(("Sending %d file descriptors %d...", numFds, fdList[0]));

    memcpy(CMSG_DATA(cmsg), fdList, sz);

    ssize_t ret = sendmsg(sockfd, &msg, 0);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Sending file descriptor: %d - %s", errno, strerror(errno)));
    } else {
        sent = static_cast<size_t>(ret);
    }

    delete [] cbuf;

    return status;
}

QStatus SocketPair(SocketFd(&sockets)[2])
{
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
    return (ret == 0) ? ER_OK : ER_FAIL;
}

QStatus SetBlocking(SocketFd sockfd, bool blocking)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    ssize_t ret;
    if (blocking) {
        ret = fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
    } else {
        ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    }
    if (ret == -1) {
        return ER_OS_ERROR;
    } else {
        return ER_OK;
    }
}

}  /* namespace */
