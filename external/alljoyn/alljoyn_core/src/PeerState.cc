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

#include <qcc/platform.h>

#include <algorithm>

#include <qcc/Debug.h>
#include <qcc/Crypto.h>
#include <qcc/time.h>

#include "PeerState.h"
#include "AllJoynCrypto.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN"

using namespace std;
using namespace qcc;

namespace ajn {

uint32_t _PeerState::EstimateTimestamp(uint32_t remote)
{
    uint32_t local = qcc::GetTimestamp();
    int32_t delta = static_cast<int32_t>(local - remote);
    int32_t oldOffset = clockOffset;

    /*
     * Clock drift adjustment. Make remote re-confirm minimum occasionally.
     * This will adjust for clock drift that is less than 100 ppm.
     */
    if ((local - lastDriftAdjustTime) > 10000) {
        lastDriftAdjustTime = local;
        ++clockOffset;
    }

    if (((oldOffset - delta) > 0) || firstClockAdjust) {
        QCC_DbgHLPrintf(("Updated clock offset old %d, new %d", clockOffset, delta));
        clockOffset = delta;
        firstClockAdjust = false;
    }

    return remote + static_cast<uint32_t>(clockOffset);
}

bool _PeerState::IsValidSerial(uint32_t serial, bool secure, bool unreliable)
{
    bool ret = false;
    /*
     * Serial 0 is always invalid.
     */
    if (serial != 0) {
        const size_t winSize = sizeof(window) / sizeof(window[0]);
        uint32_t* entry = window + (serial % winSize);
        if (*entry != serial) {
            *entry = serial;
            ret = true;
        }
    }
    return ret;

}

PeerState PeerStateTable::GetPeerState(const qcc::String& busName)
{
    lock.Lock();
    QCC_DbgHLPrintf(("PeerStateTable::GetPeerState() %s state for %s", peerMap.count(busName) ? "got" : "no", busName.c_str()));
    PeerState result = peerMap[busName];
    lock.Unlock();

    return result;
}

PeerState PeerStateTable::GetPeerState(const qcc::String& uniqueName, const qcc::String& aliasName)
{
    assert(uniqueName[0] == ':');
    PeerState result;
    lock.Lock();
    std::map<const qcc::String, PeerState>::iterator iter = peerMap.find(uniqueName);
    if (iter == peerMap.end()) {
        QCC_DbgHLPrintf(("PeerStateTable::GetPeerState() no state stored for %s aka %s", uniqueName.c_str(), aliasName.c_str()));
        result = peerMap[aliasName];
        peerMap[uniqueName] = result;
    } else {
        QCC_DbgHLPrintf(("PeerStateTable::GetPeerState() got state for %s aka %s", uniqueName.c_str(), aliasName.c_str()));
        result = iter->second;
        peerMap[aliasName] = result;
    }
    lock.Unlock();
    return result;
}

void PeerStateTable::DelPeerState(const qcc::String& busName)
{
    lock.Lock();
    QCC_DbgHLPrintf(("PeerStateTable::DelPeerState() %s for %s", peerMap.count(busName) ? "remove state" : "no state to remove", busName.c_str()));
    peerMap.erase(busName);
    if (groupKey.IsValid()) {
        /*
         * If none of the remaining peers are secure clear the group key.
         */
        std::map<const qcc::String, PeerState>::iterator iter = peerMap.begin();
        for (iter = peerMap.begin(); iter != peerMap.end(); ++iter) {
            if (iter->second->IsSecure() && !iter->second->IsLocalPeer()) {
                break;
            }
        }
        if (iter == peerMap.end()) {
            QCC_DbgHLPrintf(("Deleting stale group key"));
            groupKey.Erase();
        }
    }
    lock.Unlock();
}

void PeerStateTable::GetGroupKeyAndNonce(qcc::KeyBlob& key, qcc::KeyBlob& nonce)
{
    /*
     * If we don't have a group key generate it now.
     */
    if (!groupKey.IsValid()) {
        QCC_DbgHLPrintf(("Allocating fresh group key"));
        groupKey.Rand(Crypto_AES::AES128_SIZE, KeyBlob::AES);
        groupNonce.Rand(Crypto::NonceBytes, KeyBlob::GENERIC);
    }
    key = groupKey;
    nonce = groupNonce;
}

void PeerStateTable::Clear()
{
    lock.Lock();
    groupKey.Erase();
    groupNonce.Erase();
    peerMap.clear();
    lock.Unlock();
}

}
