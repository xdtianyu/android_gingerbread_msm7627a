/**
 * @file
 *
 * This file implements methods from IPAddress.h.
 */

/******************************************************************************
 *
 *
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

#ifdef QCC_OS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <algorithm>
#include <string.h>

#include <qcc/Debug.h>
#include <qcc/IPAddress.h>
#include <qcc/SocketTypes.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <Status.h>

#define QCC_MODULE "NETWORK"

using namespace std;
using namespace qcc;

qcc::String IPAddress::IPv4ToString(const uint8_t addr[])
{
    qcc::String oss;
    size_t pos;

    pos = 0;
    oss.append(U32ToString(static_cast<uint32_t>(addr[pos]), 10));;
    for (++pos; pos < IPv4_SIZE; ++pos) {
        oss.push_back('.');
        oss.append(U32ToString(static_cast<uint32_t>(addr[pos]), 10));
    }

    return oss;
}



qcc::String IPAddress::IPv6ToString(const uint8_t addr[])
{
    qcc::String oss;
    size_t i;
    size_t j;
    int zerocnt;
    int maxzerocnt = 0;

    for (i = 0; i < IPv6_SIZE; i += 2) {
        if (addr[i] == 0 && addr[i + 1] == 0) {
            zerocnt = 0;
            for (j = IPv6_SIZE - 2; j > i; j -= 2) {
                if (addr[j] == 0 && addr[j + 1] == 0) {
                    ++zerocnt;
                    maxzerocnt = max(maxzerocnt, zerocnt);
                } else {
                    zerocnt = 0;
                }
            }
            // Count the zero we are pointing to.
            ++zerocnt;
            maxzerocnt = max(maxzerocnt, zerocnt);

            if (zerocnt == maxzerocnt) {
                oss.push_back(':');
                if (i == 0) {
                    oss.push_back(':');
                }
                i += (zerocnt - 1) * 2;
                continue;
            }
        }
        oss.append(U32ToString((uint32_t)(addr[i] << 8 | addr[i + 1]), 16));
        if (i + 2 < IPv6_SIZE) {
            oss.push_back(':');
        }
    }
    return oss;
}


IPAddress::IPAddress(const qcc::String& addrString)
{
    QStatus status = SetAddress(addrString);
    if (ER_OK != status) {
        QCC_LogError(status, ("Could not resolve \"%s\". Defaulting to INADDR_ANY"));
        SetAddress("");
    }
}

QStatus IPAddress::SetAddress(const qcc::String& addrString)
{
    QStatus status = ER_OK;
    uint8_t segmentIndex = 0;
    size_t start = 0;
    char delim = ':';
    size_t found = addrString.find_first_of(delim);

    addrSize = 0;
    memset(addr, 0, sizeof(addr));

    if (addrString.empty()) {
        // INADDR_ANY
        addrSize = IPv4_SIZE;
        addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 0] = 0xff;
        addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 1] = 0xff;
        uint32_t inaddr = INADDR_ANY;
        memcpy(&addr[IPv6_SIZE - IPv4_SIZE], &inaddr, sizeof(inaddr));
    } else if (found != addrString.npos) {
        // IPV6
        int delimCnt = 0;

        do {
            ++delimCnt;
            found = addrString.find_first_of(delim, found + 1);
        } while (found != addrString.npos);

        addrSize = IPv6_SIZE;

        // eight groups of 4 hex digits
        for (segmentIndex = 0; (segmentIndex < IPv6_SIZE); segmentIndex += 2) {
            found = addrString.find_first_of(delim, start);
            QCC_DbgPrintf(("segmentIndex = %u  start = %d  found = %d", segmentIndex, start, found));
            if ((found - start) == 0) {
                // Got a double colon, skip the zeros
                segmentIndex += 2 * (((IPv6_SIZE / 2) - delimCnt) - 1);

                if (start == 0) {
                    segmentIndex += 2;
                    ++found;
                }

                QCC_DbgPrintf(("segmentIndex = %u", segmentIndex));

            } else if (start != addrString.length()) {
                qcc::String iss = addrString.substr(start, found - start);
                uint16_t segment = (uint16_t) StringToU32(iss, 16, 0xdead);

                addr[segmentIndex] = (segment >> 8) & 0xff;
                addr[segmentIndex + 1] = segment & 0xff;
            }
            start = found + 1;
        }
    } else if ((addrString[0] >= '0') && (addrString[0] <= '9')) {
        // IPV4
        delim = '.';
        found = addrString.find_first_of(delim);
        if (found != addrString.npos) {
            addrSize = IPv4_SIZE;
            // Encode the IPv4 address in the IPv6 address space for easy
            // conversion.
            addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 0] = 0xff;
            addr[IPv6_SIZE - IPv4_SIZE - sizeof(uint16_t) + 1] = 0xff;
            for (segmentIndex = IPv6_SIZE - IPv4_SIZE; (segmentIndex < IPv6_SIZE); segmentIndex++) {
                // four groups of decimal digits
                qcc::String iss = addrString.substr(start, found - start);
                uint16_t segment;
                segment = (uint16_t) StringToU32(iss, 10);
                *reinterpret_cast<uint8_t*>(&addr[segmentIndex]) = segment;

                start = found + 1;
                found = addrString.find_first_of(delim, start);
            }
        }
    } else {
        // Hostname
        struct addrinfo* info;
        if (0 == getaddrinfo(addrString.c_str(), NULL, NULL, &info)) {
            if (info->ai_family == AF_INET6) {
                struct sockaddr_in6* sa = (struct sockaddr_in6*) info->ai_addr;
                memcpy(addr, &sa->sin6_addr, IPv6_SIZE);
            } else if (info->ai_family == AF_INET) {
                struct sockaddr_in* sa = (struct sockaddr_in*) info->ai_addr;
                memset(addr, 0xff, IPv6_SIZE);
                memcpy(addr, &sa->sin_addr, IPv4_SIZE);
            }
        } else {
            status = ER_BAD_HOSTNAME;
        }
    }
    return status;
}

