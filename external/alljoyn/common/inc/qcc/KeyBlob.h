#ifndef _KEYBLOB_H
#define _KEYBLOB_H
/**
 * @file
 *
 * This file provides a generic key blob implementation.
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

#include <assert.h>
#include <qcc/String.h>
#include <string.h>

#include <qcc/time.h>
#include <qcc/Stream.h>

namespace qcc {

/**
 * Generic encryption key storage.
 */
class KeyBlob {

  public:

    /**
     * Type of key blob.
     */
    typedef enum {
        EMPTY,    // < Key blob is empty.
        GENERIC,  // < Generic key blob - unknown type.
        AES,      // < An AES key (length is obtained from the blob size)
        PKCS8,    // < PKCS8 encoded private key.
        PEM,      // < PEM encoded public key cert.
        INVALID   // < Invalid key bloby - This must be the last type
    } Type;

    /**
     * Constructor.
     */
    KeyBlob() : blobType(EMPTY) { }

    /**
     * Constructor that reproducibly initializes a key blob with data derived from a password.
     *
     * @param password  The password to generate the key from.
     * @param len       Length of the generated key blob
     * @param initType  The type for the key blob.
     */
    KeyBlob(const qcc::String& password, size_t len, const Type initType);

    /**
     * Constructor that initializes the key blob from a byte array.
     *
     * @param key       Pointer to memory containing data to initialize the key blob.
     * @param len       The length of the key in bytes.
     * @param initType  The type for the key blob.
     */
    KeyBlob(const uint8_t* key, size_t len, const Type initType) : blobType(EMPTY) { Set(key, len, initType); }

    /**
     * Constructor that initializes the key blob from a string.
     *
     * @param str       String containing data to initialize the key blob.
     * @param blobType  The type for the key blob.
     */
    KeyBlob(const qcc::String& str, const Type blobType) : blobType(EMPTY) { Set((const uint8_t*)str.data(), str.size(), blobType); }

    /**
     * Copy constructor.
     *
     * @param other  Key blob to copy from.
     */
    KeyBlob(const KeyBlob& other);

    /**
     * Set a keyblob to a random value.
     *
     * @param len       Length of the randomly generated key blob
     * @param blobType  The type for the key blob.
     */
    void Rand(const size_t len, const Type initType);

    /**
     * Xor a keyblob with another key blob.
     *
     * @param other  The other key blob to xor.
     */
    KeyBlob& operator^=(const KeyBlob& other);

    /**
     * Xor a keyblob with some other data.
     *
     * @param data The data to xor with the key blob.
     * @param len  The length of the data.
     *
     * @return  The number of bytes that were xor'd.
     */
    size_t Xor(const uint8_t* data, size_t len);

    /**
     * Erase this key blob.
     */
    void Erase();

    /**
     * Destructor erases the key blob contents
     */
    ~KeyBlob() { Erase(); }

    /**
     * Accessor function that returns the length of the key data.
     *
     * @return The length of the key blob.
     */
    size_t GetSize() const { return (blobType == EMPTY) ? 0 : size; }

    /**
     * Accessor function that returns the type of the key blob.
     */
    Type GetType() const { return blobType; }

    /**
     * Access function that returns the a pointer to the key blob contents.
     *
     * @return  Return a pointer to the key blob contents, or NULL if the key blob is not valid.
     */
    const uint8_t* GetData() const { return IsValid() ? data : NULL; }

    /**
     * Determine if the key is valid.
     *
     * @return true if the key is valid, otherwise false.
     */
    bool IsValid() const { return blobType != EMPTY; }

    /**
     * Set the key blob from a byte array.
     *
     * @param key   Pointer to memory containing the key blob.
     * @param len   The length of the key in bytes.
     *
     * @return ER_OK if the key was set
     */
    QStatus Set(const uint8_t* key, size_t len, Type blobType);

    /**
     * Store a key blob in a sink.
     *
     * @param sink      The data sink to write the blob to.
     */
    QStatus Store(qcc::Sink& sink) const;

    /**
     * Load a key blob from a source.
     *
     * @param source    The data source to read the blob from.
     */
    QStatus Load(qcc::Source& source);

    /**
     * Assignment of a key blob
     */
    KeyBlob& operator=(const KeyBlob& other);

    /**
     * Set an expiration date/time on a key blob.
     *
     * @param expires The expiration date/time.
     */
    void SetExpiration(const Timespec& expires) { expiration = expires; }

    /**
     * Set a tag on the keyblob. A tag in an arbitrary string of 63 characters or less.
     *
     * @param tag  The tag value to set.
     */
    void SetTag(const qcc::String& tag) { this->tag = tag.substr(0, 63); }

    /**
     * Get the tag from the keyblob. A tag in an arbitrary string that associates user specified
     * information with a keyblob. The tag is stored and loaded when the keyblob is stored or
     * loaded.
     *
     * @return   The tag if there is one.
     */
    const qcc::String& GetTag() const { return tag; }

    /**
     * Check if this keyblob has expired.
     *
     * @return Returns true if the keyblob has expired.
     */
    bool HasExpired();

  private:

    Type blobType;
    Timespec expiration;
    uint8_t* data;
    uint16_t size;
    qcc::String tag;

};

}

#endif
