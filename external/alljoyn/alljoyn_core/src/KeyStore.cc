/**
 * @file
 * The KeyStore class manages the storing and loading of key blobs from
 * external storage. The default implementation stores key blobs in a file.
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

#include <map>

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Crypto.h>
#include <qcc/Environ.h>
#include <qcc/FileStream.h>
#include <qcc/KeyBlob.h>
#include <qcc/Util.h>
#include <qcc/StringSource.h>
#include <qcc/StringSink.h>

#include <alljoyn/KeyStoreListener.h>

#include "KeyStore.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN_AUTH"

using namespace std;
using namespace qcc;

namespace ajn {


static const uint16_t KeyStoreVersion = 0x0101;


QStatus KeyStoreListener::LoadKeys(KeyStore& keyStore, const qcc::String& source, const qcc::String& password)
{
    StringSource stringSource(source);
    return keyStore.Load(stringSource, password);
}

QStatus KeyStoreListener::StoreKeys(KeyStore& keyStore, qcc::String& sink)
{
    StringSink stringSink;
    QStatus status = keyStore.Store(stringSink);
    if (status == ER_OK) {
        sink = stringSink.GetString();
    }
    return status;
}

class DefaultKeyStoreListener : public KeyStoreListener {

  public:

    DefaultKeyStoreListener(const qcc::String& application) {
        fileName = GetHomeDir() + "/.alljoyn_keystore/" + application;
    }

    void SetFileName(const char* name) {
        if (name) {
            fileName = GetHomeDir() + name;
        }
    }

    QStatus LoadRequest(KeyStore& keyStore) {
        QStatus status;
        /* Try to load the keystore */
        {
            FileSource source(fileName);
            if (source.IsValid()) {
                status = keyStore.Load(source, fileName);
                if (status == ER_OK) {
                    QCC_DbgHLPrintf(("Read key store from %s", fileName.c_str()));
                }
                return status;
            }
        }
        /* Create an empty keystore */
        {
            FileSink sink(fileName, FileSink::PRIVATE);
            if (!sink.IsValid()) {
                status = ER_BUS_WRITE_ERROR;
                QCC_LogError(status, ("Cannot initialize key store %s", fileName.c_str()));
                return status;
            }
        }
        /* Load the empty keystore */
        {
            FileSource source(fileName);
            status = keyStore.Load(source, fileName);
            if (status == ER_OK) {
                QCC_DbgHLPrintf(("Initialized key store %s", fileName.c_str()));
            } else {
                QCC_LogError(status, ("Failed to initialize key store %s", fileName.c_str()));
            }
            return status;
        }
    }

    QStatus StoreRequest(KeyStore& keyStore) {
        QStatus status;
        FileSink sink(fileName, FileSink::PRIVATE);
        if (sink.IsValid()) {
            status = keyStore.Store(sink);
            if (status == ER_OK) {
                QCC_DbgHLPrintf(("Wrote key store to %s", fileName.c_str()));
            }
        } else {
            status = ER_BUS_WRITE_ERROR;
            QCC_LogError(status, ("Cannot write key store to %s", fileName.c_str()));
        }
        return status;
    }

  private:

    qcc::String fileName;

};

KeyStore::KeyStore(const qcc::String& application) :
    application(application),
    storeState(UNAVAILABLE),
    defaultListener(new DefaultKeyStoreListener(application)),
    listener(defaultListener),
    thisGuid(),
    keyStoreKey(NULL)
{
}

KeyStore::~KeyStore()
{
    delete defaultListener;
    delete keyStoreKey;
}

QStatus KeyStore::Load(const char* fileName)
{
    ((DefaultKeyStoreListener*)defaultListener)->SetFileName(fileName);
    return (storeState == UNAVAILABLE) ?  listener->LoadRequest(*this) : ER_OK;
}

QStatus KeyStore::Reload()
{
    QStatus status;
    if (storeState == UNAVAILABLE) {
        status = ER_BUS_KEYSTORE_NOT_LOADED;
    } else {
        lock.Lock();
        keys.clear();
        storeState = UNAVAILABLE;
        status = listener->LoadRequest(*this);
        lock.Unlock();
    }
    return status;
}

static size_t EraseExpiredKeys(map<qcc::GUID, KeyBlob>& keys)
{
    size_t count = 0;
    map<qcc::GUID, KeyBlob>::iterator it = keys.begin();
    while (it != keys.end()) {
        map<qcc::GUID, KeyBlob>::iterator current = it++;
        if (current->second.HasExpired()) {
            QCC_DbgPrintf(("Deleting expired key for GUID %s", current->first.ToString().c_str()));
            keys.erase(current);
            ++count;
        }
    }
    return count;
}

QStatus KeyStore::Load(Source& source, const qcc::String& password)
{
    QCC_DbgPrintf(("KeyStore::Load"));

    /* Don't load if already loaded */
    if (storeState != UNAVAILABLE) {
        return ER_OK;
    }

    lock.Lock();

    uint8_t guidBuf[qcc::GUID::SIZE];
    size_t pulled;
    size_t len = 0;
    uint16_t version;
    KeyBlob nonce;

    /* Load and check the key store version */
    QStatus status = source.PullBytes(&version, sizeof(version), pulled);
    if ((status == ER_OK) && (version != KeyStoreVersion)) {
        status = ER_BUS_KEYSTORE_VERSION_MISMATCH;
        QCC_LogError(status, ("Keystore has wrong version expected %d got %d", KeyStoreVersion, version));
    }

    /* Load the application GUID */
    if (status == ER_OK) {
        status = source.PullBytes(guidBuf, qcc::GUID::SIZE, pulled);
        thisGuid.SetBytes(guidBuf);
    }

    /* This is the only chance to generate the key store key */
    keyStoreKey = new KeyBlob(password + GetGuid(), Crypto_AES::AES128_SIZE, KeyBlob::AES);

    /* Allow for an uninitialized (empty) key store */
    if (status == ER_NONE) {
        keys.clear();
        storeState = MODIFIED;
        status = ER_OK;
        goto ExitLoad;
    }
    if (status != ER_OK) {
        goto ExitLoad;
    }
    /* Load the nonce */
    status = nonce.Load(source);
    if (status != ER_OK) {
        goto ExitLoad;
    }
    /* Get length of the encrypted keys */
    status = source.PullBytes(&len, sizeof(len), pulled);
    if (status != ER_OK) {
        goto ExitLoad;
    }
    /* Sanity check on the length */
    if (len > 64000) {
        status = ER_BUS_CORRUPT_KEYSTORE;
        goto ExitLoad;
    }
    if (len > 0) {
        uint8_t* data = NULL;
        /*
         * Load the encrypted keys.
         */
        data = new uint8_t[len];
        status = source.PullBytes(data, len, pulled);
        if (pulled != len) {
            status = ER_BUS_CORRUPT_KEYSTORE;
        }
        if (status == ER_OK) {
            /*
             * Decrypt the key store.
             */
            Crypto_AES aes(*keyStoreKey, Crypto_AES::ENCRYPT);
            status = aes.Decrypt_CCM(data, data, len, nonce, NULL, 0, 16);
            /*
             * Unpack the guid/key pairs from an intermediate string source.
             */
            StringSource strSource(data, len);
            while (status == ER_OK) {
                status = strSource.PullBytes(guidBuf, qcc::GUID::SIZE, pulled);
                if (status == ER_OK) {
                    qcc::GUID guid;
                    guid.SetBytes(guidBuf);
                    status = keys[guid].Load(strSource);
                    QCC_DbgPrintf(("KeyStore::Load GUID %s", guid.ToString().c_str()));
                }
            }
            if (status == ER_NONE) {
                status = ER_OK;
            }
        }
        delete [] data;
    }
    if (status != ER_OK) {
        goto ExitLoad;
    }
    if (EraseExpiredKeys(keys)) {
        storeState = MODIFIED;
    } else {
        storeState = LOADED;
    }

ExitLoad:

    if (status != ER_OK) {
        keys.clear();
        storeState = MODIFIED;
    }
    lock.Unlock();
    return status;
}

QStatus KeyStore::Clear()
{
    if (storeState == UNAVAILABLE) {
        return ER_BUS_KEYSTORE_NOT_LOADED;
    }
    lock.Lock();
    keys.clear();
    storeState = MODIFIED;
    lock.Unlock();
    listener->StoreRequest(*this);
    return ER_OK;
}

QStatus KeyStore::Store(Sink& sink)
{
    QCC_DbgPrintf(("KeyStore::Store"));

    /* Cannot store if never loaded */
    if (storeState == UNAVAILABLE) {
        return ER_BUS_KEYSTORE_NOT_LOADED;
    }

    /* Don't store if not modified */
    if (storeState != MODIFIED) {
        return ER_OK;
    }

    lock.Lock();

    size_t pushed;
    QStatus status = ER_OK;

    EraseExpiredKeys(keys);

    /*
     * Pack the keys into an intermediate string sink.
     */
    StringSink strSink;
    map<qcc::GUID, KeyBlob>::iterator it;
    for (it = keys.begin(); it != keys.end(); it++) {
        strSink.PushBytes(it->first.GetBytes(), qcc::GUID::SIZE, pushed);
        (*it).second.Store(strSink);
        QCC_DbgPrintf(("KeyStore::Store GUID %s", it->first.ToString().c_str()));
    }
    size_t keysLen = strSink.GetString().size();
    KeyBlob nonce;
    nonce.Rand(16, KeyBlob::GENERIC);
    /*
     * First two bytes are the version number.
     */
    status = sink.PushBytes(&KeyStoreVersion, sizeof(KeyStoreVersion), pushed);
    if (status != ER_OK) {
        goto ExitStore;
    }
    /*
     * Store the GUID and a random nonce.
     */
    if (status == ER_OK) {
        status = sink.PushBytes(thisGuid.GetBytes(), qcc::GUID::SIZE, pushed);
        if (status == ER_OK) {
            status = nonce.Store(sink);
        }
    }
    if (status != ER_OK) {
        goto ExitStore;
    }
    if (keysLen > 0) {
        /*
         * Encrypt keys.
         */
        uint8_t* keysData = new uint8_t[keysLen + 16];
        Crypto_AES aes(*keyStoreKey, Crypto_AES::ENCRYPT);
        status = aes.Encrypt_CCM(strSink.GetString().data(), keysData, keysLen, nonce, NULL, 0, 16);
        /* Store the length of the encrypted keys */
        if (status == ER_OK) {
            status = sink.PushBytes(&keysLen, sizeof(keysLen), pushed);
        }
        /* Store the encrypted keys */
        if (status == ER_OK) {
            status = sink.PushBytes(keysData, keysLen, pushed);
        }
        delete [] keysData;
    } else {
        status = sink.PushBytes(&keysLen, sizeof(keysLen), pushed);
    }
    if (status != ER_OK) {
        goto ExitStore;
    }
    storeState = LOADED;

ExitStore:

    lock.Unlock();
    return status;
}

QStatus KeyStore::GetKey(const qcc::GUID& guid, KeyBlob& key)
{
    if (storeState == UNAVAILABLE) {
        return ER_BUS_KEYSTORE_NOT_LOADED;
    }
    QStatus status;
    lock.Lock();
    QCC_DbgPrintf(("KeyStore::GetKey %s", guid.ToString().c_str()));
    if (keys.find(guid) != keys.end()) {
        key = keys[guid];
        status = ER_OK;
    } else {
        status = ER_BUS_KEY_UNAVAILABLE;
    }
    lock.Unlock();
    return status;
}

bool KeyStore::HasKey(const qcc::GUID& guid)
{
    if (storeState == UNAVAILABLE) {
        return false;
    }
    bool hasKey;
    lock.Lock();
    hasKey = keys.count(guid) != 0;
    lock.Unlock();
    return hasKey;
}

QStatus KeyStore::AddKey(const qcc::GUID& guid, const KeyBlob& key)
{
    if (storeState == UNAVAILABLE) {
        return ER_BUS_KEYSTORE_NOT_LOADED;
    }
    lock.Lock();
    QCC_DbgPrintf(("KeyStore::AddKey %s", guid.ToString().c_str()));
    keys[guid] = key;
    storeState = MODIFIED;
    lock.Unlock();
    return ER_OK;
}

QStatus KeyStore::DelKey(const qcc::GUID& guid)
{
    if (storeState == UNAVAILABLE) {
        return ER_BUS_KEYSTORE_NOT_LOADED;
    }
    lock.Lock();
    QCC_DbgPrintf(("KeyStore::DelKey %s", guid.ToString().c_str()));
    keys.erase(guid);
    storeState = MODIFIED;
    lock.Unlock();
    listener->StoreRequest(*this);
    return ER_OK;
}


}
