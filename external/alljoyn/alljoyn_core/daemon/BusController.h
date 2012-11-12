/**
 * @file
 *
 * BusController is responsible for responding to standard DBus and Bus
 * specific messages directed at the bus itself.
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

#ifndef _ALLJOYN_BUSCONTROLLER_H
#define _ALLJOYN_BUSCONTROLLER_H

#include <qcc/platform.h>

#include <alljoyn/MsgArg.h>

#include "Bus.h"
#include "DBusObj.h"
#include "AllJoynObj.h"
#ifndef NDEBUG
#include "AllJoynDebugObj.h"
#endif

namespace ajn {

/**
 * BusController is responsible for responding to DBus and AllJoyn
 * specific messages directed at the bus itself.
 */
class BusController {
  public:

    /**
     * Constructor
     *
     * @param bus        Bus to associate with this controller.
     * @param status     ER_OK if controller was successfully initialized
     */
    BusController(Bus& bus, QStatus& status);

    /**
     * Return the daemon bus object responsible for org.alljoyn.Bus.
     *
     * @return The AllJoynObj.
     */
    AllJoynObj& GetAllJoynObj() { return alljoynObj; }

  private:

#ifndef NDEBUG
    /** BusObject responsible for org.alljoyn.Debug */
    debug::AllJoynDebugObj alljoynDebugObj;
#endif

    /** Bus object responsible for org.freedesktop.DBus */
    DBusObj dbusObj;

    /** BusObject responsible for org.alljoyn.Bus */
    AllJoynObj alljoynObj;

};

}

#endif
