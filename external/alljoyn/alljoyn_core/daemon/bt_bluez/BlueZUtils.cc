/**
 * @file
 * Utility classes for the BlueZ implementation of BT transport.
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <qcc/Socket.h>
#include <qcc/SocketStream.h>
#include <qcc/String.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>

#include "BDAddress.h"
#include "BlueZ.h"
#include "BlueZIfc.h"
#include "BlueZUtils.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN_BT"

using namespace ajn;
using namespace std;
using namespace qcc;

namespace ajn {
namespace bluez {

/******************************************************************************/

_AdapterObject::_AdapterObject(BusAttachment& bus, const qcc::String& path) :
    ProxyBusObject(bus, bzBusName, path.c_str(), 0),
    id(0)
{
    size_t i = path.size();
    while (i > 0) {
        --i;
        char c = path[i];
        if (!isdigit(c)) {
            break;
        }
        id *= 10;
        id += c - '0';
    }
}


/******************************************************************************/


BTSocketStream::BTSocketStream(SocketFd sock) :
    SocketStream(sock),
    buffer(NULL),
    offset(0),
    fill(0)
{
    struct l2cap_options opts;
    socklen_t optlen = sizeof(opts);
    int ret;
    ret = getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen);
    if (ret == -1) {
        QCC_LogError(ER_OS_ERROR, ("Failed to get in/out MTU for L2CAP socket, using default of 672"));
        inMtu = 672;
        outMtu = 672;
    } else {
        inMtu = opts.imtu;
        outMtu = opts.omtu;
    }
    buffer = new uint8_t[inMtu];
}


QStatus BTSocketStream::PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    if (!IsConnected()) {
        return ER_FAIL;
    }
    if (reqBytes == 0) {
        actualBytes = 0;
        return ER_OK;
    }

    QStatus status;
    size_t avail = fill - offset;

    if (avail > 0) {
        /* Pull from internal buffer */
        actualBytes = min(avail, reqBytes);
        memcpy(buf, &buffer[offset], actualBytes);
        offset += actualBytes;
        status = ER_OK;
    } else if (reqBytes >= inMtu) {
        /* Pull directly into user buffer */
        status = SocketStream::PullBytes(buf, reqBytes, actualBytes, timeout);
    } else {
        /* Pull into internal buffer */
        status = SocketStream::PullBytes(buffer, inMtu, avail, timeout);
        if (status == ER_OK) {
            actualBytes = min(avail, reqBytes);
            /* Partial copy from internal buffer to user buffer */
            memcpy(buf, buffer, actualBytes);
            fill = avail;
            offset = actualBytes;
        }
    }
    return status;
}


QStatus BTSocketStream::PushBytes(const void* buf, size_t numBytes, size_t& numSent)
{
    QStatus status;
    do {
        status = SocketStream::PushBytes(buf, min(numBytes, outMtu), numSent);
        if ((status == ER_OS_ERROR) && (errno == ENOMEM)) {
            // BlueZ reports ENOMEM when it should report EBUSY or EAGAIN, just wait and try again.
            Sleep(10);
        }
    } while ((status == ER_OS_ERROR) && (errno == ENOMEM));

    return status;
}


} // namespace bluez
} // namespace ajn

