/******************************************************************************
 * Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
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
 *
 ******************************************************************************/
#include <alljoyn/BusAttachment.h>
#include <Status.h>
#include <stdio.h>

#ifdef QCC_OS_GROUP_WINDOWS
# define __func__ __FUNCTION__
#endif

using namespace ajn;

bool g_verbose = false;

void Killomatic1(int repcount)
{
    if (g_verbose) {
        printf("%s, repcount = %d: ", __func__, repcount);
    }

    BusAttachment* msgBus = new BusAttachment("Killomatic", true);

    for (int i = 0; i < repcount; i++) {
        QStatus status = msgBus->Start();
        if (ER_OK != status) {
            printf("Start() failed with %s\n", QCC_StatusText(status));
            break;
        }

        status = msgBus->Stop();
        if (ER_OK != status) {
            printf("Stop() failed with %s\n", QCC_StatusText(status));
            break;
        }

        if (g_verbose) {
            printf(".");
            fflush(stdout);
        }
    }

    if (g_verbose) {
        printf("\n");
    }

    delete msgBus;
}

void Killomatic2(int repcount)
{
    if (g_verbose) {
        printf("%s, repcount = %d: ", __func__, repcount);
    }

    BusAttachment* msgBus = new BusAttachment("Killometer", true);

    for (int i = 0; i < repcount; i++) {
        QStatus status = msgBus->Start();
        if (ER_OK != status) {
            printf("Start() failed with %s\n", QCC_StatusText(status));
            break;
        }

        status = msgBus->Connect("unix:abstract=bluebayou");
        if (ER_OK != status) {
            printf("Connect() failed with %s\n", QCC_StatusText(status));
            break;
        }

        status = msgBus->Stop();
        if (ER_OK != status) {
            printf("Stop() failed with %s\n", QCC_StatusText(status));
            break;
        }

        if (g_verbose) {
            printf(".");
            fflush(stdout);
        }
    }

    if (g_verbose) {
        printf("\n");
    }

    delete msgBus;
}

int main(int argc, char** argv)
{
    int repcount = 1;
    int testnum = -1;

    for (int i = 0; i < argc; ++i) {
        if ((::strcmp("-r", argv[i]) == 0) && ((i + 1) < argc)) {
            repcount = atoi(argv[++i]);
        } else if ((::strcmp("-t", argv[i]) == 0) && ((i + 1) < argc)) {
            testnum = atoi(argv[++i]);
        } else if (::strcmp("v", argv[i]) == 0) {
            g_verbose = true;
        } else {
            printf("Unknown or missing arg for option %s\n", argv[1]);
            exit(1);
        }
    }

    switch (testnum) {
    case 1:
        Killomatic1(repcount);
        break;

    case 2:
        Killomatic2(repcount);
        break;

    default:
        Killomatic1(repcount);
        Killomatic2(repcount);
        break;
    }

    exit(0);
}


