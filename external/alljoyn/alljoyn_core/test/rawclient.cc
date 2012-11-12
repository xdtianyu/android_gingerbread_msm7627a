/**
 * @file
 * Sample implementation of an AllJoyn client the uses raw sockets.
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
#include <errno.h>

#include <qcc/Environ.h>
#include <qcc/Event.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>
#include <qcc/time.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/version.h>

#include <Status.h>

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;
using namespace ajn;

/** Sample constants */
static const char* INTERFACE_NAME = "org.alljoyn.raw_test";
static const SessionPort SESSION_PORT = 33;

/** Static data */
static BusAttachment* g_msgBus = NULL;
static Event g_discoverEvent;
static String g_wellKnownName = "org.alljoyn.raw_test";

/** AllJoynListener receives discovery events from AllJoyn */
class MyBusListener : public BusListener {
  public:

    MyBusListener() : BusListener(), sessionId(0) { }

    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        QCC_SyncPrintf("FoundAdvertisedName(name=%s, transport=0x%x, prefix=%s)\n", name, transport, namePrefix);

        if (0 == strcmp(name, g_wellKnownName.c_str())) {
            /* We found a remote bus that is advertising bbservice's well-known name so connect to it */
            SessionOpts opts(SessionOpts::TRAFFIC_RAW_RELIABLE, false, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
            QStatus status = g_msgBus->JoinSession(name, SESSION_PORT, NULL, sessionId, opts);
            if (ER_OK != status) {
                QCC_LogError(status, ("JoinSession(%s) failed", name));
            } else {
                /* Release the main thread */
                g_discoverEvent.SetEvent();
            }
        }
    }

    void LostAdvertisedName(const char* name, TransportMask transport, const char* prefix)
    {
        QCC_SyncPrintf("LostAdvertisedName(name=%s, transport=0x%x, prefix=%s)\n", name, transport, prefix);
    }

    void NameOwnerChanged(const char* name, const char* previousOwner, const char* newOwner)
    {
        QCC_SyncPrintf("NameOwnerChanged(%s, %s, %s)\n",
                       name,
                       previousOwner ? previousOwner : "null",
                       newOwner ? newOwner : "null");
    }

    SessionId GetSessionId() const { return sessionId; }

  private:
    SessionId sessionId;
};

/** Static bus listener */
static MyBusListener g_busListener;

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

static void usage(void)
{
    printf("Usage: rawclient [-h] [-n <well-known name>]\n\n");
    printf("Options:\n");
    printf("   -h                    = Print this help message\n");
    printf("   -n <well-known name>  = Well-known bus name advertised by bbservice\n");
    printf("\n");
}

/** Main entry point */
int main(int argc, char** argv)
{
    QStatus status = ER_OK;
    Environ* env;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp("-n", argv[i])) {
            ++i;
            if (i == argc) {
                printf("option %s requires a parameter\n", argv[i - 1]);
                usage();
                exit(1);
            } else {
                g_wellKnownName = argv[i];
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
    env = Environ::GetAppEnviron();
#ifdef _WIN32
    qcc::String connectArgs = env->Find("BUS_ADDRESS", "tcp:addr=127.0.0.1,port=9955");
#else
    // qcc::String connectArgs = env->Find("BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket");
    qcc::String connectArgs = env->Find("BUS_ADDRESS", "unix:abstract=alljoyn");
#endif

    /* Create message bus */
    g_msgBus = new BusAttachment("rawclient", true);

    /* Register a bus listener in order to get discovery indications */
    g_msgBus->RegisterBusListener(g_busListener);

    /* Start the msg bus */
    status = g_msgBus->Start();
    if (ER_OK != status) {
        QCC_LogError(status, ("BusAttachment::Start failed"));
    }

    /* Connect to the bus */
    if (ER_OK == status) {
        status = g_msgBus->Connect(connectArgs.c_str());
        if (ER_OK != status) {
            QCC_LogError(status, ("BusAttachment::Connect(\"%s\") failed", connectArgs.c_str()));
        }
    }

    /* Begin discovery for the well-known name of the service */
    if (ER_OK == status) {
        status = g_msgBus->FindAdvertisedName(g_wellKnownName.c_str());
        if (status != ER_OK) {
            QCC_LogError(status, ("%s.FindAdvertisedName failed", INTERFACE_NAME));
        }
    }

    /* Wait for the "FoundAdvertisedName" signal */
    if (ER_OK == status) {
        status = Event::Wait(g_discoverEvent);
    }

    /* Check the session */
    SessionId ssId = g_busListener.GetSessionId();
    if (ssId == 0) {
        status = ER_FAIL;
        QCC_LogError(status, ("Raw session id is invalid"));
    } else {
        /* Get the descriptor */
        SocketFd sockFd;
        QStatus status = g_msgBus->GetSessionFd(ssId, sockFd);
        if (status == ER_OK) {
            /* Attempt to read test string from fd */
            char buf[256];
            size_t recvd;
            while ((status == ER_OK) || (status == ER_WOULDBLOCK)) {
                status = qcc::Recv(sockFd, buf, sizeof(buf) - 1, recvd);
                if (status == ER_OK) {
                    QCC_SyncPrintf("Read %d bytes from fd\n", recvd);
                    buf[recvd] = '\0';
                    QCC_SyncPrintf("Bytes: %s\n", buf);
                    break;
                } else if (status == ER_WOULDBLOCK) {
                    qcc::Sleep(200);
                } else {
                    QCC_LogError(status, ("Read from raw fd failed"));
                }
            }
        } else {
            QCC_LogError(status, ("GetSessionFd failed"));
        }
    }

    /* Stop the bus */
    delete g_msgBus;

    printf("rawclient exiting with status %d (%s)\n", status, QCC_StatusText(status));

    return (int) status;
}
