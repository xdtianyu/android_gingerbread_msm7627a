/**
 * @file
 * The lightweight name service implementation
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <algorithm>

#if defined(QCC_OS_WINDOWS)

#define close closesocket
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#else // defined (QCC_OS_ANDROID) or defined (QCC_OS_LINUX)

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#endif // defined(QCC_OS_WINDOWS)

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/Event.h>
#include <qcc/Socket.h>
#include <qcc/SocketTypes.h>
#include <qcc/time.h>

#include "NsProtocol.h"
#include "NameService.h"

#define QCC_MODULE "NS"

using namespace std;

namespace ajn {

//
// There are configurable attributes of the name servcie which are determined
// by the configuration database.  A module name is required and is defined
// here.  An example of how to use this is in setting the interfaces the name
// service will use for discovery.
//
//   <busconfig>
//     <alljoyn module="ipns">
//       <property interfaces="*"/>
//     </alljoyn>
//   </busconfig>
//
const char* NameService::MODULE_NAME = "ipns";

//
// The name of the property used to configure the interfaces over which the
// name service should run discovery.
//
const char* NameService::INTERFACES_PROPERTY = "interfaces";

//
// The value of the interfaces property used to configure the name service
// to run discovery over all interfaces in the system.
//
const char* NameService::INTERFACES_WILDCARD = "*";

//
// This is just a random multicast group chosen out of an unreserved block of
// addresses and a port next to the AllJoyn daemon port.  In the future we
// will contact IANA an reserve a multicast group and port; but for now we
// appropriate an IPv4 Local Scope group as defined in RFC 2365.
//
const char* NameService::IPV4_MULTICAST_GROUP = "239.255.37.41";
const uint16_t NameService::MULTICAST_PORT = 9956;

//
// IPv6 multicast groups are composed of a prefix containing 0xff and then
// flags (4 bits) followed by the IPv6 Scope (4 bits) and finally the IPv4
// group, as in "ff03::239.255.37.41".  The Scope corresponding to the IPv4
// Local Scope group is defined to be "3" by RFC 2365.  Unfortunately, the
// qcc::IPAddress code can't deal with "ff03::239.255.37.41" so we have to
// translate it.
//
const char* NameService::IPV6_MULTICAST_GROUP = "ff03::efff:2529";

//
// Simple pattern matching function that supports '*' and '?' only.  Returns a
// bool in the sense of "a difference between the string and pattern exists."
// This is so it works like fnmatch or strcmp, which return a 0 if a match is
// found.
//
// We require an actual character match and do not consider an empty string
// something that can match or be matched.
//
bool WildcardMatch(qcc::String str, qcc::String pat)
{
    size_t patsize = pat.size();
    size_t strsize = str.size();
    const char* p = pat.c_str();
    const char* s = str.c_str();
    uint32_t pi, si;

    //
    // Zero length strings are unmatchable.
    //
    if (patsize == 0 || strsize == 0) {
        return true;
    }

    for (pi = 0, si = 0; pi < patsize && si < strsize; ++pi, ++si) {
        switch (p[pi]) {
        case '*':
            //
            // Point to the character after the wildcard.
            //
            ++pi;

            //
            // If the wildcard is at the end of the pattern, we match
            //
            if (pi == patsize) {
                return false;
            }

            //
            // If the next character is another wildcard, we could go through
            // a bunch of special case work to figure it all out, but in the
            // spirit of simplicity we don't deal with it and return "different".
            //
            if (p[pi] == '*' || p[pi] == '?') {
                return true;
            }

            //
            // Scan forward in the string looking for the character after the
            // wildcard.
            //
            for (; si < strsize; ++si) {
                if (s[si] == p[pi]) {
                    break;
                }
            }
            break;

        case '?':
            //
            // A question mark matches any character in the string.
            //
            break;

        default:
            //
            // If no wildcard, we just compare character for character.
            //
            if (p[pi] != s[si]) {
                return true;
            }
            break;
        }
    }

    //
    // If we fall through to here, we have matched all the way through one or
    // both of the strings.  If pi == patsize and si == strsize then we matched
    // all the way to the end of both strings and we have a match.
    //
    if (pi == patsize && si == strsize) {
        return false;
    }

    //
    // If pi < patsize and si == strsize there are characters in the pattern
    // that haven't been matched.  The only way this can be a match is if
    // that last character is a '*' meaning zero or more characters match.
    //
    if (pi < patsize && si == strsize) {
        if (p[pi] == '*') {
            return false;
        } else {
            return true;
        }
    }

    //
    // The remaining chase is pi == patsize and si < strsize which means
    // that we've got characters in the string that haven't been matched
    // by the pattern.  There's no way this can be a match.
    //
    return true;
}

NameService::NameService()
    : Thread("NameService"), m_state(IMPL_SHUTDOWN), m_callback(0),
    m_port(0), m_timer(0), m_tDuration(DEFAULT_DURATION),
    m_tRetransmit(RETRANSMIT_TIME), m_tQuestion(QUESTION_TIME),
    m_modulus(QUESTION_MODULUS), m_retries(NUMBER_RETRIES),
    m_loopback(false), m_enableIPv4(false), m_enableIPv6(false),
    m_any(false), m_wakeEvent(), m_forceLazyUpdate(false)
{
    QCC_DbgPrintf(("NameService::NameService()\n"));
}

//
// We need to get a list of interfaces and IP addresses on the system.  This is
// useful internally for our lazy update interfaces code and to the user who
// might want to determine what interfaces are up and have IP addresses assigned.
//
// Implicit in the last statement is that we need to be able to find interfaces
// that are not up.  We also need to be able to deal with multiple IP addresses
// assigned to interfaces, and also with IPv4 and IPv6 at the same time.
//
// There are a bewildering number of ways to get information about network
// devices in Linux.  Unfortunately, most of them only give us access to pieces
// of the information we need.  For example, the ioctl SIOCGIFCONF belongs to
// the IP layer and only returns information about IFF_UP interfaces that have
// assigned IP address.  Furthermore, it only returns information about IPv4
// addresses.  Since we need IPv6 addresses, and also interfaces that may not
// have a currently assigned IP address, this won't work.  The function
// getifaddrs() works on Linux, but isn't available on Android since the header
// is private and excludes information we need.  User-space tools are even
// different on different platforms, with ifconfig returning both IPv4 and IPv6
// addresses on Linux but only IPv4 addresses on Android.  Windows is from
// planet.
//
// One command that pretty much works the same on Android and generic Linux
// is "ip ad sh" which gives us exactly what we want.  This is from the iproute2
// package which uses netlink sockets, which is the new super-whoopty-wow
// control plane for networks in Linux-based systems.  We use netlink sockets
// to get what we want in Linux and Android.  Of course, as you may expect,
// libnetlink is not available on Android so we get to use low-level netlink
// sockets.  This is a bit of a pain, but is manageable since we really need
// so little information.
//
// Since we are really ultimately interested in opening separate sockets and
// doing separate IGMP joins on each interface/address combination as they
// become available, we organize the output that way instead of the more OS-like
// way of an interface with an associated list of addresses.
//

#if defined(QCC_OS_LINUX) || defined(QCC_OS_ANDROID)

static int NetlinkRouteSocket(uint32_t bufsize)
{
    int sockFd;

    if ((sockFd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
        QCC_LogError(ER_FAIL, ("NetlinkRouteSocket: Error obtaining socket: %s", strerror(errno)));
        return -1;
    }

    if (setsockopt(sockFd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        QCC_LogError(ER_FAIL, ("NetlinkRouteSocket: Can't setsockopt SO_SNDBUF: %s", strerror(errno)));
        return -1;
    }

    if (setsockopt(sockFd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        QCC_LogError(ER_FAIL, ("NetlinkRouteSocket: Can't setsockopt SO_RCVBUF: %s", strerror(errno)));
        return -1;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();

    if (bind(sockFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        QCC_LogError(ER_FAIL, ("NetlinkRouteSocket: Can't bind to NETLINK_ROUTE socket: %s", strerror(errno)));
        return -1;
    }

    return sockFd;
}

static void NetlinkSend(int sockFd, int seq, int type, int family)
{
    //
    // Header with general form of address family-dependent message
    //
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg g;
    } request;

    memset(&request, 0, sizeof(request));
    request.nlh.nlmsg_len = sizeof(request);
    request.nlh.nlmsg_type = type;

    //
    // NLM_F_REQUEST ... Indicates a request
    // NLM_F_ROOT ...... Return the entire table, not just a single entry.
    // NLM_F_MATCH ..... Return all entries matching criteria.
    //
    request.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    request.nlh.nlmsg_pid = getpid();
    request.nlh.nlmsg_seq = seq;

    request.g.rtgen_family = family;

    send(sockFd, (void*)&request, sizeof(request), 0);
}

static uint32_t NetlinkRecv(int sockFd, char* buffer, int buflen)
{
    uint32_t nBytes = 0;

    while (1) {
        int tmp = recv(sockFd, &buffer[nBytes], buflen - nBytes, 0);

        if (tmp < 0) {
            return nBytes;
        }

        if (tmp == 0) {
            return nBytes;
        }

        struct nlmsghdr* p = (struct nlmsghdr*)&buffer[nBytes];

        if (p->nlmsg_type == NLMSG_DONE) {
            break;
        }

        nBytes += tmp;
    }

    return nBytes;
}

class IfEntry {
  public:
    uint32_t m_index;
    qcc::String m_name;
    uint32_t m_mtu;
    uint32_t m_flags;
};

//
// Get a list of all interfaces (links) on the system irrespective of whether
// or not they are up or down or what kind of address they may have assigned.
// We can use the m_index field to do a "join" on the address list from below.
//
static std::list<IfEntry> NetlinkGetInterfaces(void)
{
    std::list<IfEntry> entries;

    //
    // Like many interfaces that do similar things, there's no clean way to
    // figure out beforehand how big of a buffer we are going to eventually
    // need.  Typically user code just picks buffers that are "big enough."
    // for example, iproute2 just picks a 16K buffer and fails if its not
    // "big enough."  Experimentally, we see that an 8K buffer holds 19
    // interfaces (because we have overflowed that for some devices).  Instead
    // of trying to dynamically resize the buffer as we go along, we do what
    // everyone else does and just allocate a  "big enough' buffer.  We choose
    // 64K which should handle about 150 interfaces.
    //
    const uint32_t BUFSIZE = 65536;
    char* buffer = new char[BUFSIZE];
    uint32_t len;

    //
    // If we can't get a NETLINK_ROUTE socket, treat it as a transient error
    // since we'll periodically retry looking for interfaces that come up and
    // go down.  The worst thing that will happen is we may not send
    // advertisements during the transient time; but this is guaranteed by
    // construction to be less than the advertisement timeout.
    //
    int sockFd = NetlinkRouteSocket(BUFSIZE);
    if (sockFd < 0) {
        return entries;
    }

    NetlinkSend(sockFd, 0, RTM_GETLINK, 0);
    len = NetlinkRecv(sockFd, buffer, BUFSIZE);

    for (struct nlmsghdr* nh = (struct nlmsghdr*)buffer; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
        switch (nh->nlmsg_type) {
        case NLMSG_DONE:
        case NLMSG_ERROR:
            break;

        case RTM_NEWLINK:
        {
            IfEntry entry;

            struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(nh);
            entry.m_index = ifi->ifi_index;
            entry.m_flags = ifi->ifi_flags;

            struct rtattr* rta = IFLA_RTA(ifi);
            uint32_t rtalen = IFLA_PAYLOAD(nh);

            for (; RTA_OK(rta, rtalen); rta = RTA_NEXT(rta, rtalen)) {
                switch (rta->rta_type) {
                case IFLA_IFNAME:
                    entry.m_name = qcc::String((char*)RTA_DATA(rta));
                    break;

                case IFLA_MTU:
                    entry.m_mtu = *(int*)RTA_DATA(rta);
                    break;
                }
            }
            entries.push_back(entry);
            break;
        }
        }
    }

    delete [] buffer;
    close(sockFd);

    return entries;
}

class AddrEntry {
  public:
    uint32_t m_family;
    uint32_t m_prefixlen;
    uint32_t m_flags;
    uint32_t m_scope;
    uint32_t m_index;
    qcc::String m_addr;
};

//
// Get a list of all IP addresses of a given family.  We can use the m_index
// field to do a "join" on the interface list from above.
//
static std::list<AddrEntry> NetlinkGetAddresses(uint32_t family)
{
    std::list<AddrEntry> entries;

    //
    // Like many interfaces that do similar things, there's no clean way to
    // figure out beforehand how big of a buffer we are going to eventually
    // need.  Typically user code just picks buffers that are "big enough."
    // for example, iproute2 just picks a 16K buffer and fails if its not
    // "big enough."  Experimentally, we see that an 8K buffer holds 19
    // interfaces (because we have overflowed that for some devices).  Instead
    // of trying to dynamically resize the buffer as we go along, we do what
    // everyone else does and just allocate a  "big enough' buffer.  We choose
    // 64K which should handle about 150 interfaces.
    //
    const uint32_t BUFSIZE = 65536;
    char* buffer = new char[BUFSIZE];
    uint32_t len;

    int sockFd = NetlinkRouteSocket(BUFSIZE);

    NetlinkSend(sockFd, 0, RTM_GETADDR, family);
    len = NetlinkRecv(sockFd, buffer, BUFSIZE);

    for (struct nlmsghdr* nh = (struct nlmsghdr*)buffer; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
        switch (nh->nlmsg_type) {
        case NLMSG_DONE:
        case NLMSG_ERROR:
            break;

        case RTM_NEWADDR:
        {
            AddrEntry entry;

            struct ifaddrmsg* ifa = (struct ifaddrmsg*)NLMSG_DATA(nh);

            entry.m_family = ifa->ifa_family;
            entry.m_prefixlen = ifa->ifa_prefixlen;
            entry.m_flags = ifa->ifa_flags;
            entry.m_scope = ifa->ifa_scope;
            entry.m_index = ifa->ifa_index;

            struct rtattr* rta = IFA_RTA(ifa);
            uint32_t rtalen = IFA_PAYLOAD(nh);

            for (; RTA_OK(rta, rtalen); rta = RTA_NEXT(rta, rtalen)) {
                switch (rta->rta_type) {
                case IFA_ADDRESS:
                    if (ifa->ifa_family == AF_INET) {
                        struct in_addr* p = (struct in_addr*)RTA_DATA(rta);
                        //
                        // Android seems to stash INADDR_ANY in as an
                        // IFA_ADDRESS in the case of an AF_INET address
                        // for some reason, so we ignore those.
                        //
                        if (p->s_addr != 0) {
                            char buffer[17];
                            inet_ntop(AF_INET, p, buffer, sizeof(buffer));
                            entry.m_addr = qcc::String(buffer);
                        }
                    }
                    if (ifa->ifa_family == AF_INET6) {
                        struct in6_addr* p = (struct in6_addr*)RTA_DATA(rta);
                        char buffer[41];
                        inet_ntop(AF_INET6, p, buffer, sizeof(buffer));
                        entry.m_addr = qcc::String(buffer);
                    }
                    break;

                default:
                    break;
                }
            }
            entries.push_back(entry);
        }
        break;
        }
    }

    delete [] buffer;
    close(sockFd);

    return entries;
}

static uint32_t TranslateFlags(uint32_t flags)
{
    uint32_t ourFlags = 0;
    if (flags & IFF_UP) ourFlags |= NameService::IfConfigEntry::UP;
    if (flags & IFF_BROADCAST) ourFlags |= NameService::IfConfigEntry::BROADCAST;
    if (flags & IFF_DEBUG) ourFlags |= NameService::IfConfigEntry::DEBUG;
    if (flags & IFF_LOOPBACK) ourFlags |= NameService::IfConfigEntry::LOOPBACK;
    if (flags & IFF_POINTOPOINT) ourFlags |= NameService::IfConfigEntry::POINTOPOINT;
    if (flags & IFF_RUNNING) ourFlags |= NameService::IfConfigEntry::RUNNING;
    if (flags & IFF_NOARP) ourFlags |= NameService::IfConfigEntry::NOARP;
    if (flags & IFF_PROMISC) ourFlags |= NameService::IfConfigEntry::PROMISC;
    if (flags & IFF_NOTRAILERS) ourFlags |= NameService::IfConfigEntry::NOTRAILERS;
    if (flags & IFF_ALLMULTI) ourFlags |= NameService::IfConfigEntry::ALLMULTI;
    if (flags & IFF_MASTER) ourFlags |= NameService::IfConfigEntry::MASTER;
    if (flags & IFF_SLAVE) ourFlags |= NameService::IfConfigEntry::SLAVE;
    if (flags & IFF_MULTICAST) ourFlags |= NameService::IfConfigEntry::MULTICAST;
    if (flags & IFF_PORTSEL) ourFlags |= NameService::IfConfigEntry::PORTSEL;
    if (flags & IFF_AUTOMEDIA) ourFlags |= NameService::IfConfigEntry::AUTOMEDIA;
    if (flags & IFF_DYNAMIC) ourFlags |= NameService::IfConfigEntry::DYNAMIC;
    return ourFlags;
}


QStatus NameService::IfConfig(std::vector<IfConfigEntry>& entries)
{
    QCC_DbgPrintf(("NameService::IfConfig(): The Linux way\n"));

    std::list<IfEntry> ifEntries = NetlinkGetInterfaces();
    std::list<AddrEntry> entriesIpv4 = NetlinkGetAddresses(AF_INET);
    std::list<AddrEntry> entriesIpv6 = NetlinkGetAddresses(AF_INET6);

    for (std::list<IfEntry>::const_iterator i = ifEntries.begin(); i != ifEntries.end(); ++i) {
        uint32_t nAddresses = 0;

        //
        // Are there any IPV4 addresses assigned to the currently described interface
        //
        for (std::list<AddrEntry>::const_iterator j = entriesIpv4.begin(); j != entriesIpv4.end(); ++j) {
            if ((*j).m_index == (*i).m_index) {
                IfConfigEntry entry;
                entry.m_name = (*i).m_name.c_str();
                entry.m_flags = TranslateFlags((*i).m_flags);
                entry.m_mtu = (*i).m_mtu;
                entry.m_index = (*i).m_index;

                entry.m_addr = (*j).m_addr.c_str();
                entry.m_family = (*j).m_family;

                entries.push_back(entry);
                ++nAddresses;
            }
        }

        //
        // Are there any IPV6 addresses assigned to the currently described interface
        //
        for (std::list<AddrEntry>::const_iterator j = entriesIpv6.begin(); j != entriesIpv6.end(); ++j) {
            if ((*j).m_index == (*i).m_index) {
                IfConfigEntry entry;
                entry.m_name = (*i).m_name.c_str();
                entry.m_flags = TranslateFlags((*i).m_flags);
                entry.m_mtu = (*i).m_mtu;
                entry.m_index = (*i).m_index;

                entry.m_addr = (*j).m_addr.c_str();
                entry.m_family = (*j).m_family;

                entries.push_back(entry);
                ++nAddresses;
            }
        }

        //
        // Even if there are no addresses on the interface instantaneously, we
        // need to return it since there may be an address on it in the future.
        // The flags should reflect the fact that there are no addresses
        // assigned.
        //
        if (nAddresses == 0) {
            IfConfigEntry entry;
            entry.m_name = (*i).m_name.c_str();
            entry.m_flags = (*i).m_flags;
            entry.m_mtu = (*i).m_mtu;
            entry.m_index = (*i).m_index;

            entry.m_addr = qcc::String();
            entry.m_family = AF_UNSPEC;

            entries.push_back(entry);
        }
    }

    return ER_OK;
}

#elif QCC_OS_WINDOWS

std::vector<qcc::String> GetInterfaces(void)
{
    vector<qcc::String> ifs;

    //
    // Multiple calls are a legacy of sockets stupidity since BSD 4.2
    //
    IP_ADAPTER_INFO info, * parray = 0, * pinfo = 0;
    ULONG infoLen = sizeof(info);

    //
    // Call into Windows and it will tell us how much memory it needs, if
    // more than we provide.
    //
    GetAdaptersInfo(&info, &infoLen);

    //
    // Allocate enough memory to hold the adapter information array.
    //
    parray = pinfo = reinterpret_cast<IP_ADAPTER_INFO*>(new uint8_t[infoLen]);

    //
    // Now, get the interesting information about the net devices
    //
    if (GetAdaptersInfo(pinfo, &infoLen) == NO_ERROR) {
        while (pinfo) {
            ifs.push_back(qcc::String(pinfo->AdapterName));
            pinfo = pinfo->Next;
        }
    }

    delete [] parray;

    return ifs;
}

QStatus NameService::IfConfig(std::vector<IfConfigEntry>& entries)
{
    QCC_DbgPrintf(("NameService::IfConfig(): The Windows way\n"));

    vector<qcc::String> ifs = GetInterfaces();

    //
    // Windows is from another planet, but we can't blame multiple calls to get
    // interface info on them.  It's a legacy of *nix sockets since BSD 4.2
    //
    IP_ADAPTER_ADDRESSES info, * parray = 0, * pinfo = 0;
    ULONG infoLen = sizeof(info);

    //
    // Call into Windows and it will tell us how much memory it needs, if
    // more than we provide.
    //
    GetAdaptersAddresses(AF_INET, 0, 0, &info, &infoLen);

    //
    // Allocate enough memory to hold the adapter information array.
    //
    parray = pinfo = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(new uint8_t[infoLen]);

    //
    // Now, get the interesting information about the net devices
    //
    if (GetAdaptersAddresses(AF_INET, 0, 0, pinfo, &infoLen) == NO_ERROR) {
        while (pinfo) {
            IfConfigEntry entry;

            entry.m_name = qcc::String(pinfo->AdapterName);

            char buffer[NI_MAXHOST];

            if (pinfo->FirstUnicastAddress) {
                getnameinfo(pinfo->FirstUnicastAddress->Address.lpSockaddr,
                            pinfo->FirstUnicastAddress->Address.iSockaddrLength,
                            buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST);
                entry.m_addr = buffer;
            } else {
                entry.m_addr = "";
            }

            entry.m_flags = pinfo->OperStatus == IfOperStatusUp ? IfConfigEntry::UP : 0;
            entry.m_flags |= pinfo->Flags & IP_ADAPTER_NO_MULTICAST ? 0 : IfConfigEntry::MULTICAST;
            entry.m_flags |= pinfo->IfType == IF_TYPE_SOFTWARE_LOOPBACK ? IfConfigEntry::LOOPBACK : 0;

            entry.m_family = AF_INET;
            entry.m_mtu = pinfo->Mtu;

            entries.push_back(entry);

            pinfo = pinfo->Next;
        }
    }

    delete [] parray;

    return ER_OK;
}

#else
#error No known OS specified
#endif

QStatus NameService::Init(const qcc::String& guid, bool enableIPv4, bool enableIPv6, bool loopback)
{
    QCC_DbgHLPrintf(("NameService::Init()\n"));

    //
    // Can only call Init() if the object is not running or in the process
    // of initializing
    //
    if (m_state != IMPL_SHUTDOWN) {
        return ER_FAIL;
    }

    m_state = IMPL_INITIALIZING;

    m_guid = guid;
    m_enableIPv4 = enableIPv4;
    m_enableIPv6 = enableIPv6;
    m_loopback = loopback;

    assert(IsRunning() == false);
    Start(this);

    m_state = IMPL_RUNNING;
    return ER_OK;
}

NameService::~NameService()
{
    QCC_DbgPrintf(("NameService::~NameService()\n"));

    //
    // Stop the worker thread to get things calmed down.
    //
    if (IsRunning()) {
        Stop();
        Join();
    }

    //
    // We may have some open sockets.  We aren't multithreaded any more since
    // the worker thread has stopped.
    //
    ClearLiveInterfaces();

    //
    // We can just blow away the requested interfaces without a care.
    //
    m_requestedInterfaces.clear();

    //
    // Delete any callbacks that a user of this class may have set.
    //

    delete m_callback;
    m_callback = 0;

    //
    // All shut down and ready for bed.
    //
    m_state = IMPL_SHUTDOWN;
}

//
// Long Sidebar:
//
// In order to understand all of the trouble we are going to go through below,
// it is helpful to thoroughly understand what is done on our platforms in the
// presence of multicast.  This is long reading, but worthwhile reading if you
// are trying to understand what is going on.  I don't know of anywhere you
// can find all of this written in one place.
//
// The first thing to grok is that all platforms are implemented differently.
// Windows and Linux use IGMP to enable and disable multicast, and other
// multicast-related socket calls to do the fine-grained control.  Android
// doesn't bother to compile its kernel with CONFIG_IP_MULTICAST set.  This
// doesn't mean that there is no multicast code in the Android kernel, it means
// there is no IGMP code in the kernel.  Since IGMP isn't implemented, Android
// can't use it to enable and disable multicast, so it uses wpa_supplicant
// driver-private commands instead.  This means that you will probably get
// three different answers if you ask how some piece of the multicast puzzle
// works.
//
// On the send side, multicast is controlled by the IP_MULTICAST_IF (or for
// IPv6 IPV6_MULTICAST_IF) socket.  In IPv4 you provide an IP address and in
// IPv6 you provide an interface index.  If you do nothing, or set the
// interface address to 0.0.0.0 for IPv4 or the interface index to 0 for IPv6
// the multicast output interface is essentially selected by the system routing
// code.  In Linux (and Android), multicast packets are sent out the interface
// that is used for the default route (the default interface).  You can see this
// if you type "ip ro sh".  In Windows, the system chooses its default interface
// by looking for the lowest value for the routing metric for a destination IP
// address of 224.0.0.0 in its routing table.  You can see this in the output
// of "route print".
//
// We want all of our multicast code to work in the presence of IP addresses
// changing when phones move from one Wifi access point to another, or when
// our desktop access point changes when someone with a mobile access point
// walks by; so it is important to know what will happen when these addresses
// change.
//
// On Linux, if you set the IP_MULTICAST_IF to 0.0.0.0 (or index 0 in IPv6)
// and bring down the default interface or change the IP address on the
// default interface, you will begin to fail the multicast sends with "network
// unreachable" errors since the default route goes away when you change the
// IP address (e.g, "sudo ifconfig eth1 10.4.108.237 netmask 255.255.255.0 upâ€?).
// As soon as you provide a new default route (e.g., "route add default gw
// 10.4.108.1") the multicast packets will start flowing again.
//
// In Windows, if you set the IP_MULTICAST_IF address to 0.0.0.0 and release
// the ip address (e.g., "ipconfig /release") the sends may still appear to
// work but nothing goes out the original interface.  This is because Windows
// will dynamically change the default multicast route according to its internal
// multicast routing table.  It selects another interface based on a routing
// metric, and it could, for example, just switch to a VMware virtual interface
// silently.  The name service would never know it just "broke."
//
// When we set up multicast advertisements in our system, we most likely do not
// want to route our advertisements only to the default adapter.  For example,
// on a desktop system, the default interface is probably one of the wired
// Ethernets.  We most likely want to advertise on that interface, but we may
// also want to advertise on other wired interfaces and any wireless interfaces
// as well.  We do not want the system to start changing multicast destinations
// out from under us ever.  Because of this, the only time using INADDR_ANY
// would be appropriate in the IP_MULTICAST_IF socket option is in the simplest,
// static network situations.  For the general case, we really need to keep
// multiple sockets that are each talking to an interface of interest (not IP
// address, since they can change).
//
// So we need to provide API that allows a user to specify a network interface
// over which she is interested in advertising.  This explains the method
// OpenInterface(qcc::String interface) defined below.
//
// Once we have determined that we need to use IP_MULTICAST_IF, we needed to
// understand exactly what changing the IP address out from under that call
// would do.  The first thing to observe is that IP_MULTICAST_IF takes an IP
// address in the case of IPv4, but we want to specify an interface index as
// in IPv6 or for human beings, a name (e.g., "wlan0").  It may be the case that
// the interface does not have an IP address assigned (is not up or connected to
// an access point) at the time the call to OpenInterface is made, so a call to
// setsockopt is not possible until an address is available.  If the IP address
// is not valid you will see a "network unreachable" error.  So, we need to defer
// this action until we need to do it; and we use lazy evaluation on the IP
// address and only store the interface name in OpenInterface.
//
// If we try to rely on sockets to notify us whenever an error happens and then
// try to re-open the socket using the correct interface name, as we metioned
// above, we could miss the fact that our routing has changed.  The safest thing
// to do would be to check for IP address changes and re-evaluate and open new
// sockets bound to the correct interfaces whenever we want to send a multicast
// packet.  It is conceivable that we could miss the fact that we have switched
// access points if DHCP gives us the same IP (using either the error return or
// address check methods), but that shouldn't actually matter.
//
// The receive side has similar issues.
//
// In order to receive multicast datagrams sent to a particular port, it is
// necessary to bind that local port leaving the local address unspecified
// (i.e., INADDR_ANY or in6addr_any).  What you might think of as binding is
// then actually handled by the Internet Group Management Protocol (IGMP).
// Recall that Android does not implement IGMP, so we have yet another small
// complication.
//
// Using IGMP, we join the socket to the multicast group instead of binding
// the socket to a specific interface (address) and port.  Binding the socket
// to INADDR_ANY or in6addr_any may look strange, but it is actually the right
// thing to do.
//
// The socket option for joining a multicast group, of course, works differently
// for IPv4 and IPv6.  IP_ADD_MEMBERSHIP (for IPv4) has a provided IP address
// that can be either INADDR_ANY or a specific address.  If INADDR_ANY is
// provided, the interface of the default route is added to the group, and the
// IGMP join is sent out that interface.  IPV6_ADD_MEMBERSHIP (for IPv6) has a
// provided interface index that can be either 0 or a specific interface.  If 0
// is provided, the interface of the default route is added to the group, and the
// IGMP join is sent out that interface.  If a specific interface index is
// that interface is added to the group and the IGMP join is sent out that
// interface.
//
// A side effect of the IGMP join deep down in the kernel is to enable reception
// of multicast MAC addresses in the device driver.  Since there is no IGMP in
// Android, we must rely on a multicast (Java) lock being taken by some external
// code on phones that do not leave multicast always enabled (HTC Desire, for
// example).  When the multicast lock is taken, a private driver command is sent
// to the wpa_supplicant which, in turn, calls into the driver to enable reception
// of multicast MAC packets.
//
// Similar to the situation on the send side, we most likely do not want to rely
// on the system routing tables to configure our name service.  So we really need
// to provide a specific address.
//
// If a specific IP address is provided, then that address must be an
// address assigned to a currently-UP interface.  This is the same catch-22
// as we have on the send side.  We need to lazily evaluate the interface
// when we "need" to, in order to find if an IP address has appeared on
// that interface and then join the multicast group to enable multicast on
// the underlying device.
//
// We can either hook "IP address changed" events to drive the re-evaluation
// process or we can poll for those changes.  We have a maintenance thread
// that fires off every second, so it can look for these changes.  It seems
// that "IP address changed" events are communicated by the DBus-based network
// manager in Linux, other Windows-specific events, or an unknown mechanism in
// Android.  The better part of valor seems to be to poll in the maintenance
// thread.
//
// It turns out that in Linux, the IP address passed to the join socket option
// call is actually not significant after the initial call.  It is used to look
// up an interface and its associated net device and to then set the
// PACKET_MULTICAST filter on the net device to receive packets destined for
// the specified multicast address.  If the IP address associated with the
// interface changes, multicast messages will continue to be received.
//
// Of course, Windows does it differently.  They look at the IP address passed
// to the socket option as being significant, and so if the underlying IP address
// changes on a Windows system, multicast packets will no longer be delivered.
// Because of this, the receive side of the multicast name service has also
// got to look for changes to IP address configuration and set itself up
// whenever it finds a change.
//
// So it may look overly complicated, but we provide an OpenInterface call that
// takes an interface name.  This just remembers that name.  The maintenance
// thread wakes up every second and checks to make sure we are ready to listen
// to and talk over the correct interfaces (as specified by the calls to
// OpenInterface) by closing and opening sockets.  When an advertise or cancel
// is called, we also check to make sure we are ready to talk over the correct
// interfaces before sending packets.
//
// As an aside, the daemon that owns us can be happy as a clam by simply binding
// to INADDR_ANY since the semantics of this, as interpreted by both Windows and
// Linux, are to listen for connections on all current and future interfaces and
// their IP addresses.
//

QStatus NameService::OpenInterface(const qcc::String& name)
{
    QCC_DbgPrintf(("NameService::OpenInterface(%s)\n", name.c_str()));

    //
    // Can only call OpenInterface() if the object is running.
    //
    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::OpenInterface(): Not running\n"));
        return ER_FAIL;
    }

    //
    // If the user specifies the wildcard interface name, this trumps everything
    // else.
    //
    if (name == INTERFACES_WILDCARD) {
        qcc::IPAddress wildcard("0.0.0.0");
        return OpenInterface(wildcard);
    }
    //

    // There are at least two threads that can wander through the vector below
    // so we need to protect access to the list with a convenient mutex.
    //
    m_mutex.Lock();

    for (uint32_t i = 0; i < m_requestedInterfaces.size(); ++i) {
        if (m_requestedInterfaces[i].m_interfaceName == name) {
            QCC_DbgPrintf(("NameService::OpenInterface(): Already opened.\n"));
            m_mutex.Unlock();
            return ER_OK;
        }
    }

    InterfaceSpecifier specifier;
    specifier.m_interfaceName = name;
    specifier.m_interfaceAddr = qcc::IPAddress("0.0.0.0");

    m_requestedInterfaces.push_back(specifier);
    m_forceLazyUpdate = true;
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();
    return ER_OK;
}

QStatus NameService::OpenInterface(const qcc::IPAddress& addr)
{
    QCC_DbgPrintf(("NameService::OpenInterface(%s)\n", addr.ToString().c_str()));

    //
    // Can only call OpenInterface() if the object is running.
    //
    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::OpenInterface(): Not running\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads that can wander through the vector below
    // so we need to protect access to the list with a convenient mutex.
    //
    m_mutex.Lock();

    //
    // We treat the INADDR_ANY address (and the equivalent IPv6 address as a
    // wildcard.  To have the same semantics as using INADDR_ANY in the TCP
    // transport listen spec, and avoid resulting user confusion, we need to
    // interpret this as "use any interfaces that are currently up, or may come
    // up in the future to send and receive name service messages over."  This
    // trumps anything else the user might throw at us.  We set a global flag to
    // indicate this mode of operation and clear it if we see a CloseInterface()
    // on INADDR_ANY.  These calls are not reference counted.
    //
    if (addr == qcc::IPAddress("0.0.0.0") ||
        addr == qcc::IPAddress("0::0") ||
        addr == qcc::IPAddress("::")) {
        QCC_DbgPrintf(("NameService::OpenInterface(): Wildcard address\n"));
        m_any = true;
        m_mutex.Unlock();
        return ER_OK;
    }

    for (uint32_t i = 0; i < m_requestedInterfaces.size(); ++i) {
        if (m_requestedInterfaces[i].m_interfaceAddr == addr) {
            QCC_DbgPrintf(("NameService::OpenInterface(): Already opened.\n"));
            m_mutex.Unlock();
            return ER_OK;
        }
    }

    InterfaceSpecifier specifier;
    specifier.m_interfaceName = "";
    specifier.m_interfaceAddr = addr;

    m_requestedInterfaces.push_back(specifier);
    m_forceLazyUpdate = true;
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();
    return ER_OK;
}

QStatus NameService::CloseInterface(const qcc::String& name)
{
    QCC_DbgPrintf(("NameService::CloseInterface(%s)\n", name.c_str()));

    //
    // Can only call CloseInterface() if the object is running.
    //
    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::CloseInterface(): Not running\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads that can wander through the vector below
    // so we need to protect access to the list with a convenient mutex.
    //
    m_mutex.Lock();

    //
    // use Meyers' idiom to keep iterators sane.  Note that we don't close the
    // socket in this call, we just remove the request and the lazy updator will
    // just not use it when it re-evaluates what to do (called immediately below).
    //
    for (vector<InterfaceSpecifier>::iterator i = m_requestedInterfaces.begin(); i != m_requestedInterfaces.end();) {
        if ((*i).m_interfaceName == name) {
            m_requestedInterfaces.erase(i++);
        } else {
            ++i;
        }
    }

    m_forceLazyUpdate = true;
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();
    return ER_OK;
}

QStatus NameService::CloseInterface(const qcc::IPAddress& addr)
{
    QCC_DbgPrintf(("NameService::CloseInterface(%s)\n", addr.ToString().c_str()));

    //
    // Can only call CloseInterface() if the object is running.
    //
    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::CloseInterface(): Not running\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads that can wander through the vector below
    // so we need to protect access to the list with a convenient mutex.
    //
    m_mutex.Lock();

    //
    // We treat the INADDR_ANY address (and the equivalent IPv6 address as a
    // wildcard.  We set a global flag in OpenInterface() to indicate this mode
    // of operation and clear it here.  These calls are not reference counted
    // so one call to CloseInterface(INADDR_ANY) will stop this mode
    // irrespective of how many opens are done.
    //
    if (addr == qcc::IPAddress("0.0.0.0") ||
        addr == qcc::IPAddress("0::0") ||
        addr == qcc::IPAddress("::")) {
        QCC_DbgPrintf(("NameService::CloseInterface(): Wildcard address\n"));
        m_any = false;
        m_mutex.Unlock();
        return ER_OK;
    }

    //
    // use Meyers' idiom to keep iterators sane.  Note that we don't close the
    // socket in this call, we just remove the request and the lazy updator will
    // just not use it when it re-evaluates what to do (called immediately below).
    //
    for (vector<InterfaceSpecifier>::iterator i = m_requestedInterfaces.begin(); i != m_requestedInterfaces.end();) {
        if ((*i).m_interfaceAddr == addr) {
            m_requestedInterfaces.erase(i++);
        } else {
            ++i;
        }
    }

    m_forceLazyUpdate = true;
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();
    return ER_OK;
}

void NameService::ClearLiveInterfaces(void)
{
    QCC_DbgPrintf(("NameService::ClearLiveInterfaces()\n"));

    for (uint32_t i = 0; i < m_liveInterfaces.size(); ++i) {
        if (m_liveInterfaces[i].m_sockFd == -1) {
            continue;
        }

        //
        // Arrange an IGMP drop via the appropriate setsockopt. Android
        // doesn't bother to compile its kernel with CONFIG_IP_MULTICAST set.
        // This doesn't mean that there is no multicast code in the Android
        // kernel, it means there is no IGMP code in the kernel.  What this
        // means to us is that even through we are doing an IP_DROP_MEMBERSHIP
        // request, which is ultimately an IGMP operation, the request will
        // filter through the IP code before being ignored and will do useful
        // things in the kernel even though CONFIG_IP_MULTICAST was not set
        // for the Android build -- i.e., we have to do it anyway.
        //
        if (m_liveInterfaces[i].m_address.IsIPv4()) {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(IPV4_MULTICAST_GROUP);
            mreq.imr_interface.s_addr = m_liveInterfaces[i].m_address.GetIPv4AddressNetOrder();

            if (setsockopt(m_liveInterfaces[i].m_sockFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                           reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
                QCC_DbgPrintf(("NameService::ClearLiveInterfaces(void): setsockopt(IP_DROP_MEMBERSHIP) failed\n"));
            }
        } else if (m_liveInterfaces[i].m_address.IsIPv6()) {
            struct ipv6_mreq mreq;

            //
            // If we can't convert the multicast group to an IP address, there's
            // nothing we can do about letting routers on the net know that we
            // are closing down.
            //
            qcc::String mcGroup = qcc::String(IPV6_MULTICAST_GROUP);
            if (INET_PTON(AF_INET6, mcGroup.c_str(), &mreq.ipv6mr_multiaddr) == 0) {
                QCC_DbgPrintf(("NameService::ClearLiveInterfaces(void): INET_PTON failed\n"));
                continue;
            }

            //
            // The IPv6 version of selecting the multicast interface works on
            // an interface index instead of an IP address.  This index may
            // be changed on a per-socket basis so We have to figure out what
            // this index is using a getsockopt.
            //
            uint32_t index = 0;
            socklen_t indexLen = sizeof(index);

            if (getsockopt(m_liveInterfaces[i].m_sockFd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                           reinterpret_cast<char*>(&index), &indexLen) < 0) {
                QCC_DbgPrintf(("NameService::ClearLiveInterfaces(void): getsockopt(IPV6_MULTICAST_IF) failed\n"));
                continue;
            }

            mreq.ipv6mr_interface = index;

            //
            // This call tells the OS to issue an IGMP leave event.  This is
            // primarily going to send an IGMP packet to routers on the local
            // subnet telling them that we are no longer interested in hearing
            // packets destined for our multicast group.  Various operating
            // systems take this with various degrees of seriousness.  Android
            // ignores it completely, Linux generates the packet but leaves the
            // net device enabled for multicast reception, and Windows does
            // everything.
            //
            if (setsockopt(m_liveInterfaces[i].m_sockFd, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP,
                           reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
                QCC_DbgPrintf(("NameService::ClearLiveInterfaces(void): setsockopt(IPV6_DROP_MEMBERSHIP) failed\n"));
                continue;
            }
        } else {
            QCC_LogError(ER_FAIL, ("NameService:ClearLiveInterfaces: Address not IPv4 or IPv6"));
        }

        qcc::Close(m_liveInterfaces[i].m_sockFd);
        m_liveInterfaces[i].m_sockFd = -1;
    }

    m_liveInterfaces.clear();
}

//
// N.B. This function must be called with m_mutex locked since we wander through
// the list of requested interfaces that can also be modified by the user in the
// context of her thread(s).
//
void NameService::LazyUpdateInterfaces(void)
{
    QCC_DbgPrintf(("NameService::LazyUpdateInterfaces()\n"));

    //
    // However desirable it may be, the decision to simply use an existing
    // open socket exposes us to system-dependent behavior.  For example,
    // In Linux and Windows, an IGMP join must be done on an interface that
    // is currently IFF_UP and IFF_MULTICAST with an assigned IP address.
    // On Linux, that join remains in effect (net devices will continue to
    // recieve multicast packets destined for our group) even if the net
    // device goes down and comes back up with a different IP address.  On
    // Windows, however, if the interface goes down, an IGMP drop is done
    // and multicast receives will stop.  Since the socket never returns
    // any status unless we actually send data, it is very possible that
    // the state of the system can change out from underneath us without
    // our knowledge, and we would simply stop receiving multicasts. This
    // behavior is not specified anywhere that I am aware of, so Windows
    // cannot really be said to be broken.  It is just different, like it
    // is in so many other ways.  In Android, IGMP isn't even compiled into
    // the kernel, and so an out-of-band mechanism is used (wpa_supplicant
    // private driver commands called by the Java multicast lock).
    //
    // It can be argued that since we are using Android phones (sort-of
    // Linux) when mobility is a concern, and Windows boxes would be
    // relatively static, configuration, we could get away with ignoring
    // the possibility.  Since we are really talking an average of a couple
    // of IGMP packets every 30 seconds we take the conservative approach
    // and tear down all of our sockets and restart them every time through.
    //
    ClearLiveInterfaces();

    //
    // Call IfConfig to get the list of interfaces currently configured in the
    // system.  This also pulls out interface flags, addresses and MTU.  If we
    // can't get the system interfaces, we give up for now.
    //
    QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): IfConfig()\n"));
    std::vector<IfConfigEntry> entries;
    QStatus status = IfConfig(entries);
    if (status != ER_OK) {
        QCC_LogError(status, ("LazyUpdateInterfaces: IfConfig() failed"));
        return;
    }

    //
    // There are two fundamental ways we can look for interfaces to use.  We
    // can either walk the list of IfConfig entries (real interfaces on the
    // system) looking for any that match our list of user-requested
    // interfaces; or we can walk the list of user-requested interfaces looking
    // for any that match the list of real IfConfig entries.  Since we have an
    // m_any mode that means match all real IfConfig entries, we need to walk
    // the real IfConfig entries.
    //
    for (uint32_t i = 0; i < entries.size(); ++i) {
        //
        // We expect that every device in the system must have a name.
        // It might be some crazy random GUID in Windows, but it will have
        // a name.
        //
        assert(entries[i].m_name.size());
        QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): Checking out interface %s\n", entries[i].m_name.c_str()));

        //
        // We are never interested in interfaces that are not UP, do not support
        // MULTICAST, or are LOOPBACK interfaces.  We don't allow loopbacks
        // since sending messages to the local host is handled by the
        // MULTICAST_LOOP socket option which is enabled by default.
        //
        if ((entries[i].m_flags & IfConfigEntry::UP) == 0 ||
            (entries[i].m_flags & IfConfigEntry::MULTICAST) == 0 ||
            (entries[i].m_flags & IfConfigEntry::LOOPBACK) != 0) {
            QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): not UP and MULTICAST or LOOPBACK\n"));
            continue;
        }

        //
        // When initializing the name service, the user can decide whether or
        // not she wants to advertise and listen over IPv4 or IPv6.  We need
        // to check for that configuration here.  Since the rest of the code
        // just works with the live interfaces irrespective of address family,
        // this is the only place we need to do this check.
        //
        if ((m_enableIPv4 == false && entries[i].m_family == AF_INET) ||
            (m_enableIPv6 == false && entries[i].m_family == AF_INET6)) {
            QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): family %d not enabled\n", entries[i].m_family));
            continue;
        }


        //
        // The current real interface entry is a candidate for use.  We need to
        // decide if we are actually going to use it either based on the
        // wildcard mode or the list of requestedInterfaces provided by our
        // user.
        //
        bool useEntry = false;

        if (m_any) {
            QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): Use because wildcard mode\n"));
            useEntry = true;
        } else {
            for (uint32_t j = 0; j < m_requestedInterfaces.size(); ++j) {
                //
                // If the current real interface name matches the name in the
                // requestedInterface list, we will try to use it.
                //
                if (m_requestedInterfaces[j].m_interfaceName.size() != 0 &&
                    m_requestedInterfaces[j].m_interfaceName == entries[i].m_name) {
                    QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): Found matching requestedInterface name\n"));
                    useEntry = true;
                    break;
                }

                //
                // If the current real interface IP Address matches the name in
                // the requestedInterface list, we will try to use it.
                //
                if (m_requestedInterfaces[j].m_interfaceName.size() == 0 &&
                    m_requestedInterfaces[j].m_interfaceAddr == qcc::IPAddress(entries[i].m_addr)) {
                    QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): Found matching requestedInterface address\n"));
                    useEntry = true;
                    break;
                }
            }
        }

        //
        // If we aren't configured to use this entry, or have no idea how to use
        // this entry (not AF_INET or AF_INET6), try the next one.
        //
        if (useEntry == false || (entries[i].m_family != AF_INET && entries[i].m_family != AF_INET6)) {
            QCC_DbgPrintf(("NameService::LazyUpdateInterfaces(): Won't use this IfConfig entry\n"));
            continue;
        }

        //
        // If we fall through to here, we have decided that the host configured
        // entries[i] interface describes an interface we want to use to send
        // and receive our multicast name service messages over.  We keep a list
        // of "live" interfaces that reflect the interfaces we've previously
        // made the decision to use, so we'll set up a socket and move it there.
        // We have to be careful about what kind of socket we are going to use
        // for each entry (IPv4 or IPv6).
        //
        qcc::SocketFd sockFd;

        if (entries[i].m_family == AF_INET) {
            QStatus status = qcc::Socket(qcc::QCC_AF_INET, qcc::QCC_SOCK_DGRAM, sockFd);
            if (status != ER_OK) {
                QCC_LogError(status, ("LazyUpdateInterfaces: qcc::Socket(AF_INET) failed: %d - %s", errno, strerror(errno)));
                continue;
            }
        } else if (entries[i].m_family == AF_INET6) {
            QStatus status = qcc::Socket(qcc::QCC_AF_INET6, qcc::QCC_SOCK_DGRAM, sockFd);
            if (status != ER_OK) {
                QCC_LogError(status, ("LazyUpdateInterfaces: qcc::Socket(AF_INET6) failed: %d - %s", errno, strerror(errno)));
                continue;
            }
        } else {
            assert(!"NameService::LazyUpdateInterfaces(): Unexpected value in m_family (not AF_INET or AF_INET6");
            continue;
        }

        //
        // We must be able to reuse the address (well, port, but not all systems
        // support SO_REUSEPORT) so other AllJoyn daemon instances on this host can
        // listen in if desired.
        //
        uint32_t yes = 1;
        if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes)) < 0) {
            QCC_LogError(status, ("NameService::LazyUpdateInterfaces(): setsockopt(SO_REUSEADDR) failed: %d - %s",
                                  errno, strerror(errno)));
            qcc::Close(sockFd);
            continue;
        }

        //
        // Restrict the scope of the sent muticast packets to the local subnet.
        // Of course, IPv4 and IPv6 have to do it differently.
        //
        uint32_t ttl = 1;
        if (entries[i].m_family == AF_INET) {
            if (setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IP_MULTICAST_TTL) failed: %d - %s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        if (entries[i].m_family == AF_INET6) {
            if (setsockopt(sockFd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, reinterpret_cast<const char*>(&ttl), sizeof(ttl)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IP_MULTICAST_HOPS) failed: %d - %s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        qcc::IPAddress address(entries[i].m_addr);

        //
        // In order to control which interfaces get our multicast datagrams, it
        // is necessary to do so via a socket option.  See the Long Sidebar above.
        // Yes, you have to do it differently depending on whether or not you're
        // using IPv4 or IPv6.
        //
        if (entries[i].m_family == AF_INET) {
            struct in_addr addr;
            addr.s_addr = address.GetIPv4AddressNetOrder();
            if (setsockopt(sockFd, IPPROTO_IP, IP_MULTICAST_IF,
                           reinterpret_cast<const char*>(&addr), sizeof(addr)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IP_MULTICAST_IF) failed: %d - %s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        if (entries[i].m_family == AF_INET6) {
            uint32_t index = entries[i].m_index;
            if (setsockopt(sockFd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                           reinterpret_cast<const char*>(&index), sizeof(index)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IPV6_MULTICAST_IF) failed: %d - %s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        //
        // We are going to end up binding the socket to the interface specified
        // by the IP address in the interface list, but with multicast, things
        // are a little different.  Binding to INADDR_ANY is the correct thing
        // to do.  The See the Long Sidebar above.
        //
        if (entries[i].m_family == AF_INET) {
            status = qcc::Bind(sockFd, qcc::IPAddress("0.0.0.0"), MULTICAST_PORT);
            if (status != ER_OK) {
                QCC_LogError(status, ("NameService::LazyUpdateInterfaces(): bind(0.0.0.0) failed"));
                qcc::Close(sockFd);
                continue;
            }
        }

        if (entries[i].m_family == AF_INET6) {
            status = qcc::Bind(sockFd, qcc::IPAddress("::"), MULTICAST_PORT);
            if (status != ER_OK) {
                QCC_LogError(status, ("NameService::LazyUpdateInterfaces(): bind(::) failed"));
                qcc::Close(sockFd);
                continue;
            }
        }

        //
        // Arrange an IGMP join via the appropriate setsockopt. Android
        // doesn't bother to compile its kernel with CONFIG_IP_MULTICAST set.
        // This doesn't mean that there is no multicast code in the Android
        // kernel, it means there is no IGMP code in the kernel.  What this
        // means to us is that even through we are doing an IP_ADD_MEMBERSHIP
        // request, which is ultimately an IGMP operation, the request will
        // filter through the IP code before being ignored and will do useful
        // things in the kernel even though CONFIG_IP_MULTICAST was not set
        // for the Android build -- i.e., we have to do it anyway.
        //
        if (entries[i].m_family == AF_INET) {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(IPV4_MULTICAST_GROUP);
            mreq.imr_interface.s_addr = address.GetIPv4AddressNetOrder();
            if (setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IP_ADD_MEMBERSHIP) failed: %d - %s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        if (entries[i].m_family == AF_INET6) {
            struct ipv6_mreq mreq;
            qcc::String mcGroup(IPV6_MULTICAST_GROUP);
            if (INET_PTON(AF_INET6, mcGroup.c_str(), &mreq.ipv6mr_multiaddr) == 0) {
                QCC_LogError(status, ("NameService::LazyUpdateInterfaces(): INET_PTON failed"));
                qcc::Close(sockFd);
                continue;
            }

            mreq.ipv6mr_interface = entries[i].m_index;

            if (setsockopt(sockFd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                           reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
                QCC_LogError(status, (
                                 "NameService::LazyUpdateInterfaces(): setsockopt(IPV6_ADD_MEMBERSHIP) failed: %d-%s",
                                 errno, strerror(errno)));
                qcc::Close(sockFd);
                continue;
            }
        }

        //
        // Now take the interface "live."
        //
        LiveInterface live;
        live.m_interfaceName = entries[i].m_name;
        live.m_interfaceAddr = entries[i].m_addr;
        live.m_address = address;
        live.m_mtu = entries[i].m_mtu;
        live.m_index = entries[i].m_index;
        live.m_sockFd = sockFd;
        m_liveInterfaces.push_back(live);
    }
}

QStatus NameService::Locate(const qcc::String& wkn, LocatePolicy policy)
{
    QCC_DbgHLPrintf(("NameService::Locate(): %s with policy %d\n", wkn.c_str(), policy));

    //
    // Send a request to the network over our multicast channel,
    // asking for anyone who supports the specified well-known name.
    //
    WhoHas whoHas;
    whoHas.SetTcpFlag(true);
    whoHas.SetIPv4Flag(true);
    whoHas.AddName(wkn);

    Header header;
    header.SetVersion(0);
    header.SetTimer(m_tDuration);
    header.AddQuestion(whoHas);

    //
    // Add this message to the list of outsdanding messages.  We may want
    // to retransmit this request a few times.
    //
    m_mutex.Lock();
    m_retry.push_back(header);
    m_mutex.Unlock();

    //
    // Queue this message for transmission out on the various live interfaces.
    //
    QueueProtocolMessage(header);
    return ER_OK;
}

void NameService::SetCriticalParameters(
    uint32_t tDuration,
    uint32_t tRetransmit,
    uint32_t tQuestion,
    uint32_t modulus,
    uint32_t retries)
{
    m_tDuration = tDuration;
    m_tRetransmit = tRetransmit;
    m_tQuestion = tQuestion;
    m_modulus = modulus;
    m_retries = retries;
}

void NameService::SetCallback(Callback<void, const qcc::String&, const qcc::String&, vector<qcc::String>&, uint8_t>* cb)
{
    QCC_DbgPrintf(("NameService::SetCallback()\n"));

    delete m_callback;
    m_callback = cb;
}

QStatus NameService::SetEndpoints(
    const qcc::String& ipv4address,
    const qcc::String& ipv6address,
    uint16_t port)
{
    QCC_DbgHLPrintf(("NameService::SetEndpoints(%s, %s, %d)\n", ipv4address.c_str(), ipv6address.c_str(), port));

    m_mutex.Lock();

    //
    // If getting an IPv4 address, do some reasonableness checking.
    //
    if (ipv4address.size()) {
        if (ipv4address == "0.0.0.0") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv4 address looks like INADDR_ANY\n"));
            return ER_FAIL;
        }

        if (WildcardMatch(ipv4address, "*255") == 0) {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv4 address looks like a broadcast address\n"));
            return ER_FAIL;
        }

        if (WildcardMatch(ipv4address, "127*") == 0) {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv4 address looks like a loopback address\n"));
            return ER_FAIL;
        }
    }

    //
    // If getting an IPv6 address, do some reasonableness checking.
    //
    if (ipv6address.size()) {
        if (ipv6address == "0:0:0:0:0:0:0:1") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv6 address looks like a loopback address\n"));
            return ER_FAIL;
        }

        if (ipv6address == "::1") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv6 address looks like a loopback address\n"));
            return ER_FAIL;
        }

        if (ipv6address == "::") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv6 address looks like in6addr_any\n"));
            return ER_FAIL;
        }

        if (ipv6address == "0::0") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv6 address looks like in6addr_any\n"));
            return ER_FAIL;
        }

        if (ipv6address == "ff*") {
            QCC_DbgPrintf(("NameService::SetEndpoints(): IPv6 address looks like a multicast address\n"));
            return ER_FAIL;
        }
    }

    //
    // You must provide a reasonable port.
    //
    if (port == 0) {
        QCC_DbgPrintf(("NameService::SetEndpoints(): Must provide non-zero port\n"));
        return ER_FAIL;
    }

    m_ipv4address = ipv4address;
    m_ipv6address = ipv6address;
    m_port = port;

    m_forceLazyUpdate = true;
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();

    return ER_OK;
}

QStatus NameService::GetEndpoints(
    qcc::String& ipv4address,
    qcc::String& ipv6address,
    uint16_t& port)
{
    QCC_DbgHLPrintf(("NameService::GetEndpoints(%s, %s, %d)\n", ipv4address.c_str(), ipv6address.c_str(), port));

    m_mutex.Lock();
    ipv4address = m_ipv4address;
    ipv6address = m_ipv6address;
    port = m_port;
    m_mutex.Unlock();

    return ER_OK;
}

QStatus NameService::Advertise(const qcc::String& wkn)
{
    QCC_DbgHLPrintf(("NameService::Advertise(): %s\n", wkn.c_str()));

    vector<qcc::String> wknVector;
    wknVector.push_back(wkn);

    return Advertise(wknVector);
}

QStatus NameService::Advertise(vector<qcc::String>& wkn)
{
    QCC_DbgHLPrintf(("NameService::Advertise()\n"));

    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::Advertise(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    //
    // We have our ways of figuring out what the IPv4 and IPv6 addresses should
    // be if they are not set, but we absolutely need a port.
    //
    if (m_port == 0) {
        QCC_DbgPrintf(("NameService::Advertise(): Port not set\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads wandering through the advertised list.
    // We are running short on toes, so don't shoot any more off by not being
    // thread-unaware.
    //
    m_mutex.Lock();

    //
    // Make a note to ourselves which services we are advertising so we can
    // respond to protocol questions in the future.  Only allow one entry per
    // name.
    //
    for (uint32_t i = 0; i < wkn.size(); ++i) {
        list<qcc::String>::iterator j = find(m_advertised.begin(), m_advertised.end(), wkn[i]);
        if (j == m_advertised.end()) {
            m_advertised.push_back(wkn[i]);
        } else {
            //
            // Nothing has changed, so don't bother.
            //
            QCC_DbgPrintf(("NameService::Advertise(): Duplicate advertisement\n"));
            m_mutex.Unlock();
            return ER_OK;
        }
    }

    //
    // Keep the list sorted so we can easily distinguish a change in
    // the content of the advertised names versus a change in the order of the
    // names.
    //
    m_advertised.sort();

    //
    // If the advertisement retransmission timer is cleared, then set us
    // up to retransmit.  This has to be done with the mutex locked since
    // the main thread is playing with this value as well.
    //
    if (m_timer == 0) {
        m_timer = m_tDuration;
    }

    m_mutex.Unlock();

    //
    // The underlying protocol is capable of identifying both TCP and UDP
    // services.  Right now, the only possibility is TCP, so this is not
    // exposed to the user unneccesarily.
    //
    IsAt isAt;
    isAt.SetTcpFlag(true);
    isAt.SetUdpFlag(false);

    //
    // Always send the provided daemon GUID out with the reponse.
    //
    isAt.SetGuid(m_guid);

    //
    // Send a protocol message describing the entire list of names we have
    // for the provided protocol.
    //
    isAt.SetCompleteFlag(true);

    //
    // Set the port here.  When the message goes out a selected interface, the
    // protocol handler will write out the addresses according to its rules.
    //
    isAt.SetPort(m_port);

    //
    // Always advertise the whole list of advertisements that match the protocol
    // type, not just the (possibly one) entry the user has updated us with.
    //
    for (list<qcc::String>::iterator i = m_advertised.begin(); i != m_advertised.end(); ++i) {
        isAt.AddName(*i);
    }

    //
    // The header ties the whole protocol message together.  By setting the
    // timer, we are asking for everyone who hears the message to remember
    // the advertisements for that number of seconds.
    //
    Header header;
    header.SetVersion(0);
    header.SetTimer(m_tDuration);
    header.AddAnswer(isAt);

    //
    // Queue this message for transmission out on the various live interfaces.
    //
    QueueProtocolMessage(header);
    return ER_OK;
}

QStatus NameService::Cancel(const qcc::String& wkn)
{
    QCC_DbgPrintf(("NameService::Cancel(): %s\n", wkn.c_str()));

    vector<qcc::String> wknVector;
    wknVector.push_back(wkn);

    return Cancel(wknVector);
}

QStatus NameService::Cancel(vector<qcc::String>& wkn)
{
    QCC_DbgPrintf(("NameService::Cancel()\n"));

    if (m_state != IMPL_RUNNING) {
        QCC_DbgPrintf(("NameService::Advertise(): Not IMPL_RUNNING\n"));
        return ER_FAIL;
    }

    //
    // We have our ways of figuring out what the IPv4 and IPv6 addresses should
    // be if they are not set, but we absolutely need a port.
    //
    if (m_port == 0) {
        QCC_DbgPrintf(("NameService::Advertise(): Port not set\n"));
        return ER_FAIL;
    }

    //
    // There are at least two threads wandering through the advertised list.
    // We are running short on toes, so don't shoot any more off by not being
    // thread-unaware.
    //
    m_mutex.Lock();

    //
    // Remove the given services from our list of services we are advertising.
    //
    bool changed = false;

    for (uint32_t i = 0; i < wkn.size(); ++i) {
        list<qcc::String>::iterator j = find(m_advertised.begin(), m_advertised.end(), wkn[i]);
        if (j != m_advertised.end()) {
            m_advertised.erase(j);
            changed = true;
        }
    }

    //
    // If we have no more advertisements, there is no need to repeatedly state
    // this so turn off the retransmit timer.  The main thread is playing with
    // this number too, so this must be done with the mutex locked.
    //
    if (m_advertised.size() == 0) {
        m_timer = 0;
    }

    m_mutex.Unlock();

    //
    // If we didn't actually make a change, just return.
    //
    if (changed == false) {
        return ER_OK;
    }

    //
    // Send a protocol answer message describing the list of names we have just
    // been asked to withdraw.
    //
    // This code assumes that the daemon talks over TCP.  True for now.
    //
    IsAt isAt;
    isAt.SetTcpFlag(true);
    isAt.SetUdpFlag(false);

    //
    // Always send the provided daemon GUID out with the reponse.
    //
    isAt.SetGuid(m_guid);

    //
    // Set the port here.  When the message goes out a selected interface, the
    // protocol handler will write out the addresses according to its rules.
    //
    isAt.SetPort(m_port);

    //
    // Copy the names we are withdrawing the advertisement for into the
    // protocol message object.
    //
    for (uint32_t i = 0; i < wkn.size(); ++i) {
        isAt.AddName(wkn[i]);
    }

    //
    // When witdrawing advertisements, a complete flag means that we are
    // withdrawing all of the advertisements.  If the complete flag is
    // not set, we have some advertisements remaining.
    //
    if (m_advertised.size() == 0) {
        isAt.SetCompleteFlag(true);
    }

    //
    // The header ties the whole protocol message together.  We're at version
    // zero of the protocol.
    //
    Header header;
    header.SetVersion(0);

    //
    // We want to signal that everyone can forget about these names
    // so we set the timer value to 0.
    //
    header.SetTimer(0);
    header.AddAnswer(isAt);

    //
    // Queue this message for transmission out on the various live interfaces.
    //
    QueueProtocolMessage(header);
    return ER_OK;
}

void NameService::QueueProtocolMessage(Header& header)
{
    QCC_DbgPrintf(("NameService::QueueProtocolMessage()\n"));
    m_mutex.Lock();
    m_outbound.push_back(header);
    m_wakeEvent.SetEvent();
    m_mutex.Unlock();
}

//
// If you set HAPPY_WANDERER to 1, it will enable a test behavior that
// simulates the daemon happily wandering in and out of range of an
// imaginary access point.
//
// It is essentially a trivial one-dimentional random walk across a fixed
// domain.  When Wander() is called, der froliche wandering daemon moves
// in a random direction for one meter.  When the daemon "walks" out of
// range, Wander() returns false and the test will arrange that name
// service messages are discarded.  When the daemon "walks" back into
// range, messages are delivered again.  We generally call Wander() out
// DoPeriodicMaintenance() which ticks every second, but also out of
// HandleProtocolAnswer() so the random walk is at a non-constant rate
// driven by network activity.  Very nasty.
//
// The environment is 100 meters long, and the range of the access point
// is 50 meters.  The daemon starts right at the edge of the range and is
// expected to hover around that point, but wander random distances in and
// out.
//
//   (*)                       X                         |
//    |                     <- D ->                      |
//    ---------------------------------------------------
//    0                        50                       100
//
// Since this is a very dangerous setting, turning it on is a two-step
// process (set the #define and enable the bool); and we log every action
// as an error.  It will be hard to ignore this and accidentally leave it
// turned on.
//
#define HAPPY_WANDERER 0

#if HAPPY_WANDERER

static const uint32_t WANDER_LIMIT = 100;
static const uint32_t WANDER_RANGE = WANDER_LIMIT / 2;
static const uint32_t WANDER_START = WANDER_RANGE;

bool g_enableWander = false;

void WanderInit(void)
{
    srand(time(NULL));
}

bool Wander(void)
{
    //
    // If you don't explicitly enable this behavior, Wander() always returns
    // "in-range".
    //
    if (g_enableWander == false) {
        return true;
    }

    static uint32_t x = WANDER_START;
    static bool xyzzy = false;

    if (xyzzy == false) {
        WanderInit();
        xyzzy = true;
    }

    switch (x) {
    case 0:
        // Valderi
        ++x;
        break;

    case WANDER_LIMIT:
        // Valdera
        --x;
        break;

    default:
        // Valderahahahahahaha
        x += rand() & 1 ? 1 : -1;
        break;
    }

    QCC_LogError(ER_FAIL, ("Wander(): Wandered to %d which %s in-range", x, x < WANDER_RANGE ? "is" : "is NOT"));

    return x < WANDER_RANGE;
}

#endif

void NameService::SendProtocolMessage(qcc::SocketFd sockFd, bool sockFdIsIPv4, Header& header)
{
    QCC_DbgHLPrintf(("NameService::SendProtocolMessage()\n"));

    //
    // Legacy 802.11 MACs do not do backoff and retransmission of packets
    // destined for multicast addresses.  Therefore if there is a collision
    // on the air, a multicast packet will be silently dropped.  We get
    // no indication of this at all up at the Socket level.  When a remote
    // daemon makes a WhoHas request, we have to be very careful about
    // ensuring that all other daemons on the net don't try to respond at
    // the same time.  That would mean that all responses could result in
    // collisions and all responses would be lost.  We delay a short
    // random time before sending anything to avoid the thundering herd.
    //
    qcc::Sleep(rand() % 128);

#if HAPPY_WANDERER
    if (Wander() == false) {
        QCC_LogError(ER_FAIL, ("NameService::SendProtocolMessage(): Wander(): out of range"));
        return;
    } else {
        QCC_LogError(ER_FAIL, ("NameService::SendProtocolMessage(): Wander(): in range"));
    }
#endif

    size_t size = header.GetSerializedSize();

    if (size > NS_MESSAGE_MAX) {
        QCC_LogError(ER_FAIL, ("SendProtocolMessage: Message longer than NS_MESSAGE_MAX (%d bytes)", NS_MESSAGE_MAX));
        return;
    }

    uint8_t* buffer = new uint8_t[size];
    header.Serialize(buffer);

    //
    // Send the packet.
    //
    size_t sent;
    if (sockFdIsIPv4) {
        qcc::IPAddress ipv4(IPV4_MULTICAST_GROUP);
        qcc::SendTo(sockFd, ipv4, MULTICAST_PORT, buffer, size, sent);
    } else {
        qcc::IPAddress ipv6(IPV6_MULTICAST_GROUP);
        qcc::SendTo(sockFd, ipv6, MULTICAST_PORT, buffer, size, sent);
    }

    delete [] buffer;
}

void* NameService::Run(void* arg)
{
    QCC_DbgPrintf(("NameService::Run()\n"));

    //
    // This method is executed by the name service main thread and becomes the
    // center of the name service universe.  All incoming and outgoing messages
    // percolate through this thread because of the way we have to deal with
    // interfaces coming up and going down underneath us in a mobile
    // environment.  See the "Long Sidebar" comment above for some details on
    // the pain this has caused.
    //
    // Ultimately, this means we have a number of sockets open that correspond
    // to the "live" interfaces we are listening to.  We have to listen to all
    // of these sockets in what amounts to a select() below.  That means we
    // have live FDs waiting in the select.  On the other hand, we want to be
    // responsive in the case of a user turning on wireless and immediately
    // doing a Locate().  This requirement implies that we need to update the
    // interface state whenever we do a Locate.  This Locate() will be done in
    // the context of a user thread.  So we have a requirement that we avoid
    // changing the state of the FDs in another thread and the requirement
    // that we change the state of the FDs when the user wants to Locate().
    // Either we play synchronization games and distribute our logic or do
    // everything here.  Because it is easier to manage the process in one
    // place, we have all messages gonig through this thread.
    //
    size_t bufsize = NS_MESSAGE_MAX;
    uint8_t* buffer = new uint8_t[bufsize];

    //
    // Instantiate an event that fires after one second, and once per second
    // thereafter.  Used to drive protocol maintenance functions, especially
    // dealing with interface state changes.
    //
    const uint32_t MS_PER_SEC = 1000;
    qcc::Event timerEvent(MS_PER_SEC, MS_PER_SEC);

    qcc::Timespec tNow, tLastLazyUpdate;
    GetTimeNow(&tLastLazyUpdate);

    while (!IsStopping()) {
        GetTimeNow(&tNow);

        //
        // We have a variable length number of open interfaces with their
        // corresponding sockets to wait on.  Do this allocation here to
        // avoid conflicts with Open- and CloseInterface and the underlying
        // sockets and events.
        //
        m_mutex.Lock();

        //
        // We need to figure out which interfaces we can send and receive
        // protocol messages over.  We want to do this often enough to
        // appear responsive to the user, but we also don't want to naively
        // do this fairly complicated operation every time a packet is sent
        // if our dear user has decided to write code that does lots of
        // Locate() or Advertise() calls.
        //
        // TODO:  This should ultimately be a configurable policy, but for
        // now we defer to our users and always check out the interface
        // situation before sending messages; but we never let more than
        // LAZY_UPDATE_INTERVAL seconds pass without er-evaluating our
        // interface status.
        //
        bool timedOut = tLastLazyUpdate + qcc::Timespec(LAZY_UPDATE_INTERVAL * MS_PER_SEC) < tNow;
        if (m_forceLazyUpdate || m_outbound.size() || timedOut) {
            LazyUpdateInterfaces();
            tLastLazyUpdate = tNow;
            m_forceLazyUpdate = false;
        }

        //
        // We know what interfaces can be currently used to send messages
        // over, so now send any messages we have queued for transmission.
        //
        while (m_outbound.size()) {

            //
            // The header contains a number of "questions" and "answers".
            // We just pass on questions (who-has messages) as-is.  If these
            // are answers (is-at messages) we need to worry about getting
            // the address information right.
            //
            // The rules are fairly :
            //
            // In the most general case, we have both IPv4 and IPV6 running.
            // When we send an IPv4 mulitcast, we communicate the IPv4 address
            // of the underlying interface through the IP address of the sent
            // packet.  If there is also an IPv6 address assigned to the sending
            // interface, we want to send that to the remote side as well.  We
            // do that by providing the IPv6 address in the message.  Similarly,
            // when we send an IPv6 multicast, we put a possible IPv4 address in
            // the message.
            //
            // If the user provides an IPv4 or IPv6 address in the SetEndpoints
            // call, we need to add those to the outgoing messages.  These will
            // be picked up on the other side and passed up to the discovering
            // daemon.  This trumps what was previously done by the first rule.
            //
            // N.B. We are getting a copy of the message here, so we can munge
            // the contents to our heart's delight.
            //
            Header header = m_outbound.front();

            //
            // Walk the list of live interfaces and send the protocol message
            // out each one.
            //
            for (uint32_t i = 0; i < m_liveInterfaces.size(); ++i) {
                qcc::SocketFd sockFd = m_liveInterfaces[i].m_sockFd;
                if (sockFd != -1) {
                    bool isIPv4 = m_liveInterfaces[i].m_address.IsIPv4();
                    bool isIPv6 = !isIPv4;

                    //
                    // See if there is an alternate address for this interface.
                    // If the interface is IPv4, the alternate would be IPv6
                    // and vice-versa..
                    //
                    bool haveAlternate = false;
                    qcc::IPAddress altAddress;
                    for (uint32_t j = 0; j < m_liveInterfaces.size(); ++j) {
                        if (m_liveInterfaces[i].m_sockFd == -1 ||
                            m_liveInterfaces[j].m_interfaceName != m_liveInterfaces[i].m_interfaceName) {
                            continue;
                        }
                        if ((isIPv4 && m_liveInterfaces[j].m_address.IsIPv6()) ||
                            (isIPv6 && m_liveInterfaces[j].m_address.IsIPv4())) {
                            haveAlternate = true;
                            altAddress = m_liveInterfaces[j].m_address.ToString();
                        }
                    }

                    //
                    // Walk the list of answer messages and rewrite the provided
                    // addresses.
                    //
                    for (uint8_t j = 0; j < header.GetNumberAnswers(); ++j) {
                        IsAt* isAt;
                        header.GetAnswer(j, &isAt);

                        //
                        // We're modifying the answsers in-place so clear any
                        // addresses we might have added on the last iteration.
                        //
                        isAt->ClearIPv4();
                        isAt->ClearIPv6();

                        //
                        // Add the appropriate alternate address if there, or
                        // trump with user provided addresses.
                        //
                        if (haveAlternate) {
                            if (isIPv4) {
                                isAt->SetIPv6(altAddress.ToString());
                            } else {
                                isAt->SetIPv4(altAddress.ToString());
                            }
                        }

                        if (m_ipv4address.size()) {
                            isAt->SetIPv4(m_ipv4address);
                        }

                        if (m_ipv6address.size()) {
                            isAt->SetIPv6(m_ipv6address);
                        }
                    }

                    //
                    // Send the possibly modified message out the current
                    // interface.
                    //
                    SendProtocolMessage(sockFd, isIPv4, header);
                }
            }
            //
            // The current message has been sent to all of the live interfaces,
            // so we can discard it.
            //
            m_outbound.pop_front();
        }

        //
        // Now, worry about what to do next.  Create a set of events to wait on.
        // We always wait on the stop event, the timer event and the event used
        // to signal us when an outging message is queued or a forced wakeup for
        // a lazy update is done.
        //
        vector<qcc::Event*> checkEvents, signaledEvents;
        checkEvents.push_back(&stopEvent);
        checkEvents.push_back(&timerEvent);
        checkEvents.push_back(&m_wakeEvent);

        //
        // We also need to wait on all of the sockets that correspond to the
        // "live" interfaces we need to listen for inbound multicast messages
        // on.
        //
        for (uint32_t i = 0; i < m_liveInterfaces.size(); ++i) {
            if (m_liveInterfaces[i].m_sockFd != -1) {
                checkEvents.push_back(new qcc::Event(m_liveInterfaces[i].m_sockFd, qcc::Event::IO_READ, false));
            }
        }

        //
        // We are going to go to sleep for possibly as long as a second, so
        // we definitely need to release other (user) threads that might
        // be waiting to talk to us.
        //
        m_mutex.Unlock();

        //
        // Wait for something to happen.  if we get an error, there's not
        // much we can do about it but bail.
        //
        QStatus status = qcc::Event::Wait(checkEvents, signaledEvents);
        if (status != ER_OK && status != ER_TIMEOUT) {
            QCC_LogError(status, ("NameService::Run(): Event::Wait(): Failed"));
            break;
        }

        //
        // Loop over the events for which we expect something has happened
        //
        for (vector<qcc::Event*>::iterator i = signaledEvents.begin(); i != signaledEvents.end(); ++i) {
            if (*i == &stopEvent) {
                QCC_DbgPrintf(("NameService::Run(): Stop event fired\n"));
                //
                // We heard the stop event, so reset it.  We'll pop out of the
                // server loop when we run through it again (above).
                //
                stopEvent.ResetEvent();
            } else if (*i == &timerEvent) {
                // QCC_DbgPrintf(("NameService::Run(): Timer event fired\n"));
                //
                // This is an event that fires every second to give us a chance
                // to do any protocol maintenance, like retransmitting queued
                // advertisements.
                //
                DoPeriodicMaintenance();
            } else if (*i == &m_wakeEvent) {
                QCC_DbgPrintf(("NameService::Run(): Wake event fired\n"));
                //
                // This is an event that fires whenever a message has been
                // queued on the outbound name service message queue.  We
                // always check the queue whenever we run through the loop,
                // (it'll happen before we sleep again) but we do have to reset
                // it.
                //
                m_wakeEvent.ResetEvent();
            } else {
                QCC_DbgPrintf(("NameService::Run(): Socket event fired\n"));
                //
                // This must be activity on one of our multicast listener sockets.
                //
                qcc::SocketFd sockFd = (*i)->GetFD();

                QCC_DbgPrintf(("NameService::Run(): Call qcc::RecvFrom()\n"));

                qcc::IPAddress address;
                uint16_t port;
                size_t nbytes;

                QStatus status = qcc::RecvFrom(sockFd, address, port, buffer, bufsize, nbytes);
                if (status != ER_OK) {
                    //
                    // Not many options here if an error happens either.  We bail
                    // to avoid states where we read repeatedly and basically
                    // just end up in an infinite loop getting errors.
                    //
                    // XXX We could actually close the socket and stop listening
                    // on the failing interface.
                    //
                    QCC_LogError(status, ("NameService::Run(): qcc::RecvFrom(): Failed"));
                    break;
                }

                //
                // We got a message over the multicast channel.  Deal with it.
                //
                HandleProtocolMessage(buffer, nbytes, address);
            }
        }

        //
        // Delete the checkEvents we created above
        //
        for (uint32_t i = 0; i < checkEvents.size(); ++i) {
            if (checkEvents[i] != &stopEvent && checkEvents[i] != &timerEvent && checkEvents[i] != &m_wakeEvent) {
                delete checkEvents[i];
            }
        }
    }

    delete [] buffer;
    return 0;
}

void NameService::Retry(void)
{
    static uint32_t tick = 0;

    //
    // tick holds 136 years of ticks at one per second, so we don't worry about
    // rolling over.
    //
    ++tick;

    //
    // use Meyers' idiom to keep iterators sane.
    //
    for (list<Header>::iterator i = m_retry.begin(); i != m_retry.end();) {
        uint32_t retryTick = (*i).GetRetryTick();

        //
        // If this is the first time we've seen this entry, set the first
        // retry time.
        //
        if (retryTick == 0) {
            (*i).SetRetryTick(tick + RETRY_INTERVAL);
            ++i;
            continue;
        }

        if (tick >= retryTick) {
            //
            // Send the message out over the multicast link (again).
            //
            QueueProtocolMessage(*i);

            uint32_t count = (*i).GetRetries();
            ++count;

            if (count == m_retries) {
                m_retry.erase(i++);
            } else {
                (*i).SetRetries(count);
                (*i).SetRetryTick(tick + RETRY_INTERVAL);
                ++i;
            }
        } else {
            ++i;
        }
    }
}

void NameService::Retransmit(void)
{
    QCC_DbgPrintf(("NameService::Retransmit()\n"));

    //
    // We need a valid port before we send something out to the local subnet.
    //
    if (m_port == 0) {
        QCC_DbgPrintf(("NameService::Retransmit(): Port not set\n"));
        return;
    }

    //
    // There are at least two threads wandering through the advertised list.
    // We are running short on toes, so don't shoot any more off by not being
    // thread-unaware.
    //
    m_mutex.Lock();

    //
    // The underlying protocol is capable of identifying both TCP and UDP
    // services.  Right now, the only possibility is TCP.
    //
    IsAt isAt;
    isAt.SetTcpFlag(true);
    isAt.SetUdpFlag(false);

    //
    // Always send the provided daemon GUID out with the reponse.
    //
    isAt.SetGuid(m_guid);

    //
    // Send a protocol message describing the entire (complete) list of names
    // we have.
    //
    isAt.SetCompleteFlag(true);

    isAt.SetPort(m_port);

    //
    // Add all of our adversised names to the protocol answer message.
    //
    for (list<qcc::String>::iterator i = m_advertised.begin(); i != m_advertised.end(); ++i) {
        isAt.AddName(*i);
    }
    m_mutex.Unlock();

    //
    // The header ties the whole protocol message together.  By setting the
    // timer, we are asking for everyone who hears the message to remember
    // the advertisements for that number of seconds.
    //
    Header header;
    header.SetVersion(0);
    header.SetTimer(m_tDuration);
    header.AddAnswer(isAt);

    //
    // Send the message out over the multicast link.
    //
    QueueProtocolMessage(header);
}

void NameService::DoPeriodicMaintenance(void)
{
#if HAPPY_WANDERER
    Wander();
#endif
    m_mutex.Lock();

    //
    // Retry all Locate requests to ensure that those requests actually make
    // it out on the wire.
    //
    Retry();

    //
    // If we have something exported, we will have a retransmit timer value
    // set.  If not, this value will be zero and there's nothing to be done.
    //
    if (m_timer) {
        --m_timer;
        if (m_timer == m_tRetransmit) {
            QCC_DbgPrintf(("NameService::DoPeriodicMaintenance(): Retransmit()\n"));
            Retransmit();
            m_timer = m_tDuration;
        }
    }

    m_mutex.Unlock();
}

void NameService::HandleProtocolQuestion(WhoHas whoHas, qcc::IPAddress address)
{
    QCC_DbgHLPrintf(("NameService::HandleProtocolQuestion()\n"));

    //
    // There are at least two threads wandering through the advertised list.
    // We are running short on toes, so don't shoot any more off by not being
    // thread-unaware.
    //
    m_mutex.Lock();

    //
    // Loop through the names we are being asked about, and if we have
    // advertised any of them, we are going to need to respond to this
    // question.
    //
    bool respond = false;
    for (uint32_t i = 0; i < whoHas.GetNumberNames(); ++i) {
        qcc::String wkn = whoHas.GetName(i);

        //
        // Zero length strings are unmatchable.  If you want to do a wildcard
        // match, you've got to send a wildcard character.
        //
        if (wkn.size() == 0) {
            continue;
        }

        //
        // check to see if this name on the list of names we advertise.
        //
        for (list<qcc::String>::iterator j = m_advertised.begin(); j != m_advertised.end(); ++j) {

            //
            // The requested name comes in from the WhoHas message and we
            // allow wildcards there.
            //
            if (WildcardMatch((*j), wkn)) {
                QCC_DbgHLPrintf(("NameService::HandleProtocolQuestion(): request for %s does not match my %s\n",
                                 wkn.c_str(), (*j).c_str()));
                continue;
            } else {
                respond = true;
                break;
            }
        }

        //
        // If we find a match, don't bother going any further since we need
        // to respond in any case.
        //
        if (respond) {
            break;
        }
    }

    m_mutex.Unlock();

    //
    // Since any response we send must include all of the advertisements we
    // are exporting; this just means to retransmit all of our advertisements.
    //
    if (respond) {
        Retransmit();
    }
}

void NameService::HandleProtocolAnswer(IsAt isAt, uint32_t timer, qcc::IPAddress address)
{
    QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer()\n"));

    //
    // If there are no callbacks we can't tell the user anything about what is
    // going on the net, so it's pointless to go any further.
    //

    if (m_callback == 0) {
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): No callback, so nothing to do\n"));
        return;
    }

    vector<qcc::String> wkn;

    for (uint8_t i = 0; i < isAt.GetNumberNames(); ++i) {
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got well-known name %s\n", isAt.GetName(i).c_str()));
        wkn.push_back(isAt.GetName(i));
    }

    //
    // Life is easier if we keep these things sorted.  Don't rely on the source
    // (even though it is really us) to do so.
    //
    sort(wkn.begin(), wkn.end());

    qcc::String guid = isAt.GetGuid();
    QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got GUID %s\n", guid.c_str()));

    //
    // We always get an address since we got the message over a call to
    // recvfrom().  This will either be an IPv4 or an IPv6 address.  We can
    // also get an IPv4 and/or an IPv6 address in the answer message itself.
    // We have from one to three addresses of different flavors that we need
    // to communicate back to the daemon.  It is convenient for the daemon
    // to get these addresses in the form of a "listen-spec".  These look like,
    // "tcp:addr=x, port=y".  The daemon is going to keep track of unique
    // combinations of these and must be able to handle multiple identical
    // reports since we will be getting keepalives.  What we need to do then
    // is to send a callback with a listen-spec for every address we find.
    // If we get all three addresses, we'll do three callbacks with different
    // listen-specs.
    //
    qcc::String recvfromAddress, ipv4address, ipv6address;

    recvfromAddress = address.ToString();
    QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got IP %s from protocol\n", recvfromAddress.c_str()));

    if (isAt.GetIPv4Flag()) {
        ipv4address = isAt.GetIPv4();
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got IPv4 %s from message\n", ipv4address.c_str()));
    }

    if (isAt.GetIPv6Flag()) {
        ipv6address = isAt.GetIPv6();
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got IPv6 %s from message\n", ipv6address.c_str()));
    }

    uint16_t port = isAt.GetPort();
    QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Got port %d from message\n", port));

    //
    // The longest bus address we can generate is going to be the larger
    // of an IPv4 or IPv6 address:
    //
    // "tcp:addr=255.255.255.255,port=65535"
    // "tcp:addr=ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff,port=65535"
    //
    // or 60 characters long including the trailing '\0'
    //
    char addrbuf[60];

    //
    // Call back with the address we got via recvfrom unless it is overridden by the address in the
    // message. An ipv4 address in the message overrides an ipv4 recvfrom address, an ipv6 address in
    // the message overrides an ipv6 recvfrom address.
    //
    if ((address.IsIPv4() && !ipv4address.size()) || (address.IsIPv6() && !ipv6address.size())) {
        snprintf(addrbuf, sizeof(addrbuf), "tcp:addr=%s,port=%d", recvfromAddress.c_str(), port);
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Calling back with %s\n", addrbuf));
        qcc::String busAddress(addrbuf);

        if (m_callback) {
            (*m_callback)(busAddress, guid, wkn, timer);
        }
    }

    //
    // If we received an IPv4 address in the message, call back with that one.
    //
    if (ipv4address.size()) {
        snprintf(addrbuf, sizeof(addrbuf), "tcp:addr=%s,port=%d", ipv4address.c_str(), port);
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Calling back with %s\n", addrbuf));
        qcc::String busAddress(addrbuf);

        if (m_callback) {
            (*m_callback)(busAddress, guid, wkn, timer);
        }
    }

    //
    // If we received an IPv6 address in the message, call back with that one.
    //
    if (ipv6address.size()) {
        snprintf(addrbuf, sizeof(addrbuf), "tcp:addr=%s,port=%d", ipv6address.c_str(), port);
        QCC_DbgHLPrintf(("NameService::HandleProtocolAnswer(): Calling back with %s\n", addrbuf));
        qcc::String busAddress(addrbuf);

        if (m_callback) {
            (*m_callback)(busAddress, guid, wkn, timer);
        }
    }
}

void NameService::HandleProtocolMessage(uint8_t const* buffer, uint32_t nbytes, qcc::IPAddress address)
{
    QCC_DbgHLPrintf(("NameService::HandleProtocolMessage(0x%x, %d, %s)\n", buffer, nbytes, address.ToString().c_str()));

#if HAPPY_WANDERER
    if (Wander() == false) {
        QCC_LogError(ER_FAIL, ("NameService::HandleProtocolMessage(): Wander(): out of range"));
        return;
    } else {
        QCC_LogError(ER_FAIL, ("NameService::HandleProtocolMessage(): Wander(): in range"));
    }
#endif

    Header header;
    size_t bytesRead = header.Deserialize(buffer, nbytes);
    if (bytesRead != nbytes) {
        QCC_DbgPrintf(("NameService::HandleProtocolMessage(): Deserialize(): Error\n"));
        return;
    }

    //
    // We only understand version zero packets for now.
    //
    if (header.GetVersion() != 0) {
        QCC_DbgPrintf(("NameService::HandleProtocolMessage(): Unknown version: Error\n"));
        return;
    }

    //
    // If the received packet contains questions, see if we can answer them.
    // We have the underlying device in loopback mode so we can get receive
    // our own questions.  We usually don't have an answer and so we don't
    // reply, but if we do have the requested names, we answer ourselves
    // to pass on this information to other interested bystanders.
    //
    for (uint8_t i = 0; i < header.GetNumberQuestions(); ++i) {
        HandleProtocolQuestion(header.GetQuestion(i), address);
    }

    //
    // If the received packet contains answers, see if they are answers to
    // questions we think are interesting.  Make sure we are not talking to
    // ourselves unless we are told to for debugging purposes
    //
    for (uint8_t i = 0; i < header.GetNumberAnswers(); ++i) {
        IsAt isAt = header.GetAnswer(i);
        if (m_loopback || (isAt.GetGuid() != m_guid)) {
            HandleProtocolAnswer(isAt, header.GetTimer(), address);
        }
    }
}

} // namespace ajn
