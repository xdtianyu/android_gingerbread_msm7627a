/**
 * @file
 * This class maintains information about peers connected to the bus.
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
#ifndef _ALLJOYN_PEERSTATE_H
#define _ALLJOYN_PEERSTATE_H

#ifndef __cplusplus
#error Only include PeerState.h in C++ code.
#endif

#include <qcc/platform.h>

#include <map>
#include <limits>
#include <bitset>
#include <assert.h>

#include <qcc/String.h>
#include <qcc/GUID.h>
#include <qcc/KeyBlob.h>
#include <qcc/ManagedObj.h>
#include <qcc/Mutex.h>
#include <qcc/time.h>

#include <Status.h>

namespace ajn {

/* Forward declaration */
class _PeerState;

/**
 * Enumeration for the different peer keys.
 */
typedef enum {
    PEER_SESSION_KEY = 0, /**< Unicast key for secure point-to-point communication */
    PEER_GROUP_KEY   = 1  /**< broadcast key for secure point-to-multipoint communication */
} PeerKeyType;

/**
 * PeerState is a reference counted (managed) class that keeps track of state information for other
 * peers that this peer communicates with.
 */
typedef qcc::ManagedObj<_PeerState> PeerState;

/**
 * This class maintains state information about peers connected to the bus and provides helper
 * functions that check and update various state information.
 */
class _PeerState {

    friend class AllJoynPeerObj;

  public:

    /**
     * Default constructor
     */
    _PeerState() :
        isLocalPeer(false),
        clockOffset((std::numeric_limits<int32_t>::max)()),
        firstClockAdjust(true),
        lastDriftAdjustTime(0),
        expectedSerial(0),
        isSecure(false)
    {
        ::memset(window, 0, sizeof(window));
    }

    /**
     * Get the (estimated) timestamp for this remote peer converted to local host time. The estimate
     * is updated based on the timestamp recently received.
     *
     * @param remoteTime  The timestamp received in a message from the remote peer.
     *
     * @return   The estimated timestamp for the remote peer.
     */
    uint32_t EstimateTimestamp(uint32_t remoteTime);

    /**
     * This method is called whenever a message is unmarshaled. It checks that the serial number is
     * valid by comparing against the last N serial numbers received from this peer. Secure messages
     * have additional checks for replay attacks. Unreliable messages are checked for in-order
     * arrival.
     *
     * @param serial      The serial number being checked.
     * @param secure      The message was flagged as secure
     * @param unreliable  The message is flagged as unreliable.
     *
     * @return  Returns true if the serial number is valid.
     */
    bool IsValidSerial(uint32_t serial, bool secure, bool unreliable);

    /**
     * Gets the GUID for this peer.
     *
     * @return  Returns the GUID for this peer.
     */
    const qcc::GUID& GetGuid() { return guid; }

    /**
     * Sets the GUID for this peer.
     */
    void SetGuid(const qcc::GUID& guid) { this->guid = guid; }

    /**
     * Sets the session key and nonce for this peer
     *
     * @param key        The session key to set.
     * @param nonce      The nonce to set.
     * @param keyType    Indicate if this is the unicast or broadcast key.
     */
    void SetKeyAndNonce(const qcc::KeyBlob& key, const qcc::KeyBlob& nonce, PeerKeyType keyType) {
        keys[keyType] = key;
        nonces[keyType] = nonce;
        isSecure = nonce.IsValid() && key.IsValid();
    }

    /**
     * Gets the session key and nonce for this peer.
     *
     * @param key    [out]Returns the session key.
     * @param nonce  [out]Returns the nonce.
     *
     * @return  - ER_OK if there is a session key set for this peer.
     *          - ER_BUS_KEY_UNAVAILABLE if no session key has been set for this peer.
     */
    QStatus GetKeyAndNonce(qcc::KeyBlob& key, qcc::KeyBlob& nonce, PeerKeyType keyType) {
        if (isSecure) {
            key = keys[keyType];
            nonce = nonces[keyType];
            return ER_OK;
        } else {
            return ER_BUS_KEY_UNAVAILABLE;
        }
    }

    /**
     * Clear the keys for this peer.
     */
    void ClearKeys() {
        keys[PEER_SESSION_KEY].Erase();
        keys[PEER_GROUP_KEY].Erase();
        isSecure = false;
    }

    /**
     * Tests if this peer is secure.
     *
     * @return  Returns true if a session key has been set for this peer.
     */
    bool IsSecure() { return isSecure; }

    /**
     * Tests if this peer is the local peer.
     *
     * @return  Returns true if this PeerState instance is for the local peer.
     */
    bool IsLocalPeer() { return isLocalPeer; }

    /**
     * Returns window size for serial number validation. Used by unit tests.
     *
     * @return Size of the serial number validation window.
     */
    size_t SerialWindowSize() { return sizeof(window) / sizeof(window[0]); }

  private:

    /**
     * True if this peer state is for the local peer.
     */
    bool isLocalPeer;

    /**
     * The estimated clock offset between the local peer and the remote peer. This is used to
     * convert between remote and local timestamps.
     */
    int32_t clockOffset;

    /**
     * Flag to indicate if clockOffset has been properly initialized.
     */
    bool firstClockAdjust;

    /**
     * Time of last clock drift adjustment.
     */
    uint32_t lastDriftAdjustTime;

    /**
     * The next serial number expected.
     */
    uint32_t expectedSerial;

    /**
     * Set to true if this peer has keys.
     */
    bool isSecure;

    /**
     * The GUID for this peer.
     */
    qcc::GUID guid;

    /**
     * The security nonces (unicast and broadcast) for this peer
     */
    qcc::KeyBlob nonces[2];

    /**
     * The session keys (unicast and broadcast) for this peer.
     */
    qcc::KeyBlob keys[2];

    /**
     * Serial number window. Used by IsValidSerial() to detect replay attacks. The size of the
     * window defines that largest tolerable gap between consecutive serial numbers.
     */
    uint32_t window[128];

};


/**
 * This class is a container for managing state information about remote peers.
 */
class PeerStateTable {

  public:

    /**
     * Get the peer state for given a bus name.
     *
     * @param busName   The bus name for a remote connection
     *
     * @return  The peer state.
     */
    PeerState GetPeerState(const qcc::String& busName);

    /**
     * Fnd out if the bus name is for a known peer.
     *
     * @param busName   The bus name for a remote connection
     *
     * @return  Returns true if the peer is known.
     */
    bool IsKnownPeer(const qcc::String& busName) {
        lock.Lock();
        bool known = peerMap.count(busName) > 0;
        lock.Unlock();
        return known;
    }

    /**
     * Get the peer state looking the peer state up by a unique name or a known alias for the peer.
     *
     * @param uniqueName  The bus name for a remote connection
     * @param aliasName   An alias bus name for a remote connection
     *
     * @return  The peer state.
     */
    PeerState GetPeerState(const qcc::String& uniqueName, const qcc::String& aliasName);

    /**
     * Delete peer state for a busName that is no longer in use
     *
     * @param busName  The bus name that may was been previously associated with peer state.
     */
    void DelPeerState(const qcc::String& busName);

    /**
     * Gets the group (broadcast) key and nonce for the local peer. This is used to encrypt
     * broadcast messages sent by this peer.
     *
     * @param key   [out]Returns the broadcast key.
     * @param nonce [out]Returns the nonce.
     */
    void GetGroupKeyAndNonce(qcc::KeyBlob& key, qcc::KeyBlob& nonce);

    /**
     * Clear all peer state.
     */
    void Clear();

  private:

    /**
     * The broadcast key for the local peer
     */
    qcc::KeyBlob groupKey;

    /**
     * The broadcast nonce for the local peer
     */
    qcc::KeyBlob groupNonce;

    /**
     * Mapping table from bus names to peer state.
     */
    std::map<const qcc::String, PeerState> peerMap;

    /**
     * Mutex to protect the peer table
     */
    qcc::Mutex lock;

};

}

#undef QCC_MODULE

#endif
