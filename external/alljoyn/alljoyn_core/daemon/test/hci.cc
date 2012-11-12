/**
 * @file
 *
 * This file tests sending raw HCI commands
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

#ifdef QCC_OS_GROUP_POSIX
#include <sys/ioctl.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <qcc/platform.h>

#include <qcc/Util.h>
#include <qcc/Debug.h>
#include <qcc/Socket.h>
#include <qcc/StringUtil.h>

#include <alljoyn/version.h>

#include <Status.h>

using namespace qcc;
using namespace std;
using namespace ajn;

#define QCC_MODULE "ALLJOYN"

#define GET_DEVICES_IOCTL       _IOR('H', 210, uint32_t)

/*
 * @param deviceId    The Bluetooth device id
 * @param window      The inquiry window in milliseconds (10 .. 2560)
 * @param interval    The inquiry window in milliseconds (11 .. 2560)
 * @param interlaced  If true use interlaced inquiry.
 */
namespace ajn {
namespace bluez {
extern QStatus ConfigureInquiryScan(uint16_t deviceId, uint16_t window, uint16_t interval, bool interlaced, int8_t txPower);
extern QStatus ConfigurePeriodicInquiry(uint16_t deviceId, uint16_t minPeriod, uint16_t maxPeriod, uint8_t length, uint8_t maxResponses);
}
}

using namespace ajn::bluez;


struct DeviceInfo {
    uint16_t numDevices;
    struct {
        uint32_t id;
        uint32_t pad;
    } device[4];
};

void usage()
{
    printf("Usage hci [-l]\n\n");
    printf("Options:\n");
    printf("   -h      = Print this help message\n");
    printf("   -l len     = Inquiry length\n");
    printf("   -n num     = Number of devices to discover\n");
    printf("   -i min max = Min and Max values for periodic inquiry\n");
}

int main(int argc, char** argv)
{
#ifdef WIN32
    printf("Not implemented on windows\n");
#else
    QStatus status;
    SocketFd hciFd;
    int len = 1;
    int num = 0;
    int min = 2;
    int max = 3;

    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp("-l", argv[i])) {
            ++i;
            if (i == argc) {
                usage();
                exit(1);
            }
            sscanf(argv[i], "%d", &len);
        }
        if (0 == strcmp("-n", argv[i])) {
            ++i;
            if (i == argc) {
                usage();
                exit(1);
            }
            sscanf(argv[i], "%d", &num);
        }
        if (0 == strcmp("-i", argv[i])) {
            i += 2;
            if (i >= argc) {
                usage();
                exit(1);
            }
            sscanf(argv[i - 1], argv[i], "%d %d", &min, &max);
        }
    }

    hciFd = (SocketFd)socket(AF_BLUETOOTH, QCC_SOCK_RAW, 1);
    if (!hciFd) {
        printf("failed to create socket\n");
    }
    DeviceInfo devInfo;
    memset(&devInfo, 0, sizeof(devInfo));
    devInfo.numDevices = ArraySize(devInfo.device);
    if (ioctl(hciFd, GET_DEVICES_IOCTL, &devInfo) < 0) {
        printf("failed to get BT device list (%d)\n", errno);
        return 0;
    }
    close(hciFd);

    printf("%d devices\n", devInfo.numDevices);
    for (size_t i = 0; i < devInfo.numDevices; ++i) {
        printf("device[%d] id = %u\n", static_cast<uint32_t>(i), devInfo.device[i].id);
        status = ConfigureInquiryScan(devInfo.device[i].id, 10, 2560, true, 0);
        if (status != ER_OK) {
            printf("ConfigureInquiryScan failed: %s\n", QCC_StatusText(status));
        }
    }

    for (size_t i = 0; i < devInfo.numDevices; ++i) {
        printf("ConfigurePeriodicInquiry hci%d min:%d max:%d len:%d num:%d\n", devInfo.device[i].id, min, max, len, num);
        status = ConfigurePeriodicInquiry(devInfo.device[i].id, (uint16_t)min, (uint16_t)max, (uint8_t)len, (uint8_t)num);
        if (status != ER_OK) {
            printf("ConfigurePeriodicInquiry failed: %s\n", QCC_StatusText(status));
        }
    }
#endif
    return 0;
}
