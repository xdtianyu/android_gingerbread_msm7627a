/**
 * @file
 *
 * This file implements the org.alljoyn.Bus.Peer.* interfaces
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

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Crypto.h>
#include <qcc/Util.h>
#include <qcc/StringSink.h>
#include <qcc/StringSource.h>

#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/MessageReceiver.h>

#include "SessionInternal.h"
#include "KeyStore.h"
#include "BusEndpoint.h"
#include "PeerState.h"
#include "CompressionRules.h"
#include "AllJoynPeerObj.h"
#include "SASLEngine.h"
#include "AllJoynCrypto.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

/*
 * Mutex to serialize all authentications.
 */
qcc::Mutex AllJoynPeerObj::clientLock;

AllJoynPeerObj::AllJoynPeerObj(BusAttachment& bus) : BusObject(bus, org::alljoyn::Bus::Peer::ObjectPath, false), requestThread(*this)
{
    /* Add org.alljoyn.Bus.Peer.HeaderCompression interface */
    {
        const InterfaceDescription* ifc = bus.GetInterface(org::alljoyn::Bus::Peer::HeaderCompression::InterfaceName);
        if (ifc) {
            AddInterface(*ifc);
            AddMethodHandler(ifc->GetMember("GetExpansion"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::GetExpansion));
        }
    }
    /* Add org.alljoyn.Bus.Peer.Authentication interface */
    {
        const InterfaceDescription* ifc = bus.GetInterface(org::alljoyn::Bus::Peer::Authentication::InterfaceName);
        if (ifc) {
            AddInterface(*ifc);
            AddMethodHandler(ifc->GetMember("AuthChallenge"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::AuthChallenge));
            AddMethodHandler(ifc->GetMember("ExchangeGuids"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::ExchangeGuids));
            AddMethodHandler(ifc->GetMember("GenSessionKey"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::GenSessionKey));
            AddMethodHandler(ifc->GetMember("ExchangeGroupKeys"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::ExchangeGroupKeys));
        }
    }
    /* Add org.alljoyn.Bus.Peer.Session interface */
    {
        const InterfaceDescription* ifc = bus.GetInterface(org::alljoyn::Bus::Peer::Session::InterfaceName);
        if (ifc) {
            AddInterface(*ifc);
            AddMethodHandler(ifc->GetMember("AcceptSession"), static_cast<MessageReceiver::MethodHandler>(&AllJoynPeerObj::AcceptSession));
        }
    }
}

QStatus AllJoynPeerObj::Start()
{
    bus.RegisterBusListener(*this);
    return ER_OK;
}

QStatus AllJoynPeerObj::Stop()
{
    if (requestThread.IsRunning()) {
        return requestThread.Stop();
    } else {
        return ER_OK;
    }
}

QStatus AllJoynPeerObj::Join()
{
    QStatus status = requestThread.Join();

    lock.Lock();
    std::map<qcc::String, SASLEngine*>::iterator iter = conversations.begin();
    while (iter != conversations.end()) {
        delete iter->second;
        ++iter;
    }
    conversations.clear();
    lock.Unlock();

    bus.UnregisterBusListener(*this);
    return status;
}

AllJoynPeerObj::~AllJoynPeerObj()
{
    if (requestThread.IsRunning()) {
        requestThread.Stop();
        requestThread.Join();
    }
}

QStatus AllJoynPeerObj::Init()
{
    QStatus status = bus.RegisterBusObject(*this);
    QCC_DbgHLPrintf(("AllJoynPeerObj::Init %s", QCC_StatusText(status)));
    return status;
}

void AllJoynPeerObj::ObjectRegistered(void)
{
    /* Must call base class */
    BusObject::ObjectRegistered();

}

void AllJoynPeerObj::GetExpansion(const InterfaceDescription::Member* member, Message& msg)
{
    uint32_t token = msg->GetArg(0)->v_uint32;
    MsgArg replyArg;
    QStatus status = msg->GetExpansion(token, replyArg);
    if (status == ER_OK) {
        status = MethodReply(msg, &replyArg, 1);
        if (ER_OK != status) {
            QCC_LogError(status, ("Failed to send GetExpansion reply"));
        }
    } else {
        MethodReply(msg, status);
    }
}

QStatus AllJoynPeerObj::RequestHeaderExpansion(Message& msg, RemoteEndpoint* sender)
{
    assert(sender == bus.GetInternal().GetRouter().FindEndpoint(msg->GetRcvEndpointName()));
    return requestThread.QueueRequest(msg, EXPAND_HEADER, sender->GetRemoteName());
}

/**
 * How long (in milliseconds) to wait for a response to an expansion request.
 */
#define EXPANSION_TIMEOUT   10000

void AllJoynPeerObj::ExpandHeader(Message& msg, const qcc::String& receivedFrom)
{
    QStatus status = ER_OK;
    uint32_t token = msg->GetCompressionToken();
    const HeaderFields* expFields = bus.GetInternal().GetCompressionRules().GetExpansion(token);
    if (!expFields) {
        Message replyMsg(bus);
        MsgArg arg("u", token);
        /*
         * The endpoint the message was received on knows the expansion rule for the token we just received.
         */
        ProxyBusObject remotePeerObj(bus, receivedFrom.c_str(), org::alljoyn::Bus::Peer::ObjectPath, 0);
        const InterfaceDescription* ifc = bus.GetInterface(org::alljoyn::Bus::Peer::HeaderCompression::InterfaceName);
        if (ifc == NULL) {
            status = ER_BUS_NO_SUCH_INTERFACE;
        }
        if (status == ER_OK) {
            remotePeerObj.AddInterface(*ifc);
            status = remotePeerObj.MethodCall(*(ifc->GetMember("GetExpansion")), &arg, 1, replyMsg, EXPANSION_TIMEOUT);
        }
        if (status == ER_OK) {
            status = replyMsg->AddExpansionRule(token, replyMsg->GetArg(0));
            if (status == ER_OK) {
                expFields = bus.GetInternal().GetCompressionRules().GetExpansion(token);
                if (!expFields) {
                    status = ER_BUS_HDR_EXPANSION_INVALID;
                }
            }
        }
    }
    /*
     * If we have an expansion rule we can decompress the message.
     */
    if (status == ER_OK) {
        Router& router = bus.GetInternal().GetRouter();
        BusEndpoint* sender = router.FindEndpoint(msg->GetRcvEndpointName());
        if (sender) {
            /*
             * Expand the compressed fields. Don't overwrite headers we received.
             */
            for (size_t id = 0; id < ArraySize(msg->hdrFields.field); id++) {
                if (HeaderFields::Compressible[id] && (msg->hdrFields.field[id].typeId == ALLJOYN_INVALID)) {
                    msg->hdrFields.field[id] = expFields->field[id];
                }
            }
            /*
             * Initialize ttl from the message header.
             */
            if (msg->hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].typeId != ALLJOYN_INVALID) {
                msg->ttl = msg->hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].v_uint16;
            } else {
                msg->ttl = 0;
            }
            msg->hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].Clear();
            /*
             * we have succesfully expanded the message so now it can be routed.
             */
            router.PushMessage(msg, *sender);
        }
    } else {
        QCC_LogError(status, ("Failed to expand message %s", msg->Description().c_str()));
    }

}

/*
 * Get a property
 */
QStatus AllJoynPeerObj::Get(const char* ifcName, const char* propName, MsgArg& val)
{
    QStatus status = ER_BUS_NO_SUCH_PROPERTY;

    if (strcmp(ifcName, org::alljoyn::Bus::Peer::Authentication::InterfaceName) == 0) {
        if (strcmp("Mechanisms", propName) == 0) {
            val.typeId = ALLJOYN_STRING;
            val.v_string.str = peerAuthMechanisms.c_str();
            val.v_string.len = peerAuthMechanisms.size();
            status = ER_OK;
        }
    }
    return status;
}

void AllJoynPeerObj::ExchangeGroupKeys(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status;
    PeerStateTable* peerStateTable = bus.GetInternal().GetPeerStateTable();
    KeyBlob key;
    KeyBlob nonce;

    /*
     * We expect to know the peer that is making this method call
     */
    if (peerStateTable->IsKnownPeer(msg->GetSender())) {
        StringSource src(msg->GetArg(0)->v_scalarArray.v_byte, msg->GetArg(0)->v_scalarArray.numElements);
        status = key.Load(src);
        if (status == ER_OK) {
            status = nonce.Load(src);
        }
        if (status == ER_OK) {
            /*
             * Tag the group key with the auth mechanism used by ExchangeGroupKeys then store it.
             */
            key.SetTag(msg->GetAuthMechanism());
            peerStateTable->GetPeerState(msg->GetSender())->SetKeyAndNonce(key, nonce, PEER_GROUP_KEY);
            /*
             * Return the local group key.
             */
            peerStateTable->GetGroupKeyAndNonce(key, nonce);
            StringSink snk;
            key.Store(snk);
            nonce.Store(snk);
            MsgArg replyArg("ay", snk.GetString().size(), snk.GetString().data());
            MethodReply(msg, &replyArg, 1);
        }
    } else {
        status = ER_BUS_NO_PEER_GUID;
    }
    if (status != ER_OK) {
        MethodReply(msg, status);
    }
}

void AllJoynPeerObj::ExchangeGuids(const InterfaceDescription::Member* member, Message& msg)
{
    qcc::GUID remotePeerGuid(msg->GetArg(0)->v_string.str);
    qcc::String localGuidStr = bus.GetInternal().GetKeyStore().GetGuid();
    if (!localGuidStr.empty()) {
        PeerState peerState = bus.GetInternal().GetPeerStateTable()->GetPeerState(msg->GetSender());

        QCC_DbgHLPrintf(("ExchangeGuids Local %s", localGuidStr.c_str()));
        QCC_DbgHLPrintf(("ExchangeGuids Remote %s", remotePeerGuid.ToString().c_str()));
        /*
         * Associate the remote peer GUID with the sender peer state.
         */
        peerState->SetGuid(remotePeerGuid);
        MsgArg replyArg("s", localGuidStr.c_str());
        MethodReply(msg, &replyArg, 1);
    } else {
        MethodReply(msg, ER_BUS_NO_PEER_GUID);
    }
}

/*
 * These two lengths are used in RFC 5246
 */
#define VERIFIER_LEN  12
#define NONCE_LEN     28

QStatus AllJoynPeerObj::KeyGen(PeerState& peerState, String seed, qcc::String& verifier)
{
    QStatus status;
    KeyStore& keyStore = bus.GetInternal().GetKeyStore();
    static const char* label = "session key";
    KeyBlob masterSecret;

    status = keyStore.GetKey(peerState->GetGuid(), masterSecret);
    if (status == ER_OK) {
        size_t keylen = Crypto_AES::AES128_SIZE + ajn::Crypto::NonceBytes + VERIFIER_LEN;
        uint8_t* keymatter = new uint8_t[keylen];
        /*
         * Session key and other key matter is generated using the procedure described in RFC 5246
         */
        Crypto_PseudorandomFunction(masterSecret, label, seed, keymatter, keylen);
        KeyBlob sessionKey(keymatter, Crypto_AES::AES128_SIZE, KeyBlob::AES);
        KeyBlob nonce(keymatter + Crypto_AES::AES128_SIZE, ajn::Crypto::NonceBytes, KeyBlob::GENERIC);
        /*
         * Tag the session key with auth mechanism tag from the master secret
         */
        sessionKey.SetTag(masterSecret.GetTag());
        /*
         * Store session key and nonce in the peer state.
         */
        peerState->SetKeyAndNonce(sessionKey, nonce, PEER_SESSION_KEY);
        /*
         * Return verifier string
         */
        verifier = BytesToHexString(keymatter + Crypto_AES::AES128_SIZE + ajn::Crypto::NonceBytes, VERIFIER_LEN);
        delete [] keymatter;
        QCC_DbgHLPrintf(("KeyGen verifier %s", verifier.c_str()));
    }
    /*
     * Store any changes to the key store.
     */
    keyStore.Store();
    return status;
}

void AllJoynPeerObj::GenSessionKey(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status;
    PeerState peerState = bus.GetInternal().GetPeerStateTable()->GetPeerState(msg->GetSender());
    qcc::GUID remotePeerGuid(msg->GetArg(0)->v_string.str);
    qcc::GUID localPeerGuid(msg->GetArg(1)->v_string.str);
    /*
     * Check that target GUID is our GUID.
     */
    if (bus.GetInternal().GetKeyStore().GetGuid() != localPeerGuid.ToString()) {
        MethodReply(msg, ER_BUS_NO_PEER_GUID);
    } else {
        qcc::String nonce = RandHexString(NONCE_LEN);
        qcc::String verifier;
        status = KeyGen(peerState, msg->GetArg(2)->v_string.str + nonce, verifier);
        if (status == ER_OK) {
            MsgArg replyArgs[2];
            replyArgs[0].Set("s", nonce.c_str());
            replyArgs[1].Set("s", verifier.c_str());
            MethodReply(msg, replyArgs, ArraySize(replyArgs));
        } else {
            MethodReply(msg, status);
        }
    }
}

void AllJoynPeerObj::AuthAdvance(Message& msg)
{
    QStatus status = ER_OK;
    ajn::SASLEngine* sasl;
    ajn::SASLEngine::AuthState authState;
    qcc::String outStr;
    qcc::String sender = msg->GetSender();
    qcc::String mech;

    /*
     * There can be multiple authentication conversations going on simultaneously between the
     * current peer and other remote peers but only one conversation between each pair.
     *
     * Check for existing conversation and allocate a new SASL engine if we need one
     */
    lock.Lock();
    sasl = conversations[sender];
    conversations.erase(sender);
    lock.Unlock();

    if (!sasl) {
        sasl = new SASLEngine(bus, ajn::AuthMechanism::CHALLENGER, peerAuthMechanisms.c_str(), sender.c_str(), peerAuthListener);
        qcc::String localGuidStr = bus.GetInternal().GetKeyStore().GetGuid();
        if (!localGuidStr.empty()) {
            sasl->SetLocalId(localGuidStr);
        } else {
            status = ER_BUS_NO_PEER_GUID;
        }
    }
    /*
     * Move the authentication conversation forward.
     */
    if (status == ER_OK) {
        status = sasl->Advance(msg->GetArg(0)->v_string.str, outStr, authState);
    }
    /*
     * If auth conversation was sucessful store the master secret in the key store.
     */
    if ((status == ER_OK) && (authState == SASLEngine::ALLJOYN_AUTH_SUCCESS)) {
        KeyBlob masterSecret;
        KeyStore& keyStore = bus.GetInternal().GetKeyStore();
        status = sasl->GetMasterSecret(masterSecret);
        mech = sasl->GetMechanism();
        if (status == ER_OK) {
            qcc::GUID remotePeerGuid(sasl->GetRemoteId());
            /* Tag the master secret with the auth mechanism used to generate it */
            masterSecret.SetTag(mech);
            status = keyStore.AddKey(remotePeerGuid, masterSecret);
        }
        /*
         * Report the succesful authentication to allow application to clear UI etc.
         */
        if ((status == ER_OK) && peerAuthListener) {
            peerAuthListener->AuthenticationComplete(mech.c_str(), sender.c_str(), true /* success */);
        }
        /*
         * All done with this SASL engine.
         */
        delete sasl;
        sasl = NULL;
    }
    if (status != ER_OK) {
        /*
         * Report the failed authentication to allow application to clear UI etc.
         */
        if (peerAuthListener) {
            peerAuthListener->AuthenticationComplete(mech.c_str(), sender.c_str(), false /* failure */);
        }
        /*
         * All done with this SASL engine.
         */
        delete sasl;
        sasl = NULL;
        /*
         * Let remote peer know the authentication failed.
         */
        MethodReply(msg, status);
    } else {
        MsgArg replyMsg("s", outStr.c_str());
        MethodReply(msg, &replyMsg, 1);
    }
    /*
     * If we are not done put the sasl engine on the conversations list.
     */
    if (sasl) {
        lock.Lock();
        conversations[sender] = sasl;
        lock.Unlock();
    }
}

void AllJoynPeerObj::AuthChallenge(const ajn::InterfaceDescription::Member* member, ajn::Message& msg)
{
    /*
     * Cannot authenticate if we don't have any authentication mechanisms
     */
    if (peerAuthMechanisms.empty()) {
        MethodReply(msg, ER_BUS_NO_AUTHENTICATION_MECHANISM);
        return;
    }
    /*
     * Authentication may involve user interaction or be computationally expensive so cannot be
     * allowed to block the read thread.
     */
    QStatus status = requestThread.QueueRequest(msg, AUTH_CHALLENGE);
    if (status != ER_OK) {
        MethodReply(msg, status);
    }
}

/*
 * A long timeout to allow for possible PIN entry
 */
#define AUTH_TIMEOUT      60000
#define DEFAULT_TIMEOUT   10000

QStatus AllJoynPeerObj::SecurePeerConnection(const qcc::String& busName, bool forceAuth)
{
    QStatus status;
    ajn::SASLEngine::AuthState authState;
    PeerStateTable* peerStateTable = bus.GetInternal().GetPeerStateTable();
    PeerState peerState = peerStateTable->GetPeerState(busName);
    qcc::String mech;
    const InterfaceDescription* ifc = bus.GetInterface(org::alljoyn::Bus::Peer::Authentication::InterfaceName);
    if (ifc == NULL) {
        return ER_BUS_NO_SUCH_INTERFACE;
    }

    /*
     * Clear the keys if we are forcing authentication.
     */
    if (forceAuth) {
        peerState->ClearKeys();
    }
    /*
     * Simply return if the peer is already secured.
     */
    if (peerState->IsSecure()) {
        return ER_OK;
    }
    /*
     * Cannot authenticate if we don't have an authentication mechanism
     */
    if (peerAuthMechanisms.empty()) {
        return ER_BUS_NO_AUTHENTICATION_MECHANISM;
    }

    ProxyBusObject remotePeerObj(bus, busName.c_str(), org::alljoyn::Bus::Peer::ObjectPath, 0);
    remotePeerObj.AddInterface(*ifc);

    /*
     * Exchange GUIDs with the peer, this will get us the GUID of the remote peer and also the
     * unique bus name from which we can determine if we have already have a session key, a
     * master secret or if we have to start an authentication conversation.
     */
    qcc::String localGuidStr = bus.GetInternal().GetKeyStore().GetGuid();
    MsgArg arg("s", localGuidStr.c_str());
    Message replyMsg(bus);
    status = remotePeerObj.MethodCall(*(ifc->GetMember("ExchangeGuids")), &arg, 1, replyMsg, DEFAULT_TIMEOUT);
    if (status != ER_OK) {
        /*
         * ER_BUS_REPLY_IS_ERROR_MESSAGE has a specific meaning in the public API an should not be
         * propogated to the caller from this context.
         */
        if (status == ER_BUS_REPLY_IS_ERROR_MESSAGE) {
            if (strcmp(replyMsg->GetErrorName(), "org.freedesktop.DBus.Error.ServiceUnknown") == 0) {
                status = ER_BUS_NO_SUCH_OBJECT;
            } else {
                status = ER_AUTH_FAIL;
            }
        }
        QCC_LogError(status, ("ExchangeGuids failed"));
        return status;
    }
    const qcc::String sender = replyMsg->GetSender();
    /*
     * Extract the remote guid from the message
     */
    qcc::GUID remotePeerGuid(replyMsg->GetArg(0)->v_string.str);
    qcc::String remoteGuidStr = remotePeerGuid.ToString();

    QCC_DbgHLPrintf(("ExchangeGuids Local %s", localGuidStr.c_str()));
    QCC_DbgHLPrintf(("ExchangeGuids Remote %s", remoteGuidStr.c_str()));
    /*
     * Serialize authentication conversations
     */
    clientLock.Lock();
    /*
     * Now we have the unique bus name in the reply try again to find out if we have a session key
     * for this peer. Do this after we have obtained the lock in case another thread has done an
     * authentication since we last checked for the session key above.
     */
    peerState = peerStateTable->GetPeerState(sender, busName);
    peerState->SetGuid(remotePeerGuid);
    /*
     * Clear the keys if we are forcing authentication.
     */
    if (forceAuth) {
        peerState->ClearKeys();
    }
    /*
     * We can now return if the peer is secured.
     */
    if (peerState->IsSecure()) {
        clientLock.Unlock();
        return ER_OK;
    }
    /*
     * The bus allows a peer to send signals and make method calls to itself. If we are securing the
     * local peer we obviously don't need to authenticate but we must initialize a peer state object
     * with a session key and group key.
     */
    if (remoteGuidStr == localGuidStr) {
        QCC_DbgHLPrintf(("Securing local peer to itself"));
        KeyBlob key;
        KeyBlob nonce;
        /* Use the local peer's GROUP key */
        peerStateTable->GetGroupKeyAndNonce(key, nonce);
        key.SetTag("SELF");
        peerState->SetKeyAndNonce(key, nonce, PEER_GROUP_KEY);
        /* Allocate a random session key and related nonce */
        key.Rand(Crypto_AES::AES128_SIZE, KeyBlob::AES);
        nonce.Rand(32, KeyBlob::GENERIC);
        key.SetTag("SELF");
        peerState->SetKeyAndNonce(key, nonce, PEER_SESSION_KEY);
        /* Record in the peer state that this peer is the local peer */
        peerState->isLocalPeer = true;
        clientLock.Unlock();
        return ER_OK;
    }

    assert(!peerState->isLocalPeer);

    bool firstPass = true;
    do {
        /*
         * Try to load the master secret for the remote peer. It is possible that the master secret
         * has expired or been deleted either locally or remotely so if we fail to establish a
         * session key on the first pass we start an authentication conversation to establish a new
         * master secret.
         */
        if (!bus.GetInternal().GetKeyStore().HasKey(remotePeerGuid)) {
            status = ER_AUTH_FAIL;
        } else {
            /*
             * Generate a random string - this is the local half of the seed string.
             */
            qcc::String nonce = RandHexString(NONCE_LEN);
            /*
             * Send GenSessionKey message to remote peer.
             */
            MsgArg args[3];
            args[0].Set("s", localGuidStr.c_str());
            args[1].Set("s", remoteGuidStr.c_str());
            args[2].Set("s", nonce.c_str());
            status = remotePeerObj.MethodCall(*(ifc->GetMember("GenSessionKey")), args, ArraySize(args), replyMsg, DEFAULT_TIMEOUT);
            if (status == ER_OK) {
                qcc::String verifier;
                /*
                 * The response completes the seed string so we can generate the session key.
                 */
                status = KeyGen(peerState, nonce + replyMsg->GetArg(0)->v_string.str, verifier);
                if ((status == ER_OK) && (verifier != replyMsg->GetArg(1)->v_string.str)) {
                    status = ER_AUTH_FAIL;
                }
            }
        }
        if ((status == ER_OK) || !firstPass) {
            break;
        }
        /*
         * Initiaize the SASL engine as responder (i.e. client)
         */
        SASLEngine sasl(bus, ajn::AuthMechanism::RESPONDER, peerAuthMechanisms, busName.c_str(), peerAuthListener);
        sasl.SetLocalId(localGuidStr);
        qcc::String inStr;
        qcc::String outStr;
        status = sasl.Advance(inStr, outStr, authState);
        while (status == ER_OK) {
            Message replyMsg(bus);
            MsgArg arg("s", outStr.c_str());
            status = remotePeerObj.MethodCall(*(ifc->GetMember("AuthChallenge")), &arg, 1, replyMsg, AUTH_TIMEOUT);
            if (status == ER_OK) {
                if (authState == SASLEngine::ALLJOYN_AUTH_SUCCESS) {
                    break;
                }
                inStr = qcc::String(replyMsg->GetArg(0)->v_string.str);
                status = sasl.Advance(inStr, outStr, authState);
                if (authState == SASLEngine::ALLJOYN_AUTH_SUCCESS) {
                    KeyBlob masterSecret;
                    KeyStore& keyStore = bus.GetInternal().GetKeyStore();
                    mech = sasl.GetMechanism();
                    status = sasl.GetMasterSecret(masterSecret);
                    if (status == ER_OK) {
                        /* Tag the master secret with the auth mechanism used to generate it */
                        masterSecret.SetTag(mech);
                        status = keyStore.AddKey(remotePeerGuid, masterSecret);
                    }
                }
            } else {
                status = ER_AUTH_FAIL;
            }
        }
        firstPass = false;
    } while (status == ER_OK);
    /*
     * Exchange group keys with the remote peer. This method call is encrypted using the session key
     * that we just established.
     */
    if (status == ER_OK) {
        Message replyMsg(bus);
        KeyBlob key;
        KeyBlob nonce;
        StringSink snk;
        peerStateTable->GetGroupKeyAndNonce(key, nonce);
        key.Store(snk);
        nonce.Store(snk);
        MsgArg arg("ay", snk.GetString().size(), snk.GetString().data());
        status = remotePeerObj.MethodCall(*(ifc->GetMember("ExchangeGroupKeys")), &arg, 1, replyMsg, DEFAULT_TIMEOUT, ALLJOYN_FLAG_ENCRYPTED);
        if (status == ER_OK) {
            StringSource src(replyMsg->GetArg(0)->v_scalarArray.v_byte, replyMsg->GetArg(0)->v_scalarArray.numElements);
            status = key.Load(src);
            if (status == ER_OK) {
                status = nonce.Load(src);
            }
            if (status == ER_OK) {
                /*
                 * Tag the group key with the auth mechanism used by ExchangeGroupKeys
                 */
                key.SetTag(replyMsg->GetAuthMechanism());
                peerState->SetKeyAndNonce(key, nonce, PEER_GROUP_KEY);
            }
        }
    }
    /*
     * Report the authentication completion to allow application to clear UI etc.
     */
    if (peerAuthListener) {
        peerAuthListener->AuthenticationComplete(mech.c_str(), sender.c_str(), status == ER_OK);
    }
    /*
     * All done, release the lock
     */
    clientLock.Unlock();
    /*
     * ER_BUS_REPLY_IS_ERROR_MESSAGE has a specific meaning in the public API an should not be
     * propogated to the caller from this context.
     */
    if (status == ER_BUS_REPLY_IS_ERROR_MESSAGE) {
        status = ER_AUTH_FAIL;
    }
    return status;
}

QStatus AllJoynPeerObj::RequestThread::QueueRequest(Message& msg, RequestType reqType, const qcc::String data)
{
    QCC_DbgHLPrintf(("QueueRequest %s", msg->Description().c_str()));
    lock.Lock();
    bool wasEmpty = queue.empty();
    Request req = { msg, reqType, data };
    queue.push(req);
    lock.Unlock();
    if (wasEmpty) {
        if (IsRunning()) {
            Stop();
            Join();
        }
        return Start();
    } else {
        return ER_OK;
    }
}

qcc::ThreadReturn AllJoynPeerObj::RequestThread::Run(void* args)
{
    QStatus status;

    while (!IsStopping()) {
        lock.Lock();
        Request& req(queue.front());
        QCC_DbgHLPrintf(("Dequeue request %d for %s", req.reqType, req.msg->Description().c_str()));
        lock.Unlock();
        if (!IsStopping()) {
            switch (req.reqType) {
            case SECURE_PEER:
                status = peerObj.SecurePeerConnection(req.msg->GetSender());
                if (status != ER_OK) {
                    if (peerObj.peerAuthListener) {
                        peerObj.peerAuthListener->SecurityViolation(status, req.msg);
                    }
                } else {
                    peerObj.bus.GetInternal().GetLocalEndpoint().PushMessage(req.msg);
                }
                break;

            case AUTH_CHALLENGE:
                peerObj.AuthAdvance(req.msg);
                break;

            case ACCEPT_SESSION:
                peerObj.AcceptSession(NULL, req.msg);
                break;

            case EXPAND_HEADER:
                peerObj.ExpandHeader(req.msg, req.data);
                break;
            }
        }
        lock.Lock();
        queue.pop();
        bool isEmpty = queue.empty();
        lock.Unlock();
        if (isEmpty) {
            break;
        }
    }
    return 0;
}

void AllJoynPeerObj::HandleSecurityViolation(Message& msg, QStatus status)
{
    PeerStateTable* peerStateTable = bus.GetInternal().GetPeerStateTable();

    QCC_DbgHLPrintf(("HandleSecurityViolation %s %s", QCC_StatusText(status), msg->Description().c_str()));

    if (status == ER_BUS_MESSAGE_DECRYPTION_FAILED) {
        PeerState peerState = peerStateTable->GetPeerState(msg->GetSender());
        /*
         * If we believe the peer is secure we have a clear security violation
         */
        if (peerState->IsSecure()) {
            /*
             * The keys we have for this peer are no good
             */
            peerState->ClearKeys();
        } else if (msg->IsBroadcastSignal()) {
            /*
             * Try to secure the connection back to the sender.
             */
            if (requestThread.QueueRequest(msg, SECURE_PEER) == ER_OK) {
                status = ER_OK;
            }
        }
    }
    /*
     * Report the security violation
     */
    if ((status != ER_OK) && peerAuthListener) {
        peerAuthListener->SecurityViolation(status, msg);
    }
}

void AllJoynPeerObj::NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
{
    /*
     * We are only interested in names that no longer have an owner.
     */
    if (newOwner == NULL) {
        QCC_DbgHLPrintf(("Peer %s is gone", busName));
        /*
         * Clean up peer state.
         */
        bus.GetInternal().GetPeerStateTable()->DelPeerState(busName);
        /*
         * We are no longer in an authentication conversation with this peer.
         */
        lock.Lock();
        delete conversations[busName];
        conversations.erase(busName);
        lock.Unlock();
    }
}

void AllJoynPeerObj::AcceptSession(const InterfaceDescription::Member* member, Message& msg)
{
    QStatus status;
    /*
     * Use the request thread.
     */
    if (member) {
        status = requestThread.QueueRequest(msg, ACCEPT_SESSION);
        if (status != ER_OK) {
            MethodReply(msg, status);
        }
        return;
    }

    size_t numArgs;
    const MsgArg* args;

    msg->GetArgs(numArgs, args);
    SessionPort sessionPort = args[0].v_uint16;
    SessionId sessionId = args[1].v_uint32;
    String joiner = args[2].v_string.str;
    SessionOpts opts;
    status = GetSessionOpts(args[3], opts);

    if (status == ER_OK) {
        MsgArg replyArg;

        /* Call bus listeners */
        bool isAccepted = bus.GetInternal().CallAcceptListeners(sessionPort, joiner.c_str(), opts);

        /* Reply to AcceptSession */
        replyArg.Set("b", isAccepted);
        status = MethodReply(msg, &replyArg, 1);
        if ((status == ER_OK) && isAccepted) {
            /* Let listeners know the join was successfully accepted */
            bus.GetInternal().CallJoinedListeners(sessionPort, sessionId, joiner.c_str());
        }
    } else {
        MethodReply(msg, status);
    }
}

}
