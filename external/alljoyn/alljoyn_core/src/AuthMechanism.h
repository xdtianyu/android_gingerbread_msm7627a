#ifndef _ALLJOYN_AUTHMECHANISM_H
#define _ALLJOYN_AUTHMECHANISM_H
/**
 * @file
 * This file defines the base class for authentication mechanisms and the authentication
 * Mechanism manager
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

#ifndef __cplusplus
#error Only include AuthMechanism.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/StringMapKey.h>
#include <qcc/KeyBlob.h>
#include <qcc/Debug.h>

#include <alljoyn/AuthListener.h>

#include <map>

#include "KeyStore.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN_AUTH"

namespace ajn {

/**
 * Base class for authentication mechanisms that can be registered with the AllJoynAuthentication Manager
 */
class AuthMechanism {
  public:

    /** Authentication type  */
    typedef enum {
        CHALLENGER, /**< A server usually provides the challenges */
        RESPONDER   /**< A client usually provide the responses */
    } AuthRole;

    /**
     * Enumeration for authentication status
     */
    typedef enum {
        ALLJOYN_AUTH_OK,       ///< Indicates the authentication exchange is complete
        ALLJOYN_AUTH_CONTINUE, ///< Indicates the authentication exchange is continuing
        ALLJOYN_AUTH_RETRY,    ///< Indicates the authentication failed but should be retried
        ALLJOYN_AUTH_FAIL,     ///< Indicates the authentication failed
        ALLJOYN_AUTH_ERROR     ///< Indicates the authentication challenge or response was badly formed
    } AuthResult;

    /**
     * Initialize this authentication mechanism. This method is called by the SASL engine
     * immediately after the authentication constructor is called. Classes that define this method
     * should call this base class method.
     *
     * @param authRole   Indicates if the authentication method is initializing as a challenger or a responder.
     * @param authPeer   The bus name of the remote peer that is being authenticated.
     *
     * @return ER_OK if the authentication mechanism was succesfully initialized.
     */
    virtual QStatus Init(AuthRole authRole, const qcc::String& authPeer) {
        this->authPeer = authPeer;
        this->authRole = authRole;
        ++authCount;
        return ER_OK;
    }

    /**
     * Challenges flow from servers to clients.
     *
     * Process a response from a client and returns a challenge.
     *
     * @param response Response from client
     * @param result   Returns:
     *                  - ALLJOYN_AUTH_OK  if the authentication is complete,
     *                  - ALLJOYN_AUTH_CONTINUE if the authentication is incomplete,
     *                  - ALLJOYN_AUTH_ERROR    if the response was rejected.
     */
    virtual qcc::String Challenge(const qcc::String& response, AuthResult& result) = 0;

    /**
     * Request the initial challenge. Will be an empty string if this authentication mechanism does not send an initial challenge.
     *
     * @param result  Returns:
     *                - ALLJOYN_AUTH_OK        if the authentication is complete,
     *                - ALLJOYN_AUTH_CONTINUE  if the authentication is incomplete
     *                - ALLJOYN_AUTH_FAIL      if the authentication conversation failed on startup
     * @return returns a string with the initial challenge
     */
    virtual qcc::String InitialChallenge(AuthResult& result) { result = ALLJOYN_AUTH_CONTINUE; return ""; }

    /**
     * Responses flow from clients to servers.
     *
     * Process a challenge and generate a response
     *
     * @param challenge Challenge provided by the server
     * @param result    Returns:
     *                      - ALLJOYN_AUTH_OK  if the authentication is complete,
     *                      - ALLJOYN_AUTH_CONTINUE if the authentication is incomplete,
     *                      - ALLJOYN_AUTH_ERROR    if the response was rejected.
     */
    virtual qcc::String Response(const qcc::String& challenge, AuthResult& result) = 0;

    /**
     * Request the initial response. Will be an empty string if this authentication mechanism does not send an initial response.
     *
     * @param result  Returns:
     *                - ALLJOYN_AUTH_OK        if the authentication is complete,
     *                - ALLJOYN_AUTH_CONTINUE  if the authentication is incomplete
     *                - ALLJOYN_AUTH_FAIL      if the authentication conversation failed on startup
     * @return a string
     */
    virtual qcc::String InitialResponse(AuthResult& result) { result = ALLJOYN_AUTH_CONTINUE; return ""; }

    /**
     * Get the name for the authentication mechanism
     *
     * @return string with the name for the authentication mechanism
     */
    virtual const char* GetName() = 0;

    /**
     * Accessor function to get the master secret from authentication mechanisms that negotiate one.
     *
     * @param secret  The master secret key blob
     *
     * @return   - ER_OK on success.
     *           - ER_BUS_KEY_UNAVAILABLE if unable to get the master secret.
     */
    QStatus GetMasterSecret(qcc::KeyBlob& secret)
    {
        if (masterSecret.IsValid()) {
            secret = masterSecret;
            return ER_OK;
        } else {
            return ER_BUS_KEY_UNAVAILABLE;
        }
    }

    /**
     * Indicates if the authentication mechanism is interactive (i.e. involves application or user
     * input) or is automatic. If an authentication mechanism is not interactive it is not worth
     * making multiple authentication attempts because the result will be the same each time. On the
     * other hand, authentication methods that involve user input, such as password entry would
     * normally allow one or more retries.
     *
     * @return  Returns true if this authentication method involves user interaction.
     */
    virtual bool IsInteractive() { return false; }

    /**
     * Destructor
     */
    virtual ~AuthMechanism() { }

  protected:

    /**
     * Key blob if mechanism negotiates a master secret.
     */
    qcc::KeyBlob masterSecret;

    /**
     * Class instance for interacting with user and/or application to obtain a password and other
     * information.
     */
    AuthListener* listener;

    /**
     * Default constructor
     */
    AuthMechanism(KeyStore& keyStore, AuthListener* listener) : listener(listener), keyStore(keyStore), authCount(0) { }

    /**
     * Default constructor
     */
    AuthMechanism(KeyStore& keyStore) : listener(NULL), keyStore(keyStore), authCount(0) { }

    /**
     * The keyStore
     */
    KeyStore& keyStore;

    /**
     * The number of times this authentication has been attempted.
     */
    uint16_t authCount;

    /**
     * The current role of the authenticating peer.
     */
    AuthRole authRole;

    /**
     * A name for the remote peer that is being authenticated.
     */
    qcc::String authPeer;
};


/**
 * This class manages authentication mechanisms
 */
class AuthManager {
  public:

    /**
     * Constructor
     */
    AuthManager(KeyStore& keyStore) : keyStore(keyStore) { }

    /**
     * Type for an factory for an authentication mechanism. AllJoynAuthentication mechanism classes
     * provide a function of this type when registering with the AllJoynAuthentication mechanism manager.
     *
     * @param listener  Required for authentication mechanisms that interact with the user or
     *                  application. Can be NULL if not required.
     */
    typedef AuthMechanism* (*AuthMechFactory)(KeyStore& keyStore, AuthListener* listener);

    /**
     * Registers an authentication mechanism factory function and associates it with a
     * specific authentication mechanism name.
     */
    void RegisterMechanism(AuthMechFactory factory, const char* mechanismName)
    {
        authMechanisms[mechanismName] = factory;
    }

    /**
     * Filter out mechanisms with names not listed in the string.
     */
    size_t FilterMechanisms(const qcc::String& list)
    {
        size_t num = 0;
        std::map<qcc::StringMapKey, AuthMechFactory>::iterator it;

        for (it = authMechanisms.begin(); it != authMechanisms.end(); it++) {
            if (list.find(it->first.c_str()) == qcc::String::npos) {
                authMechanisms.erase(it);
                it = authMechanisms.begin();
                num = 0;
            } else {
                ++num;
            }
        }
        return num;
    }

    /**
     * Check that the list of names are registered mechanisms.
     */
    QStatus CheckNames(qcc::String list)
    {
        QStatus status = ER_OK;
        while (!list.empty()) {
            size_t pos = list.find_first_of(' ');
            qcc::String name = list.substr(0, pos);
            if (!authMechanisms.count(name)) {
                status = ER_BUS_INVALID_AUTH_MECHANISM;
                QCC_LogError(status, ("Unknown authentication mecnanism %s", name.c_str()));
                break;
            }
            list.erase(0, (pos == qcc::String::npos) ? pos : pos + 1);
        }
        return status;
    }

    /**
     * Returns an authentication mechanism object for the requested authentication mechanism.
     *
     * @param mechanismName String representing the name of the authentication mechanism
     *
     * @param listener  Required for authentication mechanisms that interact with the user or
     *                  application. Can be NULL if not required.
     *
     * @return An object that implements the requested authentication mechanism or NULL if there is
     *         no such object. Note this function will also return NULL if the authentication
     *         mechanism requires a listener and none has been provided.
     */
    AuthMechanism* GetMechanism(const qcc::String& mechanismName, AuthListener* listener = NULL)
    {
        std::map<qcc::StringMapKey, AuthMechFactory>::iterator it = authMechanisms.find(mechanismName);
        if (authMechanisms.end() != it) {
            return (it->second)(keyStore, listener);
        } else {
            return NULL;
        }
    }


  private:

    /**
     * Reference to the keyStore
     */
    KeyStore& keyStore;

    /**
     * Maps authentication mechanisms names to factory functions
     */
    std::map<qcc::StringMapKey, AuthMechFactory> authMechanisms;
};

}

#undef QCC_MODULE

#endif
