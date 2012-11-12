/**
 * @file BusAttachment.h is the top-level object responsible for connecting to a
 * message bus.
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
#ifndef _ALLJOYN_BUSATTACHMENT_H
#define _ALLJOYN_BUSATTACHMENT_H

#ifndef __cplusplus
#error Only include BusAttachment.h in C++ code.
#endif

#include <qcc/platform.h>

#include <qcc/String.h>
#include <alljoyn/KeyStoreListener.h>
#include <alljoyn/AuthListener.h>
#include <alljoyn/BusListener.h>
#include <alljoyn/BusObject.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/Session.h>
#include <alljoyn/SessionListener.h>
#include <alljoyn/SessionPortListener.h>
#include <Status.h>

namespace ajn {

class RemoteEndpoint;

/**
 * %BusAttachment is the top-level object responsible for connecting to and optionally managing a message bus.
 */
class BusAttachment : public MessageReceiver {
  public:

    /**
     * Pure virtual base class implemented by classes that wish to call JoinSessionAsync().
     */
    class JoinSessionAsyncCB {
      public:
        /** Destructor */
        virtual ~JoinSessionAsyncCB() { }

        /**
         * Called when JoinSessionAsync() completes.
         *
         * @param status       ER_OK if successful
         * @param sessionId    Unique identifier for session.
         * @param opts         Session options.
         * @param context      User defined context which will be passed as-is to callback.
         */
        virtual void JoinSessionCB(QStatus status, SessionId sessionId, SessionOpts opts, void* context) = 0;
    };

    /**
     * Construct a BusAttachment.
     *
     * @param applicationName       Name of the application.
     * @param allowRemoteMessages   True if this attachment is allowed to receive messages from remote devices.
     */
    BusAttachment(const char* applicationName, bool allowRemoteMessages = false);

    /** Destructor */
    virtual ~BusAttachment();

    /**
     * Create an interface description with a given name.
     *
     * Typically, interfaces that are implemented by BusObjects are created here.
     * Interfaces that are implemented by remote objects are added automatically by
     * the bus if they are not already present via ProxyBusObject::IntrospectRemoteObject().
     *
     * Because interfaces are added both explicitly (via this method) and implicitly
     * (via @c ProxyBusObject::IntrospectRemoteObject), there is the possibility that creating
     * an interface here will fail because the interface already exists. If this happens, the
     * ER_BUS_IFACE_ALREADY_EXISTS will be returned and NULL will be returned in the iface [OUT]
     * parameter.
     *
     * Interfaces created with this method need to be activated using InterfaceDescription::Activate()
     * once all of the methods, signals, etc have been added to the interface. The interface will
     * be unaccessible (via BusAttachment::GetInterfaces() or BusAttachment::GetInterface()) until
     * it is activated.
     *
     * @param name   The requested interface name.
     * @param[out] iface
     *      - Interface description
     *      - NULL if cannot be created.
     * @param secure If true the interface is secure and method calls and signals will be encrypted.
     *
     * @return
     *      - #ER_OK if creation was successful.
     *      - #ER_BUS_IFACE_ALREADY_EXISTS if requested interface already exists
     * @see ProxyBusObject::IntrospectRemoteObject, InterfaceDescription::Activate, BusAttachment::GetInterface
     */
    QStatus CreateInterface(const char* name, InterfaceDescription*& iface, bool secure = false);

    /**
     * Initialize one more interface descriptions from an XML string in DBus introspection format.
     * The root tag of the XML can be a \<node\> or a standalone \<interface\> tag. To initialize more
     * than one interface the interfaces need to be nested in a \<node\> tag.
     *
     * Note that when this method fails during parsing, the return code will be set accordingly.
     * However, any interfaces which were successfully parsed prior to the failure may be registered
     * with the bus.
     *
     * @param xml     An XML string in DBus introspection format.
     *
     * @return
     *      - #ER_OK if parsing is completely successful.
     *      - An error status otherwise.
     */
    QStatus CreateInterfacesFromXml(const char* xml);

    /**
     * Returns the existing activated InterfaceDescriptions.
     *
     * @param ifaces     A pointer to an InterfaceDescription array to receive the interfaces. Can be NULL in
     *                   which case no interfaces are returned and the return value gives the number
     *                   of interface available.
     * @param numIfaces  The size of the InterfaceDescription array. If this value is smaller than the total
     *                   number of interfaces only numIfaces will be returned.
     *
     * @return  The number of interfaces returned or the total number of interfaces if ifaces is NULL.
     */
    size_t GetInterfaces(const InterfaceDescription** ifaces = NULL, size_t numIfaces = 0) const;

    /**
     * Retrieve an existing activated InterfaceDescription.
     *
     * @param name       Interface name
     *
     * @return
     *      - A pointer to the registered interface
     *      - NULL if interface doesn't exist
     */
    const InterfaceDescription* GetInterface(const char* name) const;

    /**
     * Delete an interface description with a given name.
     *
     * Deleting an interface is only allowed if that interface has never been activated.
     *
     * @param iface  The un-activated interface to be deleted.
     *
     * @return
     *      - #ER_OK if deletion was successful
     *      - #ER_BUS_NO_SUCH_INTERFACE if interface was not found
     */
    QStatus DeleteInterface(InterfaceDescription& iface);

    /**
     * Start the message bus.
     *
     * This method only begins the process of starting the bus. Sending and receiving messages
     * cannot begin until the bus is connected.
     *
     * There are two ways to determine whether the bus is currently connected:
     *    -# BusAttachment::IsConnected() returns true
     *    -# BusObject::ObjectRegistered() is called by the bus
     *
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_BUS_ALREADY_STARTED if already started
     *      - Other error status codes indicating a failure
     */
    QStatus Start();

    /**
     * Stop the message bus.
     *
     * @param blockUntilStopped   Block the caller until the bus is stopped
     *
     * @return
     *      - #ER_OK if successful.
     *      - An error status if unable to stop the message bus
     */
    QStatus Stop(bool blockUntilStopped = true);

    /**
     * Returns true if the mesage bus has been started.
     */
    bool IsStarted() { return isStarted; }

    /**
     * Returns true if the mesage bus has been requested to stop.
     */
    bool IsStopping() { return isStopping; }

    /**
     * Wait for the message bus to be stopped. This method blocks the calling thread until another thread
     * calls the Stop() method. Return immediately if the message bus has not been started.
     */
    void WaitStop();

    /**
     * Connect to a remote bus address.
     *
     * @param connectSpec  A transport connection spec string of the form:
     *                     @c "<transport>:<param1>=<value1>,<param2>=<value2>...[;]"
     * @param newep        FOR INTERNAL USE ONLY - External users must set to NULL (the default)
     *
     * @return
     *      - #ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Connect(const char* connectSpec, RemoteEndpoint** newep = NULL);

    /**
     * Disconnect a remote bus address connection.
     *
     * @param connectSpec  The transport connection spec used to connect.
     *
     * @return
     *          - #ER_OK if successful
     *          - #ER_BUS_BUS_NOT_STARTED if the bus is not started
     *          - #ER_BUS_NOT_CONNECTED if the %BusAttachment is not connected to the bus
     *          - Other error status codes indicating a failure
     */
    QStatus Disconnect(const char* connectSpec);

    /**
     * Indicate whether bus is currently connected.
     *
     * Messages can only be sent or received when the bus is connected.
     *
     * @return true if the bus is connected.
     */
    bool IsConnected() const;

    /**
     * Register a BusObject
     *
     * @param obj      BusObject to register.
     *
     * @return
     *      - #ER_OK if successful.
     *      - #ER_BUS_BAD_OBJ_PATH for a bad object path
     */
    QStatus RegisterBusObject(BusObject& obj);

    /**
     * Unregister a BusObject
     *
     * @param object  Object to be unregistered.
     */
    void UnregisterBusObject(BusObject& object);

    /**
     * Get the org.freedesktop.DBus proxy object.
     *
     * @return org.freedesktop.DBus proxy object
     */
    const ProxyBusObject& GetDBusProxyObj();

    /**
     * Get the org.alljoyn.Bus proxy object.
     *
     * @return org.alljoyn.Bus proxy object
     */
    const ProxyBusObject& GetAllJoynProxyObj();

    /**
     * Get the org.alljoyn.Debug proxy object.
     *
     * @return org.alljoyn.Debug proxy object
     */
    const ProxyBusObject& GetAllJoynDebugObj();

    /**
     * Get the unique name of this BusAttachment.
     *
     * @return The unique name of this BusAttachment.
     */
    const qcc::String& GetUniqueName() const;

    /**
     * Get the guid of the local daemon as a string
     *
     * @return GUID of local AllJoyn daemon as a string.
     */
    const qcc::String& GetGlobalGUIDString() const;

    /**
     * Register a signal handler.
     *
     * Signals are forwarded to the signalHandler if sender, interface, member and path
     * qualifiers are ALL met.
     *
     * @param receiver       The object receiving the signal.
     * @param signalHandler  The signal handler method.
     * @param member         The interface/member of the signal.
     * @param srcPath        The object path of the emitter of the signal or NULL for all paths.
     * @return #ER_OK
     */
    QStatus RegisterSignalHandler(MessageReceiver* receiver,
                                  MessageReceiver::SignalHandler signalHandler,
                                  const InterfaceDescription::Member* member,
                                  const char* srcPath);

    /**
     * Unregister a signal handler.
     *
     * Remove the signal handler that was registered with the given parameters.
     *
     * @param receiver       The object receiving the signal.
     * @param signalHandler  The signal handler method.
     * @param member         The interface/member of the signal.
     * @param srcPath        The object path of the emitter of the signal or NULL for all paths.
     * @return #ER_OK
     */
    QStatus UnregisterSignalHandler(MessageReceiver* receiver,
                                    MessageReceiver::SignalHandler signalHandler,
                                    const InterfaceDescription::Member* member,
                                    const char* srcPath);

    /**
     * Unregister all signal and reply handlers for the specified message receiver. This function is
     * intended to be called from within the destructor of a MessageReceiver class instance. It
     * prevents any pending signals or replies from accessing the MessageReceiver after the message
     * receiver has been freed.
     *
     * @param receiver       The message receiver that is being unregistered.
     * @return ER_OK if successful.
     */
    QStatus UnregisterAllHandlers(MessageReceiver* receiver);

    /**
     * Enable peer-to-peer security. This function must be called by applications that want to use
     * secure interfaces. This bus must have been started by calling BusAttachment::Start() before this
     * function is called.
     *
     * @param authMechanisms  The authentication mechanism(s) to use for peer-to-peer authentication.
     *                        If this parameter is NULL peer-to-peer authentication is disabled.
     *
     * @param listener        Passes password and other authentication related requests to the application.
     *
     * @param keyStoreFileName Optional parameter to specify the filename of the default key store.  The
     *                         default value is the applicationName parameter of BusAttachment().
     *
     * @return
     *      - #ER_OK if peer security was enabled.
     *      - #ER_BUS_BUS_NOT_STARTED BusAttachment::Start has not be called
     */
    QStatus EnablePeerSecurity(const char* authMechanisms, AuthListener* listener,
                               const char* keyStoreFileName = NULL);

    /**
     * Register an object that will receive bus event notifications.
     *
     * @param listener  Object instance that will receive bus event notifications.
     */
    virtual void RegisterBusListener(BusListener& listener);

    /**
     * Unregister an object that was previously registered with RegisterBusListener.
     *
     * @param listener  Object instance to un-register as a listener.
     */
    virtual void UnregisterBusListener(BusListener& listener);

    /**
     * Set a key store listener to listen for key store load and store requests.
     * This overrides the internal key store listener.
     *
     * @param listener  The key store listener to set.
     */
    void RegisterKeyStoreListener(KeyStoreListener& listener);

    /**
     * Reloads the key store for this bus attachment. This function would normally only be called in
     * the case where a single key store is shared between multiple bus attachments, possibly by different
     * applications. It is up to the applications to coordinate how and when the shared key store is
     * modified.
     *
     * @return - ER_OK if the key store was succesfully reloaded
     *         - An error status indicating that the key store reload failed.
     */
    QStatus ReloadKeyStore();

    /**
     * Clears all stored keys from the key store. All store keys and authentication information is
     * deleted and cannot be recovered. Any passwords or other credentials will need to be reentered
     * when establishing secure peer connections.
     */
    void ClearKeyStore();

    /**
     * Clear the keys associated with a specific peer identified by its GUID.
     *
     * @param guid  The guid of a remote authenticated peer.
     *
     * @return  - ER_OK if the key was cleared
     *          - ER_UNKNOWN_GUID if there is no peer with the specified GUID
     *          - Other errors
     */
    QStatus ClearKeys(const qcc::String& guid);

    /**
     * Adds a logon entry string for the requested authentication mechanism to the key store. This
     * allows an authenticating server to generate offline authentication credentials for securely
     * logging on a remote peer using a user-name and password credentials pair. This only applies
     * to authentication mechanisms that support a user name + password logon functionality.
     *
     * @param authMechanism The authentication mechanism.
     * @param userName      The user name to use for generating the logon entry.
     * @param password      The password to use for generating the logon entry. If the password is
     *                      NULL the logon entry is deleted from the key store.
     *
     * @return
     *      - #ER_OK if the logon entry was generated.
     *      - #ER_BUS_INVALID_AUTH_MECHANISM if the authentication mechanism does not support
     *                                       logon functionality.
     *      - #ER_BAD_ARG_2 indicates a null string was used as the user name.
     *      - #ER_BAD_ARG_3 indicates a null string was used as the password.
     *      - Other error status codes indicating a failure
     */
    QStatus AddLogonEntry(const char* authMechanism, const char* userName, const char* password);

    /**
     * Request a well-known name.
     * This method is a shortcut/helper that issues an org.freedesktop.DBus.RequestName method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  requestedName  Well-known name being requested.
     * @param[in]  flags          Bitmask of DBUS_NAME_FLAG_* defines (see DBusStd.h)
     *
     * @return
     *      - #ER_OK iff daemon response was received and request was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus RequestName(const char* requestedName, uint32_t flags);

    /**
     * Release a previously requeted well-known name.
     * This method is a shortcut/helper that issues an org.freedesktop.DBus.ReleaseName method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  name          Well-known name being released.
     *
     * @return
     *      - #ER_OK iff daemon response was received amd the name was successfully released.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus ReleaseName(const char* name);

    /**
     * Add a DBus match rule.
     * This method is a shortcut/helper that issues an org.freedesktop.DBus.AddMatch method call to the local daemon.
     *
     * @param[in]  rule  Match rule to be added (see DBus specification for format of this string).
     *
     * @return
     *      - #ER_OK if the AddMatch request was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus AddMatch(const char* rule);

    /**
     * Remove a DBus match rule.
     * This method is a shortcut/helper that issues an org.freedesktop.DBus.RemoveMatch method call to the local daemon.
     *
     * @param[in]  rule  Match rule to be removed (see DBus specification for format of this string).
     *
     * @return
     *      - #ER_OK if the RemoveMatch request was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus RemoveMatch(const char* rule);

    /**
     * Advertise the existence of a well-known name to other (possibly disconnected) AllJoyn daemons.
     *
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.AdvertisedName method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  name          the well-known name to advertise. (Must be owned by the caller via RequestName).
     * @param[in]  transports    Set of transports to use for sending advertisment.
     *
     * @return
     *      - #ER_OK iff daemon response was received and advertise was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus AdvertiseName(const char* name, TransportMask transports);

    /**
     * Stop advertising the existence of a well-known name to other AllJoyn daemons.
     *
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.CancelAdvertiseName method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  name          A well-known name that was previously advertised via AdvertiseName.
     * @param[in]  transports    Set of transports whose name advertisment will be cancelled.
     *
     * @return
     *      - #ER_OK iff daemon response was received and advertisements were sucessfully stopped.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus CancelAdvertiseName(const char* name, TransportMask transports);

    /**
     * Register interest in a well-known name prefix for the purpose of discovery.
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.FindAdvertisedName method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  namePrefix    Well-known name prefix that application is interested in receiving
     *                           BusListener::FoundAdvertisedName notifications about.
     *
     * @return
     *      - #ER_OK iff daemon response was received and discovery was successfully started.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus FindAdvertisedName(const char* namePrefix);

    /**
     * Cancel interest in a well-known name prefix that was previously
     * registered with FindAdvertisedName.  This method is a shortcut/helper
     * that issues an org.alljoyn.Bus.CancelFindAdvertisedName method
     * call to the local daemon and interprets the response.
     *
     * @param[in]  namePrefix    Well-known name prefix that application is no longer interested in receiving
     *                           BusListener::FoundAdvertisedName notifications about.
     *
     * @return
     *      - #ER_OK iff daemon response was received and cancel was successfully completed.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus CancelFindAdvertisedName(const char* namePrefix);

    /**
     * Make a SessionPort available for external BusAttachments to join.
     *
     * Each BusAttachment binds its own set of SessionPorts. Session joiners use the bound session
     * port along with the name of the attachement to create a persistent logical connection (called
     * a Session) with the original BusAttachment.
     *
     * A SessionPort and bus name form a unique identifier that BusAttachments use when joining a
     * session.
     *
     * SessionPort values can be pre-arranged between AllJoyn services and their clients (well-known
     * SessionPorts).
     *
     * Once a session is joined using one of the service's well-known SessionPorts, the service may
     * bind additional SessionPorts (dyanamically) and share these SessionPorts with the joiner over
     * the original session. The joiner can then create additional sessions with the service by
     * calling JoinSession with these dynamic SessionPort ids.
     *
     * @param[in,out] sessionPort      SessionPort value to bind or SESSION_PORT_ANY to allow this method
     *                                 to choose an available port. On successful return, this value
     *                                 contains the chosen SessionPort.
     *
     * @param[in]     opts             Session options that joiners must agree to in order to
     *                                 successfully join the session.
     *
     * @param[in]     listener  Called by the bus when session related events occur.
     *
     * @return
     *      - #ER_OK iff daemon response was received and the bind operation was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus BindSessionPort(SessionPort& sessionPort, const SessionOpts& opts, SessionPortListener& listener);

    /**
     * Cancel an existing port binding.
     *
     * @param[in]   sessionPort    Existing session port to be un-bound.
     *
     * @return
     *      - #ER_OK iff daemon response was received and the bind operation was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus UnbindSessionPort(SessionPort sessionPort);

    /**
     * Join a session.
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.JoinSession method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  sessionHost      Bus name of attachment that is hosting the session to be joined.
     * @param[in]  sessionPort      SessionPort of sessionHost to be joined.
     * @param[in]  listener         Optional listener called when session related events occur. May be NULL.
     * @param[out] sessionId        Unique identifier for session.
     * @param[in,out] opts          Session options.
     *
     * @return
     *      - #ER_OK iff daemon response was received and the session was successfully joined.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus JoinSession(const char* sessionHost, SessionPort sessionPort, SessionListener* listener,
                        SessionId& sessionId, SessionOpts& opts);

    /**
     * Join a session.
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.JoinSession method call to the local daemon
     * and interprets the response.
     *
     * This call executes asynchronously. When the JoinSession response is received, the callback will be called.
     *
     * @param[in]  sessionHost      Bus name of attachment that is hosting the session to be joined.
     * @param[in]  sessionPort      SessionPort of sessionHost to be joined.
     * @param[in]  listener         Optional listener called when session related events occur. May be NULL.
     * @param[in]  opts             Session options.
     * @param[in]  callback         Called when JoinSession response is received.
     * @param[in]  context          User defined context which will be passed as-is to callback.
     *
     * @return
     *      - #ER_OK iff method call to local daemon response was was successful.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus JoinSessionAsync(const char* sessionHost,
                             SessionPort sessionPort,
                             SessionListener* listener,
                             const SessionOpts& opts,
                             BusAttachment::JoinSessionAsyncCB* callback,
                             void* context = NULL);

    /**
     * Set the SessionListener for an existing sessionId.
     * Calling this method will override the listener set by a previoius call to SetSessionListener or any
     * listener specified in JoinSession.
     *
     * @param sessionId    The session id of an existing session.
     * @param listener     The SessionListener to associate with the session. May be NULL to clear previous listener.
     * @return  ER_OK if successful.
     */
    QStatus SetSessionListener(SessionId sessionId, SessionListener* listener);

    /**
     * Leave an existing session.
     * This method is a shortcut/helper that issues an org.alljoyn.Bus.LeaveSession method call to the local daemon
     * and interprets the response.
     *
     * @param[in]  sessionId     Session id.
     *
     * @return
     *      - #ER_OK iff daemon response was received and the leave operation was successfully completed.
     *      - #ER_BUS_NOT_CONNECTED if a connection has not been made with a local bus.
     *      - Other error status codes indicating a failure.
     */
    QStatus LeaveSession(const SessionId& sessionId);

    /**
     * Get the file descriptor for a raw (non-message based) session.
     *
     * @param sessionId   Id of an existing streamming session.
     * @param sockFd      [OUT] Socket file descriptor for session.
     *
     * @return ER_OK if successful.
     */
    QStatus GetSessionFd(SessionId sessionId, qcc::SocketFd& sockFd);

    /**
     * Determine whether a given well-known name exists on the bus.
     * This method is a shortcut/helper that issues an org.freedesktop.DBus.NameHasOwner method call to the daemon
     * and interprets the response.
     *
     * @param[in]  name       The well known name that the caller is inquiring about.
     * @param[out] hasOwner   If return is ER_OK, indicates whether name exists on the bus.
     *                        If return is not ER_OK, param is not modified.
     * @return
     *      - #ER_OK if name ownership was able to be determined.
     *      - An error status otherwise
     */
    QStatus NameHasOwner(const char* name, bool& hasOwner);

    /**
     * Get the peer GUID for this peer or an authenticated remote peer. Peer GUIDs are used by the
     * authentication mechanisms to uniquely and identify a remote application instance. The peer
     * GUID for a remote peer is only available if the remote peer has been authenticated.
     *
     * @param name  Name of a remote peer or NULL to get the local (our) peer GUID.
     * @param guid  Returns the guid for the local or remote peer depending on the value of name.
     *
     * @return
     *      - #ER_OK if the requested GUID was obtained.
     *      - An error status otherwise.
     */
    QStatus GetPeerGUID(const char* name, qcc::String& guid);

    /**
     * This sets the debug level of the local AllJoyn daemon if that daemon
     * was built in debug mode.
     *
     * The debug level can be set for individual subsystems or for "ALL"
     * subsystems.  Common subsystems are "ALLJOYN" for core AllJoyn code,
     * "ALLJOYN_OBJ" for the sessions management code, "ALLJOYN_BT" for the
     * Bluetooth subsystem, "ALLJOYN_BTC" for the Bluetooth topology manager,
     * and "ALLJOYN_NS" for the TCP name services.  Debug levels for specific
     * subsystems override the setting for "ALL" subsystems.  For example if
     * "ALL" is set to 7, but "ALLJOYN_OBJ" is set to 1, then detailed debug
     * output will be generated for all subsystems expcept for "ALLJOYN_OBJ"
     * which will only generate high level debug output.  "ALL" defaults to 0
     * which is off, or no debug output.
     *
     * The debug output levels are actually a bit field that controls what
     * output is generated.  Those bit fields are described below:
     *
     *     - 0x1: High level debug prints (these debug printfs are not common)
     *     - 0x2: Normal debug prints (these debug printfs are common)
     *     - 0x4: Function call tracing (these debug printfs are used
     *            sporadically)
     *     - 0x8: Data dump (really only used in the "SOCKET" module - can
     *            generate a *lot* of output)
     *
     * Typically, when enabling debug for a subsystem, the level would be set
     * to 7 which enables High level debug, normal debug, and function call
     * tracing.  Setting the level 0, forces debug output to be off for the
     * specified subsystem.
     *
     * @param module    name of the module to generate debug output
     * @param level     debug level to set for the module
     *
     * @return
     *     - #ER_OK if debug request was successfully sent to the AllJoyn
     *       daemon.
     *     - #ER_BUS_NO_SUCH_OBJECT if daemon was not built in debug mode.
     */
    QStatus SetDaemonDebug(const char* module, uint32_t level);

    /**
     * Returns the current non-absolute real-time clock used internally by AllJoyn. This value can be
     * compared with the timestamps on messages to calculate the time since a timestamped message
     * was sent.
     *
     * @return  The current timestamp in milliseconds.
     */
    static uint32_t GetTimestamp();

    /// @cond ALLJOYN_DEV
    /**
     * @internal
     * Class for internal state for bus attachment.
     */
    class Internal;

    /**
     * @internal
     * Get a pointer to the internal BusAttachment state.
     *
     * @return A pointer to the internal state.
     */
    Internal& GetInternal() { return *busInternal; }

    /**
     * @internal
     * Get a pointer to the internal BusAttachment state.
     *
     * @return A pointer to the internal state.
     */
    const Internal& GetInternal() const { return *busInternal; }
    /// @endcond
  protected:
    /// @cond ALLJOYN_DEV
    /**
     * @internal
     * Construct a BusAttachment.
     *
     * @param internal  Internal state.
     */
    BusAttachment(Internal* internal);
    /// @endcond
  private:

    /**
     * Assignment operator is private.
     */
    BusAttachment& operator=(const BusAttachment& other) { return *this; }

    /**
     * Copy constructor is private.
     */
    BusAttachment(const BusAttachment& other) { }

    /**
     * JoinSession method_reply handler. (Internal use only)
     */
    void JoinSessionMethodCB(Message& message, void* context);

    bool isStarted;           /**< Indicates if the bus has been started */
    bool isStopping;          /**< Indicates Stop has been called */
    Internal* busInternal;    /**< Internal state information */
};

}

#endif
