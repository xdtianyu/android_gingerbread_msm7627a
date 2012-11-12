/**
 * @file
 *
 * Define classes to support Hashing and BigNum.  Linux version uses openssl.
 * Windows version rolls its own here.
 *
 */

/******************************************************************************
 *
 *
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

#ifndef _OS_QCC_CRYPTO_H
#define _OS_QCC_CRYPTO_H

#include <qcc/platform.h>

#include <algorithm>
#include <Windows.h>
#include <Wincrypt.h>
#include <memory>

#include <qcc/String.h>
#include <qcc/Debug.h>
#include <qcc/string.h>
#include <Status.h>


using namespace std;

namespace qcc {

/**
 * A large number abstraction class.  One use for this class is in the
 * StunTransactionID class.
 */
class Crypto_BigNum {
  private:
    uint8_t* num;       ///< Storage for the big number
    size_t byteSize;    ///< Size of the BigNum.

    /**
     * Discard the previously stored number, freeing memory that may be
     * allocated.
     */
    void Clean(void) {
        if (num) {
            free(num);
            num = NULL;
        }
        byteSize = 0;
    }

    /**
     * Copy the BigNum from src into *this allocating memory as needed.
     *
     * @param src   Source BigNum to copy the value from.
     *
     * @return  Indication of success or failure.
     */
    QStatus AllocAndCopy(const Crypto_BigNum& src);

  public:
    /**
     * Default constuctor allocates space and initializes the big number storage.
     */
    Crypto_BigNum(void) : num(NULL), byteSize(0) { }

    /**
     * The copy constructor initializes the big number storage to reference a
     * copy of the initializer big number.
     *
     * @param other The other big number used for initialization.
     */
    Crypto_BigNum(const Crypto_BigNum& other) : num(NULL), byteSize(0) { AllocAndCopy(other); }

    /**
     * The destructor releases the memor used for the big number storage.
     */
    ~Crypto_BigNum(void) { free(num); }

    /**
     * Overload the assignment operator to properly copy the big number value into local storage.
     *
     * @param other Reference to the big number to copy the value from.
     *
     * @return  A const reference to our self.
     */
    const Crypto_BigNum& operator=(const Crypto_BigNum& other)
    {
        AllocAndCopy(other);
        return *this;
    }

    /**
     * Compare another BigNum value with our own value.
     *
     * @return  -1 for *this < other, 0 for *this == other, and 1 for *this > other.
     */
    int Compare(const Crypto_BigNum& other) const
    {
        if ((num == NULL) && (other.num == NULL)) {
            return 0;
        } else if (num == NULL) {
            return -1;
        } else if (other.num == NULL) {
            return 1;
        } else {
            size_t cmpSize = min(byteSize, other.byteSize);
            if (byteSize > cmpSize) {
                size_t i;

                for (i = 0; i < byteSize - cmpSize; ++i) {
                    if (num[i] != 0) {
                        return -1;
                    }
                }
            } else if (other.byteSize > cmpSize) {
                size_t i;

                for (i = 0; i < other.byteSize - cmpSize; ++i) {
                    if (other.num[i] != 0) {
                        return 1;
                    }
                }
            }

            return memcmp(num + (byteSize - cmpSize),
                          other.num + (other.byteSize - cmpSize),
                          cmpSize);
        }
    }

    /**
     * Determine if another big number has the same value as we do.
     *
     * @param other The other big number for comparison.
     *
     * @return  "true" if the values are the same.
     */
    bool operator==(const Crypto_BigNum& other) const { return Compare(other) == 0; }

    /**
     * Determine if another big number has a different value as we do.
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
    void GenerateRandomValue(size_t bits);

    /**
     * Convert a number of bytes in to a big number value.
     *
     * @param buf       An array of octets containing the number to be
     *                  converted in big-endian order.
     * @param bufSize   The number of octets to convert.
     */
    void Parse(const uint8_t* buf, size_t bufSize);

    /**
     * Convert a big number into an array of octets in big-endian order.
     *
     * @param buf       An array with sufficient space to store the big number.
     * @param bufSize   The number of octets to convert.
     */
    void RenderBinary(uint8_t* buf, size_t bufSize) const {
        if (num) {
            memcpy(buf, num, min(bufSize, byteSize));
        }
    }

    /**
     * Render a big number as a string in big-endian order
     */
    qcc::String RenderString(bool toLower = false) {
        return BytesToHexString(num, byteSize, toLower);
    }
};


/**
 * Generic hash calculation interface abstraction class.
 */
class Crypto_Hash {
  private:
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
    HCRYPTKEY cryptKey;
    HMAC_INFO HmacInfo;

  protected:
    /// Typedef for abstracting the hash algorithm specifier.
    typedef ALG_ID Algorithm;

    static const Algorithm SHA1 = CALG_SHA1;    ///< SHA1 algorithm specifier
    static const size_t SHA1_SIZE = 20;         ///< SHA1 digest size.

    static const Algorithm MD5 = CALG_MD5;      ///< MD5 algorithm specifier
    static const size_t MD5_SIZE = 16;          ///< MD5 digest size.

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

    /**
     * Returns the digest size depening on the algorithm.
     *
     * @param alg  The algorithm to return the digest size.
     *
     * @return  Number of octets in the digest.
     */
    static const size_t GetDigestSize(Algorithm alg)
    {
        switch (alg) {
        case SHA1: return SHA1_SIZE;

        case MD5:  return MD5_SIZE;
        }
        return 0;
    }

  public:
    /**
     * Default constructor initializes the HMAC context.
     */
    Crypto_Hash(void) : hProv(), hHash(), cryptKey() {
        ZeroMemory(&HmacInfo, sizeof(HmacInfo));
    }

    /**
     * The destructor cleans up the HMAC context.
     */
    virtual ~Crypto_Hash(void) {
        if (0 != hHash) { CryptDestroyHash(hHash); }
        if (0 != hProv) { CryptReleaseContext(hProv, 0); }
        if (0 != cryptKey) { CryptDestroyKey(cryptKey); }
    }


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
     */
    QStatus Update(const uint8_t* buf, size_t bufSize);

    /**
     * Retrieve the digest into the supplied buffer.  It is assumed that
     * buffer is large enough to store the digest.
     *
     * @param digest    Buffer for storing the digest.
     */
    QStatus GetDigest(uint8_t* digest);
};

}   /* namespace */

#endif
