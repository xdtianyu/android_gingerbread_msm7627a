/**
 * @file
 * BTAccessor declaration for BlueZ
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
#ifndef _ALLJOYN_BTACCESSOR_H
#define _ALLJOYN_BTACCESSOR_H

#include <qcc/platform.h>

#include <qcc/Event.h>
#include <qcc/ManagedObj.h>
#include <qcc/String.h>
#include <qcc/Timer.h>
#include <qcc/XmlElement.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/MessageReceiver.h>
#include <alljoyn/ProxyBusObject.h>

#include "BDAddress.h"
#include "BlueZUtils.h"
#include "BTController.h"
#include "BTNodeDB.h"
#include "BTTransport.h"
#include "RemoteEndpoint.h"

#include <Status.h>


namespace ajn {

class BTTransport::BTAccessor : public MessageReceiver, public qcc::AlarmListener {
  public:
    /**
     * Constructor
     *
     * @param transport
     * @param busGuid
     */
    BTAccessor(BTTransport* transport, const qcc::String& busGuid);

    /**
     * Destructor
     */
    ~BTAccessor();

    /**
     * Start the underlying Bluetooth subsystem.
     *
     * @return ER_OK if successful.
     */
    QStatus Start();

    /**
     * Start the underlying Bluetooth subsystem.
     */
    void Stop();

    /**
     * Start discovery (inquiry)
     *
     * @param ignoreAddrs   Set of BD Addresses to ignore
     * @param duration      Number of seconds to discover (0 = forever, default is 0)
     */
    QStatus StartDiscovery(const BDAddressSet& ignoreAddrs, uint32_t duration = 0);

    /**
     * Stop discovery (inquiry)
     */
    QStatus StopDiscovery();

    /**
     * Start discoverability (inquiry scan)
     */
    QStatus StartDiscoverability(uint32_t duration = 0);

    /**
     * Stop discoverability (inquiry scan)
     */
    QStatus StopDiscoverability();

    /**
     * Set SDP information
     *
     * @param uuidRev   Bus UUID revision to advertise in the SDP record
     * @param bdAddr    Bluetooth device address that of the node that is connectable
     * @param psm       L2CAP PSM number accepting connections
     * @param adInfo    Map of bus node GUIDs and bus names to advertise
     */
    QStatus SetSDPInfo(uint32_t uuidRev,
                       const BDAddress& bdAddr,
                       uint16_t psm,
                       const BTNodeDB& adInfo);

    /**
     * Make the Bluetooth device connectable.
     *
     * @param addr[out] Bluetooth device address that is connectable
     * @param psm[out]  L2CAP PSM that is connectable (0 if not connectable)
     *
     * @return  ER_OK if device is now connectable
     */
    QStatus StartConnectable(BDAddress& addr,
                             uint16_t& psm);

    /**
     * Make the Bluetooth device not connectable.
     */
    void StopConnectable();

    /**
     * Accepts an incoming connection from a remote Bluetooth device.
     *
     * @param alljoyn       BusAttachment that will be connected to the resulting endpoint
     * @param connectEvent  The event signalling the incoming connection
     *
     * @return  A newly instatiated remote endpoint for the Bluetooth connection (NULL indicates a failure)
     */
    RemoteEndpoint* Accept(BusAttachment& alljoyn,
                           qcc::Event* connectEvent);

    /**
     * Create an outgoing connection to a remote Bluetooth device.  If the
     * L2CAP PSM are not specified, then an SDP query will be performed to get
     * that information.
     *
     * @param alljoyn   BusAttachment that will be connected to the resulting endpoint
     * @param connAddr  Bluetooth device address where the real connection will go
     * @param devAddr   Bluetooth device address of the device desired to be reached
     *
     * @return  A newly instatiated remote endpoint for the Bluetooth connection (NULL indicates a failure)
     */
    RemoteEndpoint* Connect(BusAttachment& alljoyn,
                            const BTBusAddress& connAddr,
                            const BTBusAddress& devAddr);

    /**
     * Disconnect from the specified remote Bluetooth device.
     *
     * @param bdAddr    Bluetooth device address to disconnect from
     *
     * @return  ER_OK if successful
     */
    QStatus Disconnect(const BTBusAddress& addr);

    /**
     * Perform an SDP queary on the specified device to get the bus information.
     *
     * @param addr          Bluetooth device address to retrieve the SDP record from
     * @param uuidRev[out]  Bus UUID revision to found in the SDP record
     * @param connAddr[out] Address of the Bluetooth device accepting connections.
     * @param adInfo[out]   Map of bus node GUIDs and bus names being advertised
     */
    QStatus GetDeviceInfo(const BDAddress& addr,
                          uint32_t* uuidRev,
                          BTBusAddress* connAddr,
                          BTNodeDB* adInfo);

    /**
     * Accessor to get the L2CAP connect event object.
     *
     * @return pointer to the L2CAP connect event object.
     */
    qcc::Event* GetL2CAPConnectEvent() { return l2capEvent; }

  private:

    struct DispatchInfo;

    void ConnectBlueZ();
    void DisconnectBlueZ();

    /* Adapter management functions */
    QStatus EnumerateAdapters();
    void AdapterAdded(const char* adapterObjPath);
    void AdapterRemoved(const char* adapterObjPath);
    void DefaultAdapterChanged(const char* adapterObjPath);

    /* Adapter management signal handlers */
    void AdapterAddedSignalHandler(const InterfaceDescription::Member* member,
                                   const char* sourcePath,
                                   Message& msg);
    void AdapterRemovedSignalHandler(const InterfaceDescription::Member* member,
                                     const char* sourcePath,
                                     Message& msg);
    void DefaultAdapterChangedSignalHandler(const InterfaceDescription::Member* member,
                                            const char* sourcePath,
                                            Message& msg);
    void AdapterPropertyChangedSignalHandler(const InterfaceDescription::Member* member,
                                             const char* sourcePath,
                                             Message& msg);


    /* Device presence management signal handlers */
    void DeviceFoundSignalHandler(const InterfaceDescription::Member* member,
                                  const char* sourcePath,
                                  Message& msg);

    /* support */
    QStatus FillAdapterAddress(bluez::AdapterObject& adapter);
    QStatus AddRecord(const char* recordXml,
                      uint32_t& newHandle);
    void RemoveRecord();
    void AlarmTriggered(const qcc::Alarm& alarm, QStatus reason);
    static bool FindAllJoynUUID(const MsgArg* uuids,
                                size_t listSize,
                                uint32_t& uuidRev);
    static QStatus ProcessSDPXML(qcc::XmlParseContext& xmlctx,
                                 uint32_t* uuidRev,
                                 BDAddress* connAddr,
                                 uint16_t* connPSM,
                                 BTNodeDB* adInfo);
    static QStatus ProcessXMLAdvertisementsAttr(const qcc::XmlElement* elem,
                                                BTNodeDB& adInfo,
                                                uint32_t remoteVersion);
    QStatus GetDeviceObjPath(const BDAddress& bdAddr,
                             qcc::String& devObjPath);
    QStatus DiscoveryControl(const InterfaceDescription::Member& method);
    QStatus SetDiscoverabilityProperty();

    bluez::AdapterObject GetAdapterObject(const qcc::String& adapterObjPath) const
    {
        bluez::AdapterObject adapter;
        assert(!adapter->IsValid());
        adapterLock.Lock();
        AdapterMap::const_iterator it(adapterMap.find(adapterObjPath));
        if (it != adapterMap.end()) {
            adapter = it->second;
        }
        adapterLock.Unlock();
        return adapter;
    }

    bluez::AdapterObject GetDefaultAdapterObject() const
    {
        adapterLock.Lock();
        bluez::AdapterObject adapter(defaultAdapterObj);
        adapterLock.Unlock();
        return adapter;
    }

    bluez::AdapterObject GetAnyAdapterObject() const
    {
        adapterLock.Lock();
        bluez::AdapterObject adapter(anyAdapterObj);
        adapterLock.Unlock();
        return adapter;
    }

    qcc::Alarm DispatchOperation(DispatchInfo* op, uint32_t delay = 0)
    {
        qcc::Alarm alarm(delay, this, 0, (void*)op);
        bzBus.GetInternal().GetDispatcher().AddAlarm(alarm);
        return alarm;
    }



/******************************************************************************/


    typedef std::map<qcc::StringMapKey, bluez::AdapterObject> AdapterMap;

    class FoundInfo {
      public:
        FoundInfo() :
            uuidRev(bt::INVALID_UUIDREV),
            timestamp(0)
        { }
        uint32_t uuidRev;
        uint32_t timestamp;
        qcc::Alarm alarm;
    };
    typedef std::map<BDAddress, FoundInfo> FoundInfoMap;


    struct DispatchInfo {
        typedef enum {
            STOP_DISCOVERY,
            STOP_DISCOVERABILITY,
            ADAPTER_ADDED,
            ADAPTER_REMOVED,
            DEFAULT_ADAPTER_CHANGED,
            DEVICE_FOUND
        } DispatchTypes;
        DispatchTypes operation;

        DispatchInfo(DispatchTypes operation) : operation(operation) { }
        virtual ~DispatchInfo() { }
    };

    struct AdapterDispatchInfo : public DispatchInfo {
        qcc::String adapterPath;

        AdapterDispatchInfo(DispatchTypes operation, const char* adapterPath) :
            DispatchInfo(operation), adapterPath(adapterPath) { }
    };

    struct DeviceDispatchInfo : public DispatchInfo {
        BDAddress addr;
        uint32_t uuidRev;

        DeviceDispatchInfo(DispatchTypes operation, const BDAddress& addr, uint32_t uuidRev) :
            DispatchInfo(operation), addr(addr), uuidRev(uuidRev) { }
    };

    struct MsgDispatchInfo : public DispatchInfo {
        MsgArg* args;
        size_t argCnt;

        MsgDispatchInfo(DispatchTypes operation, MsgArg* args, size_t argCnt) :
            DispatchInfo(operation), args(args), argCnt(argCnt) { }
    };


    BusAttachment bzBus;
    const qcc::String busGuid;

    ProxyBusObject bzManagerObj;
    bluez::AdapterObject defaultAdapterObj;
    bluez::AdapterObject anyAdapterObj;
    AdapterMap adapterMap;
    mutable qcc::Mutex adapterLock; // Generic lock for adapter related objects, maps, etc.

    BTTransport* transport;

    uint32_t recordHandle;

    mutable qcc::Mutex deviceLock; // Generic lock for device related objects, maps, etc.
    FoundInfoMap foundDevices;  // Map of found AllJoyn devices w/ UUID-Rev and expire time.
    BDAddressSet ignoreAddrs;

    bool bluetoothAvailable;
    bool discoverable;
    bool discoveryActive;

    qcc::SocketFd l2capLFd;
    qcc::Event* l2capEvent;

    struct {
        struct {
            struct {
                const InterfaceDescription* interface;
                // Methods (not all; only those needed)
                const InterfaceDescription::Member* DefaultAdapter;
                const InterfaceDescription::Member* ListAdapters;
                // Signals
                const InterfaceDescription::Member* AdapterAdded;
                const InterfaceDescription::Member* AdapterRemoved;
                const InterfaceDescription::Member* DefaultAdapterChanged;
            } Manager;

            struct {
                const InterfaceDescription* interface;
                // Methods (not all; only those needed)
                const InterfaceDescription::Member* AddRecord;
                const InterfaceDescription::Member* RemoveRecord;
            } Service;

            struct {
                const InterfaceDescription* interface;
                // Methods (not all; only those needed)
                const InterfaceDescription::Member* CreateDevice;
                const InterfaceDescription::Member* FindDevice;
                const InterfaceDescription::Member* GetProperties;
                const InterfaceDescription::Member* ListDevices;
                const InterfaceDescription::Member* RemoveDevice;
                const InterfaceDescription::Member* SetProperty;
                const InterfaceDescription::Member* StartDiscovery;
                const InterfaceDescription::Member* StopDiscovery;
                // Signals
                const InterfaceDescription::Member* DeviceCreated;
                const InterfaceDescription::Member* DeviceDisappeared;
                const InterfaceDescription::Member* DeviceFound;
                const InterfaceDescription::Member* DeviceRemoved;
                const InterfaceDescription::Member* PropertyChanged;
            } Adapter;

            struct {
                const InterfaceDescription* interface;
                // Methods (not all; only those needed)
                const InterfaceDescription::Member* DiscoverServices;
                const InterfaceDescription::Member* GetProperties;
                // Signals
                const InterfaceDescription::Member* DisconnectRequested;
                const InterfaceDescription::Member* PropertyChanged;
            } Device;
        } bluez;
    } org;

};


} // namespace ajn


#endif
