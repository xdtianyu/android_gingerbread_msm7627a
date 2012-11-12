/* Copyright (c) 2010, 2011 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>

#define LOG_TAG "RouteController"
#include <cutils/log.h>


#include "RouteController.h"

static char IP_PATH[] = "/system/bin/ip";

const char *RouteController::MAIN_TABLE = "254";

extern "C" int logwrap(int argc, const char **argv, int background);

RouteController::RouteController() {
}

RouteController::~RouteController() {
}

int RouteController::runIpCmd(const char *cmd) {
    char buffer[255];

    strncpy(buffer, cmd, sizeof(buffer)-1);

    const char *args[32];
    char *next = buffer;
    char *tmp;

    args[0] = IP_PATH;
    int i = 1;

    while ((tmp = strsep(&next, " "))) {
        args[i++] = tmp;
        if (i == 32) {
            LOGE("ip argument overflow");
            errno = E2BIG;
            return -1;
        }
    }
    args[i] = NULL;

    return logwrap(i, args, 0);
}

int RouteController::repSrcRoute
(
    const char *iface,
    const char *srcPrefix,
    const char *gateway,
    const char *table,
    const char *ipver
)
{
    if (repDefRoute(iface, gateway, table, ipver) != 0)
        return -1;
    delRule(table, ipver);
    return addRule(srcPrefix, table, ipver);
}

int RouteController::delSrcRoute
(
    const char *table,
    const char *ipver
)
{
    if (delDefRoute(table, ipver) != 0)
        return -1;
    return delRule(table, ipver);
}

int RouteController::addDstRoute
(
    const char *iface,
    const char *dstPrefix,
    const char *gateway,
    const char *table
)
{
    char buffer[255];

    if (gateway) {
        snprintf(buffer, sizeof buffer,
                 "route add %s via %s dev %s scope global table %s",
                 dstPrefix, gateway, iface, table);
    } else {
        snprintf(buffer, sizeof buffer,
                 "route add %s dev %s table %s", dstPrefix, iface, table);
    }

    // Blindly do this as addition of duplicate route fails; we want
    // add to succeed if the requested route exists and logwrapper
    // does not set errno to EEXIST. So delete prior to add
    delDstRoute(dstPrefix, table);

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}

int RouteController::delDstRoute
(
    const char *dstPrefix,
    const char *table
)
{
    char buffer[255];
    snprintf(buffer, sizeof buffer, "route del %s table %s", dstPrefix, table);

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}

int RouteController::replaceDefRoute
(
    const char *iface,
    const char *gateway,
    const char *ipver
)
{
    if(repDefRoute(iface, gateway, MAIN_TABLE, ipver) != 0)
        return -1;
    return 0;
}

int RouteController::repDefRoute
(
    const char *iface,
    const char *gateway,
    const char *table,
    const char *ipver
)
{
    char buffer[255];

    if (gateway) {
        snprintf(buffer, sizeof buffer,
                 "%s route replace default via %s dev %s scope global table %s",
                 ipver, gateway, iface, table);
    } else {
        snprintf(buffer, sizeof buffer,
                 "%s route replace default dev %s table %s", ipver, iface, table);
    }

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}

int RouteController::delDefRoute
(
    const char *table,
    const char *ipver
)
{
    char buffer[255];

    snprintf(buffer, sizeof buffer,
             "%s route del default table %s", ipver, table);

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}

int RouteController::flushCache() {
    return runIpCmd("route flush cached");
}

int RouteController::addRule
(
    const char *address,
    const char *table,
    const char *ipver
)
{
    char buffer[255];

    snprintf(buffer, sizeof buffer,
             "%s rule add from %s lookup %s", ipver, address, table);

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}

int RouteController::delRule
(
    const char *table,
    const char *ipver
)
{
    char buffer[255];

    snprintf(buffer, sizeof buffer,
             "%s rule del table %s", ipver, table);

    int r = runIpCmd(buffer);

    if (r == 0)
        return flushCache();
    else
        return -1;
}
