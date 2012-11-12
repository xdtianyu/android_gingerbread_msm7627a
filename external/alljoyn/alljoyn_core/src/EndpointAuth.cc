/**
 * @file
 * EndpointAuth is a utility class responsible for adding authentication
 * to BusEndpoint implementations.
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

#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Debug.h>
#include <qcc/Util.h>

#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/Message.h>

#include "RemoteEndpoint.h"
#include "EndpointAuth.h"
#include "BusUtil.h"
#include "SASLEngine.h"
#include "BusInternal.h"


#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {


static const uint32_t HELLO_RESPONSE_TIMEOUT = 5000;


QStatus EndpointAuth::Hello()
{
    QStatus status;
    Message hello(bus);
    Message response(bus);
    uint32_t serial;

    status = hello->HelloMessage(endpoint.features.isBusToBus, endpoint.features.allowRemote, serial);
    if (status != ER_OK) {
        return status;
    }
    /*
     * Send the hello message and wait for a response
     */
    status = hello->Deliver(endpoint);
    if (status != ER_OK) {
        return status;
    }

    status = response->Unmarshal(endpoint, false, true, HELLO_RESPONSE_TIMEOUT);
    if (status != ER_OK) {
        return status;
    }
    if (response->GetType() == MESSAGE_ERROR) {
        qcc::String msg;
        QCC_DbgPrintf(("error: %s", response->GetErrorName(&msg)));
        QCC_DbgPrintf(("%s", msg.c_str()));
        return ER_BUS_ESTABLISH_FAILED;
    }
    if (response->GetType() != MESSAGE_METHOD_RET) {
        return ER_BUS_ESTABLISH_FAILED;
    }
    if (response->GetReplySerial() != serial) {
        return ER_BUS_UNKNOWN_SERIAL;
    }
    /*
     * Remote name for the endpoint is the sender of the reply.
     */
    remoteName = response->GetSender();
    QCC_DbgHLPrintf(("EP remote %sname %s", endpoint.features.isBusToBus ? "(bus-to-bus) " : "", remoteName.c_str()));

    /*
     * bus-to-bus establishment uses an extended "hello" method.
     */
    if (endpoint.features.isBusToBus) {
        status = response->UnmarshalArgs("ssu");
        if (ER_OK == status) {
            uniqueName = response->GetArg(0)->v_string.str;
            remoteGUID = qcc::GUID(response->GetArg(1)->v_string.str);
            remoteProtocolVersion = response->GetArg(2)->v_uint32;
            if (remoteGUID == bus.GetInternal().GetGlobalGUID()) {
                QCC_DbgPrintf(("BusHello was sent to self"));
                return ER_BUS_SELF_CONNECT;
            } else {
                QCC_DbgPrintf(("Connection id: \"%s\", remoteGUID: \"%s\"\n", uniqueName.c_str(), remoteGUID.ToString().c_str()));
            }
        } else {
            return status;
        }
    } else {
        status = response->UnmarshalArgs("s");
        uniqueName = response->GetArg(0)->v_string.str;
        QCC_DbgPrintf(("Connection id: %s\n", response->GetArg(0)->v_string.str));
        if (status != ER_OK) {
            return status;
        }
    }
    /*
     * Validate the unique name
     */
    if (!IsLegalUniqueName(uniqueName.c_str())) {
        status = ER_BUS_BAD_BUS_NAME;
    }
    return status;
}


QStatus EndpointAuth::WaitHello()
{
    QStatus status;
    Message hello(bus);

    status = hello->Unmarshal(endpoint, false);
    if (ER_OK == status) {
        if (hello->GetType() != MESSAGE_METHOD_CALL) {
            QCC_DbgPrintf(("First message must be Hello/BusHello method call"));
            return ER_BUS_ESTABLISH_FAILED;
        }
        if (strcmp(hello->GetInterface(), org::freedesktop::DBus::InterfaceName) == 0) {
            if (hello->GetCallSerial() == 0) {
                QCC_DbgPrintf(("Hello expected non-zero serial"));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetDestination(), org::freedesktop::DBus::WellKnownName) != 0) {
                QCC_DbgPrintf(("Hello expected destination \"%s\"", org::freedesktop::DBus::WellKnownName));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetObjectPath(), org::freedesktop::DBus::ObjectPath) != 0) {
                QCC_DbgPrintf(("Hello expected object path \"%s\"", org::freedesktop::DBus::ObjectPath));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetMemberName(), "Hello") != 0) {
                QCC_DbgPrintf(("Hello expected member \"Hello\""));
                return ER_BUS_ESTABLISH_FAILED;
            }
            endpoint.features.isBusToBus = false;
            endpoint.features.allowRemote = (0 != (hello->GetFlags() & ALLJOYN_FLAG_ALLOW_REMOTE_MSG));
            /*
             * Remote name for the endpoint is the unique name we are allocating.
             */
            remoteName = uniqueName;
        } else if (strcmp(hello->GetInterface(), org::alljoyn::Bus::InterfaceName) == 0) {
            if (hello->GetCallSerial() == 0) {
                QCC_DbgPrintf(("Hello expected non-zero serial"));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetDestination(), org::alljoyn::Bus::WellKnownName) != 0) {
                QCC_DbgPrintf(("Hello expected destination \"%s\"", org::alljoyn::Bus::WellKnownName));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetObjectPath(), org::alljoyn::Bus::ObjectPath) != 0) {
                QCC_DbgPrintf(("Hello expected object path \"%s\"", org::alljoyn::Bus::ObjectPath));
                return ER_BUS_ESTABLISH_FAILED;
            }
            if (strcmp(hello->GetMemberName(), "BusHello") != 0) {
                QCC_DbgPrintf(("Hello expected member \"BusHello\""));
                return ER_BUS_ESTABLISH_FAILED;
            }
            size_t numArgs;
            const MsgArg* args;
            status = hello->UnmarshalArgs("su");
            hello->GetArgs(numArgs, args);
            if ((ER_OK == status) && (2 == numArgs) && (ALLJOYN_STRING == args[0].typeId) && (ALLJOYN_UINT32 == args[1].typeId)) {
                remoteGUID = qcc::GUID(args[0].v_string.str);
                remoteProtocolVersion = args[1].v_uint32;
                if (remoteGUID == bus.GetInternal().GetGlobalGUID()) {
                    QCC_DbgPrintf(("BusHello was sent by self"));
                    return ER_BUS_SELF_CONNECT;
                }
            } else {
                QCC_DbgPrintf(("BusHello expected 2 args with signature \"su\""));
                return ER_BUS_ESTABLISH_FAILED;
            }
            endpoint.features.isBusToBus = true;
            endpoint.features.allowRemote = true;
            /*
             * Remote name for the endpoint is the sender of the hello.
             */
            remoteName = hello->GetSender();
        } else {
            QCC_DbgPrintf(("Hello expected interface \"%s\" or \"%s\"", org::freedesktop::DBus::InterfaceName,
                           org::alljoyn::Bus::InterfaceName));
            return ER_BUS_ESTABLISH_FAILED;
        }
        QCC_DbgHLPrintf(("EP remote %sname %s", endpoint.features.isBusToBus ? "(bus-to-bus) " : "", remoteName.c_str()));
        status = hello->HelloReply(endpoint.features.isBusToBus, uniqueName);
    }
    if (ER_OK == status) {
        status = hello->Deliver(endpoint);
    }
    return status;
}


static const char NegotiateUnixFd[] = "NEGOTIATE_UNIX_FD";
static const char AgreeUnixFd[] = "AGREE_UNIX_FD";

qcc::String EndpointAuth::SASLCallout(SASLEngine& sasl, const qcc::String& extCmd)
{
    qcc::String rsp;

    if (sasl.GetRole() == AuthMechanism::RESPONDER) {
        if (extCmd.empty() && endpoint.features.handlePassing) {
            rsp = NegotiateUnixFd;
#ifdef QCC_OS_WINDOWS
            rsp += " " + qcc::U32ToString(qcc::GetPid());
#endif
            endpoint.features.handlePassing = false;
        } else if (extCmd.find(AgreeUnixFd) == 0) {
            endpoint.features.handlePassing = true;
            endpoint.processId = qcc::StringToU32(extCmd.substr(sizeof(AgreeUnixFd) - 1), 0, -1);
        }
    } else {
        if (extCmd.find(NegotiateUnixFd) == 0) {
            rsp = AgreeUnixFd;
#ifdef QCC_OS_WINDOWS
            rsp += " " + qcc::U32ToString(qcc::GetPid());
#endif
            endpoint.features.handlePassing = true;
            endpoint.processId = qcc::StringToU32(extCmd.substr(sizeof(NegotiateUnixFd) - 1), 0, -1);
        }
    }
    return rsp;
}

QStatus EndpointAuth::Establish(const qcc::String& authMechanisms,
                                qcc::String& authUsed)
{
    QStatus status = ER_OK;
    size_t numPushed;
    SASLEngine::AuthState state;
    qcc::String inStr;
    qcc::String outStr;

    QCC_DbgPrintf(("EndpointAuth::Establish authMechanisms=\"%s\"", authMechanisms.c_str()));

    if (isAccepting) {
        SASLEngine sasl(bus, AuthMechanism::CHALLENGER, authMechanisms, NULL, NULL, this);
        /*
         * The server's GUID is sent to the client when the authentication succeeds
         */
        String guidStr = bus.GetInternal().GetGlobalGUID().ToString();
        sasl.SetLocalId(guidStr);
        while (true) {
            /*
             * Get the challenge
             */
            inStr.clear();
            status = endpoint.GetSource().GetLine(inStr);
            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to read from stream"));
                goto ExitEstablish;
            }
            status = sasl.Advance(inStr, outStr, state);
            if (status != ER_OK) {
                QCC_DbgPrintf(("Server authentication failed %s", QCC_StatusText(status)));
                goto ExitEstablish;
            }
            if (state == SASLEngine::ALLJOYN_AUTH_SUCCESS) {
                /*
                 * Remember the authentication mechanism that was used
                 */
                authUsed = sasl.GetMechanism();
                break;
            }
            /*
             * Send the response
             */
            status = endpoint.GetSink().PushBytes((void*)(outStr.data()), outStr.length(), numPushed);
            if (status == ER_OK) {
                QCC_DbgPrintf(("Sent %s", outStr.c_str()));
            } else {
                QCC_LogError(status, ("Failed to write to stream"));
                goto ExitEstablish;
            }
        }
        /*
         * Wait for the hello message
         */
        status = WaitHello();
    } else {
        SASLEngine sasl(bus, AuthMechanism::RESPONDER, authMechanisms, NULL, NULL, endpoint.features.isBusToBus ? NULL : this);
        while (true) {
            status = sasl.Advance(inStr, outStr, state);
            if (status != ER_OK) {
                QCC_DbgPrintf(("Client authentication failed %s", QCC_StatusText(status)));
                goto ExitEstablish;
            }
            /*
             * Send the response
             */
            status = endpoint.GetSink().PushBytes((void*)(outStr.data()), outStr.length(), numPushed);
            if (status == ER_OK) {
                QCC_DbgPrintf(("Sent %s", outStr.c_str()));
            } else {
                QCC_LogError(status, ("Failed to write to stream"));
                goto ExitEstablish;
            }
            if (state == SASLEngine::ALLJOYN_AUTH_SUCCESS) {
                /*
                 * Get the server's GUID
                 */
                qcc::String id = sasl.GetRemoteId();
                if (!qcc::GUID::IsGUID(id)) {
                    QCC_DbgPrintf(("Expected GUID got: %s", id.c_str()));
                    status = ER_BUS_ESTABLISH_FAILED;
                    goto ExitEstablish;
                }
                remoteGUID = qcc::GUID(id);
                /*
                 * Remember the authentication mechanism that was used
                 */
                authUsed = sasl.GetMechanism();
                break;
            }
            /*
             * Get the challenge
             */
            inStr.clear();
            status = endpoint.GetSource().GetLine(inStr);
            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to read from stream"));
                goto ExitEstablish;
            }
        }
        /*
         * Send the hello message and wait for a response
         */
        status = Hello();
    }

ExitEstablish:


    QCC_DbgPrintf(("Establish complete %s", QCC_StatusText(status)));

    return status;
}

}
