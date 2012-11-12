/**
 * @file Crypto.cc
 *
 * Implementation for methods from Crypto.h
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

#include <assert.h>
#include <ctype.h>

#include <qcc/platform.h>
#include <qcc/Debug.h>
#include <qcc/Crypto.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "CRYPTO"

namespace qcc {

const Crypto_Hash::Algorithm qcc::Crypto_Hash::SHA1 = EVP_sha1;
const Crypto_Hash::Algorithm qcc::Crypto_Hash::MD5 = EVP_md5;
const Crypto_Hash::Algorithm qcc::Crypto_Hash::SHA256 = EVP_sha256;

QStatus Crypto_Hash::Init(Algorithm alg, const uint8_t* hmacKey, size_t keyLen)
{
    QStatus status = ER_OK;

    if (hmacKey == NULL) {
        assert(keyLen == 0);
        if (EVP_DigestInit(&ctx.md, (*alg)()) == 0) {
            status = ER_CRYPTO_ERROR;
            QCC_LogError(status, ("Initializing hash digest"));
        }
        usingMD = true;
    } else {
        assert(keyLen > 0);
        HMAC_CTX_init(&ctx.hmac);
        HMAC_Init_ex(&ctx.hmac, hmacKey, keyLen, (*alg)(), NULL);
        usingHMAC = true;
    }
    return status;
}

Crypto_Hash::~Crypto_Hash(void)
{
    if (usingHMAC) {
        HMAC_CTX_cleanup(&ctx.hmac);
    } else if (usingMD) {
        EVP_MD_CTX_cleanup(&ctx.md);
    }
}

Crypto_Hash& Crypto_Hash::operator=(const Crypto_Hash& other)
{
    if (other.usingHMAC) {
#if OPENSSL_VERSION_NUMBER < 0x1000000
        usingHMAC = false;
#else
        HMAC_CTX_copy(&ctx.hmac, (HMAC_CTX*)&other.ctx.hmac);
        usingHMAC = true;
#endif
        usingMD = false;
    } else if (other.usingMD) {
        EVP_MD_CTX_copy(&ctx.md, &other.ctx.md);
        usingMD = true;
        usingHMAC = false;
    } else {
        usingMD = false;
        usingHMAC = false;
    }
    return *this;
}

Crypto_Hash::Crypto_Hash(const Crypto_Hash& other)
{
    if (other.usingHMAC) {
#if OPENSSL_VERSION_NUMBER < 0x1000000
        usingHMAC = false;
#else
        HMAC_CTX_copy(&ctx.hmac, (HMAC_CTX*)&other.ctx.hmac);
        usingHMAC = true;
#endif
        usingMD = false;
    } else if (other.usingMD) {
        EVP_MD_CTX_copy(&ctx.md, &other.ctx.md);
        usingMD = true;
        usingHMAC = false;
    } else {
        usingMD = false;
        usingHMAC = false;
    }
}


QStatus Crypto_Hash::Update(const uint8_t* buf, size_t bufSize)
{
    QStatus status = ER_OK;
    assert(buf != NULL);

    if (usingHMAC) {
        HMAC_Update(&ctx.hmac, buf, bufSize);
    } else if (usingMD) {
        if (EVP_DigestUpdate(&ctx.md, buf, bufSize) == 0) {
            status = ER_CRYPTO_ERROR;
            QCC_LogError(status, ("Updating hash digest"));
        }
    } else {
        status = ER_CRYPTO_HASH_UNINITIALIZED;
        QCC_LogError(status, ("Hash function not initialized"));
    }
    return status;
}

QStatus Crypto_Hash::Update(BIGNUM* bn)
{
    QStatus status;
    if (bn) {
        size_t len = BN_num_bytes(bn);
        uint8_t* buf = new uint8_t[BN_num_bytes(bn)];
        BN_bn2bin(bn, buf);
        status = Update(buf, len);
        delete [] buf;
    } else {
        status = ER_BAD_ARG_1;
    }
    return status;
}

QStatus Crypto_Hash::Update(const qcc::String& str)
{
    return Update((const uint8_t*)str.data(), str.size());
}

QStatus Crypto_Hash::GetDigest(uint8_t* digest)
{
    QStatus status = ER_OK;
    assert(digest != NULL);
    assert(usingHMAC || usingMD);

    if (usingHMAC) {
        HMAC_Final(&ctx.hmac, digest, NULL);
        usingHMAC = false;
    } else if (usingMD) {
        if (EVP_DigestFinal(&ctx.md, digest, NULL) == 0) {
            status = ER_CRYPTO_ERROR;
            QCC_LogError(status, ("Finalizing hash digest"));
        }
        usingMD = false;
    } else {
        status = ER_CRYPTO_HASH_UNINITIALIZED;
        QCC_LogError(status, ("Hash function not initialized"));
    }
    return status;
}

int Crypto_BigNum::Compare(const Crypto_BigNum& other) const
{
    if ((num == NULL) && (other.num == NULL)) {
        return 0;
    } else if (num == NULL) {
        return -1;
    } else if (other.num == NULL) {
        return 1;
    } else {
        return BN_cmp(num, other.num);
    }
}

size_t Crypto_BigNum::RenderBinary(uint8_t buf[], size_t bufSize) const
{
    size_t size = BN_num_bytes(num);
    if (size > bufSize) {
        uint8_t* tmp = new uint8_t[size];
        BN_bn2bin(num, tmp);
        memcpy(buf, tmp, bufSize);
        delete [] tmp;
        return bufSize;
    } else {
        BN_bn2bin(num, buf);
        return size;
    }
}

qcc::String Crypto_BigNum::RenderString(bool toLower)
{
    char* str = BN_bn2hex(num);
    if (toLower) {
        char* p = str;
        while (char c = *p) {
            *p++ = tolower(c);
        }
    }
    qcc::String outStr = str;
    OPENSSL_free(str);
    return outStr;
}

qcc::String RandHexString(size_t len, bool toLower)
{
    Crypto_BigNum bigNum;
    bigNum.GenerateRandomValue(len * 8);
    return bigNum.RenderString(toLower);
}


QStatus Crypto_PseudorandomFunction(const KeyBlob& secret, const char* label, const qcc::String& seed, uint8_t* out, size_t outLen)
{
    if (!label) {
        return ER_BAD_ARG_2;
    }
    if (!out) {
        return ER_BAD_ARG_4;
    }
    Crypto_SHA256 hash;
    uint8_t digest[Crypto_SHA256::DIGEST_SIZE];

    for (size_t len = 0; len < outLen; len += sizeof(digest)) {
        /*
         * Initialize SHA1 in HMAC mode with the secret
         */
        hash.Init(secret.GetData(), secret.GetSize());
        /*
         * If this is not the first iteration hash in the digest from the previous iteration.
         */
        if (len > 0) {
            hash.Update(digest, sizeof(digest));
        }
        hash.Update((const uint8_t*)label, strlen(label));
        hash.Update((const uint8_t*)seed.data(), seed.size());
        hash.GetDigest(digest);
        memcpy(out, digest, (std::min)(sizeof(digest), outLen - len));
        out += sizeof(digest);
    }
    return ER_OK;
}

}
