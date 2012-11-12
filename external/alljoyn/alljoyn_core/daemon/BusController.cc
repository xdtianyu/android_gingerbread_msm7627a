/**
 * @file
 *
 * BusController is responsible for responding to standard DBus messages
 * directed at the bus itself.
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

#include "BusController.h"
#include "DaemonRouter.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN_DAEMON"

using namespace std;
using namespace qcc;

namespace ajn {

BusController::BusController(Bus& bus, QStatus& status) :
#ifndef NDEBUG
    alljoynDebugObj(bus),
#endif
    dbusObj(bus),
    alljoynObj(bus)
{
    DaemonRouter& router(reinterpret_cast<DaemonRouter&>(bus.GetInternal().GetRouter()));
    router.SetBusController(*this);
    status = dbusObj.Init();
    if (ER_OK == status) {
        status = alljoynObj.Init();
    }

#ifndef NDEBUG
    // AlljoynDebugObj must be initialized last.
    if (ER_OK == status) {
        status = alljoynDebugObj.Init();
    }
#endif
}

#ifndef NDEBUG
debug::AllJoynDebugObj* debug::AllJoynDebugObj::self = NULL;
#endif

}
