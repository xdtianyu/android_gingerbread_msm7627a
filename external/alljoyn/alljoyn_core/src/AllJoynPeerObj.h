/**
 * @file
 * The AllJoynPeer object implements interfaces for AllJoyn functionality including header compression and
 * security.
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
#ifndef _ALLJOYN_ALLJOYNPEEROBJ_H
#define _ALLJOYN_ALLJOYNPEEROBJ_H

#include <qcc/platform.h>

#include <map>
#include <queue>

#include <qcc/GUID.h>
#include <qcc/String.h>
#include <qcc/Thread.h>

#include <alljoyn/BusObject.h>
#include <alljoyn/Message.h>

#include "BusEndpoint.h"
#include "RemoteEndpoint.h"
#include "PeerState.h"

namespace ajn {


/* Forward declaration */
class SASLEngine;
class BusAttachment;
class AuthListener;

/**
 * The AllJoynPeer object @c /org/alljoyn/Bus/Peer implements interfaces that provide AllJoyn
 * functionality.
 *
 * %AllJoynPeerObj inherits from BusObject
 */
class AllJoynPeerObj : public BusObject, public BusListener {
  public:

    /**
     * Constructor
     *
     * @param bus          Bus to associate with /org/alljoyn/Bus/Peer message handler.
     */
    AllJoynPeerObj(BusAttachment& bus);

    /**
     * Initialize and register this AllJoynPeerObj instance.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Init();

    /**
     * Called when object is successfully registered.
     */
    void ObjectRegistered(void);

    /**
     * This function is called when a message with a compressed header has been received but the
     * compression token is unknown. A method call is made to the remote peer to obtain the
     * expansion rule for the compression token.
     *
     * @param msg     The message to be expanded.
     * @param sender  The remote endpoint that the compressed message was received on.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus RequestHeaderExpansion(Message& msg, RemoteEndpoint* sender);

    /**
     * Setup for peer-to-peer authentication. The authentication mechanisms listed can only be used
     * if they are already registered with bus. The authentication mechanisms names are separated by
     * space characters.
     *
     * @param authMechanisms   The names of the authentication mechanisms to set
     * @param listener         Required for authentication mechanisms that require interation with the user
     *                         or application. Can be NULL if not required.
     */
    void SetupPeerAuthentication(const qcc::String& authMechanisms, AuthListener* listener) {
        peerAuthMechanisms = authMechanisms;
        peerAuthListener = listener;
    }

    /**
     * Secure the connection to a remote peer. A secure connection is authenticated and has
     * established a session key with a remote peer.
     *
     * @param busName   The bus name of the remote peer we are securing.
     * @param forceAuth If true, forces an re-authentication even if the peer connection is already
     *                  authenticated.
     *
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus SecurePeerConnection(const qcc::String& busName, bool forceAuth = false);

    /**
     * Reports a security failure. This would normally be due to stale or expired keys.
     *
     * @param msg     The message that had the security violation.
     * @param status  A status code indicating the type of security violation.
     *
     * @return   - ER_OK if the security failure was handled.
     *           - ER_BUS_SECURITY_FATAL if the security failure is not recoverable.
     */
    void HandleSecurityViolation(Message& msg, QStatus status);

    /**
     * Start AllJoynPeerObj.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Start();

    /**
     * Stop AllJoynPeerObj.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Stop();

    /**
     * Although AllJoynPeerObj is not a thread it contains threads that may need to be joined.
     *
     * @return
     *      - ER_OK if successful.
     *      - An error status otherwise
     */
    QStatus Join();

    /**
     * Destructor
     */
    ~AllJoynPeerObj();

  private:

    /**
     * Header decompression method'
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void GetExpansion(const InterfaceDescription::Member* member, Message& msg);

    /**
     * ExchangeGuids method call handler
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void ExchangeGuids(const InterfaceDescription::Member* member, Message& msg);

    /**
     * GenSessionKey method call handler
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void GenSessionKey(const InterfaceDescription::Member* member, Message& msg);

    /**
     * ExchangeGroupKeys method call handler
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void ExchangeGroupKeys(const InterfaceDescription::Member* member, Message& msg);

    /**
     * Authentication challenge method call handler
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void AuthChallenge(const InterfaceDescription::Member* member, Message& msg);

    /**
     * Process a message to advance an authentication conversation.
     *
     * @param msg  The auth challenge message
     */
    void AuthAdvance(Message& msg);

    /**
     * Process a message to advance an authentication conversation.
     *
     * @param msg  The auth challenge message
     */
    void ExpandHeader(Message& msg, const qcc::String& sendingEndpoint);

    /**
     * Session key generation algorithm.
     *
     * @param peerState  The peer state object where the session key will be store.
     * @param seed       A seed string negotiated by the peers.
     * @param verifier   A verifier string that used to verify the session key.
     */
    QStatus KeyGen(PeerState& peerState, qcc::String seed, qcc::String& verifier);

    /**
     * Get a property from this object
     * @param ifcName the name of the interface
     * @param propName the name of the property requested
     * @param[out] val return the value stored in the property.
     * @return
     *      - ER_OK if property found
     *      - ER_BUS_NO_SUCH_PROPERTY if property not found
     */
    QStatus Get(const char* ifcName, const char* propName, MsgArg& val);

    /**
     * Called by the bus when the ownership of any well-known name changes.
     *
     * @param busName        The well-known name that has changed.
     * @param previousOwner  The unique name that previously owned the name or NULL if there was no previous owner.
     * @param newOwner       The unique name that now owns the name or NULL if the there is no new owner.
     */
    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner);

    /**
     * AcceptSession method handler called when the local daemon asks permission to accpet a JoinSession request.
     *
     * @param member  The member that was called
     * @param msg     The method call message
     */
    void AcceptSession(const InterfaceDescription::Member* member, Message& msg);

    /**
     * The peer-to-peer authentication mechanisms available to this object
     */
    qcc::String peerAuthMechanisms;

    /**
     * The listener for interacting with the application
     */
    AuthListener* peerAuthListener;

    /**
     * Peer endpoints currently in an authentication conversation
     */
    std::map<qcc::String, SASLEngine*> conversations;

    /**
     * Short term lock to protect the peer object.
     */
    qcc::Mutex lock;

    /**
     * Long term lock held while securing a connection. This serializes all client initiated authentications.
     */
    static qcc::Mutex clientLock;

    /**
     * Types of request that can be queued.
     */
    typedef enum {
        SECURE_PEER,
        AUTH_CHALLENGE,
        EXPAND_HEADER,
        ACCEPT_SESSION
    } RequestType;

    /**
     * Class for handling deferred peer requests.
     */
    class RequestThread : public qcc::Thread {
      public:
        RequestThread(AllJoynPeerObj& peerObj) : qcc::Thread("RequestThread"), peerObj(peerObj) { }

        QStatus QueueRequest(Message& msg, RequestType reqType, const qcc::String data = "");

      private:
        qcc::ThreadReturn STDCALL Run(void* args);

        struct Request {
            Message msg;
            RequestType reqType;
            const qcc::String data;
        };

        qcc::Mutex lock;
        AllJoynPeerObj& peerObj;
        std::queue<Request> queue;

    };

    RequestThread requestThread;

};

}

#endif
