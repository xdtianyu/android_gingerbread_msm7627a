/**
 * @file
 * BTAccessor implementation for BlueZ
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


/*
 * TODO:
 *
 * - Check if a discovered device via DeviceFound is already paired.  If so,
 *   don't bother calling CreateDevice, let BlueZ do so and let BlueZ continue
 *   to manage the device.
 *
 * - If we call CreateDevice for a discovered device, but another BlueZ device
 *   manager tool calls CreatePairedDevice, don't remove the device if it does
 *   not have AllJoyn support.  The 'Paired" property will be set if another BlueZ
 *   device manager calls CreatePairedDevice.
 *
 * - Work with BlueZ community to develop a better system to allow autonomous
 *   connections like that needed by AllJoyn.
 *   - Get SDP information without the need to call CreateDevice.
 *   - Add a method to allow BlueZ to update its UUID list for remote devices
 *     without the need to remove the device and re-add it.
 */


#include <qcc/platform.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>

#include <qcc/SocketStream.h>
#include <qcc/String.h>
#include <qcc/StringSource.h>
#include <qcc/Timer.h>
#include <qcc/XmlElement.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/version.h>

#include "BDAddress.h"
#include "BlueZ.h"
#include "BlueZHCIUtils.h"
#include "BlueZIfc.h"
#include "BTAccessor.h"
#include "BTController.h"
#include "BlueZBTEndpoint.h"
#include "BTTransport.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN_BT"


#define SignalHandler(_a) static_cast<MessageReceiver::SignalHandler>(_a)

using namespace ajn;
using namespace ajn::bluez;
using namespace qcc;
using namespace std;

namespace ajn {

/******************************************************************************/

/*
 * Uncomment this line to enable simple pairing debug mode for air sniffing.
 */
#define ENABLE_AIR_SNIFFING

/*
 * Timeout for various operations
 */
#define BT_DEFAULT_TO      10000
#define BT_GETPROP_TO      3000
#define BT_SDPQUERY_TO     200000
#define BT_CREATE_DEV_TO   200000

#define MAX_CONNECT_ATTEMPTS  3
#define MAX_CONNECT_WAITS    30

#define FOUND_DEVICE_INFO_TIMEOUT 30000

static const char alljoynUUIDBase[] = ALLJOYN_BT_UUID_BASE;
#define ALLJOYN_BT_UUID_REV_SIZE (sizeof("12345678") - 1)
#define ALLJOYN_BT_UUID_BASE_SIZE (sizeof(alljoynUUIDBase) - 1)


static const char sdpXmlTemplate[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<record>"
    "    <attribute id=\"0x0000\">"
    "        <uint32 value=\"0x4F492354\"/>"
    "    </attribute>"
    "    <attribute id=\"0x0002\">"
    "        <uint32 value=\"0x00000001\"/>"
    "    </attribute>"
    "    <attribute id=\"0x0008\">"
    "        <uint8 value=\"0xFF\"/>"
    "    </attribute>"
    "    <attribute id=\"0x0004\">"
    "        <sequence>"
    "            <sequence>"
    "                <uuid value=\"0x0100\"/>"
    "            </sequence>"
    "        </sequence>"
    "    </attribute>"
    "    <attribute id=\"0x0005\">"
    "        <sequence>"
    "            <uuid value=\"0x00001002\"/>"
    "        </sequence>"
    "    </attribute>"
    "    <attribute id=\"0x0001\">"
    "        <sequence>"
    "            <uuid value=\"%08x%s\"/>" // AllJoyn UUID - filled in later
    "        </sequence>"
    "    </attribute>"
    "    <attribute id=\"0x0400\">"    // AllJoyn Version number
    "        <uint32 value=\"%#08x\"/>" // filled in later
    "    </attribute>"
    "    <attribute id=\"0x0401\">"
    "        <text value=\"%s\"/>"      // Filled in with dynamically determined BD Address
    "    </attribute>"
    "    <attribute id=\"0x0402\">"
    "        <uint16 value=\"%#08x\"/>" // Filled in with dynamically determined L2CAP PSM number
    "    </attribute>"
    "    <attribute id=\"0x0404\">"
    "        <sequence>%s</sequence>"  // Filled in with advertisement information
    "    </attribute>"
    "    <attribute id=\"0x0100\">"
    "        <text value=\"AllJoyn\"/>"
    "    </attribute>"
    "    <attribute id=\"0x0101\">"
    "        <text value=\"AllJoyn Distributed Message Bus\"/>"
    "    </attribute>"
    "</record>";





/******************************************************************************/

BTTransport::BTAccessor::BTAccessor(BTTransport* transport,
                                    const qcc::String& busGuid) :
    bzBus("BlueZTransport"),
    busGuid(busGuid),
    transport(transport),
    recordHandle(0),
    bluetoothAvailable(false),
    discoverable(false),
    discoveryActive(false),
    l2capLFd(-1),
    l2capEvent(NULL)
{
    size_t tableIndex, member;

    // Must be initialized after 'bzBus' is initialized!
    bzManagerObj = ProxyBusObject(bzBus, bzBusName, bzMgrObjPath, 0);

    for (tableIndex = 0; tableIndex < ifcTableSize; ++tableIndex) {
        InterfaceDescription* ifc;
        const InterfaceTable& table(ifcTables[tableIndex]);
        bzBus.CreateInterface(table.ifcName, ifc);

        if (ifc) {
            for (member = 0; member < table.tableSize; ++member) {
                ifc->AddMember(table.desc[member].type,
                               table.desc[member].name,
                               table.desc[member].inputSig,
                               table.desc[member].outSig,
                               table.desc[member].argNames,
                               table.desc[member].annotation);
            }
            ifc->Activate();

            if (table.desc == bzManagerIfcTbl) {
                org.bluez.Manager.interface =             ifc;
                org.bluez.Manager.DefaultAdapter =        ifc->GetMember("DefaultAdapter");
                org.bluez.Manager.ListAdapters =          ifc->GetMember("ListAdapters");
                org.bluez.Manager.AdapterAdded =          ifc->GetMember("AdapterAdded");
                org.bluez.Manager.AdapterRemoved =        ifc->GetMember("AdapterRemoved");
                org.bluez.Manager.DefaultAdapterChanged = ifc->GetMember("DefaultAdapterChanged");

                bzBus.RegisterSignalHandler(this,
                                            SignalHandler(&BTTransport::BTAccessor::AdapterAddedSignalHandler),
                                            org.bluez.Manager.AdapterAdded, bzMgrObjPath);

                bzBus.RegisterSignalHandler(this,
                                            SignalHandler(&BTTransport::BTAccessor::AdapterRemovedSignalHandler),
                                            org.bluez.Manager.AdapterRemoved, bzMgrObjPath);

                bzBus.RegisterSignalHandler(this,
                                            SignalHandler(&BTTransport::BTAccessor::DefaultAdapterChangedSignalHandler),
                                            org.bluez.Manager.DefaultAdapterChanged, bzMgrObjPath);

            } else if (table.desc == bzAdapterIfcTbl) {
                org.bluez.Adapter.interface =          ifc;
                org.bluez.Adapter.CreateDevice =       ifc->GetMember("CreateDevice");
                org.bluez.Adapter.FindDevice =         ifc->GetMember("FindDevice");
                org.bluez.Adapter.GetProperties =      ifc->GetMember("GetProperties");
                org.bluez.Adapter.ListDevices =        ifc->GetMember("ListDevices");
                org.bluez.Adapter.RemoveDevice =       ifc->GetMember("RemoveDevice");
                org.bluez.Adapter.SetProperty =        ifc->GetMember("SetProperty");
                org.bluez.Adapter.StartDiscovery =     ifc->GetMember("StartDiscovery");
                org.bluez.Adapter.StopDiscovery =      ifc->GetMember("StopDiscovery");
                org.bluez.Adapter.DeviceCreated =      ifc->GetMember("DeviceCreated");
                org.bluez.Adapter.DeviceDisappeared =  ifc->GetMember("DeviceDisappeared");
                org.bluez.Adapter.DeviceFound =        ifc->GetMember("DeviceFound");
                org.bluez.Adapter.DeviceRemoved =      ifc->GetMember("DeviceRemoved");
                org.bluez.Adapter.PropertyChanged =    ifc->GetMember("PropertyChanged");

            } else if (table.desc == bzServiceIfcTbl) {
                org.bluez.Service.interface =          ifc;
                org.bluez.Service.AddRecord =          ifc->GetMember("AddRecord");
                org.bluez.Service.RemoveRecord =       ifc->GetMember("RemoveRecord");

            } else {
                org.bluez.Device.interface =           ifc;
                org.bluez.Device.DiscoverServices =    ifc->GetMember("DiscoverServices");
                org.bluez.Device.GetProperties =       ifc->GetMember("GetProperties");
                org.bluez.Device.DisconnectRequested = ifc->GetMember("DisconnectRequested");
                org.bluez.Device.PropertyChanged =     ifc->GetMember("PropertyChanged");
            }
        }
    }

    bzManagerObj.AddInterface(*org.bluez.Manager.interface);
}


BTTransport::BTAccessor::~BTAccessor()
{
    adapterMap.clear();
    if (l2capEvent) {
        delete l2capEvent;
    }
}


QStatus BTTransport::BTAccessor::Start()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::Start()"));

    QStatus status;

    /* Start the control bus */
    status = bzBus.Start();
    if (status == ER_OK) {
        qcc::String rules[] = {
            qcc::String("type='signal',sender='") + bzBusName + "',interface='" + bzManagerIfc + "'",
            qcc::String("type='signal',sender='") + bzBusName + "',interface='" + bzAdapterIfc + "'",
            qcc::String("type='signal',sender='") + bzBusName + "',interface='" + bzDeviceIfc  + "'",
            qcc::String("type='signal',sender='") + ajn::org::freedesktop::DBus::WellKnownName + "',interface='" + ajn::org::freedesktop::DBus::InterfaceName + "'"
        };

        MsgArg arg;
        Message reply(bzBus);
        const ProxyBusObject& dbusObj = bzBus.GetDBusProxyObj();
        const InterfaceDescription* ifc(bzBus.GetInterface(ajn::org::freedesktop::DBus::InterfaceName));
        const InterfaceDescription::Member* addMatch;
        const InterfaceDescription::Member* nameHasOwner;
        const InterfaceDescription::Member* nameOwnerChanged;

        /* Get environment variable for the system bus */
        Environ* env(Environ::GetAppEnviron());
#ifdef ANDROID
        qcc::String connectArgs(env->Find("DBUS_SYSTEM_BUS_ADDRESS",
                                          "unix:path=/dev/socket/dbus"));
#else
        qcc::String connectArgs(env->Find("DBUS_SYSTEM_BUS_ADDRESS",
                                          "unix:path=/var/run/dbus/system_bus_socket"));
#endif

        assert(ifc);
        if (!ifc) {
            status = ER_FAIL;
            QCC_LogError(status, ("Failed to get DBus interface description from AllJoyn"));
            goto exit;
        }

        addMatch = ifc->GetMember("AddMatch");
        nameHasOwner = ifc->GetMember("NameHasOwner");
        nameOwnerChanged = ifc->GetMember("NameOwnerChanged");

        /* Create the endpoint for talking to the Bluetooth subsystem */
        status = bzBus.Connect(connectArgs.c_str());
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to create UNIX endpoint"));
            goto exit;
        }

        /* Add Match rules */
        for (size_t i = 0; (status == ER_OK) && (i < ArraySize(rules)); ++i) {
            arg.Set("s", rules[i].c_str());
            status = dbusObj.MethodCall(*addMatch, &arg, 1, reply);
            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to add match rule: \"%s\"", rules[i].c_str()));
                QCC_DbgHLPrintf(("reply msg: %s\n", reply->ToString().c_str()));
            }
        }

        // Find out if the Bluetooth subsystem is running...
        arg.Set("s", bzBusName);
        status = dbusObj.MethodCall(*nameHasOwner, &arg, 1, reply);
        if (status != ER_OK) {
            QCC_LogError(status, ("Failure calling %s.NameHasOwner",
                                  ajn::org::freedesktop::DBus::InterfaceName));
            QCC_DbgHLPrintf(("reply msg: %s\n", reply->ToString().c_str()));
            bzBus.Stop();
            bzBus.WaitStop();
        } else if (reply->GetArg(0)->v_bool) {
            ConnectBlueZ();
        }
    }

exit:
    return status;
}


void BTTransport::BTAccessor::Stop()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::Stop()"));
    if (bluetoothAvailable) {
        DisconnectBlueZ();
    }
    bzBus.Stop();
    bzBus.WaitStop();
}


void BTTransport::BTAccessor::ConnectBlueZ()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::ConnectBlueZ()"));
    /*
     * It's ok if no adapters were found, we'll tell the upper layers everything is OK so
     * that when an adapter does become available it can be used. If there is an adapter we
     * can update the service record.
     */
    if (!bluetoothAvailable && (EnumerateAdapters() == ER_OK)) {
        bluetoothAvailable = true;
        transport->BTDeviceAvailable(true);
    }
}


void BTTransport::BTAccessor::DisconnectBlueZ()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::DisconnectBlueZ()"));

    transport->BTDeviceAvailable(false);

    /*
     * Unregister any registered services
     */
    if (recordHandle != 0) {
        QCC_DbgPrintf(("Removing record handle %x (disconnct from BlueZ)", recordHandle));
        RemoveRecord();
    }

    /*
     * Shut down all endpoints
     */
    vector<RemoteEndpoint*>::iterator eit;
    transport->threadListLock.Lock();
    for (eit = transport->threadList.begin(); eit != transport->threadList.end(); ++eit) {
        (*eit)->Stop();
    }
    transport->threadListLock.Unlock();

    /*
     * Close down the listen file descriptors
     */
    StopConnectable();

    bluetoothAvailable = false;

    /*
     * Invalidate the adapters.
     */
    adapterLock.Lock();
    adapterMap.clear();
    defaultAdapterObj = AdapterObject();
    anyAdapterObj = AdapterObject();
    adapterLock.Unlock();
}


QStatus BTTransport::BTAccessor::StartDiscovery(const BDAddressSet& ignoreAddrs, uint32_t duration)
{
    this->ignoreAddrs = ignoreAddrs;

    deviceLock.Lock();
    set<BDAddress>::const_iterator it;
    for (it = ignoreAddrs->begin(); it != ignoreAddrs->end(); ++it) {
        FoundInfoMap::iterator devit = foundDevices.find(*it);
        if (devit != foundDevices.end()) {
            bzBus.GetInternal().GetDispatcher().RemoveAlarm(devit->second.alarm);
            foundDevices.erase(devit);
        }
    }
    deviceLock.Unlock();

    QStatus status = DiscoveryControl(*org.bluez.Adapter.StartDiscovery);
    if (duration > 0) {
        DispatchOperation(new DispatchInfo(DispatchInfo::STOP_DISCOVERY),  duration * 1000);
    }
    discoveryActive = true;
    return status;
}


QStatus BTTransport::BTAccessor::StopDiscovery()
{
    QStatus status = DiscoveryControl(*org.bluez.Adapter.StopDiscovery);
    discoveryActive = false;
    return status;
}


QStatus BTTransport::BTAccessor::StartDiscoverability(uint32_t duration)
{
    QStatus status = ER_FAIL;
    discoverable = true;
    if (bluetoothAvailable) {
        status = SetDiscoverabilityProperty();
        if (duration > 0) {
            DispatchOperation(new DispatchInfo(DispatchInfo::STOP_DISCOVERABILITY),  duration * 1000);
        }
    }
    return status;
}


QStatus BTTransport::BTAccessor::StopDiscoverability()
{
    QStatus status = ER_FAIL;
    discoverable = false;
    if (bluetoothAvailable) {
        status = SetDiscoverabilityProperty();
    }
    return status;
}


QStatus BTTransport::BTAccessor::SetSDPInfo(uint32_t uuidRev,
                                            const BDAddress& bdAddr,
                                            uint16_t psm,
                                            const BTNodeDB& adInfo)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::SetSDPInfo(uuidRev = %08x, bdAddr = %s, psm = %04x, adInfo = <%u nodes>)",
                  uuidRev, bdAddr.ToString().c_str(), psm, adInfo.Size()));
    QStatus status = ER_OK;

    if (uuidRev == bt::INVALID_UUIDREV) {
        if (recordHandle != 0) {
            QCC_DbgPrintf(("Removing record handle %x (no more records)", recordHandle));
            RemoveRecord();
        }
    } else {
        qcc::String nameList;
        BTNodeDB::const_iterator nodeit;
        QCC_DbgPrintf(("Setting SDP record contents:"));
        for (nodeit = adInfo.Begin(); nodeit != adInfo.End(); ++nodeit) {
            const BTNodeInfo& node = *nodeit;
            NameSet::const_iterator nameit;
            QCC_DbgPrintf(("    %s:", node->GetBusAddress().ToString().c_str()));
            nameList +=
                "<sequence>"
                "  <text value=\"" + node->GetGUID() + "\"/>"
                "  <uint64 value=\"" + U64ToString(node->GetBusAddress().addr.GetRaw()) + "\"/>"
                "  <uint16 value=\"" + U32ToString(node->GetBusAddress().psm) + "\"/>"
                "  <sequence>";
            for (nameit = node->GetAdvertiseNamesBegin(); nameit != node->GetAdvertiseNamesEnd(); ++nameit) {
                QCC_DbgPrintf(("        %s", nameit->c_str()));
                nameList += "<text value=\"" + *nameit + "\"/>";
            }
            nameList +=
                "  </sequence>"
                "</sequence>";
        }

        const int sdpXmlSize = (sizeof(sdpXmlTemplate) +
                                ALLJOYN_BT_UUID_BASE_SIZE +
                                sizeof("12:34:56:78:9a:bc") +
                                (3 * sizeof("0x12345678")) +
                                nameList.size());
        char* sdpXml(new char[sdpXmlSize]);

        if (snprintf(sdpXml, sdpXmlSize, sdpXmlTemplate,
                     uuidRev, alljoynUUIDBase,
                     GetNumericVersion(),
                     bdAddr.ToString().c_str(),
                     psm,
                     nameList.c_str()) > sdpXmlSize) {
            status = ER_OUT_OF_MEMORY;
            QCC_LogError(status, ("AdvertiseBus(): Allocated SDP XML buffer too small (BUG - this should never happen)"));
            assert("SDP XML buffer too small" == NULL);

        } else {
            if (recordHandle) {
                QCC_DbgPrintf(("Removing record handle %x (old record)", recordHandle));
                RemoveRecord();
            }

            QCC_DbgPrintf(("Adding Record: UUID = %08x%s", uuidRev, alljoynUUIDBase));
            uint32_t newHandle;
            status = AddRecord(sdpXml, newHandle);
            if (status == ER_OK) {
                if ((recordHandle != 0) && (recordHandle != newHandle)) {
                    QCC_DbgPrintf(("Removing extraneous AllJoyn service record (%x).", recordHandle));
                    RemoveRecord();
                }
                recordHandle = newHandle;
                QCC_DbgPrintf(("Got record handle %x", recordHandle));
            }
        }
        delete [] sdpXml;
    }

    return status;
}


QStatus BTTransport::BTAccessor::AddRecord(const char* recordXml,
                                           uint32_t& newHandle)
{
    QStatus status = ER_FAIL;
    AdapterObject adapter = GetAnyAdapterObject();
    if (adapter->IsValid()) {
        MsgArg arg("s", recordXml);
        Message rsp(bzBus);

        status = adapter->MethodCall(*org.bluez.Service.AddRecord, &arg, 1, rsp, BT_DEFAULT_TO);
        if (status == ER_OK) {
            rsp->GetArg(0)->Get("u", &newHandle);
        } else {
            qcc::String errMsg;
            const char* errName = rsp->GetErrorName(&errMsg);
            QCC_LogError(status, ("AddRecord method call failed (%s - %s)",
                                  errName, errMsg.c_str()));
        }
    }
    return status;
}


void BTTransport::BTAccessor::RemoveRecord()
{
    QStatus status = ER_FAIL;
    AdapterObject adapter = GetAnyAdapterObject();
    if (adapter->IsValid()) {
        uint32_t doomedHandle = recordHandle;
        recordHandle = 0;
        MsgArg arg("u", doomedHandle);
        Message rsp(bzBus);

        status = adapter->MethodCall(*org.bluez.Service.RemoveRecord, &arg, 1, rsp, BT_DEFAULT_TO);
        if (status != ER_OK) {
            recordHandle = doomedHandle;
            qcc::String errMsg;
            const char* errName = rsp->GetErrorName(&errMsg);
            QCC_LogError(status, ("RemoveRecord method call failed (%s - %s)",
                                  errName, errMsg.c_str()));
        }
    }
}


QStatus BTTransport::BTAccessor::StartConnectable(BDAddress& addr,
                                                  uint16_t& psm)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::StartConnectable()"));

    QStatus status = ER_OK;
    L2CAP_SOCKADDR l2capAddr;
    int ret;

    addr = GetDefaultAdapterObject()->address;

    l2capLFd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, L2CAP_PROTOCOL_ID);
    if (l2capLFd == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("StartConnectable(): Create socket failed (errno: %d - %s)", errno, strerror(errno)));
    } else {
        QCC_DbgPrintf(("BTTransport::BTAccessor::StartConnectable(): l2capLFd = %d", l2capLFd));

        memset(&l2capAddr, 0, sizeof(l2capAddr));
        addr.CopyTo(l2capAddr.bdaddr.b, true);
        l2capAddr.sa_family = AF_BLUETOOTH;

        for (psm = 0x1001; (psm < 0x8fff); psm += 2) {
            l2capAddr.psm = htole16(psm);         // BlueZ requires PSM to be in little-endian format
            ret = bind(l2capLFd, (struct sockaddr*)&l2capAddr, sizeof(l2capAddr));
            if (ret != -1) {
                break;
            }
        }
        if (ret == -1) {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("StartConnectable(): Failed to find an unused PSM (bind errno: %d - %s)", errno, strerror(errno)));
            QCC_DbgPrintf(("Closing l2capLFd: %d", l2capLFd));
            shutdown(l2capLFd, SHUT_RDWR);
            close(l2capLFd);
            l2capLFd = -1;
            psm = bt::INVALID_PSM;
        } else {
            QCC_DbgPrintf(("Bound PSM: %#04x", psm));
            ConfigL2cap(l2capLFd);
            ret = listen(l2capLFd, 1);
            if (ret == -1) {
                status = ER_OS_ERROR;
                QCC_LogError(status, ("StartConnectable(): Listen socket failed (errno: %d - %s)", errno, strerror(errno)));
                QCC_DbgPrintf(("Closing l2capLFd: %d", l2capLFd));
                shutdown(l2capLFd, SHUT_RDWR);
                close(l2capLFd);
                l2capLFd = -1;
                psm = bt::INVALID_PSM;
            }
        }
    }


    if (l2capLFd != -1) {
        l2capEvent = new Event(l2capLFd, Event::IO_READ, false);
    } else if (l2capEvent) {
        delete l2capEvent;
        l2capEvent = NULL;
    }

    return status;
}


void BTTransport::BTAccessor::StopConnectable()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::StopConnectable()"));
    if (l2capLFd != -1) {
        QCC_DbgPrintf(("Closing l2capLFd: %d", l2capLFd));
        shutdown(l2capLFd, SHUT_RDWR);
        close(l2capLFd);
        l2capLFd = -1;
        if (l2capEvent) {
            delete l2capEvent;
            l2capEvent = NULL;
        }
    }
}


QStatus BTTransport::BTAccessor::FillAdapterAddress(AdapterObject& adapter)
{
    QStatus status = ER_FAIL;

    if (adapter->IsValid()) {
        Message rsp(bzBus);
        const char* bdAddrStr;
        const MsgArg* arg;

        status = adapter->MethodCall(*org.bluez.Adapter.GetProperties, NULL, 0, rsp, BT_DEFAULT_TO);
        if (status != ER_OK) {
            qcc::String errMsg;
            const char* errName = rsp->GetErrorName(&errMsg);
            QCC_LogError(status, ("Failed to get the adapter device address for %s: %s - %s",
                                  adapter->GetPath().c_str(),
                                  errName, errMsg.c_str()));
            goto exit;
        }

        arg = rsp->GetArg(0);

        if (arg) {
            status = arg->GetElement("{ss}", "Address", &bdAddrStr);
            if (status == ER_OK) {
                status = adapter->address.FromString(bdAddrStr);
            }

            arg->GetElement("{sb}", "Discovering", &adapter->bluezDiscovering);
        }
    }

exit:
    return status;
}


RemoteEndpoint* BTTransport::BTAccessor::Accept(BusAttachment& alljoyn,
                                                Event* connectEvent)
{
    BlueZBTEndpoint* conn(NULL);
    SocketFd sockFd;
    BT_SOCKADDR remoteAddr;
    socklen_t ralen = sizeof(remoteAddr);
    BDAddress remAddr;
    QStatus status;
    int flags, ret;
    SocketFd listenFd = connectEvent->GetFD();

    sockFd = accept(listenFd, (struct sockaddr*)&remoteAddr, &ralen);
    if (sockFd == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Accept socket failed (errno: %d - %s)",
                              errno, strerror(errno)));
        goto exit;
    } else {
        QCC_DbgPrintf(("BTTransport::BTAccessor::Accept(listenFd = %d): sockFd = %d",
                       listenFd, sockFd));
        uint8_t nul = 255;
        size_t recvd;
        status = qcc::Recv(sockFd, &nul, 1, recvd);
        if ((status != ER_OK) || (nul != 0)) {
            status = (status == ER_OK) ? ER_FAIL : status;
            QCC_LogError(status, ("Did not receive initial nul byte"));
            goto exit;
        }
    }

    remAddr.CopyFrom(remoteAddr.l2cap.bdaddr.b, true);
    if (!transport->CheckIncomingAddress(remAddr)) {
        status = ER_BUS_CONNECTION_REJECTED;
        QCC_DbgPrintf(("Rejected connection from: %s", remAddr.ToString().c_str()));
        goto exit;
    }

    QCC_DbgPrintf(("Accepted connection from: %s", remAddr.ToString().c_str()));

    flags = fcntl(sockFd, F_GETFL);
    ret = fcntl(sockFd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Could not set L2CAP socket to non-blocking"));
    }

exit:
    if (status != ER_OK) {
        if (sockFd > 0) {
            QCC_DbgPrintf(("Closing sockFd: %d", sockFd));
            shutdown(sockFd, SHUT_RDWR);
            close(sockFd);
            sockFd = -1;
        }
    } else {
        BTBusAddress dummyAddr;
        conn = new BlueZBTEndpoint(alljoyn, true, sockFd, dummyAddr);
    }

    return conn;
}


RemoteEndpoint* BTTransport::BTAccessor::Connect(BusAttachment& alljoyn,
                                                 const BTBusAddress& connAddr,
                                                 const BTBusAddress& devAddr)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::Connect(connAddr = %s, devAddr = %s)",
                  connAddr.ToString().c_str(), devAddr.ToString().c_str()));

    if (!connAddr.IsValid()) {
        return NULL;
    }

    BlueZBTEndpoint* conn(NULL);
    int ret;
    int flags;
    int sockFd(-1);
    BT_SOCKADDR skaddr;
    QStatus status = ER_OK;
    bool resumeDiscovery = false;

    if (discoveryActive) {
        resumeDiscovery = true;
        QCC_DbgPrintf(("Pausing discovery"));
        DiscoveryControl(*org.bluez.Adapter.StopDiscovery);
    }

    memset(&skaddr, 0, sizeof(skaddr));

    skaddr.l2cap.sa_family = AF_BLUETOOTH;
    skaddr.l2cap.psm = htole16(connAddr.psm);         // BlueZ requires PSM to be in little-endian format
    connAddr.addr.CopyTo(skaddr.l2cap.bdaddr.b, true);

    for (int tries = 0; tries < MAX_CONNECT_ATTEMPTS; ++tries) {
        sockFd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, L2CAP_PROTOCOL_ID);
        if (sockFd != -1) {
            ConfigL2cap(sockFd);
        } else {
            status = ER_OS_ERROR;
            QCC_LogError(status, ("Create socket failed - %s (errno: %d - %s)",
                                  connAddr.ToString().c_str(), errno, strerror(errno)));
            qcc::Sleep(200);
            continue;
        }
        QCC_DbgPrintf(("BTTransport::BTAccessor::Connect(%s, %s): sockFd = %d",
                       connAddr.ToString().c_str(), devAddr.ToString().c_str(), sockFd));

        /* Attempt to connect */
        ret = connect(sockFd, (struct sockaddr*)&skaddr, sizeof(skaddr));
        if (ret == -1) {
            status = ER_BUS_CONNECT_FAILED;
            close(sockFd);
            sockFd = -1;
            if ((errno == ECONNREFUSED) || (errno == EBADFD)) {
                QCC_LogError(status, ("Connect failed - %s (errno: %d - %s)",
                                      connAddr.ToString().c_str(), errno, strerror(errno)));
                qcc::Sleep(200);
                continue;
            }
        } else {
            status = ER_OK;
        }
        break;
    }
    if (status != ER_OK) {
        QCC_LogError(status, ("Connect to %sx failed (errno: %d - %s)",
                              connAddr.ToString().c_str(), errno, strerror(errno)));
        goto exit;
    }
    /*
     * BlueZ sockets are badly behaved. Even though the connect returned the
     * connection may not be fully up.  To code around this we poll on
     * getsockup until we get success.
     */
    for (int tries = 0; tries < MAX_CONNECT_WAITS; ++tries) {
        uint8_t opt[8];
        socklen_t optLen = sizeof(opt);
        ret = getsockopt(sockFd, SOL_L2CAP, L2CAP_CONNINFO, opt, &optLen);
        if (ret == -1) {
            if (errno == ENOTCONN) {
                qcc::Sleep(100);
            } else {
                status = ER_FAIL;
                QCC_LogError(status, ("Connection failed to come up (errno: %d - %s)", errno, strerror(errno)));
                goto exit;
            }
        } else {
            uint8_t nul = 0;
            size_t sent;
            status = qcc::Send(sockFd, &nul, 1, sent);
            if (status != ER_OK) {
                QCC_LogError(status, ("Failed to send nul byte (errno: %d - %s)", errno, strerror(errno)));
                goto exit;
            }
            QCC_DbgPrintf(("BTTransport::BTAccessor::Connect() success sockFd = %d connAddr = %s",
                           sockFd, connAddr.addr.ToString().c_str()));
            break;
        }
    }

    flags = fcntl(sockFd, F_GETFL);
    ret = fcntl(sockFd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        status = ER_OS_ERROR;
        QCC_LogError(status, ("Could not set socket to non-blocking"));
        goto exit;
    }

exit:

    if (status == ER_OK) {
        conn = new BlueZBTEndpoint(alljoyn, false, sockFd, devAddr);
    } else {
        if (sockFd > 0) {
            QCC_DbgPrintf(("Closing sockFd: %d", sockFd));
            shutdown(sockFd, SHUT_RDWR);
            close(sockFd);
            sockFd = -1;
        }
    }

    if (resumeDiscovery) {
        QCC_DbgPrintf(("Resuming discovery"));
        DiscoveryControl(*org.bluez.Adapter.StartDiscovery);
    }

    return conn;
}


QStatus BTTransport::BTAccessor::Disconnect(const BTBusAddress& addr)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::Disconnect(addr = %s)", addr.ToString().c_str()));
    QStatus status(ER_BUS_BAD_TRANSPORT_ARGS);

    RemoteEndpoint* ep(NULL);
    vector<RemoteEndpoint*>::iterator eit;

    transport->threadListLock.Lock();
    for (eit = transport->threadList.begin(); eit != transport->threadList.end(); ++eit) {
        if (addr == static_cast<BlueZBTEndpoint*>(*eit)->GetBTBusAddress()) {
            ep = *eit;
        }
        if (ep) {
            status = ep->Stop();
            break;
        }
    }
    transport->threadListLock.Unlock();
    return status;
}


QStatus BTTransport::BTAccessor::EnumerateAdapters()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::EnumerateAdapters()"));
    QStatus status;
    Message rsp(bzBus);

    status = bzManagerObj.MethodCall(*org.bluez.Manager.ListAdapters, NULL, 0, rsp, BT_DEFAULT_TO);
    if (status == ER_OK) {
        size_t numAdapters;
        const MsgArg* adapters;

        rsp->GetArg(0)->Get("ao", &numAdapters, &adapters);

        for (size_t i = 0; i < numAdapters; ++i) {
            const char* adapter;
            adapters[i].Get("o", &adapter);
            AdapterAdded(adapter);
        }
    } else {
        QCC_LogError(status, ("EnumerateAdapters(): 'ListAdapters' method call failed"));
    }

    status = bzManagerObj.MethodCall(*org.bluez.Manager.DefaultAdapter, NULL, 0, rsp, BT_DEFAULT_TO);
    if (status == ER_OK) {
        const MsgArg* rspArg(rsp->GetArg(0));
        qcc::String defaultAdapterObjPath(rspArg->v_string.str, rspArg->v_string.len);
        size_t pos(defaultAdapterObjPath.find_last_of('/'));
        if (pos != qcc::String::npos) {
            adapterLock.Lock();
            defaultAdapterObj = GetAdapterObject(defaultAdapterObjPath);
            if (defaultAdapterObj->IsValid()) {
                qcc::String anyAdapterObjPath(defaultAdapterObjPath.substr(0, pos + 1) + "any");
                anyAdapterObj = AdapterObject(bzBus, anyAdapterObjPath);
                anyAdapterObj->AddInterface(*org.bluez.Service.interface);
            } else {
                status = ER_FAIL;
            }
            adapterLock.Unlock();
        } else {
            QCC_DbgHLPrintf(("Invalid object path: \"%s\"", rspArg->v_string.str));
            status = ER_FAIL;
        }
    } else {
        QCC_DbgHLPrintf(("Finding default adapter path failed, most likely no bluetooth device connected (status = %s)",
                         QCC_StatusText(status)));
    }

    return status;
}


void BTTransport::BTAccessor::AdapterAdded(const char* adapterObjPath)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::AdapterAdded(adapterObjPath = \"%s\")", adapterObjPath));

    AdapterObject ao = GetAdapterObject(adapterObjPath);
    if (ao->IsValid()) {
        QCC_LogError(ER_FAIL, ("Adapter %s already exists", adapterObjPath));
        return;
    }

    qcc::String objPath(adapterObjPath);
    AdapterObject newAdapterObj(bzBus, objPath);

    if (newAdapterObj->GetInterface(bzServiceIfc) == NULL) {
        newAdapterObj->AddInterface(*org.bluez.Service.interface);
        newAdapterObj->AddInterface(*org.bluez.Adapter.interface);
    }

    QStatus status = FillAdapterAddress(newAdapterObj);
    if (status != ER_OK) {
        return;
    }

    adapterLock.Lock();
    adapterMap[newAdapterObj->GetPath()] = newAdapterObj;

    bzBus.RegisterSignalHandler(this,
                                SignalHandler(&BTTransport::BTAccessor::DeviceFoundSignalHandler),
                                org.bluez.Adapter.DeviceFound, adapterObjPath);

    bzBus.RegisterSignalHandler(this,
                                SignalHandler(&BTTransport::BTAccessor::AdapterPropertyChangedSignalHandler),
                                org.bluez.Adapter.PropertyChanged, adapterObjPath);

    adapterLock.Unlock();

    /*
     * Configure the inquiry scan parameters the way we want them.
     */
    ConfigureInquiryScan(newAdapterObj->id, 11, 1280, true, 8);

#ifdef ENABLE_AIR_SNIFFING
    ConfigureSimplePairingDebugMode(newAdapterObj->id, true);
#endif
}


void BTTransport::BTAccessor::AdapterRemoved(const char* adapterObjPath)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::AdapterRemoved(adapterObjPath = \"%s\")", adapterObjPath));

    bzBus.UnregisterSignalHandler(this,
                                  SignalHandler(&BTTransport::BTAccessor::DeviceFoundSignalHandler),
                                  org.bluez.Adapter.DeviceFound, adapterObjPath);

    bzBus.UnregisterSignalHandler(this,
                                  SignalHandler(&BTTransport::BTAccessor::AdapterPropertyChangedSignalHandler),
                                  org.bluez.Adapter.PropertyChanged, adapterObjPath);

    adapterLock.Lock();
    AdapterMap::iterator ait(adapterMap.find(adapterObjPath));
    if (ait != adapterMap.end()) {
        if (ait->second == defaultAdapterObj) {
            defaultAdapterObj = AdapterObject();
            bluetoothAvailable = false;
            transport->BTDeviceAvailable(false);
        }
        adapterMap.erase(ait);
    }
    adapterLock.Unlock();
}


void BTTransport::BTAccessor::DefaultAdapterChanged(const char* adapterObjPath)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::DefaultAdapterChanged(adapterObjPath = \"%s\")", adapterObjPath));

    adapterLock.Lock();
    defaultAdapterObj = GetAdapterObject(adapterObjPath);
    if (defaultAdapterObj->IsValid()) {
        qcc::String defaultAdapterObjPath(adapterObjPath);
        size_t pos = defaultAdapterObjPath.find_last_of('/');
        if (pos != qcc::String::npos) {
            qcc::String anyAdapterObjPath(defaultAdapterObjPath.substr(0, pos + 1) + "any");
            anyAdapterObj = AdapterObject(bzBus, anyAdapterObjPath);
            anyAdapterObj->AddInterface(*org.bluez.Service.interface);
        }

        bluetoothAvailable = true;
        transport->BTDeviceAvailable(true);
    }
    adapterLock.Unlock();
}


void BTTransport::BTAccessor::AlarmTriggered(const Alarm& alarm, QStatus reason)
{
    DispatchInfo* op = static_cast<DispatchInfo*>(alarm.GetContext());

    if (reason == ER_OK) {
        switch (op->operation) {
        case DispatchInfo::STOP_DISCOVERY:
            QCC_DbgPrintf(("Stopping Discovery"));
            StopDiscovery();
            break;

        case DispatchInfo::STOP_DISCOVERABILITY:
            QCC_DbgPrintf(("Stopping Discoverability"));
            StopDiscoverability();
            break;

        case DispatchInfo::ADAPTER_ADDED:
            AdapterAdded(static_cast<AdapterDispatchInfo*>(op)->adapterPath.c_str());
            break;

        case DispatchInfo::ADAPTER_REMOVED:
            AdapterRemoved(static_cast<AdapterDispatchInfo*>(op)->adapterPath.c_str());
            break;

        case DispatchInfo::DEFAULT_ADAPTER_CHANGED:
            DefaultAdapterChanged(static_cast<AdapterDispatchInfo*>(op)->adapterPath.c_str());
            break;

        case DispatchInfo::DEVICE_FOUND:
            transport->DeviceChange(static_cast<DeviceDispatchInfo*>(op)->addr,
                                    static_cast<DeviceDispatchInfo*>(op)->uuidRev);
            break;
        }
    }

    delete op;
}


void BTTransport::BTAccessor::AdapterAddedSignalHandler(const InterfaceDescription::Member* member,
                                                        const char* sourcePath,
                                                        Message& msg)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::AdapterAddedSignalHandler - signal from \"%s\"", sourcePath));
    DispatchOperation(new AdapterDispatchInfo(DispatchInfo::ADAPTER_ADDED, msg->GetArg(0)->v_objPath.str));
}


void BTTransport::BTAccessor::AdapterRemovedSignalHandler(const InterfaceDescription::Member* member,
                                                          const char* sourcePath,
                                                          Message& msg)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::AdapterRemovedSignalHandler - signal from \"%s\"", sourcePath));
    DispatchOperation(new AdapterDispatchInfo(DispatchInfo::ADAPTER_REMOVED, msg->GetArg(0)->v_objPath.str));
}


void BTTransport::BTAccessor::DefaultAdapterChangedSignalHandler(const InterfaceDescription::Member* member,
                                                                 const char* sourcePath,
                                                                 Message& msg)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::DefaultAdapterChangedSignalHandler - signal from \"%s\"", sourcePath));
    /*
     * We are in a signal handler so kick off the restart in a new thread.
     */
    DispatchOperation(new AdapterDispatchInfo(DispatchInfo::DEFAULT_ADAPTER_CHANGED, msg->GetArg(0)->v_objPath.str));
}


void BTTransport::BTAccessor::DeviceFoundSignalHandler(const InterfaceDescription::Member* member,
                                                       const char* sourcePath,
                                                       Message& msg)
{
    const char* addrStr;
    MsgArg* dictionary;
    QStatus status;

    // Use wildcard "*" for the dictionary array so that MsgArg gives us a
    // MsgArg pointer to an array of dictionary elements instead of an array
    // of MsgArg's of dictionary elements.
    status = msg->GetArgs("s*", &addrStr, &dictionary);
    if (status != ER_OK) {
        QCC_LogError(status, ("Parsing args from DeviceFound signal"));
        return;
    }

    BDAddress addr(addrStr);
    //QCC_DbgTrace(("BTTransport::BTAccessor::DeviceFoundSignalHandler - found addr = %s", addrStr));

    if (ignoreAddrs->find(addr) != ignoreAddrs->end()) {
        // We found the piconet/scatternet we are on so ignore the inquiry response.
        return;
    }

    const MsgArg* uuids;
    size_t listSize;

    // We can safely assume that dictonary is an array of dictionary elements
    // because the core AllJoyn code validated the args before calling us.
    status = dictionary->GetElement("{sas}", "UUIDs", &listSize, &uuids);
    if (status == ER_OK) {
        //QCC_DbgPrintf(("BTTransport::BTAccessor::DeviceFoundSignalHandler(): checking %s (%d UUIDs)",
        //               addrStr, listSize));

        qcc::String uuid;
        uint32_t uuidRev = bt::INVALID_UUIDREV;
        bool found = FindAllJoynUUID(uuids, listSize, uuidRev);

        if (found) {
            deviceLock.Lock();
            FoundInfo& foundInfo = foundDevices[addr];

            QCC_DbgHLPrintf(("Found AllJoyn device: %s  uuidRev = %08x  foundInfo.uuidRev = %08x", addrStr, uuidRev, foundInfo.uuidRev));

            if ((foundInfo.uuidRev == bt::INVALID_UUIDREV) ||
                (foundInfo.uuidRev != uuidRev)) {
                // Newly found device or changed advertisments, so inform the topology manager.

                foundInfo.uuidRev = uuidRev;
                DispatchOperation(new DeviceDispatchInfo(DispatchInfo::DEVICE_FOUND, addr, uuidRev));
            }

            deviceLock.Unlock();
        }
    }
}


bool BTTransport::BTAccessor::FindAllJoynUUID(const MsgArg* uuids,
                                              size_t listSize,
                                              uint32_t& uuidRev)
{
    // Search the UUID list for AllJoyn UUIDs.
    for (size_t i = 0; i < listSize; ++i) {
        const char* uuid;
        QStatus status = uuids[i].Get("s", &uuid);

        if ((status == ER_OK) &&
            (strcasecmp(alljoynUUIDBase, uuid + ALLJOYN_BT_UUID_REV_SIZE) == 0)) {
            qcc::String uuidRevStr(uuid, ALLJOYN_BT_UUID_REV_SIZE);
            uuidRev = StringToU32(uuidRevStr, 16);
            return true;
        }
    }
    return false;
}


QStatus BTTransport::BTAccessor::GetDeviceInfo(const BDAddress& addr,
                                               uint32_t* uuidRev,
                                               BTBusAddress* connAddr,
                                               BTNodeDB* adInfo)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::GetDeviceInfo(addr = %s, ...)", addr.ToString().c_str()));
    QStatus status;
    qcc::String devObjPath;
    bool resumeDiscovery = false;

    if (discoveryActive) {
        resumeDiscovery = true;
        QCC_DbgPrintf(("Pausing discovery"));
        DiscoveryControl(*org.bluez.Adapter.StopDiscovery);
    }

    status = GetDeviceObjPath(addr, devObjPath);
    if (status == ER_OK) {
        Message rsp(bzBus);
        MsgArg arg("s", "");

        ProxyBusObject dev(bzBus, bzBusName, devObjPath.c_str(), 0);
        dev.AddInterface(*org.bluez.Device.interface);

        QCC_DbgPrintf(("Getting service info for AllJoyn service"));
        status = dev.MethodCall(*org.bluez.Device.DiscoverServices, &arg, 1, rsp, BT_SDPQUERY_TO);
        if (status == ER_OK) {
            MsgArg* records;
            size_t numRecords;
            rsp->GetArg(0)->Get("a{us}", &numRecords, &records);

            /* Find AllJoyn SDP record */
            for (size_t i = 0; i < numRecords; ++i) {
                const char* record;
                uint32_t handle;
                records[i].Get("{us}", &handle, &record);

                StringSource rawXmlSrc(record);
                XmlParseContext xmlctx(rawXmlSrc);
                BDAddress bdAddr;
                uint16_t psm;

                status = ProcessSDPXML(xmlctx, uuidRev, &bdAddr, &psm, adInfo);
                if (status == ER_OK) {
                    if (connAddr) {
                        *connAddr = BTBusAddress(bdAddr, psm);
                    }
                    QCC_DbgPrintf(("Found AllJoyn UUID: psm %#04x", psm));
                    break;
                }
            }
        }
    }

    if (resumeDiscovery) {
        QCC_DbgPrintf(("Resuming discovery"));
        DiscoveryControl(*org.bluez.Adapter.StartDiscovery);
    }

    return status;
}


QStatus BTTransport::BTAccessor::ProcessSDPXML(XmlParseContext& xmlctx,
                                               uint32_t* uuidRev,
                                               BDAddress* connAddr,
                                               uint16_t* connPSM,
                                               BTNodeDB* adInfo)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::ProcessSDPXML()"));
    bool foundConnAddr = !connAddr;
    bool foundUUIDRev = !uuidRev;
    bool foundPSM = !connPSM;
    bool foundAdInfo = !adInfo;
    uint32_t remoteVersion = 0;
    QStatus status;

    status = XmlElement::Parse(xmlctx);
    if (status != ER_OK) {
        QCC_LogError(status, ("Parsing SDP XML"));
        goto exit;
    }

    if (xmlctx.root.GetName().compare("record") == 0) {
        const vector<XmlElement*>& recElements(xmlctx.root.GetChildren());
        vector<XmlElement*>::const_iterator recElem;

        for (recElem = recElements.begin(); recElem != recElements.end(); ++recElem) {
            if ((*recElem)->GetName().compare("attribute") == 0) {
                const XmlElement* sequenceTag;
                const XmlElement* uuidTag;
                uint32_t attrId(StringToU32((*recElem)->GetAttribute("id")));
                const vector<XmlElement*>& valElements((*recElem)->GetChildren());
                vector<XmlElement*>::const_iterator valElem(valElements.begin());

                switch (attrId) {
                case 0x0001:
                    if (uuidRev) {
                        sequenceTag = (*valElem)->GetChild("sequence");

                        if (sequenceTag) {
                            uuidTag = sequenceTag->GetChild("uuid");
                        } else {
                            uuidTag = (*valElem)->GetChild("uuid");
                        }

                        if (uuidTag) {
                            const std::map<qcc::String, qcc::String>& attrs(uuidTag->GetAttributes());
                            const std::map<qcc::String, qcc::String>::const_iterator valueIt(attrs.find("value"));
                            if (valueIt != attrs.end()) {
                                qcc::String uuidStr = valueIt->second;
                                if (uuidStr.compare(ALLJOYN_BT_UUID_REV_SIZE, ALLJOYN_BT_UUID_BASE_SIZE, alljoynUUIDBase) == 0) {
                                    *uuidRev = StringToU32(uuidStr.substr(0, ALLJOYN_BT_UUID_REV_SIZE), 16);
                                    foundUUIDRev = true;
                                }
                            }
                        }
                    }
                    break;

                case ALLJOYN_BT_VERSION_NUM_ATTR:
                    while ((valElem != valElements.end()) && ((*valElem)->GetName().compare("uint32") != 0)) {
                        ++valElem;
                    }
                    if (valElem == valElements.end()) {
                        status = ER_FAIL;
                        QCC_LogError(status, ("Missing uint32 value for Alljoyn version number"));
                        goto exit;
                    } else {
                        String verStr = (*valElem)->GetAttributes().find("value")->second;
                        remoteVersion = StringToU32(verStr);
                        if (remoteVersion < GenerateVersionValue(2, 0, 0)) {
                            QCC_DbgHLPrintf(("Remote device is running an unsupported version of AllJoyn"));
                            status = ER_FAIL;
                            goto exit;
                        }
                    }
                    QCC_DbgPrintf(("    Attribute ID: %04x  ALLJOYN_BT_VERSION_NUM_ATTR: %u.%u.%u",
                                   attrId,
                                   GetVersionArch(remoteVersion),
                                   GetVersionAPILevel(remoteVersion),
                                   GetVersionRelease(remoteVersion)));
                    break;

                case ALLJOYN_BT_CONN_ADDR_ATTR:
                    if (connAddr) {
                        while ((valElem != valElements.end()) && ((*valElem)->GetName().compare("text") != 0)) {
                            ++valElem;
                        }
                        if (valElem == valElements.end()) {
                            status = ER_FAIL;
                            QCC_LogError(status, ("Missing text value for BD Address"));
                            goto exit;
                        }
                        qcc::String addrStr = (*valElem)->GetAttributes().find("value")->second;
                        status = connAddr->FromString(addrStr);
                        if (status != ER_OK) {
                            QCC_LogError(status, ("Failed to parse the BD Address: \"%s\"", addrStr.c_str()));
                            goto exit;
                        }
                        foundConnAddr = true;
                        QCC_DbgPrintf(("    Attribute ID: %04x  ALLJOYN_BT_CONN_ADDR_ATTR: %s", attrId, addrStr.c_str()));
                    }
                    break;

                case ALLJOYN_BT_L2CAP_PSM_ATTR:
                    if (connPSM) {
                        while ((valElem != valElements.end()) && ((*valElem)->GetName().compare("uint16") != 0)) {
                            ++valElem;
                        }
                        if (valElem == valElements.end()) {
                            status = ER_FAIL;
                            QCC_LogError(status, ("Missing uint16 value for PSM number"));
                            goto exit;
                        }
                        qcc::String psmStr = (*valElem)->GetAttributes().find("value")->second;
                        QCC_DbgPrintf(("    Attribute ID: %04x  ALLJOYN_BT_L2CAP_PSM_ATTR: %s", attrId, psmStr.c_str()));
                        *connPSM = StringToU32(psmStr);
                        if ((*connPSM < 0x1001) || ((*connPSM & 0x1) != 0x1) || (*connPSM > 0x8fff)) {
                            *connPSM = bt::INVALID_PSM;
                        }
                        foundPSM = true;
                    }
                    break;

                case ALLJOYN_BT_ADVERTISEMENTS_ATTR:
                    if (adInfo) {
                        if (remoteVersion == 0) {
                            status = ER_FAIL;
                            QCC_LogError(status, ("AllJoyn version attribute must appear before the advertisements in the SDP record."));
                            goto exit;
                        }
                        status = ProcessXMLAdvertisementsAttr(*valElem, *adInfo, remoteVersion);
                        if (status != ER_OK) {
                            status = ER_FAIL;
                            QCC_LogError(status, ("Failed to parse advertisement information"));
                            goto exit;
                        }
                        foundAdInfo = true;

#ifndef NDEBUG
                        QCC_DbgPrintf(("    Attribute ID: %04x  ALLJOYN_BT_ADVERTISEMENTS_ATTR:", attrId));
                        BTNodeDB::const_iterator nodeit;
                        for (nodeit = adInfo->Begin(); nodeit != adInfo->End(); ++nodeit) {
                            const BTNodeInfo& node = *nodeit;
                            QCC_DbgPrintf(("       %s (GUID: %s)", node->GetBusAddress().ToString().c_str(), node->GetGUID().c_str()));
                            NameSet::const_iterator name;
                            for (name = node->GetAdvertiseNamesBegin(); name != node->GetAdvertiseNamesEnd(); ++name) {
                                QCC_DbgPrintf(("           \"%s\"", name->c_str()));
                            }
                        }
#endif
                    }
                    break;

                default:
                    // QCC_DbgPrintf(("    Attribute ID: %04x", attrId));
                    break;
                }
            }
        }

        if ((connPSM && (*connPSM == bt::INVALID_PSM)) ||
            (!foundConnAddr || !foundUUIDRev || !foundPSM || !foundAdInfo)) {
            status = ER_FAIL;
        }
    } else {
        status = ER_FAIL;
        QCC_LogError(status, ("ProcessSDP(): Unexpected root tag parsing SDP XML: \"%s\"", xmlctx.root.GetName().c_str()));
    }

exit:
    if (status != ER_OK) {
        if (uuidRev) *uuidRev = bt::INVALID_UUIDREV;
        if (connAddr) *connAddr = BDAddress();
        if (connPSM) *connPSM = bt::INVALID_PSM;
        if (adInfo) adInfo->Clear();
    }

    return status;
}


QStatus BTTransport::BTAccessor::ProcessXMLAdvertisementsAttr(const XmlElement* elem, BTNodeDB& adInfo, uint32_t remoteVersion)
{
    QStatus status = ER_OK;
    /*
     * The levels of sequence tags is a bit confusing when parsing.  The first
     * sequence level is effectively an array of tuples.  The second sequence
     * level is effectively the tuple of bus GUID, Bluetooth device address,
     * L2CAP PSM, and an array of advertised names.  The third sequence level
     * is just the list of advertised names.
     */

    if (elem && (elem->GetName().compare("sequence") == 0)) {
        // This sequence is essentially just a list of nodes.
        const vector<XmlElement*>& xmlNodes = elem->GetChildren();

        for (size_t i = 0; i < xmlNodes.size(); ++i) {
            const XmlElement* xmlNode = xmlNodes[i];

            if (xmlNode && (xmlNode->GetName().compare("sequence") == 0)) {
                // This sequence is a map between bus GUIDs and the advertised names for the given node.
                const vector<XmlElement*>& tupleElements = xmlNode->GetChildren();
                bool gotGUID = false;
                bool gotBDAddr = false;
                bool gotPSM = false;
                bool gotNames = false;
                BTNodeInfo nodeInfo;
                BDAddress addr;
                uint16_t psm = bt::INVALID_PSM;

                /*
                 * The first four elements must be the GUID, BT devie address,
                 * PSM, and list of advertised names.  Future versions of
                 * AllJoyn may extend the SDP record with additional elements,
                 * but this set in this order is the minimum requirement.  Any
                 * missing information means the SDP record is malformed and
                 * we should ignore it.
                 */
                if ((tupleElements[0]) && (tupleElements[0]->GetName().compare("text") == 0)) {
                    String guidStr = tupleElements[0]->GetAttribute("value");
                    nodeInfo->SetGUID(Trim(guidStr));
                    gotGUID = !guidStr.empty();
                }
                if ((tupleElements[1]) && (tupleElements[1]->GetName().compare("uint64") == 0)) {
                    String addrStr = Trim(tupleElements[1]->GetAttribute("value"));
                    addr.SetRaw(StringToU64(addrStr, 0));
                    gotBDAddr = addr.GetRaw() != 0;
                }
                if ((tupleElements[2]) && (tupleElements[2]->GetName().compare("uint16") == 0)) {
                    String psmStr = Trim(tupleElements[2]->GetAttribute("value"));
                    psm = StringToU32(psmStr, 0, bt::INVALID_PSM);
                    gotPSM = psm != bt::INVALID_PSM;
                }
                if ((tupleElements[3]) && (tupleElements[3]->GetName().compare("sequence") == 0)) {
                    // This sequence is just the list of advertised names for the given node.
                    const vector<XmlElement*>& nameList = tupleElements[3]->GetChildren();
                    for (size_t k = 0; k < nameList.size(); ++k) {
                        if (nameList[k] && (nameList[k]->GetName().compare("text") == 0)) {
                            // A bug in BlueZ adds a space to the end of our text string.
                            const qcc::String name = Trim(nameList[k]->GetAttribute("value"));
                            nodeInfo->AddAdvertiseName(name);
                        }
                    }
                    gotNames = true;
                }
                if (gotGUID && gotBDAddr && gotPSM && gotNames) {
                    nodeInfo->SetBusAddress(BTBusAddress(addr, psm));
                    adInfo.AddNode(nodeInfo);
                } else {
                    // Malformed SDP record, ignore this device.
                    status = ER_FAIL;
                    goto exit;
                }
            }
        }
    }

exit:
    return status;
}


QStatus BTTransport::BTAccessor::GetDeviceObjPath(const BDAddress& bdAddr,
                                                  qcc::String& devObjPath)
{
    const qcc::String& bdAddrStr(bdAddr.ToString());
    QCC_DbgTrace(("BTTransport::BTAccessor::GetDeviceObjPath(bdAddr = %s)", bdAddrStr.c_str()));
    QStatus status(ER_NONE);
    Message rsp(bzBus);
    MsgArg arg("s", bdAddrStr.c_str());
    vector<AdapterObject> adapterList;
    AdapterObject adapter;

    // Get a copy of all the adapter objects to check.
    adapterList.reserve(adapterMap.size());
    adapterLock.Lock();
    for (AdapterMap::const_iterator ait = adapterMap.begin(); ait != adapterMap.end(); ++ait) {
        adapterList.push_back(ait->second);
    }
    adapterLock.Unlock();

    for (vector<AdapterObject>::const_iterator it = adapterList.begin(); it != adapterList.end(); ++it) {
        if (status != ER_OK) {
            status = (*it)->MethodCall(*org.bluez.Adapter.FindDevice, &arg, 1, rsp, BT_DEFAULT_TO);
            if (status == ER_OK) {
                adapter = (*it);
#ifndef NDEBUG
            } else {
                qcc::String errMsg;
                const char* errName = rsp->GetErrorName(&errMsg);
                QCC_DbgPrintf(("GetDeviceObjPath(): FindDevice method call: %s - %s", errName, errMsg.c_str()));
#endif
            }
        }
    }

    if (status != ER_OK) {
        // Not found on any of the adapters, so create it on the default adapter.
        adapter = GetDefaultAdapterObject();
        if (adapter->IsValid()) {
            status = adapter->MethodCall(*org.bluez.Adapter.CreateDevice, &arg, 1, rsp, BT_CREATE_DEV_TO);
#ifndef NDEBUG
            if (status != ER_OK) {
                qcc::String errMsg;
                const char* errName = rsp->GetErrorName(&errMsg);
                QCC_DbgPrintf(("GetDeviceObjPath(): CreateDevice method call: %s - %s", errName, errMsg.c_str()));
            }
#endif
        }
    }

    if (status == ER_OK) {
        char* objPath;
        rsp->GetArg(0)->Get("o", &objPath);
        devObjPath = objPath;
    }

    return status;
}


QStatus BTTransport::BTAccessor::DiscoveryControl(const InterfaceDescription::Member& method)
{
    QCC_DbgTrace(("BTTransport::BTAccessor::DiscoveryControl(method = %s)", method.name.c_str()));
    QStatus status = ER_FAIL;

    AdapterObject adapter = GetDefaultAdapterObject();

    if (adapter->IsValid()) {
        Message rsp(bzBus);

        status = adapter->MethodCall(method, NULL, 0, rsp, BT_DEFAULT_TO);
        if (status == ER_OK) {
            bool started = &method == org.bluez.Adapter.StartDiscovery;
            QCC_DbgHLPrintf(("%s discovery", started ? "Started" : "Stopped"));
            if (started) {
                static const uint16_t MIN_PERIOD = 6;
                static const uint16_t MAX_PERIOD = 10;
                static const uint8_t LENGTH = 2;
                static const uint8_t NUM_RESPONSES = 8;

                ConfigurePeriodicInquiry(adapter->id, MIN_PERIOD, MAX_PERIOD, LENGTH, NUM_RESPONSES);
            }
        } else {
            qcc::String errMsg;
            const char* errName = rsp->GetErrorName(&errMsg);
            QCC_LogError(status, ("Call to org.bluez.Adapter.%s failed %s - %s",
                                  method.name.c_str(),
                                  errName, errMsg.c_str()));
        }
    }
    return status;
}


QStatus BTTransport::BTAccessor::SetDiscoverabilityProperty()
{
    QCC_DbgTrace(("BTTransport::BTAccessor::SetDiscoverability(%s)", discoverable ? "true" : "false"));
    QStatus status(ER_OK);
    AdapterMap::iterator adapterIt;
    list<AdapterObject> adapterList;
    MsgArg discVal("b", discoverable);
    MsgArg dargs[2];

    dargs[0].Set("s", "Discoverable");
    dargs[1].Set("v", &discVal);

    // Not a good idea to call a method while iterating through the list of
    // adapters since it could change during the time it takes to call the
    // method and holding the lock for that long could be problematic.
    adapterLock.Lock();
    for (adapterIt = adapterMap.begin(); adapterIt != adapterMap.end(); ++adapterIt) {
        adapterList.push_back(adapterIt->second);
    }
    adapterLock.Unlock();

    QCC_DbgHLPrintf(("%s discoverability", discoverable ? "Enabled" : "Disabled"));

    for (list<AdapterObject>::const_iterator it = adapterList.begin(); it != adapterList.end(); ++it) {
        Message reply(bzBus);
        status = (*it)->MethodCall(*org.bluez.Adapter.SetProperty, dargs, ArraySize(dargs));
        if (status != ER_OK) {
            QCC_LogError(status, ("Failed to set 'Discoverable' %s on %s",
                                  discoverable ? "true" : "false", (*it)->GetPath().c_str()));
        }
    }
    return status;
}


void BTTransport::BTAccessor::AdapterPropertyChangedSignalHandler(const InterfaceDescription::Member* member,
                                                                  const char* sourcePath,
                                                                  Message& msg)
{
    // QCC_DbgTrace(("BTTransport::BTAccessor::AdapterPropertyChangedSignalHandler"));

    AdapterObject adapter = GetAdapterObject(sourcePath);
    if (adapter->IsValid()) {
        const char* property;
        const MsgArg* value;

        msg->GetArgs("sv", &property, &value);

        //QCC_DbgPrintf(("New value for property on %s: %s = %s", sourcePath, property, value->ToString().c_str()));

        if (strcmp(property, "Discoverable") == 0) {
            bool disc;
            value->Get("b", &disc);

            if (!disc && discoverable) {
                // Adapter just became UNdiscoverable when it should still be discoverable.
                MsgArg discVal("b", true);
                MsgArg dargs[2];

                dargs[0].Set("s", "Discoverable");
                dargs[1].Set("v", &discVal);

                adapter->MethodCall(*org.bluez.Adapter.SetProperty, dargs, ArraySize(dargs));
            }
        } else if (strcmp(property, "Discovering") == 0) {
            value->Get("b", &(adapter->bluezDiscovering));
            if ((adapter == GetDefaultAdapterObject()) && !adapter->bluezDiscovering) {
                // The default adapter has stopped discovering.  Flush the
                // found devices cache so that any devices found will be
                // passed on to the topology manager the next time the default
                // adapter starts up.  When discovery is started using
                // org.bluez D-Bus serivce, BlueZ automatically cycles between
                // discovering and idle.  The typical cycle time is ~5 seconds
                // discovering and ~10 seconds idle.
                foundDevices.clear();
            }
        }
    }
}



} // namespace ajn
