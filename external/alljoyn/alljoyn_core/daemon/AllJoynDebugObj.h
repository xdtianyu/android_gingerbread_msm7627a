/**
 * @file
 * BusObject responsible for implementing the AllJoyn methods (org.alljoyn.Debug)
 * for messages controlling debug output.
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
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
#ifndef _ALLJOYN_ALLJOYNDEBUGOBJ_H
#define _ALLJOYN_ALLJOYNDEBUGOBJ_H

// Include contents in debug builds only.
#ifndef NDEBUG

#include <qcc/platform.h>

#include <assert.h>

#include <map>

#include <qcc/Log.h>
#include <qcc/String.h>
#include <qcc/StringMapKey.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusObject.h>

#include "Bus.h"

namespace ajn {

namespace debug {

class AllJoynDebugObjAddon {
  public:
    virtual ~AllJoynDebugObjAddon() { }

    typedef QStatus (AllJoynDebugObjAddon::* MethodHandler)(Message& message, std::vector<MsgArg>& replyArgs);
};


/**
 * BusObject responsible for implementing the AllJoyn methods at org.alljoyn.Debug
 * for messages controlling debug output.
 *
 * @cond ALLJOYN_DEV
 *
 * This is implemented entirely in the header file for the following reasons:
 *
 * - It is instantiated by the BusController in debug builds only.
 * - It is easily excluded from release builds by conditionally including it.
 *
 * @endcond
 */
class AllJoynDebugObj : public BusObject {

    friend class ajn::BusController;  // Only the bus controller can instantiate us.

  public:
    class Properties {
      public:
        struct Info {
            const char* name;
            const char* signature;
            uint8_t access;
        };

        Properties() { }
        virtual ~Properties() { }
        virtual QStatus Get(const char* propName, MsgArg& val) const { return ER_BUS_NO_SUCH_PROPERTY; }
        virtual QStatus Set(const char* propName, MsgArg& val) { return ER_BUS_NO_SUCH_PROPERTY; }
        virtual void GetProperyInfo(const Info*& info, size_t& infoSize) = 0;
    };

    struct MethodInfo {
        const char* name;
        const char* inputSig;
        const char* outSig;
        const char* argNames;
        AllJoynDebugObjAddon::MethodHandler handler;
    };

  private:
    typedef std::map<qcc::StringMapKey, Properties*> PropertyStore;
    typedef std::pair<AllJoynDebugObjAddon*, AllJoynDebugObjAddon::MethodHandler> AddonMethodHandler;
    typedef std::map<const InterfaceDescription::Member*, AddonMethodHandler> AddonMethodHandlerMap;


  public:

    static AllJoynDebugObj* GetAllJoynDebugObj()
    {
        // This object is a quasi-singleton.  It gets instantiated and
        // destroyed by the BusController object.  Attempts to get a pointer
        // when there is no BusController is invalid.
        assert(self && "Invalid access to AllJoynDebugObj; read the comments");
        return self;
    }

    /**
     * Initialize and register this DBusObj instance.
     *
     * @return ER_OK if successful.
     */
    QStatus Init()
    {
        QStatus status;

        /* Make this object implement org.alljoyn.Bus */
        const InterfaceDescription* alljoynDbgIntf = bus.GetInterface(org::alljoyn::Daemon::Debug::InterfaceName);
        if (!alljoynDbgIntf) {
            status = ER_BUS_NO_SUCH_INTERFACE;
            return status;
        }

        status = AddInterface(*alljoynDbgIntf);
        if (status == ER_OK) {
            /* Hook up the methods to their handlers */
            const MethodEntry methodEntries[] = {
                { alljoynDbgIntf->GetMember("SetDebugLevel"),
                  static_cast<MessageReceiver::MethodHandler>(&AllJoynDebugObj::SetDebugLevel) },
            };

            status = AddMethodHandlers(methodEntries, ArraySize(methodEntries));

            if (status == ER_OK) {
                status = bus.RegisterBusObject(*this);
            }
        }
        return status;
    }


    QStatus AddDebugInterface(AllJoynDebugObjAddon* addon,
                              const char* ifaceName,
                              const MethodInfo* methodInfo,
                              size_t methodInfoSize,
                              Properties& ifaceProperties)
    {
        assert(addon);
        assert(ifaceName);

        InterfaceDescription* ifc;
        MethodEntry* methodEntries = methodInfoSize ? new MethodEntry[methodInfoSize] : NULL;
        qcc::String ifaceNameStr = ifaceName;

        bus.UnregisterBusObject(*this);

        QStatus status = bus.CreateInterface(ifaceName, ifc);
        if (status != ER_OK) goto exit;

        for (size_t i = 0; i < methodInfoSize; ++i) {
            ifc->AddMember(MESSAGE_METHOD_CALL,
                           methodInfo[i].name,
                           methodInfo[i].inputSig,
                           methodInfo[i].outSig,
                           methodInfo[i].argNames,
                           0);
            const InterfaceDescription::Member* member = ifc->GetMember(methodInfo[i].name);
            methodEntries[i].member = member;
            methodEntries[i].handler = static_cast<MessageReceiver::MethodHandler>(&AllJoynDebugObj::GenericMethodHandler);
            methodHandlerMap[member] = AddonMethodHandler(addon, methodInfo[i].handler);
        }

        const AllJoynDebugObj::Properties::Info* propInfo;
        size_t propInfoSize;

        ifaceProperties.GetProperyInfo(propInfo, propInfoSize);
        for (size_t i = 0; i < propInfoSize; ++i) {
            ifc->AddProperty(propInfo[i].name, propInfo[i].signature, propInfo[i].access);
        }

        ifc->Activate();

        status = AddInterface(*ifc);
        if (status != ER_OK) goto exit;

        if (methodEntries) {
            status = AddMethodHandlers(methodEntries, methodInfoSize);
            if (status != ER_OK) goto exit;
        }

        properties.insert(std::pair<qcc::StringMapKey, Properties*>(ifaceNameStr, &ifaceProperties));

    exit:
        bus.RegisterBusObject(*this);
        if (methodEntries) delete[] methodEntries;
        return status;
    }


    QStatus Get(const char* ifcName, const char* propName, MsgArg& val)
    {
        PropertyStore::const_iterator it = properties.find(ifcName);
        if (it == properties.end()) {
            return ER_BUS_NO_SUCH_PROPERTY;
        }
        return it->second->Get(propName, val);
    }


    QStatus Set(const char* ifcName, const char* propName, MsgArg& val)
    {
        PropertyStore::const_iterator it = properties.find(propName);
        if (it == properties.end()) {
            return ER_BUS_NO_SUCH_PROPERTY;
        }
        return it->second->Set(propName, val);
    }


    void GetProp(const InterfaceDescription::Member* member, Message& msg)
    {
        const qcc::String guid(bus.GetInternal().GetGlobalGUID().ToShortString());
        qcc::String sender(msg->GetSender());
        // Only allow local connections to get properties
        if (sender.substr(1, guid.size()) == guid) {
            BusObject::GetProp(member, msg);
        } // else someone off-device is trying to set our debug output, punish them by not responding.
    }


  private:

    /**
     * Constructor
     *
     * @param bus        Bus to associate with org.freedesktop.DBus message handler.
     */
    AllJoynDebugObj(Bus& bus) : BusObject(bus, org::alljoyn::Daemon::Debug::ObjectPath) { self = this; }

    /**
     * Destructor
     */
    ~AllJoynDebugObj() { self = NULL; }

    /**
     * Handles the SetDebugLevel method call.
     *
     * @param member    Member
     * @param msg       The incoming message
     */
    void SetDebugLevel(const InterfaceDescription::Member* member, Message& msg)
    {
        const qcc::String guid(bus.GetInternal().GetGlobalGUID().ToShortString());
        qcc::String sender(msg->GetSender());
        // Only allow local connections to set the debug level
        if (sender.substr(1, guid.size()) == guid) {
            const char* module;
            uint32_t level;
            QStatus status = msg->GetArgs("su", &module, &level);
            if (status == ER_OK) {
                QCC_SetDebugLevel(module, level);
                MethodReply(msg, (MsgArg*)NULL, 0);
            } else {
                MethodReply(msg, "org.alljoyn.Debug.InternalError", QCC_StatusText(status));
            }
        } // else someone off-device is trying to set our debug output, punish them by not responding.
    }


    void GenericMethodHandler(const InterfaceDescription::Member* member, Message& msg)
    {
        AddonMethodHandlerMap::iterator it = methodHandlerMap.find(member);
        if (it != methodHandlerMap.end()) {
            // Call the addon's method handler
            std::vector<MsgArg> replyArgs;
            QStatus status = (it->second.first->*it->second.second)(msg, replyArgs);

            if (status == ER_OK) {
                MethodReply(msg, &replyArgs.front(), replyArgs.size());
            } else {
                MethodReply(msg, "org.alljoyn.Debug.InternalError", "Failure processing method call");
            }
        } else {
            MethodReply(msg, "org.alljoyn.Debug.InternalError", "Unknown method");
        }
    }


    PropertyStore properties;

    AddonMethodHandlerMap methodHandlerMap;

    static AllJoynDebugObj* self;
};


} // namespace debug
} // namespace ajn

#endif
#endif
