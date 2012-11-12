/**
 * @file
 * The KeyStore class manages the storing and loading of key blobs from external storage.
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
#ifndef _ALLJOYN_KEYSTORE_H
#define _ALLJOYN_KEYSTORE_H

#ifndef __cplusplus
#error Only include KeyStore.h in C++ code.
#endif

#include <map>

#include <qcc/platform.h>

#include <qcc/GUID.h>
#include <qcc/String.h>
#include <qcc/KeyBlob.h>
#include <qcc/Mutex.h>
#include <qcc/Stream.h>

#include <alljoyn/KeyStoreListener.h>

#include <Status.h>


namespace ajn {

/**
 * Forward declaration.
 */
class KeyStore;

/**
 * The %KeyStore class manages the storing and loading of key blobs from
 * external storage.
 */
class KeyStore {
  public:

    /**
     * KeyStore constructor
     */
    KeyStore(const qcc::String& application);

    /**
     * KeyStore destructor
     */
    ~KeyStore();

    /**
     * Load the key store
     *
     * @param fileName The filename of the default key store.  When NULL the value is the applicationName
     *                 parameter of BusAttachment().
     */
    QStatus Load(const char* fileName);

    /**
     * Store the key store
     */
    QStatus Store() { return (storeState == MODIFIED) ?  listener->StoreRequest(*this) : ER_OK; }

    /**
     * Read keys into the key store from a specific source.
     *
     * @param source    The source to read the keys from.
     * @param password  The password required to decrypt the key store
     *
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     *
     */
    QStatus Load(qcc::Source& source, const qcc::String& password);

    /**
     * Re-read keys from the key store
     *
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     *
     */
    QStatus Reload();

    /**
     * Write the current keys from the key store to a specific sink
     *
     * @param sink The sink to write the keys to.
     * @return
     *      - ER_OK if successful
     *      - An error status otherwise
     */
    QStatus Store(qcc::Sink& sink);

    /**
     * Get a key blob from the key store
     *
     * @param guid     The unique identifier for the key
     * @param key      The key blob to get
     * @return
     *      - ER_OK if successful
     *      - ER_BUS_KEY_UNAVAILABLE if key is unavailable
     *      - ER_BUS_KEY_EXPIRED if the requested key has expired
     */
    QStatus GetKey(const qcc::GUID& guid, qcc::KeyBlob& key);

    /**
     * Add a key blob to the key store
     *
     * @param guid     The unique identifier for the key
     * @param key      The key blob to add
     * @param expires  Time when the key expires.
     * @return ER_OK
     */
    QStatus AddKey(const qcc::GUID& guid, const qcc::KeyBlob& key);

    /**
     * Remove a key blob from the key store
     *
     * @param guid  The unique identifier for the key
     * return ER_OK
     */
    QStatus DelKey(const qcc::GUID& guid);

    /**
     * Test is there is a requested key blob in the key store
     *
     * @param guid  The unique identifier for the key
     * @return      Returns true if the requested key is in the key store.
     */
    bool HasKey(const qcc::GUID& guid);

    /**
     * Return the GUID for this key store
     * @param[out] guid return the GUID for this key store
     * @return
     *      - ER_OK on success
     *      - ER_BUS_KEY_STORE_NOT_LOADED if the GUID is not available
     */
    QStatus GetGuid(qcc::GUID& guid)
    {
        if (storeState == UNAVAILABLE) {
            return ER_BUS_KEY_STORE_NOT_LOADED;
        } else {
            guid = thisGuid;
            return ER_OK;
        }
    }

    /**
     * Return the GUID for this key store as a hex-encoded string.
     *
     * @return  Returns the hex-encode string for the GUID or an empty string if the key store is not loaded.
     */
    qcc::String GetGuid() {  return (storeState == UNAVAILABLE) ? "" : thisGuid.ToString(); }

    /**
     * Override the default listener so the application can provide the load and store
     * @param listener the listener that will override the default listener
     */
    void SetListener(KeyStoreListener& listener) { this->listener = &listener; }

    /**
     * Get the name of the application that owns this key store.
     *
     * @return The application name.
     */
    const qcc::String& GetApplication() { return application; }

    /**
     * Clear all keys from the key store.
     *
     * @return  ER_OK if the key store was cleared.
     */
    QStatus Clear();

  private:

    /**
     * Assignment operator is private
     */
    KeyStore& operator=(const KeyStore& other) { return *this; }

    /**
     * Copy constructor is private
     */
    KeyStore(const KeyStore& other) { }

    /**
     * The application that owns this key store
     */
    qcc::String application;

    /**
     * State of the key store
     */
    enum {
        UNAVAILABLE, /**< Key store has not been loaded */
        LOADED,      /**< Key store is loaded */
        MODIFIED     /**< Key store has been modified since it was loaded */
    } storeState;

    /**
     * Default constructor is private
     */
    KeyStore();

    /**
     * In memory copy of the key store
     */
    std::map<qcc::GUID, qcc::KeyBlob> keys;

    /**
     * Default listener for handling load/store requests
     */
    KeyStoreListener* defaultListener;

    /**
     * Listener for handling load/store requests
     */
    KeyStoreListener* listener;

    /**
     * The guid for this key store
     */
    qcc::GUID thisGuid;

    /**
     * Mutex to protect key store
     */
    qcc::Mutex lock;

    /**
     * Key for encrypting/decrypting the key store.
     */
    qcc::KeyBlob* keyStoreKey;
};

}

#endif
