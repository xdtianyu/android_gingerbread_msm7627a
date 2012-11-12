/**
 * @file
 * AllJoyn service that implements interfaces and members affected test.conf.
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
#include <qcc/platform.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/Debug.h>
#include <qcc/Environ.h>
#include <qcc/GUID.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/version.h>
#include <alljoyn/MsgArg.h>

#include <Status.h>


#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;
using namespace ajn;

/** Static top level message bus object */
static BusAttachment* g_msgBus = NULL;

/** Signal handler */
static void SigIntHandler(int sig)
{
    if (NULL != g_msgBus) {
        QStatus status = g_msgBus->Stop(false);
        if (ER_OK != status) {
            QCC_LogError(status, ("BusAttachment::Stop() failed"));
        }
    }
}

namespace org {
namespace alljoyn {
namespace DaemonTest {
const char* InterfaceName = "org.alljoyn.DaemonTestIfc";
const char* WellKnownName = "org.alljoyn.DaemonTest";
const char* ObjectPath = "/org/alljoyn/DaemonTest";
}
namespace DaemonTestDefaultDeny {
const char* InterfaceName = "org.alljoyn.DaemonTestDefaultDeny";
}
}
}

class LocalTestObject : public BusObject {
  public:

    LocalTestObject(BusAttachment& bus, const char* path, unsigned long reportInterval)
        : BusObject(bus, path),
        reportInterval(reportInterval)
    {
        QStatus status;

        /* Add the test interface to this object */
        const InterfaceDescription* regTestIntf = bus.GetInterface(::org::alljoyn::DaemonTest::InterfaceName);
        assert(regTestIntf);
        AddInterface(*regTestIntf);

        const InterfaceDescription* regTestDenyIntf = bus.GetInterface(::org::alljoyn::DaemonTestDefaultDeny::InterfaceName);
        assert(regTestDenyIntf);
        AddInterface(*regTestDenyIntf);

        /* Register the signal handler with the bus */
        const InterfaceDescription::Member* member = regTestIntf->GetMember("Signal");
        assert(member);
        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&LocalTestObject::SignalHandler),
                                           member,
                                           NULL);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to register signal handler"));
        }

        /* Register the method handlers with the object */
        const MethodEntry methodEntries[] = {
            { regTestIntf->GetMember("DefaultDenySend"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Method) },
            { regTestIntf->GetMember("DefaultDenyReceive"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Method) },
            { regTestIntf->GetMember("UserDenySend"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Method) },
            { regTestIntf->GetMember("DefaultAllowSend"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Method) },
            { regTestDenyIntf->GetMember("Send"), static_cast<MessageReceiver::MethodHandler>(&LocalTestObject::Method) }
        };
        status = AddMethodHandlers(methodEntries, ArraySize(methodEntries));
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to register method handlers for LocalTestObject"));
        }
    }

    void ObjectRegistered(void) {
        BusObject::ObjectRegistered();

        /* Request a well-known name */
        /* Note that you cannot make a blocking method call here */
        const ProxyBusObject& dbusObj = bus.GetDBusProxyObj();
        MsgArg args[2];
        args[0].Set("s", ::org::alljoyn::DaemonTest::WellKnownName);
        args[1].Set("u", 6);
        QStatus status = dbusObj.MethodCallAsync(::ajn::org::freedesktop::DBus::InterfaceName,
                                                 "RequestName",
                                                 this,
                                                 static_cast<MessageReceiver::ReplyHandler>(&LocalTestObject::NameAcquiredCB),
                                                 args,
                                                 ArraySize(args));
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to request name %s", ::org::alljoyn::DaemonTest::WellKnownName));
        }
    }

    void NameAcquiredCB(Message& msg, void* context)
    {
    }

    void SignalHandler(const InterfaceDescription::Member* member,
                       const char* sourcePath,
                       Message& msg)
    {
        map<qcc::String, size_t>::const_iterator it;

        ++rxCounts[qcc::String(sourcePath)];

        if ((rxCounts[qcc::String(sourcePath)] % reportInterval) == 0) {
            for (it = rxCounts.begin(); it != rxCounts.end(); ++it) {
                QCC_SyncPrintf("RxSignal: %s - %u\n", it->first.c_str(), it->second);
            }
        }
    }

    void Method(const InterfaceDescription::Member* member, Message& msg)
    {
        /* Reply with same string that was sent to us */
        printf("Method called: %s.%s\n", msg->GetInterface(), msg->GetMemberName());
        QStatus status = MethodReply(msg, ER_OK);
        if (ER_OK != status) {
            QCC_LogError(status, ("Ping: Error sending reply"));
        }
    }


    map<qcc::String, size_t> rxCounts;

    unsigned long signalDelay;
    unsigned long reportInterval;
};


static void usage(void)
{
    printf("Usage: DaemonTest [-i] [-h]\n\n");
    printf("Options:\n");
    printf("   -h   = Print this help message\n");
    printf("   -i # = Signal report interval (number of signals tx/rx per update; default = 1000)\n");
}



/** Main entry point */
int main(int argc, char** argv, char** envArg)
{
    QStatus status = ER_OK;
    unsigned long reportInterval = 1000;
    qcc::GUID guid;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp("-i", argv[i])) {
            ++i;
            if (i == argc) {
                printf("option %s requires a parameter\n", argv[i - 1]);
                usage();
                exit(1);
            } else {
                reportInterval = strtoul(argv[i], NULL, 10);
            }
        } else if (0 == strcmp("-h", argv[i])) {
            usage();
            exit(0);
        } else {
            status = ER_FAIL;
            printf("Unknown option %s\n", argv[i]);
            usage();
            exit(1);
        }
    }

    /* Get env vars */
    Environ* env = Environ::GetAppEnviron();
    qcc::String clientArgs = env->Find("DBUS_SESSION_BUS_ADDRESS", "unix:abstract=alljoyn");

    /* Create message bus */
    g_msgBus = new BusAttachment("DaemonTester");

    /* Add org.alljoyn.DaemonTest interface */
    InterfaceDescription* testIntf = NULL;
    status = g_msgBus->CreateInterface(::org::alljoyn::DaemonTest::InterfaceName, testIntf);
    if (ER_OK == status) {
        testIntf->AddSignal("Signal",             NULL,        NULL, 0);
        testIntf->AddMethod("DefaultDenySend",    NULL,  NULL, NULL, 0);
        testIntf->AddMethod("DefaultDenyReceive", NULL,  NULL, NULL, 0);
        testIntf->AddMethod("UserDenySend",       NULL,  NULL, NULL, 0);
        testIntf->AddMethod("DefaultAllowSend",   NULL,  NULL, NULL, 0);
        testIntf->Activate();
    } else {
        QCC_LogError(status, ("Failed to create interface %s", ::org::alljoyn::DaemonTest::InterfaceName));
    }

    status = g_msgBus->CreateInterface(::org::alljoyn::DaemonTestDefaultDeny::InterfaceName, testIntf);
    if (ER_OK == status) {
        testIntf->AddMember(MESSAGE_METHOD_CALL, "Send", NULL,  NULL, NULL, 0);
        testIntf->Activate();
    } else {
        QCC_LogError(status, ("Failed to create interface %s", ::org::alljoyn::DaemonTest::InterfaceName));
    }

    /* Start the msg bus */
    if (ER_OK == status) {
        status = g_msgBus->Start();
    } else {
        QCC_LogError(status, ("BusAttachment::Start failed"));
    }

    /* Register local objects and connect to the daemon */
    if (ER_OK == status) {
        LocalTestObject testObj(*g_msgBus, ::org::alljoyn::DaemonTest::ObjectPath, reportInterval);
        g_msgBus->RegisterBusObject(testObj);

        /* Connect to the daemon and wait for the bus to exit */
        status = g_msgBus->Connect(clientArgs.c_str());
        if (ER_OK == status) {
            g_msgBus->WaitStop();
        } else {
            QCC_LogError(status, ("Failed to connect to \"%s\"", clientArgs.c_str()));
        }
    }

    /* Clean up msg bus */
    BusAttachment* deleteMe = g_msgBus;
    g_msgBus = NULL;
    delete deleteMe;

    printf("%s exiting with status %d (%s)\n", argv[0], status, QCC_StatusText(status));

    return (int) status;
}
