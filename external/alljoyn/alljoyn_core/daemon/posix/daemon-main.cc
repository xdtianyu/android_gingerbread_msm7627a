/**
 * @file
 * AllJoyn-Daemon - POSIX version
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

#include <errno.h>
#ifndef QCC_OS_ANDROID
#include <pwd.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include <qcc/String.h>
#include <qcc/StringSource.h>
#include <qcc/StringUtil.h>
#include <qcc/Environ.h>
#include <qcc/FileStream.h>
#include <qcc/Log.h>
#include <qcc/Logger.h>
#include <qcc/Util.h>

#include <alljoyn/version.h>

#include <Status.h>

#include "Transport.h"
#include "DaemonTCPTransport.h"
#include "DaemonUnixTransport.h"
#include "BTTransport.h"
#include "Bus.h"
#include "BusController.h"
#include "ConfigDB.h"

#if !defined(DAEMON_LIB)
#include <sys/prctl.h>
#include <linux/capability.h>
extern "C" {
extern int capset(cap_user_header_t hdrp, const cap_user_data_t datap);
}
#if defined(QCC_OS_ANDROID)
#define BLUETOOTH_UID 1002
#endif
#endif


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


static volatile sig_atomic_t reload;
static volatile sig_atomic_t quit;

/*
 * Simple config to allow all messages with PolicyDB tied into DaemonRouter and
 * to provide some non-default limits for the daemon tcp transport.
 */
static const char internalConfig[] =
    "<busconfig>"
    "  <type>alljoyn</type>"
    "  <listen>unix:abstract=alljoyn</listen>"
    "  <listen>bluetooth:</listen>"
    "  <listen>tcp:addr=0.0.0.0,port=9955</listen>"
    "  <policy context=\"default\">"
    "    <allow send_interface=\"*\"/>"
    "    <allow receive_interface=\"*\"/>"
    "    <allow own=\"*\"/>"
    "    <allow user=\"*\"/>"
    "    <allow send_requested_reply=\"true\"/>"
    "    <allow receive_requested_reply=\"true\"/>"
    "  </policy>"
    "  <limit name=\"auth_timeout\">32768</limit>"
    "  <limit name=\"max_incomplete_connections_tcp\">16</limit>"
    "  <limit name=\"max_completed_connections_tcp\">64</limit>"
    "  <alljoyn module=\"ipns\">"
    "    <property interfaces=\"*\"/>"
    "  </alljoyn>"
    "</busconfig>";


void SignalHandler(int sig)
{
    switch (sig) {
    case SIGHUP:
        if (!reload) {
            reload = 1;
        }
        break;

    case SIGINT:
    case SIGTERM:
        quit = 1;
        break;
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
        argc(argc), argv(argv), fork(false), noFork(false), noBT(false), noTCP(false), noSwitchUser(false),
        printAddressFd(-1), printPidFd(-1),
        session(false), system(false), internal(false), configService(false), verbosity(LOG_WARNING)
    { }

    ParseResultCode ParseResult();

    qcc::String GetConfigFile() const { return configFile; }
    bool GetFork() const { return fork; }
    bool GetNoFork() const { return noFork; }
    bool GetNoBT() const { return noBT; }
    bool GetNoTCP() const { return noTCP; }
    bool GetNoSwitchUser() const { return noSwitchUser; }
    int GetPrintAddressFd() const { return printAddressFd; }
    int GetPrintPidFd() const { return printPidFd; }
    int GetVerbosity() const { return verbosity; }
    bool GetInternalConfig() const { return internal; }
    bool GetServiceConfig() const { return configService; }

  private:
    int argc;
    char** argv;

    qcc::String configFile;
    bool fork;
    bool noFork;
    bool noBT;
    bool noTCP;
    bool noSwitchUser;
    int printAddressFd;
    int printPidFd;
    bool session;
    bool system;
    bool internal;
    bool configService;
    int verbosity;

    void PrintUsage();
};


void OptParse::PrintUsage()
{
    qcc::String cmd = argv[0];
    cmd = cmd.substr(cmd.find_last_of('/') + 1);

    fprintf(stderr,
            "%s [--session | --system | --internal | --config-file=FILE"
#if defined(QCC_OS_ANDROID) && defined(DAEMON_LIB)
            " | --config-service"
#endif
            "]\n"
            "%*s [--print-address[=DESCRIPTOR]] [--print-pid[=DESCRIPTOR]]\n"
            "%*s [--fork | --nofork] [--no-bt] [--no-tcp] [--no-switch-user]\n"
            "%*s [--verbosity=LEVEL] [--version]\n\n"
            "    --session\n"
            "        Use the standard configuration for the per-login-session message bus.\n\n"
            "    --system\n"
            "        Use the standard configuration for the system message bus.\n\n"
            "    --internal\n"
            "        Use a basic internally defined message bus for AllJoyn.\n\n"
#if defined(QCC_OS_ANDROID) && defined(DAEMON_LIB)
            "    --config-service\n"
            "        Use a configuration passed from the calling service.\n\n"
#endif
            "    --config-file=FILE\n"
            "        Use the specified configuration file.\n\n"
            "    --print-address[=DESCRIPTOR]\n"
            "        Print the socket address to STDOUT or the specified descriptor\n\n"
            "    --print-pid[=DESCRIPTOR]\n"
            "        Print the process ID to STDOUT or the specified descriptor\n\n"
            "    --fork\n"
            "        Force the daemon to fork and run in the background.\n\n"
            "    --nofork\n"
            "        Force the daemon to only run in the foreground (override config file\n"
            "        setting).\n\n"
            "    --no-bt\n"
            "        Disable the Bluetooth transport (override config file setting).\n\n"
            "    --no-tcp\n"
            "        Disable the TCP transport (override config file setting).\n\n"
            "    --no-switch-user\n"
            "        Don't switch from root to "
#if defined(QCC_OS_ANDROID)
            "bluetooth.\n\n"
#else
            "the user specified in the config file.\n\n"
#endif
            "    --verbosity=LEVEL\n"
            "        Set the logging level to LEVEL.\n\n"
            "    --version\n"
            "        Print the version and copyright string, and exit.\n",
            cmd.c_str(),
            static_cast<int>(cmd.size()), "",
            static_cast<int>(cmd.size()), "",
            static_cast<int>(cmd.size()), "");
}


OptParse::ParseResultCode OptParse::ParseResult()
{
    ParseResultCode result(PR_OK);
    int i;

    if (argc == 1) {
        result = PR_MISSING_OPTION;
        goto exit;
    }

    for (i = 1; i < argc; ++i) {
        qcc::String arg(argv[i]);

        if (arg.compare("--version") == 0) {
            printf("AllJoyn Message Bus Daemon version: %s\n"
                   "Copyright (c) 2009-2011 Qualcomm Innovation Center, Inc.\n"
                   "Licensed under Apache2.0: http://www.apache.org/licenses/LICENSE-2.0.html\n"
                   "\n"
                   "Build: %s\n", GetVersion(), GetBuildInfo());
            result = PR_EXIT_NO_ERROR;
            goto exit;
        } else if (arg.compare("--session") == 0) {
            if (!configFile.empty() || internal) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            configFile = "/etc/dbus-1/session.conf";
        } else if (arg.compare("--system") == 0) {
            if (!configFile.empty() || internal) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            configFile = "/etc/dbus-1/system.conf";
        } else if (arg.compare("--internal") == 0) {
            if (!configFile.empty()) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            internal = true;
        } else if (arg.compare("--config-file") == 0) {
            if (!configFile.empty() || internal) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            ++i;
            if (i == argc) {
                result = PR_MISSING_OPTION;
                goto exit;
            }
            configFile = argv[i];
        } else if (arg.compare(0, sizeof("--config-file") - 1, "--config-file") == 0) {
            if (!configFile.empty() || internal) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            configFile = arg.substr(sizeof("--config-file"));
#if defined(QCC_OS_ANDROID) && defined(DAEMON_LIB)
        } else if (arg.compare(0, sizeof("--config-service") - 1, "--config-service") == 0) {
            if (!configFile.empty() || internal) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            configService = true;
#endif
        } else if (arg.compare(0, sizeof("--print-address") - 1, "--print-address") == 0) {
            if (arg[sizeof("--print-address") - 1] == '=') {
                printAddressFd = StringToI32(arg.substr(sizeof("--print-address")), 10, -2);
            } else {
                if (((i + 1) == argc) || ((argv[i + 1][0] == '-') && (argv[i + 1][1] == '-'))) {
                    printAddressFd = STDOUT_FILENO;
                } else {
                    ++i;
                    printAddressFd = StringToI32(argv[i], 10, -2);
                }
            }
            if (printAddressFd < -1) {
                result = PR_INVALID_OPTION;
                goto exit;
            }
        } else if (arg.substr(0, sizeof("--print-pid") - 1).compare("--print-pid") == 0) {
            if (arg[sizeof("--print-pid") - 1] == '=') {
                printPidFd = StringToI32(arg.substr(sizeof("--print-pid")), 10, -2);
            } else {
                if (((i + 1) == argc) || ((argv[i + 1][0] == '-') && (argv[i + 1][1] == '-'))) {
                    printPidFd = STDOUT_FILENO;
                } else {
                    ++i;
                    printPidFd = StringToI32(argv[i], 10, -2);
                }
            }
            if (printPidFd < -1) {
                result = PR_INVALID_OPTION;
                goto exit;
            }
        } else if (arg.compare("--fork") == 0) {
            if (noFork) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            fork = true;
        } else if (arg.compare("--nofork") == 0) {
            if (fork) {
                result = PR_OPTION_CONFLICT;
                goto exit;
            }
            noFork = true;
        } else if (arg.compare("--no-bt") == 0) {
            noBT = true;
        } else if (arg.compare("--no-tcp") == 0) {
            noTCP = true;
        } else if (arg.compare("--no-switch-user") == 0) {
            noSwitchUser = true;
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
    struct sigaction act, oldact;
    sigset_t sigmask, waitmask;
    uint32_t pid(GetPid());
    ConfigDB* config(ConfigDB::GetConfigDB());

    // block all signals by default for all threads
    sigfillset(&sigmask);
    sigdelset(&sigmask, SIGSEGV);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

    act.sa_handler = SignalHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIGHUP, &act, &oldact);
    sigaction(SIGINT, &act, &oldact);
    sigaction(SIGTERM, &act, &oldact);

    const ConfigDB::ListenList& listenList = config->GetListen();
    ConfigDB::ListenList::const_iterator it = listenList.begin();
    String listenSpecs;

    while (it != listenList.end()) {
        qcc::String addrStr(*it);
        bool skip = false;
        if (it->compare(0, sizeof("unix:") - 1, "unix:") == 0) {
            if (it->compare(sizeof("unix:") - 1, sizeof("tmpdir=") - 1, "tmpdir=") == 0) {
                // Process tmpdir specially.
                qcc::String randStr = it->substr(sizeof("unix:tmpdir=") - 1) + "/alljoyn-";
                addrStr = ("unix:abstract=" + RandomString(randStr.c_str()));
            }
            if (config->GetType() == "system") {
                // Add the system bus unix address to the app's environment
                // for use by the BlueZ transport code since it needs it for
                // communicating with BlueZ.
                Environ* env = Environ::GetAppEnviron();
                const qcc::String var("DBUS_SYSTEM_BUS_ADDRESS");
                env->Add(var, addrStr);
            }

        } else if (it->compare(0, sizeof("tcp:") - 1, "tcp:") == 0) {
            skip = opts.GetNoTCP();

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

    TransportFactoryContainer cntr;
    cntr.Add(new TransportFactory<DaemonTCPTransport>("tcp", false));
    cntr.Add(new TransportFactory<DaemonUnixTransport>("unix", false));
    cntr.Add(new TransportFactory<BTTransport>("bluetooth", false));

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

    int fd;
    qcc::String pidfn(config->GetPidfile());

    fd = opts.GetPrintAddressFd();
    if (fd >= 0) {
        qcc::String localAddrs(ajBus.GetLocalAddresses());
        localAddrs += "\n";
        int ret = write(fd, localAddrs.c_str(), localAddrs.size());
        if (ret == -1) {
            Log(LOG_ERR, "Failed to print address string: %s\n", strerror(errno));
        }
    }
    fd = opts.GetPrintPidFd();
    if ((fd >= 0) || !pidfn.empty()) {
        qcc::String pidStr(U32ToString(pid));
        pidStr += "\n";
        if (fd > 0) {
            int ret = write(fd, pidStr.c_str(), pidStr.size());
            if (ret == -1) {
                Log(LOG_ERR, "Failed to print pid: %s\n", strerror(errno));
            }
        }
        if (!pidfn.empty()) {
            FileSink pidfile(pidfn);
            if (pidfile.IsValid()) {
                size_t sent;
                pidfile.PushBytes(pidStr.c_str(), pidStr.size(), sent);
            }
        }
    }

    sigfillset(&waitmask);
    sigdelset(&waitmask, SIGHUP);
    sigdelset(&waitmask, SIGINT);
    sigdelset(&waitmask, SIGTERM);

    quit = 0;

    while (!quit) {
        reload = 0;
        sigsuspend(&waitmask);

        if (reload && !opts.GetInternalConfig()) {
            Log(LOG_INFO, "Reloading config files.\n");

            config->LoadConfigFile();

            std::vector<std::pair<qcc::String, std::vector<qcc::String> > > nameList;
            std::vector<std::pair<qcc::String, std::vector<qcc::String> > >::const_iterator nit;

            ajBus.GetUniqueNamesAndAliases(nameList);

            for (nit = nameList.begin(); nit != nameList.end(); ++nit) {
                if (nit->second.size() > 0) {
                    const std::vector<qcc::String>& aliasList(nit->second);
                    std::vector<qcc::String>::const_iterator ait;
                    for (ait = aliasList.begin(); ait != aliasList.end(); ++ait) {
                        config->NameOwnerChanged(*ait, NULL, &nit->first);
                    }
                }
            }
        }
    }

    Log(LOG_INFO, "Terminating.\n");
    ajBus.StopListen(listenSpecs.c_str());
    ajBus.Stop();
    ajBus.WaitStop();

    if (!pidfn.empty()) {
        unlink(pidfn.c_str());
    }

    return DAEMON_EXIT_OK;
}


//
// This code can be run as a native executable, in which case the linker arranges to
// call main(), or it can be run as an Android Service.  In this case, the daemon
// is implemented as a static library which is linked into a JNI dynamic library and
// called from the Java service code.
//
#if defined(DAEMON_LIB)
extern "C" int DaemonMain(int argc, char** argv, char* serviceConfig)
#else
int main(int argc, char** argv, char** env)
#endif
{
#if defined(QCC_OS_ANDROID) && !defined(DAEMON_LIB)
    //
    // Initialize the environment for Android if we use the command line to
    // run the daemon.  If we are using the Android service launcher, there is
    // no environment there, so we use arguments and a passed-in config file
    // which we deal with in a bit.
    //
    environ = env;
#endif

    LoggerSetting* loggerSettings(LoggerSetting::GetLoggerSetting(argv[0]));

    OptParse opts(argc, argv);
    OptParse::ParseResultCode parseCode(opts.ParseResult());
    ConfigDB* config(ConfigDB::GetConfigDB());

    switch (parseCode) {
    case OptParse::PR_OK:
        break;

    case OptParse::PR_EXIT_NO_ERROR:
        delete config;
        return DAEMON_EXIT_OK;

    default:
        delete config;
        return DAEMON_EXIT_OPTION_ERROR;
    }

    loggerSettings->SetLevel(opts.GetVerbosity());

    if (opts.GetInternalConfig()) {
        StringSource src(internalConfig);
        config->LoadSource(src);
#if defined(QCC_OS_ANDROID) && defined(DAEMON_LIB)
    } else if (opts.GetServiceConfig()) {
        StringSource src(serviceConfig);
        config->LoadSource(src);
#endif
    } else {
        config->SetConfigFile(opts.GetConfigFile());
        if (!config->LoadConfigFile()) {
            delete config;
            return DAEMON_EXIT_CONFIG_ERROR;
        }
    }

    loggerSettings->SetSyslog(config->GetSyslog());
    loggerSettings->SetFile((opts.GetFork() || (config->GetFork() && !opts.GetNoFork())) ? NULL : stderr);

#if !defined(DAEMON_LIB)
    if (!opts.GetNoSwitchUser()) {
        // Keep all capabilities before switching users
        prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

#if defined(QCC_OS_ANDROID)
        // Android uses hard coded UIDs.
        setuid(BLUETOOTH_UID);
#else
        if ((getuid() == 0) && !config->GetUser().empty()) {
            // drop root privileges if <user> is specified.
            qcc::String user = config->GetUser();
            struct passwd* pwent;
            setpwent();
            while ((pwent = getpwent())) {
                if (user.compare(pwent->pw_name) == 0) {
                    Log(LOG_INFO, "Dropping root privileges (running as %s)\n", pwent->pw_name);
                    setuid(pwent->pw_uid);
                    break;
                }
            }
            endpwent();
            if (!pwent) {
                Log(LOG_ERR, "Failed to drop root privileges - userid does not exist: %s\n",
                    user.c_str());
                delete config;
                return DAEMON_EXIT_CONFIG_ERROR;
            }
        }
#endif

        // Set the capabilities we need.
        struct __user_cap_header_struct header;
        struct __user_cap_data_struct cap;
        header.version = _LINUX_CAPABILITY_VERSION;
        header.pid = 0;
        cap.permitted = (1 << CAP_NET_RAW |
                         1 << CAP_NET_ADMIN |
                         1 << CAP_NET_BIND_SERVICE);
        cap.effective = cap.permitted;
        cap.inheritable = 0;
        capset(&header, &cap);
    }
#endif

    if (opts.GetFork() || (config->GetFork() && !opts.GetNoFork())) {
        Log(LOG_DEBUG, "Forking into daemon mode...\n");
        pid_t pid = fork();
        if (pid == -1) {
            Log(LOG_ERR, "Failed to fork(): %s\n", strerror(errno));
            delete config;
            return DAEMON_EXIT_FORK_ERROR;
        } else if (pid > 0) {
            // Unneeded parent process, just exit.
            _exit(DAEMON_EXIT_OK);
        } else {
            // create new session ID
            pid_t sid = setsid();
            if (sid < 0) {
                Log(LOG_ERR, "Failed to set session ID: %s\n", strerror(errno));
                delete config;
                return DAEMON_EXIT_SESSION_ERROR;
            }
        }
    }

    int ret = daemon(opts);

    delete config;

    return ret;
}
