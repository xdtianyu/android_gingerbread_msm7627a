/**
 * @file
 * AllJoyn-Daemon - Windows version
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
#include <qcc/String.h>
#include <qcc/StringUtil.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <vector>

#include <qcc/Logger.h>
#include <qcc/Environ.h>
#include <qcc/StringSource.h>
#include <qcc/Util.h>
#include <qcc/FileStream.h>

#include <alljoyn/version.h>

#include <Status.h>

#include "Transport.h"
#include "DaemonTCPTransport.h"
#include "Bus.h"
#include "BusController.h"
#include "ConfigDB.h"
#include "BusInternal.h"

#define DAEMONLIBRARY_EXPORTS
#include "DaemonLib.h"
#include <share.h>

#define DAEMON_EXIT_OK            0
#define DAEMON_EXIT_OPTION_ERROR  1
#define DAEMON_EXIT_CONFIG_ERROR  2
#define DAEMON_EXIT_STARTUP_ERROR 3
#define DAEMON_EXIT_FORK_ERROR    4
#define DAEMON_EXIT_IO_ERROR      5
#define DAEMON_EXIT_SESSION_ERROR 6

using namespace ajn;
using namespace qcc;
using namespace std;

static const char defaultConfig[] =
    "<busconfig>"
    "  <type>alljoyn</type>"
    "  <listen>tcp:addr=0.0.0.0,port=9955</listen>"
    //"  <listen>bluetooth:</listen>"
    "  <policy context=\"default\">"
    "    <!-- Allow everything to be sent -->"
    "    <allow send_destination=\"*\" eavesdrop=\"true\"/>"
    "    <!-- Allow everything to be received -->"
    "    <allow eavesdrop=\"true\"/>"
    "    <!-- Allow anyone to own anything -->"
    "    <allow own=\"*\"/>"
    "  </policy>"
    "  <limit name=\"auth_timeout\">32768</limit>"
    "  <limit name=\"max_incomplete_connections_tcp\">16</limit>"
    "  <limit name=\"max_completed_connections_tcp\">64</limit>"
    "  <alljoyn module=\"ipns\">"
    "    <property interfaces=\"*\"/>"
    "  </alljoyn>"
    "</busconfig>";

BusAttachment* g_ajBus = NULL;

void SignalHandler(int signal)
{
    if (g_ajBus) {
        Log(LOG_INFO, "Terminating.\n");
        g_ajBus->Stop();
        g_ajBus = NULL;
    }
}

class OptParse {
  public:
    enum ParseResultCode {
        PR_OK,
        PR_EXIT_NO_ERROR,
        PR_OPTION_CONFLICT,
        PR_INVALID_OPTION,
        PR_MISSING_OPTION
    };

    OptParse(int argc, char** argv) :
        argc(argc),
        argv(argv),
        useDefaultConfig(true),
        noBT(false),
        printAddress(false),
        verbosity(LOG_WARNING)
    { configFile.clear(); }

    ParseResultCode ParseResult();

    qcc::String GetConfigFile() const { return configFile; }
    bool UseDefaultConfig() const { return useDefaultConfig; }
    bool PrintAddress() const { return printAddress; }
    int GetVerbosity() const { return verbosity; }
    bool GetNoBT() const { return noBT; }

  private:
    int argc;
    char** argv;

    qcc::String configFile;
    bool useDefaultConfig;
    bool noBT;
    bool printAddress;
    int verbosity;

    void PrintUsage();
};


void OptParse::PrintUsage()
{
    printf("%s [--config-file=FILE] [--print-address] [--verbosity=LEVEL] [--no-bt] [--version]\n\n"
           "    --config-file=FILE\n"
           "        Use the specified configuration file.\n\n"
           "    --print-address\n"
           "        Print the socket address to STDOUT\n\n"
           "    --no-bt\n"
           "        Disable the Bluetooth transport (override config file setting).\n\n"
           "    --verbosity=LEVEL\n"
           "        Set the logging level to LEVEL.\n\n"
           "    --version\n"
           "        Print the version and copyright string, and exit.\n",
           argv[0]);
}


OptParse::ParseResultCode OptParse::ParseResult()
{
    ParseResultCode result(PR_OK);
    int i;

    for (i = 1; i < argc; ++i) {
        qcc::String arg(argv[i]);

        if (arg.compare("--version") == 0) {
            printf("AllJoyn Message Bus Daemon version: %s\n"
                   "Copyright (c) 2009-2011 Qualcomm Innovation Center, Inc.\n"
                   "Licensed under Apache2.0: http://www.apache.org/licenses/LICENSE-2.0.html\n"
                   "\n"
                   "Build: %s\n", ajn::GetVersion(), GetBuildInfo());
            result = PR_EXIT_NO_ERROR;
            goto exit;
        } else if (arg.compare("--config-file") == 0) {
            if (!configFile.empty()) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            ++i;
            if (i == argc) {
                result = PR_MISSING_OPTION;
                goto exit;
            }
            configFile = argv[i];
            useDefaultConfig = false;
        } else if (arg.compare(0, sizeof("--config-file") - 1, "--config-file") == 0) {
            if (!configFile.empty()) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            configFile = arg.substr(sizeof("--config-file"));
            useDefaultConfig = false;
        } else if (arg.compare("--print-address") == 0) {
            printAddress = true;
        } else if (arg.compare("--no-bt") == 0) {
            noBT = true;
        } else if (arg.substr(0, sizeof("--verbosity") - 1).compare("--verbosity") == 0) {
            verbosity = StringToI32(arg.substr(sizeof("--verbosity")));
        } else if ((arg.compare("--help") == 0) || (arg.compare("-h") == 0)) {
            PrintUsage();
            result = PR_EXIT_NO_ERROR;
            goto exit;
        } else {
            result = PR_INVALID_OPTION;
            goto exit;
        }
    }

exit:
    switch (result) {
    case PR_OPTION_CONFLICT:
        fprintf(stderr, "Option \"%s\" is in conflict with a previous option.\n", argv[i]);
        break;

    case PR_INVALID_OPTION:
        fprintf(stderr, "Invalid option: \"%s\"\n", argv[i]);
        break;

    case PR_MISSING_OPTION:
        fprintf(stderr, "No config file specified.\n");
        PrintUsage();
        break;

    default:
        break;
    }
    return result;
}

int daemon(OptParse& opts)
{
    ConfigDB* config(ConfigDB::GetConfigDB());

    signal(SIGTERM, SignalHandler);
    signal(SIGINT, SignalHandler);

    const ConfigDB::ListenList& listenList = config->GetListen();
    ConfigDB::ListenList::const_iterator it = listenList.begin();
    qcc::String listenSpecs;
    bool skip = false;

    while (it != listenList.end()) {
        qcc::String addrStr(*it);
        if (it->compare(0, sizeof("tcp:") - 1, "tcp:") == 0) {
            // No special processing needed for TCP.
        } else if (it->compare("bluetooth:") == 0) {
            skip = opts.GetNoBT();
        } else {
            Log(LOG_ERR, "Unsupported listen address: %s (ignoring)\n", it->c_str());
            ++it;
            continue;
        }

        if (skip) {
            Log(LOG_INFO, "Skipping transport for address: %s\n", addrStr.c_str());
        } else {
            Log(LOG_INFO, "Setting up transport for address: %s\n", addrStr.c_str());
            if (!listenSpecs.empty()) {
                listenSpecs.append(';');
            }
            listenSpecs.append(addrStr);
        }
        ++it;
    }

    if (listenSpecs.empty()) {
        Log(LOG_ERR, "No listen address specified.  Aborting...\n");
        return DAEMON_EXIT_CONFIG_ERROR;
    }

    // Do the real AllJoyn work here
    QStatus status;

    //
    // Teach the transport list how to make the transports that we support.  If
    // specified in the listen spec, they will be instantiated.
    //
    TransportFactoryContainer cntr;
    cntr.Add(new TransportFactory<DaemonTCPTransport>("tcp", false));

    Bus ajBus("alljoyn-daemon", cntr, listenSpecs.c_str());
    BusController ajBusController(ajBus, status);
    if (ER_OK != status) {
        Log(LOG_ERR, "Failed to create BusController: %s\n", QCC_StatusText(status));
        return DAEMON_EXIT_STARTUP_ERROR;
    }

    status = ajBus.Start();
    if (status != ER_OK) {
        Log(LOG_ERR, "Failed to start AllJoyn system: %s\n", QCC_StatusText(status));
        return DAEMON_EXIT_STARTUP_ERROR;
    }

    if (!config->GetAuth().empty()) {
        if (ajBus.GetInternal().FilterAuthMechanisms(config->GetAuth()) == 0) {
            Log(LOG_ERR, "No supported authentication mechanisms.  Aborting...\n");
            ajBus.Stop();
            return DAEMON_EXIT_STARTUP_ERROR;
        }
    }

    status = ajBus.StartListen(listenSpecs.c_str());
    if (ER_OK != status) {
        Log(LOG_ERR, "Failed to start listening on specified addresses\n");
        ajBus.Stop();
        return DAEMON_EXIT_STARTUP_ERROR;
    }


    if (opts.PrintAddress()) {
        qcc::String addrStr(listenSpecs);
        addrStr += "\n";
        printf("%s", addrStr.c_str());
    }

    g_ajBus = &ajBus;
    /*
     * Wait until bus is stopped
     */
    ajBus.WaitStop();

    return DAEMON_EXIT_OK;
}

DAEMONLIBRARY_API int LoadDaemon(int argc, char** argv)
{
    LoggerSetting* loggerSettings(LoggerSetting::GetLoggerSetting(argv[0]));
    loggerSettings->SetSyslog(false);
    if (g_isManaged) {
        FILE* pFile = _fsopen(g_logFilePathName, "a+", _SH_DENYNO);
        if (pFile == NULL)
            return 911;
        loggerSettings->SetFile(pFile);
    } else
        loggerSettings->SetFile(stdout);
    OptParse opts(argc, argv);
    OptParse::ParseResultCode parseCode(opts.ParseResult());
    ConfigDB* config(ConfigDB::GetConfigDB());

    switch (parseCode) {
    case OptParse::PR_OK:
        break;

    case OptParse::PR_EXIT_NO_ERROR:
        return DAEMON_EXIT_OK;

    default:
        return DAEMON_EXIT_OPTION_ERROR;
    }

    loggerSettings->SetLevel(opts.GetVerbosity());

    if (opts.UseDefaultConfig()) {
        StringSource src(defaultConfig);
        if (!config->LoadSource(src)) {
            return DAEMON_EXIT_CONFIG_ERROR;
        }
    } else {
        config->SetConfigFile(opts.GetConfigFile());
        if (!config->LoadConfigFile()) {
            fprintf(stderr, "Invalid configuration file specified: \"%s\"\n", opts.GetConfigFile().c_str());
            return DAEMON_EXIT_CONFIG_ERROR;
        }
    }

    return daemon(opts);
}
