/**
 * @file
 * The KeyStoreListener class handled requests to load or store the key store.
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
#ifndef _ALLJOYN_KEYSTORE_LISTENER_H
#define _ALLJOYN_KEYSTORE_LISTENER_H

#ifndef __cplusplus
#error Only include KeyStoreListener.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <Status.h>


namespace ajn {

/**
 * Forward declaration.
 */
class KeyStore;

/**
 * An application can provide a key store listener to override the default key store Load and Store
 * behavior. This will override the default key store behavior.
 */
class KeyStoreListener {

  public:
    /**
     * Virtual destructor for derivable class.
     */
    virtual ~KeyStoreListener() { }

    /**
     * This method is called when a key store needs to be loaded.
     * @remark The application must immediately call <tt>#LoadKeys</tt> to make the key store available.
     *
     * @param keyStore   Reference to the KeyStore to be loaded.
     *
     * @return
     *      - #ER_OK if the load request was satisfied
     *      - An error status otherwise
     *
     */
    virtual QStatus LoadRequest(KeyStore& keyStore) = 0;

    /**
     * Read keys into the key store from a specific source.
     *
     * @param keyStore  The keyStore to load. This is the keystore indicated in the LoadRequest call.
     * @param source    The source string to read the keys from.
     * @param password  The password required to decrypt the key store
     *
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     *
     */
    virtual QStatus LoadKeys(KeyStore& keyStore, const qcc::String& source, const qcc::String& password);

    /**
     * This method is called when a key store needs to be stored.
     * @remark The application must call <tt>#StoreKeys</tt> before the key store can be used.
     *
     * @param keyStore   Reference to the KeyStore to be stored.
     *
     * @return
     *      - #ER_OK if the store request was satisfied
     *      - An error status otherwise
     */
    virtual QStatus StoreRequest(KeyStore& keyStore) = 0;

    /**
     * Write the current keys from the key store to a specific sink
     *
     * @param keyStore  The keyStore to store. This is the keystore indicated in the StoreRequest call.
     * @param sink      The sink string to write the keys to.
     * @return
     *      - #ER_OK if successful
     *      - An error status otherwise
     */
    virtual QStatus StoreKeys(KeyStore& keyStore, qcc::String& sink);

};

}

#endif
