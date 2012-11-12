/**
 * @file
 * Sample implementation of an AllJoyn remote bus connect.
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
#include <qcc/Debug.h>
#include <qcc/Thread.h>

#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <vector>

#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Event.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/version.h>
#include <alljoyn/AllJoynStd.h>

#include <Status.h>

#define QCC_MODULE "ALLJOYN"

#define METHODCALL_TIMEOUT 30000
#define MAX_PINGS 100000

using namespace std;
using namespace qcc;
using namespace ajn;

/** Sample constants */
namespace com {
namespace quic {
namespace alljoyn_test {
const char* InterfaceName = "com.quic.alljoyn_test";
namespace values {
const char* InterfaceName = "com.quic.alljoyn_test.values";
}

}
}
}

qcc::String ConnectAddr;

/** AllJoynListener receives discovery events from AllJoyn */
class MyBusListener : public BusListener {
  public:
    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        QCC_SyncPrintf("NameOwnerChanged(%s, %s, %s)\n",
                       busName,
                       previousOwner ? previousOwner : "null",
                       newOwner ? newOwner : "null");
    }


};


/** Static data */
static MyBusListener g_busListener;
static BusAttachment* g_msgBus = NULL;

/* Usage  */
void usage()
{
    printf("\n\n  ./bb_codisco -C <connect bus addr>\n\n");
}

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


/** Main entry point */
int main(int argc, char** argv)
{
    QStatus status = ER_OK;
    Environ* env;

#ifdef _WIN32
    WSADATA wsaData;
    WORD version = MAKEWORD(2, 0);
    int error = WSAStartup(version, &wsaData);
#endif

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    if (argc < 2) {
        printf("\n Incorrect options ");
        usage();
        exit(1);
    }


    if (0 == strcmp("-C", argv[1])) {
        if (5 == argc) {
            printf("\n -C option to be suceeded by Object path ");
            usage();
            exit(1);
        } else
            /* Set the object path here */
            ConnectAddr.append(argv[2]);
    }



    /* Get env vars */
    env = Environ::GetAppEnviron();
#ifdef _WIN32
    qcc::String connectArgs = env->Find("BUS_ADDRESS", "tcp:addr=127.0.0.1,port=9955");
#else
    qcc::String connectArgs = env->Find("BUS_ADDRESS", "unix:abstract=alljoyn");
#endif

    /* Create message bus */
    g_msgBus = new BusAttachment("bb_remote_connect", true);

    /* Register a bus listener in order to get discovery indications */
    if (ER_OK == status) {
        g_msgBus->RegisterBusListener(g_busListener);
    }

    /* Start the msg bus */
    if (ER_OK == status) {
        status = g_msgBus->Start();
        if (ER_OK != status) {
            QCC_LogError(status, ("BusAttachment::Start failed"));
        }
    }

    /* Connect to the bus */
    if (ER_OK == status) {
        status = g_msgBus->Connect(connectArgs.c_str());
        if (ER_OK != status) {
            QCC_LogError(status, ("BusAttachment::Connect(\"%s\") failed", connectArgs.c_str()));
        }
    }

    for (int i = 0; i < 10000; i++) {
        /* Connecting to the remote bus with addr provided  */
        if (ER_OK == status) {
            Message reply(*g_msgBus);
            const ProxyBusObject& alljoynObj = g_msgBus->GetAllJoynProxyObj();
            MsgArg connectArg("s", ConnectAddr.c_str());
            status = alljoynObj.MethodCall(::org::alljoyn::Bus::InterfaceName, "Connect", &connectArg, 1, reply, 5000);
            if (ER_OK != status) {
                QCC_LogError(status, ("%s.Method call Connect failed"));
                printf("Method call Connect  to remote PBus instance failed ");
                return status;
            }
            if (MESSAGE_ERROR == reply->GetType()) {
                qcc::String errMsg;
                const char* errName = reply->GetErrorName(&errMsg);
                QCC_LogError(ER_FAIL, ("Received ERROR %s (%s) in response to org.alljoyn.Connect(%s)", errName, errMsg.c_str(), ConnectAddr.c_str()));
            } else {
                QCC_LogError(ER_OK, ("Received method return  %d", reply->GetType()));
            }

        }



        /* Disconencting from remote bus */
        if (ER_OK == status) {
            Message reply(*g_msgBus);
            const ProxyBusObject& alljoynObj = g_msgBus->GetAllJoynProxyObj();
            MsgArg connectArg("s", ConnectAddr.c_str());
            status = alljoynObj.MethodCall(::org::alljoyn::Bus::InterfaceName, "Disconnect", &connectArg, 1, reply, 5000);
            if (ER_OK != status) {
                QCC_LogError(status, ("%s.Method call Disconnect failed"));
                printf("Method call Disconnect to remote PBus instance failed ");
                return status;
            }
        }
    }


    /* Stop the bus */
    status = g_msgBus->Stop();
    if (ER_OK != status) {
        QCC_LogError(status, ("BusAttachment::Stop failed"));
    }


    /* Deallocate bus */
    BusAttachment* deleteMe = g_msgBus;
    g_msgBus = NULL;
    delete deleteMe;


    printf("Client exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int) status;
}
