/**
 * @file
 * BTAccessor declaration for BlueZ
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
#ifndef _ALLJOYN_BLUEZ_H
#define _ALLJOYN_BLUEZ_H

#include <qcc/platform.h>


#define SOL_BLUETOOTH  274
#define SOL_L2CAP        6
#define SOL_RFCOMM      18
#define BT_SECURITY      4
#define BT_SECURITY_LOW  1

#define RFCOMM_CONNINFO  2
#define L2CAP_CONNINFO   2
#define L2CAP_OPTIONS    1

#define RFCOMM_PROTOCOL_ID 3
#define L2CAP_PROTOCOL_ID  0

namespace ajn {
namespace bluez {
typedef struct _BDADDR {
    unsigned char b[6];
} BDADDR;

typedef struct _RFCOMM_SOCKADDR {
    uint16_t sa_family;
    BDADDR bdaddr;
    uint8_t channel;
} RFCOMM_SOCKADDR;

typedef struct _L2CAP_SOCKADDR {
    uint16_t sa_family;
    uint16_t psm;
    BDADDR bdaddr;
    uint16_t cid;
} L2CAP_SOCKADDR;

typedef union _BT_SOCKADDR {
    L2CAP_SOCKADDR l2cap;
    RFCOMM_SOCKADDR rfcomm;
} BT_SOCKADDR;

struct l2cap_options {
    uint16_t omtu;
    uint16_t imtu;
    uint16_t flush_to;
    uint8_t mode;
};

struct sockaddr_hci {
    sa_family_t family;
    uint16_t dev;
};




} // namespace bluez
} // namespace ajn


#endif
