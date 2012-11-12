/*
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
 */

#include <qcc/platform.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <qcc/Log.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Mutex.h>
#include <cassert>
#include <cstdio>

#include <set>
#include <map>
#include <vector>

using namespace ajn;
using namespace std;
using namespace qcc;

/* constants */
static const char* TEST_SERVICE_INTERFACE_NAME = "org.alljoyn.bus.test.sessions";
static const char* TEST_SERVICE_OBJECT_PATH = "/sessions";

/* forward declaration */
class SessionTestObject;
class MyBusListener;

/* Local Types */
struct DiscoverInfo {
    String peerName;
    TransportMask transport;
    DiscoverInfo(const char* peerName, TransportMask transport) : peerName(peerName), transport(transport) { }

    bool operator<(const DiscoverInfo& other) const
    {
        return (peerName < other.peerName) || ((peerName == other.peerName) && (transport < other.transport));
    }
};

struct SessionPortInfo {
    SessionPort port;
    String sessionHost;
    SessionOpts opts;
    SessionPortInfo() : port(0) { }
    SessionPortInfo(SessionPort port, const String& sessionHost, const SessionOpts& opts) : port(port), sessionHost(sessionHost), opts(opts) { }
};

struct SessionInfo {
    SessionId id;
    SessionPortInfo portInfo;
    vector<String> peerNames;
    SessionInfo() : id(0) { }
    SessionInfo(SessionId id, const SessionPortInfo& portInfo) : id(0), portInfo(portInfo) { }
};

/* static data */
static ajn::BusAttachment* s_bus = NULL;
static MyBusListener* s_busListener = NULL;

static set<String> s_requestedNames;
static set<String> s_advertisements;
static set<DiscoverInfo> s_discoverSet;
static map<SessionPort, SessionPortInfo> s_sessionPortMap;
static map<SessionId, SessionInfo> s_sessionMap;
static Mutex s_lock;

/*
 * get a line of input from the the file pointer (most likely stdin).
 * This will capture the the num-1 characters or till a newline character is
 * entered.
 *
 * @param[out] str a pointer to a character array that will hold the user input
 * @param[in]  num the size of the character array 'str'
 * @param[in]  fp  the file pointer the sting will be read from. (most likely stdin)
 *
 * @return returns the same string as 'str' if there has been a read error a null
 *                 pointer will be returned and 'str' will remain unchanged.
 */
char* get_line(char*str, size_t num, FILE*fp)
{
    char*p = fgets(str, num, fp);

    // fgets will capture the '\n' character if the string entered is shorter than
    // num. Remove the '\n' from the end of the line and replace it with nul '\0'.
    if (p != NULL) {
        size_t last = strlen(str) - 1;
        if (str[last] == '\n') {
            str[last] = '\0';
        }
    }
    return p;
}

/* Bus object */
class SessionTestObject : public BusObject {
  public:

    SessionTestObject(BusAttachment& bus, const char* path) : BusObject(bus, path), chatSignalMember(NULL)
    {
        QStatus status;

        /* Add the session test interface to this object */
        const InterfaceDescription* testIntf = bus.GetInterface(TEST_SERVICE_INTERFACE_NAME);
        assert(testIntf);
        AddInterface(*testIntf);

        /* Store the Chat signal member away so it can be quickly looked up when signals are sent */
        chatSignalMember = testIntf->GetMember("Chat");
        assert(chatSignalMember);

        /* Register signal handler */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&SessionTestObject::ChatSignalHandler),
                                            chatSignalMember,
                                            NULL);

        if (ER_OK != status) {
            printf("Failed to register signal handler for SessionTestObject::Chat (%s)\n", QCC_StatusText(status));
        }
    }

    /** Send a Chat signal */
    QStatus SendChatSignal(SessionId id, const char* msg, uint8_t flags) {

        MsgArg chatArg("s", msg);
        return Signal(NULL, id, *chatSignalMember, &chatArg, 1, 0, flags);
    }

    /** Receive a signal from another Chat client */
    void ChatSignalHandler(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
    {
        printf("RX chat from %s[%u]: %s\n", msg->GetSender(), msg->GetSessionId(), msg->GetArg(0)->v_string.str);
    }

  private:
    const InterfaceDescription::Member* chatSignalMember;
};

class MyBusListener : public BusListener, public SessionPortListener, public SessionListener {
    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        printf("Discovered name : \"%s\"\n", name);

        s_discoverSet.insert(DiscoverInfo(name, transport));
    }

    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
        printf("NameOwnerChanged: name=%s, oldOwner=%s, newOwner=%s\n", busName, previousOwner ? previousOwner : "<none>",
               newOwner ? newOwner : "<none>");
    }

    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
    {
        bool ret = false;
        s_lock.Lock();
        map<SessionPort, SessionPortInfo>::iterator it = s_sessionPortMap.find(sessionPort);
        if (it != s_sessionPortMap.end()) {
            printf("Accepting join request on %u from %s\n", sessionPort, joiner);
            ret = true;
        } else {
            printf("Rejecting join attempt to unregistered port %u from %s\n", sessionPort, joiner);
        }
        s_lock.Unlock();
        return ret;
    }

    void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
    {
        s_lock.Lock();
        map<SessionPort, SessionPortInfo>::iterator it = s_sessionPortMap.find(sessionPort);
        if (it != s_sessionPortMap.end()) {
            s_bus->SetSessionListener(id, this);
            map<SessionId, SessionInfo>::iterator sit = s_sessionMap.find(id);
            if (sit == s_sessionMap.end()) {
                SessionInfo sessionInfo(id, it->second);
                s_sessionMap[id] = sessionInfo;
            }
            s_sessionMap[id].peerNames.push_back(joiner);
            s_lock.Unlock();
            printf("SessionJoined with %s (id=%u)\n", joiner, id);
        } else {
            s_lock.Unlock();
            printf("Leaving unexpected session %u with %s\n", id, joiner);
            s_bus->LeaveSession(id);
        }
    }

    void SessionLost(SessionId id)
    {
        s_lock.Lock();
        map<SessionId, SessionInfo>::iterator it = s_sessionMap.find(id);
        if (it != s_sessionMap.end()) {
            s_sessionMap.erase(it);
            s_lock.Unlock();
            printf("Session %u is lost\n", id);
        } else {
            s_lock.Unlock();
            printf("SessionLost for unknown sessionId %u\n", id);
        }
    }
};

static void usage()
{
    printf("Usage: sessions [-h]\n");
    exit(1);
}

static String NextTok(String& inStr)
{
    String ret;
    size_t off = inStr.find_first_of(' ');
    if (off == String::npos) {
        ret = inStr;
        inStr.clear();
    } else {
        ret = inStr.substr(0, off);
        inStr = Trim(inStr.substr(off));
    }
    return Trim(ret);
}

static void DoRequestName(const String& name)
{
    QStatus status = s_bus->RequestName(name.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE);
    if (status == ER_OK) {
        s_lock.Lock();
        s_requestedNames.insert(name);
        s_lock.Unlock();
    } else {
        printf("RequestName(%s) failed with %s\n", name.c_str(), QCC_StatusText(status));
    }
}

static void DoReleaseName(const String& name)
{
    QStatus status = s_bus->ReleaseName(name.c_str());
    if (status == ER_OK) {
        s_lock.Lock();
        s_requestedNames.erase(name);
        s_lock.Unlock();
    } else {
        printf("ReleaseName(%s) failed with %s\n", name.c_str(), QCC_StatusText(status));
    }
}

static void DoBind(SessionPort port, const SessionOpts& opts)
{
    if (port == 0) {
        printf("Invalid session port (%u) specified to BindSessionPort\n", port);
        return;
    } else if ((opts.traffic < SessionOpts::TRAFFIC_MESSAGES) || (opts.traffic > SessionOpts::TRAFFIC_RAW_UNRELIABLE)) {
        printf("Invalid SesionOpts.traffic (0x%x) specified to BindSessionPort\n", (unsigned int) opts.traffic);
        return;
    } else if (opts.proximity > SessionOpts::PROXIMITY_ANY) {
        printf("Invalid SessionOpts.proximity (0x%x) specified to BindSessionPort\n", (unsigned int) opts.proximity);
        return;
    } else if (opts.transports == 0) {
        printf("Invalid SessionOpts.transports (0x%x) specified to BindSessionPort\n", (unsigned int) opts.transports);
    }
    QStatus status = s_bus->BindSessionPort(port, opts, *s_busListener);
    if (status == ER_OK) {
        s_lock.Lock();
        s_sessionPortMap.insert(pair<SessionPort, SessionPortInfo>(port, SessionPortInfo(port, s_bus->GetUniqueName(), opts)));
        s_lock.Unlock();
    } else {
        printf("BusAttachment::BindSessionPort(%u, <>, <>) failed with %s\n", port, QCC_StatusText(status));
    }
}

static void DoUnbind(SessionPort port)
{
    if (port == 0) {
        printf("Invalid session port (%u) specified to BindSessionPort\n", port);
        return;
    }
    QStatus status = s_bus->UnbindSessionPort(port);
    if (status == ER_OK) {
        s_lock.Lock();
        s_sessionPortMap.erase(port);
        s_lock.Unlock();
    } else {
        printf("BusAttachment::UnbindSessionPort(%u) failed with %s\n", port, QCC_StatusText(status));
    }
}

static void DoAdvertise(String name, TransportMask transports)
{
    QStatus status = s_bus->AdvertiseName(name.c_str(), transports);
    if (status == ER_OK) {
        s_lock.Lock();
        s_advertisements.insert(name);
        s_lock.Unlock();
    } else {
        printf("BusAttachment::AdvertiseName(%s, 0x%x) failed with %s\n", name.c_str(), transports, QCC_StatusText(status));
    }
}

static void DoCancelAdvertise(String name, TransportMask transports)
{
    if (transports == 0) {
        printf("Invalid transports (0x%x) specified to canceladvertise\n", transports);
        return;
    }
    QStatus status = s_bus->CancelAdvertiseName(name.c_str(), transports);
    if (status == ER_OK) {
        s_lock.Lock();
        s_advertisements.erase(name);
        s_lock.Unlock();
    } else {
        printf("BusAttachment::AdvertiseName(%s, 0x%x) failed with %s\n", name.c_str(), transports, QCC_StatusText(status));
    }
}

static void DoFind(String name)
{
    QStatus status = s_bus->FindAdvertisedName(name.c_str());
    if (status != ER_OK) {
        printf("BusAttachment::FindAdvertisedName(%s) failed with %s\n", name.c_str(), QCC_StatusText(status));
    }
}

static void DoCancelFind(String name)
{
    QStatus status = s_bus->CancelFindAdvertisedName(name.c_str());
    if (status != ER_OK) {
        printf("BusAttachment::CancelFindAdvertisedName(%s) failed with %s\n", name.c_str(), QCC_StatusText(status));
    }
}


static void DoList()
{
    printf("---------Locally Owned Names-------------------\n");
    printf("  %s\n", s_bus->GetUniqueName().c_str());
    set<String>::const_iterator nit = s_requestedNames.begin();
    while (nit != s_requestedNames.end()) {
        printf("  %s\n", nit++->c_str());
    }

    printf("---------Outgoing Advertisments----------------\n");
    s_lock.Lock();
    set<String>::const_iterator ait = s_advertisements.begin();
    while (ait != s_advertisements.end()) {
        printf("  %s\n", ait->c_str());
        ait++;
    }
    printf("---------Discovered Names----------------------\n");
    set<DiscoverInfo>::const_iterator dit = s_discoverSet.begin();
    while (dit != s_discoverSet.end()) {
        printf("   Peer: %s, transport=0x%x\n", dit->peerName.c_str(), dit->transport);
        ++dit;
    }
    printf("---------Bound Session Ports-------------------\n");
    map<SessionPort, SessionPortInfo>::const_iterator spit = s_sessionPortMap.begin();
    while (spit != s_sessionPortMap.end()) {
        printf("   Port: %u, isMultipoint=%s, traffic=%u, proximity=%u, transports=0x%x\n",
               spit->first,
               (spit->second.opts.isMultipoint ? "true" : "false"),
               spit->second.opts.traffic,
               spit->second.opts.proximity,
               spit->second.opts.transports);
        ++spit;
    }
    printf("---------Active sessions-----------------------\n");
    map<SessionId, SessionInfo>::const_iterator sit = s_sessionMap.begin();
    while (sit != s_sessionMap.end()) {
        printf("   SessionId: %u, Creator: %s, Port:%u, isMultipoint=%s, traffic=%u, proximity=%u, transports=0x%x\n",
               sit->first,
               sit->second.portInfo.sessionHost.c_str(),
               sit->second.portInfo.port,
               sit->second.portInfo.opts.isMultipoint ? "true" : "false",
               sit->second.portInfo.opts.traffic,
               sit->second.portInfo.opts.proximity,
               sit->second.portInfo.opts.transports);
        if (!sit->second.peerNames.empty()) {
            printf("    Peers: ");
            for (size_t j = 0; j < sit->second.peerNames.size(); ++j) {
                printf("%s%s", sit->second.peerNames[j].c_str(), (j == sit->second.peerNames.size() - 1) ? "" : ",");
            }
            printf("\n");
        }
        ++sit;
    }
    s_lock.Unlock();
}

static void DoJoin(String name, SessionPort port, const SessionOpts& opts)
{
    SessionId id;
    SessionOpts optsOut = opts;
    QStatus status = s_bus->JoinSession(name.c_str(), port, s_busListener, id, optsOut);
    if (status == ER_OK) {
        s_lock.Lock();
        s_sessionMap.insert(pair<SessionId, SessionInfo>(id, SessionInfo(id, SessionPortInfo(port, name, optsOut))));
        s_lock.Unlock();
    } else {
        printf("JoinSession(%s, %u, ...) failed with %s\n", name.c_str(), port, QCC_StatusText(status));
    }
}

static void DoLeave(SessionId id)
{
    /* Validate session id */
    map<SessionId, SessionInfo>::const_iterator it = s_sessionMap.find(id);
    if (it != s_sessionMap.end()) {
        QStatus status = s_bus->LeaveSession(id);
        if (status != ER_OK) {
            printf("SessionLost(%u) failed with %s\n", id, QCC_StatusText(status));
        }
        s_lock.Lock();
        s_sessionMap.erase(id);
        s_lock.Unlock();
    } else {
        printf("Invalid session id %u specified in LeaveSession\n", id);
    }
}

int main(int argc, char** argv)
{
    QStatus status = ER_OK;

    /* Parse command line args */
    for (int i = 1; i < argc; ++i) {
        if (0 == ::strcmp("-h", argv[i])) {
            usage();
        } else {
            printf("Unknown argument \"%s\"\n", argv[i]);
            usage();
        }
    }

    /* Create message bus */
    BusAttachment* bus = new BusAttachment("sessions", true);
    s_bus = bus;

    /* Create org.alljoyn.bus.test.sessions interface */
    InterfaceDescription* testIntf = NULL;
    status = bus->CreateInterface(TEST_SERVICE_INTERFACE_NAME, testIntf);
    if (ER_OK == status) {
        testIntf->AddSignal("Chat", "s",  "str", 0);
        testIntf->Activate();
    } else {
        printf("Failed to create interface \"%s\" (%s)\n", TEST_SERVICE_INTERFACE_NAME, QCC_StatusText(status));
    }

    /* Create and register the bus object that will be used to send and receive signals */
    SessionTestObject sessionTestObj(*bus, TEST_SERVICE_OBJECT_PATH);
    bus->RegisterBusObject(sessionTestObj);

    /* Start the msg bus */
    if (ER_OK == status) {
        status = bus->Start();
        if (ER_OK != status) {
            printf("BusAttachment::Start failed (%s)\n", QCC_StatusText(status));
        }
    }

    /* Register a bus listener */
    if (ER_OK == status) {
        s_busListener = new MyBusListener();
        s_bus->RegisterBusListener(*s_busListener);
    }

    /* Get env vars */
    const char* connectSpec = getenv("BUS_ADDRESS");
    if (connectSpec == NULL) {
#ifdef _WIN32
        connectSpec = "tcp:addr=127.0.0.1,port=9955";
#else
        connectSpec = "unix:abstract=alljoyn";
#endif
    }

    /* Connect to the local daemon */
    if (ER_OK == status) {
        status = s_bus->Connect(connectSpec);
        if (ER_OK != status) {
            printf("BusAttachment::Connect(%s) failed (%s)\n", connectSpec, QCC_StatusText(status));
        }
    }

    /* Parse commands from stdin */
    const int bufSize = 1024;
    char buf[bufSize];
    while ((ER_OK == status) && (get_line(buf, bufSize, stdin))) {
        String line(buf);
        String cmd = NextTok(line);
        if (cmd == "debug") {
            String module = NextTok(line);
            String level = NextTok(line);
            if (module.empty() || level.empty()) {
                printf("Usage: debug <modulename> <level>\n");
            } else {
                QCC_SetDebugLevel(module.c_str(), StringToU32(level));
            }
        } else if (cmd == "requestname") {
            String name = NextTok(line);
            if (name.empty()) {
                printf("Usage: requestname <name>\n");
            } else {
                DoRequestName(name);
            }
        } else if (cmd == "releasename") {
            String name = NextTok(line);
            if (name.empty()) {
                printf("Usage: releasename <name>\n");
            } else {
                DoReleaseName(name);
            }
        } else if (cmd == "bind") {
            SessionOpts opts;
            SessionPort port = static_cast<SessionPort>(StringToU32(NextTok(line), 0, 0));
            if (port == 0) {
                printf("Usage: bind <port> [isMultipoint] [traffic] [proximity] [transports]\n");
                continue;
            }
            opts.isMultipoint = (NextTok(line) == "true");
            opts.traffic = static_cast<SessionOpts::TrafficType>(StringToU32(NextTok(line), 0, 0x1));
            opts.proximity = static_cast<SessionOpts::Proximity>(StringToU32(NextTok(line), 0, 0xFF));
            opts.transports = static_cast<TransportMask>(StringToU32(NextTok(line), 0, 0xFFFF));
            DoBind(port, opts);
        } else if (cmd == "unbind") {
            SessionPort port = static_cast<SessionPort>(StringToU32(NextTok(line), 0, 0));
            if (port == 0) {
                printf("Usage: unbind <port>\n");
                continue;
            }
            DoUnbind(port);
        } else if (cmd == "advertise") {
            String name = NextTok(line);
            if (name.empty()) {
                printf("Usage: advertise <name> [transports]\n");
                continue;
            }
            TransportMask transports = static_cast<TransportMask>(StringToU32(NextTok(line), 0, 0xFFFF));
            DoAdvertise(name, transports);
        } else if (cmd == "canceladvertise") {
            String name = NextTok(line);
            if (name.empty()) {
                printf("Usage: canceladvertise <name> [transports]\n");
                continue;
            }
            TransportMask transports = static_cast<TransportMask>(StringToU32(NextTok(line), 0, 0xFFFF));
            DoCancelAdvertise(name, transports);
        } else if (cmd == "find") {
            String namePrefix = NextTok(line);
            if (namePrefix.empty()) {
                printf("Usage: find <name_prefix>\n");
                continue;
            }
            DoFind(namePrefix);
        } else if (cmd == "cancelfind") {
            String namePrefix = NextTok(line);
            if (namePrefix.empty()) {
                printf("Usage: cancelfind <name_prefix>\n");
                continue;
            }
            DoCancelFind(namePrefix);
        } else if (cmd == "list") {
            DoList();
        } else if (cmd == "join") {
            String name = NextTok(line);
            SessionPort port = static_cast<SessionPort>(StringToU32(NextTok(line), 0, 0));
            if (name.empty() || (port == 0)) {
                printf("Usage: join <name> <port> [isMultipoint] [traffic] [proximity] [transports]\n");
                continue;
            }
            SessionOpts opts;
            opts.isMultipoint = (NextTok(line) == "true");
            opts.traffic = static_cast<SessionOpts::TrafficType>(StringToU32(NextTok(line), 0, 0x1));
            opts.proximity = static_cast<SessionOpts::Proximity>(StringToU32(NextTok(line), 0, 0xFF));
            opts.transports = static_cast<TransportMask>(StringToU32(NextTok(line), 0, 0xFFFF));
            DoJoin(name, port, opts);
        } else if (cmd == "leave") {
            SessionId id = static_cast<SessionId>(StringToU32(NextTok(line), 0, 0));
            if (id == 0) {
                printf("Usage: leave <sessionId>\n");
                continue;
            }
            DoLeave(id);
        } else if (cmd == "chat") {
            uint8_t flags = 0;
            qcc::String tok = NextTok(line);
            if (tok == "-c") {
                flags |= ALLJOYN_FLAG_COMPRESSED;
                tok = NextTok(line);
            }
            SessionId id = StringToU32(tok);
            String chatMsg = Trim(line);
            if ((id == 0) || chatMsg.empty()) {
                printf("Usage: chat [-c] <sessionId> <msg>\n");
                continue;
            }
            sessionTestObj.SendChatSignal(id, chatMsg.c_str(), flags);
        } else if (cmd == "exit") {
            break;
        } else if (cmd == "help") {
            printf("debug <module_name> <level>                                   - Set debug level for a module\n");
            printf("requestname <name>                                            - Request a well-known name\n");
            printf("releasename <name>                                            - Release a well-known name\n");
            printf("bind <port> [isMultipoint] [traffic] [proximity] [transports] - Bind a session port\n");
            printf("unbind <port>                                                 - Unbind a session port\n");
            printf("advertise <name> [transports]                                 - Advertise a name\n");
            printf("canceladvertise <name> [transports]                           - Cancel an advertisement\n");
            printf("find <name_prefix>                                            - Discover names that begin with prefix\n");
            printf("cancelfind <name_prefix>                                      - Cancel discovering names that begins with prefix\n");
            printf("list                                                          - List port bindings, discovered names and active sessions\n");
            printf("join <name> <port> [isMultipoint] [traffic] [proximity] [transports] - Join a session\n");
            printf("leave <sessionId>                                             - Leave a session\n");
            printf("chat [-c] <sessionId> <msg>                                   - Send a message over a given session\n");
            printf("                                                                If present option -c means use header compression\n");
            printf("exit                                                          - Exit this program\n");
            printf("\n");
        } else {
            printf("Unknown command: %s\n", cmd.c_str());
        }
    }

    /* Cleanup */
    delete bus;
    bus = NULL;
    s_bus = NULL;

    if (NULL != s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }

    return (int) status;
}

