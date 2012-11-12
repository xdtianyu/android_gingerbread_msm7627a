/**
 * @file
 * Message Bus Client
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

#include <signal.h>
#include <stdio.h>
#include <vector>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Util.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/version.h>

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


static void usage(void)
{
    printf("Usage: bbdisco [-h]\n\n");
    printf("Options:\n");
    printf("   -h   = Print help message\n");
}


/** Main entry point */
int main(int argc, char** argv)
{
    QStatus status = ER_OK;

#ifdef _WIN32
    WSADATA wsaData;
    WORD version = MAKEWORD(2, 0);
    int error = WSAStartup(version, &wsaData);
#endif

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Install SIGINT handler */
    signal(SIGINT, SigIntHandler);

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp("-h", argv[1])) {
            usage();
            exit(0);
        } else {
            status = ER_FAIL;
            printf("Unknown option %s\n", argv[i]);
            usage();
            exit(1);
        }
    }

    printf("This sample has been disabled\n");
    exit(1);

    /* Create message bus */
    g_msgBus = new BusAttachment("bbdisco");

    Environ* env = Environ::GetAppEnviron();
    qcc::String clientArgs = env->Find("BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket");
    // qcc::String clientArgs = env->Find("BUS_ADDRESS", "unix:abstract=alljoyn");

    /* Start the msg bus */
    status = g_msgBus->Start();
    if (ER_OK == status) {

        status = g_msgBus->Connect(clientArgs.c_str());
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to connect with \"%s\"", clientArgs.c_str()));
        }

        /*
         * Wait until bus is stopped
         */
        g_msgBus->WaitStop();

    } else {
        QCC_LogError(status, ("BusAttachment::Start failed"));
    }

    /* Clean up msg bus */
    if (g_msgBus) {
        BusAttachment* deleteMe = g_msgBus;
        g_msgBus = NULL;
        delete deleteMe;
    }

    return (int) status;
}
