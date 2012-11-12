#ifndef _CRYPTO_H
#define _CRYPTO_H
/**
 * @file
 *
 * This file provide wrappers around cryptographic algorithms.
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

#include <qcc/platform.h>

#include <assert.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#include <qcc/KeyBlob.h>
#include <qcc/Stream.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>


namespace qcc {

/**
 * RSA public-key encryption and decryption class
 */
class Crypto_RSA {

  public:

    /**
     * Class for calling back to get a passphrase for encrypting or decrypting a private key.
     */
    class PassphraseListener {
      public:
        /**
         * Virtual destructor for derivable class.
         */
        virtual ~PassphraseListener() { }

        /**
         * Callback to request a passphrase.
         *
         * @param passphrase  Returns the passphrase.
         * @param toWrite     If true indicates the passphrase is being used to write a private key,
         *                    if false indicates the passphrase is for reading a private key.
         *
         * @return  Returns false if the passphrase request has been rejected.
         */
        virtual bool GetPassphrase(qcc::String& passphrase, bool toWrite) = 0;
    };

    /**
     * Constructor to generate a new key pair
     *
     * @param modLen    The length of the modulus in bytes
     * @param exponent  The exponent must be an odd number, most commonly 65537
     */
    Crypto_RSA(uint32_t modLen, uint32_t exponent = 65537) { Generate(modLen, exponent); }

    /**
     * Default constructor.
     */
    Crypto_RSA();

    /**
     * Constructor to initialize a public key from a PEM-encoded X509 certificate.
     *
     * @param pem   The PEM encoded string.
     */
    Crypto_RSA(const qcc::String& pem);

    /**
     * Constructor to initialize a key-pair from a PKCS#8 or PEM encoded string.
     *
     * @param pem        The PEM or PKCS#8 encoded string
     * @param listener   The listener to call if a passphrase is required to decrypt a private key.
     */
    Crypto_RSA(const qcc::String& pkcs8, PassphraseListener* listener);

    /**
     * Constructor to initialize a key-pair from a PKCS#8 or PEM encoded string.
     *
     * @param pem        The PEM or PKCS#8 encoded string
     * @param passphrase The passphrase required to decode the private key
     */
    Crypto_RSA(const qcc::String& pkcs8, const qcc::String& passphrase);

    /**
     * Set a key-pair from a PKCS#8 or PEM encoded string.
     *
     * @param pkcs8       The PEM or PKCS#8 encoded string
     * @param passphrase The passphrase required to decode the private key
     * @return  - ER_OK if the key was decrypted.
     *          - ER_AUTH_FAIL if the passphrase was incorrect or the PEM string was invalid.
     *          - Other error status.
     */
    QStatus FromPKCS8(const qcc::String& pkcs8, const qcc::String& passphrase);

    /**
     * Set a key-pair from a PKCS#8 or PEM encoded string.
     *
     * @param pkcs8      The PEM or PKCS#8 encoded string
     * @param listener   The listener to call if a passphrase is required to decrypt a private key.
     * @return  - ER_OK if the key was decrypted.
     *          - ER_AUTH_FAIL if the passphrase was incorrect or the PEM string was invalid.
     *          - ER_AUTH_REJECT if the listener rejected the authentication request.
     *          - Other error status.
     */
    QStatus FromPKCS8(const qcc::String& pkcs8, PassphraseListener* listener);

    /**
     * Set a public key from a PEM-encoded X509 certificate.
     *
     * @param pem   The PEM encoded cert..
     */
    QStatus FromPEM(const qcc::String& pem);

    /**
     * Generate a public/private key pair
     */
    void Generate(uint32_t modLen = 512, uint32_t exponent = 65537);

    /**
     * Returns the PEM (PKCS#8) encoded string for a private key. Returns an empty string if the key
     * is not a private key.
     *
     * @param pem          Returns the PEM encoded private key.
     * @param passphrase   The passphrase to encrypt the private key
     * @return  - ER_OK if the key was encoded.
     *          - Other error status.
     */
    QStatus PrivateToPEM(qcc::String& pem, const qcc::String& passphrase);

    /**
     * Returns the PEM (PKCS#8) encoded string for a private key. Returns an empty string if the key
     * is not a private key.
     *
     * @param pem         Returns the PEM encoded private key.
     * @param listener   The listener to call to obtain a passphrase to encrypt the private key.
     * @return  - ER_OK if the key was encoded.
     *          - ER_AUTH_REJECT if the listener rejected the authentication request.
     *          - Other error status.
     */
    QStatus PrivateToPEM(qcc::String& pem, PassphraseListener* listener);

    /**
     * Returns the PEM encoded string for a public key.
     *
     * @param pem   Returns the PEM encoded public key.
     * @return  - ER_OK if the key was encoded.
     *          - Other error status.
     */
    QStatus PublicToPEM(qcc::String& pem);

    /**
     * Returns the RSA modulus size in bytes. The size can be used to determine the buffer size that
     * must must allocated to receive an encrypted value.
     *
     * @return The modulus size in bytes.
     */
    size_t GetSize();

    /**
     * Returns the maximum size of the message digest (or other data) that can be encrypted. This is
     * also the minimum size of the buffer that must be provided when decrypting.
     *
     * @return The maximum digest size in bytes.
     */
    size_t MaxDigestSize() { return (GetSize() - 12); }

    /**
     * Encrypt data using the public key.
     *
     * @param inData   The data to be encrypted
     * @param inLen    The length of the data to be encrypted
     * @param outData  The buffer to receive the encrypted data, must be <= GetSize()
     * @param outLen   On input the length of outData, this must be >= GetSize(), on return the
     *                 number of bytes on inData encrypted.
     *
     * @return  ER_OK if the data was encrypted.
     */
    QStatus PublicEncrypt(const uint8_t* inData, size_t inLen, uint8_t* outData, size_t& outLen);

    /**
     * Decrypt data using the private key.
     *
     * @param inData   The data to be decrypted
     * @param inLen    The length of the data to be decrypted
     * @param outData  The buffer to receive the decrypted data, must be <= GetSize()
     * @param outLen   On input the length of outData, this must be >= GetSize(), on return the
     *                 number of bytes on inData decrypted.
     *
     * @return  ER_OK if the data was encrypted.
     */
    QStatus PrivateDecrypt(const uint8_t* inData, size_t inLen, uint8_t* outData, size_t& outLen);

    /**
     * Encrypt data using the private key.
     *
     * @param inData   The data to be encrypted
     * @param inLen    The length of the data to be encrypted
     * @param outData  The buffer to receive the encrypted data, must be <= GetSize()
     * @param outLen   On input the length of outData, this must be >= GetSize(), on return the
     *                 number of bytes on inData encrypted.
     *
     * @return  ER_OK if the data was encrypted.
     */
    QStatus PrivateEncrypt(const uint8_t* inData, size_t inLen, uint8_t* outData, size_t& outLen);

    /**
     * Decrypt data using the public key.
     *
     * @param inData   The data to be decrypted
     * @param inLen    The length of the data to be decrypted
     * @param outData  The buffer to receive the decrypted data, must be <= GetSize()
     * @param outLen   On input the length of outData, this must be >= GetSize(), on return the
     *                 number of bytes on inData decrypted.
     *
     * @return  ER_OK if the data was encrypted.
     */
    QStatus PublicDecrypt(const uint8_t* inData, size_t inLen, uint8_t* outData, size_t& outLen);

    /**
     * Generate a self-issued X509 certificate for this RSA key.
     *
     * @param name  The common name for this certificate.
     * @param app   The application that is requesting this certificate.
     */
    QStatus MakeSelfCertificate(const qcc::String& name, const qcc::String& app);

    /**
     * Use this private key to sign a document and return the digital signature.
     *
     * @param doc       The document to sign.
     * @param docLen    The length of the document to sign.
     * @param signature Returns the digital signature.
     * @param sigLen    On input the length of signature, this must be >= GetSize(), on return the
     *                  number of bytes in the signature.
     *
     * @return     - ER_OK if the signature was generated.
     *             - An error status indicating the error.
     */
    QStatus Sign(const uint8_t* data, size_t len, uint8_t* signature, size_t& sigLen);

    /**
     * Use this public key to verify the digital signature on a document.
     *
     * @param doc       The document to sign.
     * @param docLen    The length of the document to sign.
     * @param signature Returns the digital signature.
     * @param sigLen    On input the length of signature, this must be >= GetSize(), on return the
     *                  number of bytes in the signature.
     *
     * @return     - ER_OK if the signature was verified.
     *             - ER_AUTH_FAIL if the verfication failed.
     *             - Other status codes indicating an error.
     */
    QStatus Verify(const uint8_t* data, size_t len, const uint8_t* signature, size_t sigLen);

    /**
     * Returns a human readable string for a cert if there is one associated with this key.
     *
     * @return A string for the cert or and empty string if there is no cert.
     */
    qcc::String CertToString();

    /**
     * Destructor
     */
    ~Crypto_RSA();

  private:

    /**
     * Assignment operator is private
     */
    Crypto_RSA& operator=(const Crypto_RSA& other) {
        if (this != &other) {
            key = other.key;
        }
        return *this;
    }

    /**
     * Copy constructor is private
     */
    Crypto_RSA(const Crypto_RSA& other) { key = other.key; }

    void* cert;

    void* key;
};

/**
 * AES block encryption/decryption class
 */
class Crypto_AES  {

  public:

    /**
     * Size of an AES128 key in bytes
     */
    static const size_t AES128_SIZE = (128 / 8);

    /**
     * Flag to constructor indicating key is being used to encryption
     */
    static const bool ENCRYPT = true;

    /**
     * Flag to constructor indicating key is being used to decryption
     */
    static const bool DECRYPT = false;

    /**
     * CryptoAES constructor. A specific class instance can be used to either encrypt or decrypt.
     *
     * @param key      The AES key
     * @param encrypt  Indicates if the class instance is for encrypting or decrypting.
     */
    Crypto_AES(const KeyBlob& key, bool encrypt);

    /**
     * Data in encrypted or decrypted in 16 byte blocks.
     */
    class Block {
      public:
        uint8_t data[16];
        /**
         * Constructor that initializes the block.
         *
         * @param ival  The initial value.
         */
        Block(uint8_t ival) { memset(data, ival, sizeof(data)); }
        /**
         * Default constructor.
         */
        Block() { };
        /**
         * Null pad end of block
         */
        void Pad(size_t padLen) {
            assert(padLen <= 16);
            if (padLen > 0) { memset(&data[16 - padLen], 0, padLen); }
        }
    };

    /**
     * Helper function to return the number of blocks required to hold a encrypt a number of bytes.
     *
     * @param len  The data length
     * @return     The number of blocks for data of the requested length.
     */
    static size_t NumBlocks(size_t len) { return (len + sizeof(Block) - 1) / sizeof(Block); }

    /**
     * Encrypt some data blocks.
     *
     * @param in          An array of data blocks to encrypt
     * @param out         The encrypted data blocks
     * @param numBlocks   The number of blocks to encrypt.
     *
     * @return ER_OK if the data was encrypted.
     */
    QStatus Encrypt(const Block* in, Block* out, uint32_t numBlocks);

    /**
     * Encrypt some data. The encrypted data is padded as needed to round up to a whole number of blocks.
     *
     * @param in          An array of data blocks to encrypt
     * @param len         The length of the input data
     * @param out         The encrypted data blocks
     * @param numBlocks   The number of blocks for the encrypted data, compute this using NumEncryptedBlocks(len)
     * @return ER_OK if the data was encrypted.
     */
    QStatus Encrypt(const void* in, size_t len, Block* out, uint32_t numBlocks);

    /**
     * Decrypt some data blocks.
     *
     * @param in          An array of data blocks to decrypt
     * @param out         The decrypted data blocks
     * @param numBlocks   The number of blocks to decrypt.
     * @return ER_OK if the data was decrypted.
     */
    QStatus Decrypt(const Block* in, Block* out, uint32_t numBlocks);

    /**
     * Decrypt some data. The decrypted data is truncated at len
     *
     * @param in          An array of data blocks to decrypt.
     * @param len         The number of blocks of input data.
     * @param out         The decrypted data.
     * @param numBlocks   The number of blocks for the decrypted data, compute this using NumEncryptedBlocks(len)
     *
     * @return ER_OK if the data was encrypted.
     */
    QStatus Decrypt(const Block* in, uint32_t numBlocks, void* out, size_t len);

    /**
     * Encrypt some data using CCM mode.
     *
     * @param in          Pointer to the data to encrypt
     * @param out         The encrypted data, this can be the same as in. The size of this buffer
     *                    must be large enough to hold the encrypted input data and the
     *                    authentication field. This means at least (len + authLen) bytes.
     * @param len         On input the length of the input data,returns the length of the output data.
     * @param nonce       A nonce. A different nonce must be used each time.
     * @param addData     Additional data to be authenticated.
     * @param addLen      Length of the additional data.
     * @param authField   Returns the authentication field.
     * @param authLen     Lengh of the authentication field, must be in range 4..16
     *
     * @return ER_OK if the data was encrypted.
     */
    QStatus Encrypt_CCM(const void* in, void* out, size_t& len, const KeyBlob& nonce, const void* addData, size_t addLen, uint8_t authLen = 8);

    /**
     * Convenience wrapper for encrypting and authenticating a header and message in-place.
     *
     * @param msg      Pointer to the entire message. The message buffer must be long enough to
     *                 allow for the authentication field (of lenght authLen) to be appended.
     * @param msgLen   On input, the length in bytes of the plaintext message, on output the expanded
     *                 length of the encrypted message.
     * @param hdrLen   Length in bytes of the header portion of the message
     */
    QStatus Encrypt_CCM(void* msg, size_t& msgLen, size_t hdrLen, const KeyBlob& nonce, uint8_t authLen = 8)
    {
        if (!msg) {
            return ER_BAD_ARG_1;
        }
        if (msgLen < hdrLen) {
            return ER_BAD_ARG_2;
        }
        size_t len = msgLen - hdrLen;
        QStatus status = Encrypt_CCM((uint8_t*)msg + hdrLen, (uint8_t*)msg + hdrLen, len, nonce, msg, hdrLen, authLen);
        msgLen = hdrLen + len;
        return status;
    }

    /**
     * Decrypt some data using CCM mode.
     *
     * @param in          An array of to encrypt
     * @param out         The encrypted data blocks, this can be the same as in.
     * @param len         On input the length of the input data, returns the length of the output data.
     * @param nonce       A nonce. A different nonce must be used each time.
     * @param addData     Additional data to be authenticated.
     * @param addLen      Length of the additional data.
     * @param authLen     Length of the authentication field, must be in range 4..16
     *
     * @return ER_OK if the data was decrypted and verified.
     *         ER_AUTH_FAIL if the decryption failed.
     */
    QStatus Decrypt_CCM(const void* in, void* out, size_t& len, const KeyBlob& nonce, const void* addData, size_t addLen, uint8_t authLen = 8);

    /**
     * Convenience wrapper for decrypting and authenticating a header and message in-place.
     *
     * @param msg      Pointer to the entire message.
     * @param msgLen   On input, the length in bytes of the encrypted message, on output the
     *                 length of the plaintext message.
     * @param hdrLen   Length in bytes of the header portion of the message
     */
    QStatus Decrypt_CCM(void* msg, size_t& msgLen, size_t hdrLen, const KeyBlob& nonce, uint8_t authLen = 8)
    {
        if (!msg) {
            return ER_BAD_ARG_1;
        }
        if (msgLen < hdrLen) {
            return ER_BAD_ARG_2;
        }
        size_t len = msgLen - hdrLen;
        QStatus status = Decrypt_CCM((uint8_t*)msg + hdrLen, (uint8_t*)msg + hdrLen, len, nonce, msg, hdrLen, authLen);
        msgLen = hdrLen + len;
        return status;
    }

    /**
     * Destructor
     */
    ~Crypto_AES();

  private:

    Crypto_AES() { }

    /**
     * Copy constructor is private and does nothing
     */
    Crypto_AES(const Crypto_AES& other) { }

    /**
     * Assigment operator is private and does nothing
     */
    Crypto_AES& operator=(const Crypto_AES& other) { return *this; }

    /**
     * Internal function used by Encrypt_CCM and Decrypt_CCM
     */
    void Compute_CCM_AuthField(Block& T, uint8_t M, uint8_t L, const KeyBlob& nonce, const uint8_t* mData, size_t mLen, const uint8_t* addData, size_t addLen);

    /**
     * Flag indicating if the class instance is encrypting or decrypting
     */
    bool encrypt;

    /**
     * Private internal key state
     */
    uint8_t* keyState;

};

/**
 * Wrapper class wraps the BIGNUM interface from the OpenSSL library.
 */
class Crypto_BigNum {
  public:
    /**
     * Default constuctor allocates space and initializes the big number storage.
     */
    Crypto_BigNum(void) : num(BN_new()) { }

    /**
     * The copy constructor initializes the big number storage to reference a
     * copy of the initializer big number.
     *
     * @param other The other big number used for initialization.
     */
    Crypto_BigNum(const Crypto_BigNum& other) : num(BN_dup(other.num)) { }

    /**
     * The destructor releases the memory used for the big number storage.
     */
    ~Crypto_BigNum(void) { BN_free(num); }

    /**
     * Overload the assignment operator to properly copy the big number value into local storage.
     *
     * @param other Reference to the big number to copy the value from.
     *
     * @return  A const reference to our self.
     */
    const Crypto_BigNum& operator=(const Crypto_BigNum& other)
    {
        if (&other != this) {
            BN_copy(num, other.num);
        }
        return *this;
    }

    /**
     * Compare another BigNum value with our own value.
     *
     * @return  -1 for *this < other, 0 for *this == other, and 1 for *this > other.
     */
    int Compare(const Crypto_BigNum& other) const;

    /**
     * Determine if another big number has the same value as we do.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if the values are the same.
     */
    bool operator==(const Crypto_BigNum& other) const { return Compare(other) == 0; }

    /**
     * Determine if another big number has a different value than we do.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if the values are different.
     */
    bool operator!=(const Crypto_BigNum& other) const { return Compare(other) != 0; }

    /**
     * Determine if *this > other.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if *this > other.
     */
    bool operator>(const Crypto_BigNum& other) const { return Compare(other) > 0; }

    /**
     * Determine if *this >= other.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if *this >= other.
     */
    bool operator>=(const Crypto_BigNum& other) const { return Compare(other) >= 0; }

    /**
     * Determine if *this < other.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if *this < other.
     */
    bool operator<(const Crypto_BigNum& other) const { return Compare(other) < 0; }

    /**
     * Determine if *this <= other.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if *this <= other.
     */
    bool operator<=(const Crypto_BigNum& other) const { return Compare(other) <= 0; }

    /**
     * Set the big number to a cryptographically random number of a specified
     * number of bits.
     *
     * @param bits  The size of the number in bits.
     */
    void GenerateRandomValue(size_t bits) { BN_rand(num, bits, -1, 0); }

    /**
     * Convert a number of bytes in to a big number value.
     *
     * @param buf       An array of octets containing the number to be
     *                  converted in big-endian order.
     * @param bufSize   The number of octets to convert.
     */
    void Parse(const uint8_t buf[], size_t bufSize) { num = BN_bin2bn(buf, bufSize, num); }

    /**
     * Convert a big number into an array of octets in big-endian order.
     *
     * @param buf       An array to store the big number.
     * @param bufSize   The number of octets to convert.
     *
     * @return  The number of octets actually converted.
     */
    size_t RenderBinary(uint8_t buf[], size_t bufSize) const;

    /**
     * Render a big number as a string in big-endian order
     */
    qcc::String RenderString(bool toLower = false);

  private:
    BIGNUM* num;    ///< Storage for the big number.

};

/**
 * Generic hash calculation interface abstraction class.
 */
class Crypto_Hash {
  public:

    /**
     * Default constructor initializes the HMAC context.
     */
    Crypto_Hash(void) : usingHMAC(false), usingMD(false) { }

    /**
     * The destructor cleans up the HMAC context.
     */
    virtual ~Crypto_Hash(void);

    /**
     * Virtual initializer to be implemented by derivative classes.  The
     * derivative classes should call the protected Crypto_Hash::Init()
     * function with the appropriate algorithm.
     *
     * @param hmacKey   [optional] An array containing the HMAC key.
     *                             (Defaults to NULL for no key.)
     * @param keyLen    [optional] The size of the HMAC key.
     *                             (Defaults to 0 for no key.)
     *
     * @return  Indication of success or failure.
     */
    virtual QStatus Init(const uint8_t* hmacKey = NULL, size_t keyLen = 0) = 0;

    /**
     * Update the digest with the contents of the specified buffer.
     *
     * @param buf       Buffer with data for feeding to the hash algorithm.
     * @param bufSize   Size of the buffer.
     *
     * @return  Indication of success or failure.
     */
    QStatus Update(const uint8_t* buf, size_t bufSize);

    /**
     * Update the digest with the contents of a BIGNUM
     *
     * @param bn   The BIGNUM to hash.
     *
     * @return  Indication of success or failure.
     */
    QStatus Update(BIGNUM* bn);

    /**
     * Update the digest with the contents of a string
     *
     * @param str   The string to hash.
     *
     * @return  Indication of success or failure.
     */
    QStatus Update(const qcc::String& str);

    /**
     * Retrieve the digest into the supplied buffer.  It is assumed that buffer is large enough to
     * store the digest. After the digest has been computed the hash instance is not longer usable
     * until re-initialized.
     *
     * @param digest    Buffer for storing the digest.
     *
     * @return  Indication of success or failure.
     */
    QStatus GetDigest(uint8_t* digest);

    /**
     * Assigns a hash. The state of the hash is duplicated in the destination.
     */
    Crypto_Hash& operator =(const Crypto_Hash& other);

    /**
     * Copy constructor. The state of the hash is duplicated in the destination.
     */
    Crypto_Hash(const Crypto_Hash& other);

  protected:

    /// Typedef for abstracting the hash algorithm specifier.
    typedef const EVP_MD* (*Algorithm)(void);

    static const Algorithm SHA1;        ///< SHA1 algorithm specifier
    static const size_t SHA1_SIZE = SHA_DIGEST_LENGTH;      ///< SHA1 digest size.

    static const Algorithm MD5;         ///< MD5 algorithm specifier
    static const size_t MD5_SIZE = MD5_DIGEST_LENGTH;       ///< MD5 digest size.

    static const Algorithm SHA256;      ///< SHA256 algorithm specifier
    static const size_t SHA256_SIZE = SHA256_DIGEST_LENGTH;    ///< SHA256 digest size.

    /**
     * The common initializer.  Derivative classes should call this from their
     * public implementation of Init().
     *
     * @param alg       Algorithm to be used for the hash function.
     * @param hmacKey   An array containing the HMAC key.
     * @param keyLen    The size of the HMAC key.
     *
     * @return  Indication of success or failure.
     */
    QStatus Init(Algorithm alg, const uint8_t* hmacKey = NULL, size_t keyLen = 0);

  private:
    bool usingHMAC;     ///< Flag indicating if computing an HMAC
    bool usingMD;       ///< Flag indicating if computing an MD hash.

    /// Union of context storage for HMAC or MD.
    union {
        HMAC_CTX hmac;  ///< Storage for the HMAC context.
        EVP_MD_CTX md;  ///< Storage for the MD context.
    } ctx;

};

/**
 * SHA1 hash calculation interface abstraction class.
 */
class Crypto_SHA1 : public Crypto_Hash {
  public:
    virtual QStatus Init(const uint8_t* hmacKey = NULL, size_t keyLen = 0)
    {
        return Crypto_Hash::Init(SHA1, hmacKey, keyLen);
    }

    static const size_t DIGEST_SIZE = Crypto_Hash::SHA1_SIZE;
};

/**
 * MD5 hash calculation interface abstraction class.
 */
class Crypto_MD5 : public Crypto_Hash {
  public:
    virtual QStatus Init(const uint8_t* hmacKey = NULL, size_t keyLen = 0)
    {
        return Crypto_Hash::Init(MD5, hmacKey, keyLen);
    }

    static const size_t DIGEST_SIZE = Crypto_Hash::MD5_SIZE;
};

/**
 * SHA256 hash calculation interface abstraction class.
 */
class Crypto_SHA256 : public Crypto_Hash {
  public:
    virtual QStatus Init(const uint8_t* hmacKey = NULL, size_t keyLen = 0)
    {
        return Crypto_Hash::Init(SHA256, hmacKey, keyLen);
    }

    static const size_t DIGEST_SIZE = Crypto_Hash::SHA256_SIZE;
};

/**
 * This function uses one of more HMAC hashes to implement the PRF (Pseudorandom Function) defined
 * in RFC 5246 and is used to expand a secret into an arbitrarily long block of data from which keys
 * can be derived. Per the recommendation in RFC 5246 this function uses the SHA256 hash function.
 *
 * @param secret  A keyblob containing the secret being expanded.
 * @param label   An ASCII string that is hashed in with the other data.
 */
QStatus Crypto_PseudorandomFunction(const KeyBlob& secret, const char* label, const qcc::String& seed, uint8_t* out, size_t outLen);


/**
 *  Secure Remote Password (SRP6) class. This implements the core algorithm for Secure Remote
 *  Password authentication protocol as defined in RFC 5054.
 */
class Crypto_SRP {
  public:

    /**
     * Initialize the client side of SRP. The client is initialized with a string from the server
     * that encodes the startup parameters.
     *
     * @param fromServer   Hex-encoded parameter string from the server.
     * @param toServer     Retusn a hex-encode parameter string to be sent to the server.
     *
     * @return  - ER_OK if the initialization was succesful.
     *          - ER_BAD_STRING_ENCODING if the hex-encoded parameter block is badly formed.
     *          - ER_CRYPTO_INSUFFICIENT_SECURITY if the parameters are not acceptable.
     *          - ER_CRYPTO_ILLEGAL_PARAMETERS if any parameters have illegal values.
     */
    QStatus ClientInit(const qcc::String& fromServer, qcc::String& toServer);

    /**
     * Final phase of the client side .
     *
     * @param id   The user use id to authenticate.
     * @param pwd  The password corresponding to the user id.
     *
     * @return ER_OK or an error status.
     */
    QStatus ClientFinish(const qcc::String& id, const qcc::String& pwd);

    /**
     * Initialize the server side of SRP. The server is initialized with the user id and password
     * and returns a hex-encoded string to be sent to the client that encodes the startup parameters.
     *
     * @param id        The id of the user being authenticated.
     * @param pwd       The password corresponding to the user id.
     * @param toClient  Hex-encoded parameter string to be sent to the client.
     *
     * @return  - ER_OK or an error status.
     */
    QStatus ServerInit(const qcc::String& id, const qcc::String& pwd, qcc::String& toClient);

    /**
     * Initialize the server side of SRP using a hex-encoded verifier string that encodes the user name and
     * password. Returns a hex-encoded string to be sent to the client that encodes the startup parameters.
     *
     * @param verifier   An encoding of the user id and password that is used to verify the client.
     * @param toClient   Hex-encoded parameter string to be sent to the client.
     *
     * @return  - ER_OK or an error status.
     */
    QStatus ServerInit(const qcc::String& verifier, qcc::String& toClient);

    /**
     * Final phase of the server side of SRP. The server is provided with a string from the client
     * that encodes the client's parameters.
     *
     * @param fromClient   Hex-encoded parameter string from the client.
     *
     * @return  - ER_OK if the initialization was succesful.
     *          - ER_BAD_STRING_ENCODING if the hex-encoded parameter block is badly formed.
     *          - ER_CRYPTO_ILLEGAL_PARAMETERS if any parameters have illegal values.
     */
    QStatus ServerFinish(const qcc::String fromClient);

    /**
     * Get encoded verifier. The verifier can stored for future use by the server-side of the
     * protocol without requiring knowledge of the password. This function can be called immediately
     * after ServerInit() has been called.
     *
     * @return Returns the verifier string.
     */
    qcc::String ServerGetVerifier();

    /**
     * Get the computed premaster secret. This function should be called after ServerFinish() or
     * ClientFinish() to obtain the shared premaster secret.
     *
     * @param premaster  Returns a key blob initialized with the premaster secret.
     */
    void GetPremasterSecret(KeyBlob& premaster);

    /**
     * Constructor
     */
    Crypto_SRP();

    /**
     * Destructor
     */
    ~Crypto_SRP();

    /**
     * Test interface runs the RFC 5246 test vector.
     */
    QStatus TestVector();

  private:

    /**
     * Copy constructor is private and does nothing
     */
    Crypto_SRP(const Crypto_SRP& other) { }

    /**
     * Assigment operator is private and does nothing
     */
    Crypto_SRP& operator=(const Crypto_SRP& other) { return *this; }

    void ServerCommon(qcc::String& toClient);

    class BN;
    BN* bn;

};


}

#endif
