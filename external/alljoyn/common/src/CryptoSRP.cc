/**
 * @file CryptoSRP.cc
 *
 * Class for SRP
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
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <openssl/bn.h>
#include <openssl/sha.h>

#include <qcc/Debug.h>
#include <qcc/Crypto.h>
#include <qcc/KeyBlob.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>

#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "CRYPTO"

static const uint8_t Prime1024[] = {
    0xEE, 0xAF, 0x0A, 0xB9, 0xAD, 0xB3, 0x8D, 0xD6, 0x9C, 0x33, 0xF8, 0x0A, 0xFA, 0x8F, 0xC5, 0xE8,
    0x60, 0x72, 0x61, 0x87, 0x75, 0xFF, 0x3C, 0x0B, 0x9E, 0xA2, 0x31, 0x4C, 0x9C, 0x25, 0x65, 0x76,
    0xD6, 0x74, 0xDF, 0x74, 0x96, 0xEA, 0x81, 0xD3, 0x38, 0x3B, 0x48, 0x13, 0xD6, 0x92, 0xC6, 0xE0,
    0xE0, 0xD5, 0xD8, 0xE2, 0x50, 0xB9, 0x8B, 0xE4, 0x8E, 0x49, 0x5C, 0x1D, 0x60, 0x89, 0xDA, 0xD1,
    0x5D, 0xC7, 0xD7, 0xB4, 0x61, 0x54, 0xD6, 0xB6, 0xCE, 0x8E, 0xF4, 0xAD, 0x69, 0xB1, 0x5D, 0x49,
    0x82, 0x55, 0x9B, 0x29, 0x7B, 0xCF, 0x18, 0x85, 0xC5, 0x29, 0xF5, 0x66, 0x66, 0x0E, 0x57, 0xEC,
    0x68, 0xED, 0xBC, 0x3C, 0x05, 0x72, 0x6C, 0xC0, 0x2F, 0xD4, 0xCB, 0xF4, 0x97, 0x6E, 0xAA, 0x9A,
    0xFD, 0x51, 0x38, 0xFE, 0x83, 0x76, 0x43, 0x5B, 0x9F, 0xC6, 0x1D, 0x2F, 0xC0, 0xEB, 0x06, 0xE3
};

static const uint8_t Prime1536[] = {
    0x9D, 0xEF, 0x3C, 0xAF, 0xB9, 0x39, 0x27, 0x7A, 0xB1, 0xF1, 0x2A, 0x86, 0x17, 0xA4, 0x7B, 0xBB,
    0xDB, 0xA5, 0x1D, 0xF4, 0x99, 0xAC, 0x4C, 0x80, 0xBE, 0xEE, 0xA9, 0x61, 0x4B, 0x19, 0xCC, 0x4D,
    0x5F, 0x4F, 0x5F, 0x55, 0x6E, 0x27, 0xCB, 0xDE, 0x51, 0xC6, 0xA9, 0x4B, 0xE4, 0x60, 0x7A, 0x29,
    0x15, 0x58, 0x90, 0x3B, 0xA0, 0xD0, 0xF8, 0x43, 0x80, 0xB6, 0x55, 0xBB, 0x9A, 0x22, 0xE8, 0xDC,
    0xDF, 0x02, 0x8A, 0x7C, 0xEC, 0x67, 0xF0, 0xD0, 0x81, 0x34, 0xB1, 0xC8, 0xB9, 0x79, 0x89, 0x14,
    0x9B, 0x60, 0x9E, 0x0B, 0xE3, 0xBA, 0xB6, 0x3D, 0x47, 0x54, 0x83, 0x81, 0xDB, 0xC5, 0xB1, 0xFC,
    0x76, 0x4E, 0x3F, 0x4B, 0x53, 0xDD, 0x9D, 0xA1, 0x15, 0x8B, 0xFD, 0x3E, 0x2B, 0x9C, 0x8C, 0xF5,
    0x6E, 0xDF, 0x01, 0x95, 0x39, 0x34, 0x96, 0x27, 0xDB, 0x2F, 0xD5, 0x3D, 0x24, 0xB7, 0xC4, 0x86,
    0x65, 0x77, 0x2E, 0x43, 0x7D, 0x6C, 0x7F, 0x8C, 0xE4, 0x42, 0x73, 0x4A, 0xF7, 0xCC, 0xB7, 0xAE,
    0x83, 0x7C, 0x26, 0x4A, 0xE3, 0xA9, 0xBE, 0xB8, 0x7F, 0x8A, 0x2F, 0xE9, 0xB8, 0xB5, 0x29, 0x2E,
    0x5A, 0x02, 0x1F, 0xFF, 0x5E, 0x91, 0x47, 0x9E, 0x8C, 0xE7, 0xA2, 0x8C, 0x24, 0x42, 0xC6, 0xF3,
    0x15, 0x18, 0x0F, 0x93, 0x49, 0x9A, 0x23, 0x4D, 0xCF, 0x76, 0xE3, 0xFE, 0xD1, 0x35, 0xF9, 0xBB
};

/* test vector client random number */
static const uint8_t test_a[] = {
    0x60, 0x97, 0x55, 0x27, 0x03, 0x5C, 0xF2, 0xAD, 0x19, 0x89, 0x80, 0x6F, 0x04, 0x07, 0x21, 0x0B,
    0xC8, 0x1E, 0xDC, 0x04, 0xE2, 0x76, 0x2A, 0x56, 0xAF, 0xD5, 0x29, 0xDD, 0xDA, 0x2D, 0x43, 0x93
};

/* test vector server random number */
static const uint8_t test_b[] = {
    0xE4, 0x87, 0xCB, 0x59, 0xD3, 0x1A, 0xC5, 0x50, 0x47, 0x1E, 0x81, 0xF0, 0x0F, 0x69, 0x28, 0xE0,
    0x1D, 0xDA, 0x08, 0xE9, 0x74, 0xA0, 0x04, 0xF4, 0x9E, 0x61, 0xF5, 0xD1, 0x05, 0x28, 0x4D, 0x20,
};

/* test vector premaster secret */
static const uint8_t test_pms[] = {
    0xB0, 0xDC, 0x82, 0xBA, 0xBC, 0xF3, 0x06, 0x74, 0xAE, 0x45, 0x0C, 0x02, 0x87, 0x74, 0x5E, 0x79,
    0x90, 0xA3, 0x38, 0x1F, 0x63, 0xB3, 0x87, 0xAA, 0xF2, 0x71, 0xA1, 0x0D, 0x23, 0x38, 0x61, 0xE3,
    0x59, 0xB4, 0x82, 0x20, 0xF7, 0xC4, 0x69, 0x3C, 0x9A, 0xE1, 0x2B, 0x0A, 0x6F, 0x67, 0x80, 0x9F,
    0x08, 0x76, 0xE2, 0xD0, 0x13, 0x80, 0x0D, 0x6C, 0x41, 0xBB, 0x59, 0xB6, 0xD5, 0x97, 0x9B, 0x5C,
    0x00, 0xA1, 0x72, 0xB4, 0xA2, 0xA5, 0x90, 0x3A, 0x0B, 0xDC, 0xAF, 0x8A, 0x70, 0x95, 0x85, 0xEB,
    0x2A, 0xFA, 0xFA, 0x8F, 0x34, 0x99, 0xB2, 0x00, 0x21, 0x0D, 0xCC, 0x1F, 0x10, 0xEB, 0x33, 0x94,
    0x3C, 0xD6, 0x7F, 0xC8, 0x8A, 0x2F, 0x39, 0xA4, 0xBE, 0x5B, 0xEC, 0x4E, 0xC0, 0xA3, 0x21, 0x2D,
    0xC3, 0x46, 0xD7, 0xE4, 0x74, 0xB2, 0x9E, 0xDE, 0x8A, 0x46, 0x9F, 0xFE, 0xCA, 0x68, 0x6E, 0x5A
};

/* test vector user id */
static const char* test_I = "alice";

/* test vector password */
static const char* test_P = "password123";

/* test vector salt */
const uint8_t test_s[] = {
    0xBE, 0xB2, 0x53, 0x79, 0xD1, 0xA8, 0x58, 0x1E, 0xB5, 0xA7, 0x27, 0x67, 0x3A, 0x24, 0x41, 0xEE,
};


/* We only trust primes that we know */
static bool IsValidPrimeGroup(BIGNUM* N, BIGNUM* g)
{
    BIGNUM* tmp = BN_new();
    bool ok = true;
    size_t len = BN_num_bits(N);
    BN_ULONG group;

    switch (len) {
    case 1024:
        BN_bin2bn(Prime1024, sizeof(Prime1024), tmp);
        group = 2;
        break;

    case 1536:
        BN_bin2bn(Prime1536, sizeof(Prime1536), tmp);
        group = 2;
        break;

    default:
        ok = false;
    }
    if (ok) {
        ok = (BN_cmp(N, tmp) == 0) && BN_is_word(g, group);
    }
    BN_free(tmp);
    return ok;
}


namespace qcc {


static bool test = false;

static const int SALT_LEN = 40;


class Crypto_SRP::BN {

  public:
    BN_CTX* ctx;
    BIGNUM* a;
    BIGNUM* b;
    BIGNUM* g;
    BIGNUM* k;
    BIGNUM* s;
    BIGNUM* u;
    BIGNUM* v;
    BIGNUM* x;
    BIGNUM* A;
    BIGNUM* B;
    BIGNUM* N;
    BIGNUM* pms;

    BN() {
        ctx = BN_CTX_new();
        BN_CTX_start(ctx);
        a = BN_CTX_get(ctx);
        b = BN_CTX_get(ctx);
        g = BN_CTX_get(ctx);
        k = BN_CTX_get(ctx);
        s = BN_CTX_get(ctx);
        u = BN_CTX_get(ctx);
        v = BN_CTX_get(ctx);
        x = BN_CTX_get(ctx);
        A = BN_CTX_get(ctx);
        B = BN_CTX_get(ctx);
        N = BN_CTX_get(ctx);
        pms = BN_CTX_get(ctx);
    }

    ~BN() {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
};

static bool PAD(BIGNUM* v, uint8_t* buf, size_t len)
{
    size_t sz = BN_num_bytes(v);
    if (sz > len) {
        return false;
    } else {
        BN_bn2bin(v, buf + (len - sz));
        memset(buf, 0, (len - sz));
        return true;
    }
}

static qcc::String& operator+=(qcc::String& str, BIGNUM* bn)
{
    char* hex;
    hex = BN_bn2hex(bn);
    str += hex;
    OPENSSL_free(hex);
    return str;
}

/*
 * Parse 4 BIGNUMs from ":" seperated hex-encoded string.
 */
static QStatus Parse_Parameters(qcc::String str, BIGNUM* bn1, BIGNUM* bn2, BIGNUM* bn3, BIGNUM* bn4)
{
    BIGNUM* bn[4] = { bn1, bn2, bn3, bn4 };
    size_t i;

    for (i = 0; i < ArraySize(bn); ++i) {
        qcc::String val = HexStringToByteString(str);
        if (val.empty()) {
            return ER_BAD_STRING_ENCODING;
        }
        BN_bin2bn((const uint8_t*)val.data(), val.size(), bn[i]);
        size_t pos = str.find_first_of(':');
        if (pos == qcc::String::npos) {
            break;
        }
        str.erase(0, pos + 1);
    }
    if (i == 3) {
        return ER_OK;
    } else {
        return ER_BAD_STRING_ENCODING;
    }
}

Crypto_SRP::Crypto_SRP() : bn(new BN) {
}

Crypto_SRP::~Crypto_SRP()
{
    if (test) {
        printf("s = %s\n", BN_bn2hex(bn->s));
        printf("N = %s\n", BN_bn2hex(bn->N));
        printf("g = %s\n", BN_bn2hex(bn->g));
        printf("k = %s\n", BN_bn2hex(bn->k));
        printf("x = %s\n", BN_bn2hex(bn->x));
        printf("v = %s\n", BN_bn2hex(bn->v));
        printf("a = %s\n", BN_bn2hex(bn->a));
        printf("b = %s\n", BN_bn2hex(bn->b));
        printf("A = %s\n", BN_bn2hex(bn->A));
        printf("B = %s\n", BN_bn2hex(bn->B));
        printf("u = %s\n", BN_bn2hex(bn->u));
        printf("premaster secret = %s\n", BN_bn2hex(bn->pms));
    }
    delete bn;
}


/* fromServer string is N:g:s:B   toServer string is A */
QStatus Crypto_SRP::ClientInit(const qcc::String& fromServer, qcc::String& toServer)
{
    QStatus status = ER_OK;

    /* Parse N, g, s, and B from parameter string */
    status = Parse_Parameters(fromServer, bn->N, bn->g, bn->s, bn->B);
    if (status != ER_OK) {
        return status;
    }

    /*
     * Check that N and g are valid
     */
    if (!IsValidPrimeGroup(bn->N, bn->g)) {
        return ER_CRYPTO_INSUFFICIENT_SECURITY;
    }

    /*
     * Check that B is valid - B is computed %N so should be <= N and cannot be zero
     */
    if (BN_is_zero(bn->B) || (BN_cmp(bn->B, bn->N) >= 0)) {
        return ER_CRYPTO_ILLEGAL_PARAMETERS;
    }

    /* Generate client random number */
    if (test) {
        BN_bin2bn(test_a, sizeof(test_a), bn->a);
    } else {
        BN_rand(bn->a, 32, -1, 0);
    }

    /* Compute A = g^a % N */
    BN_mod_exp(bn->A, bn->g, bn->a, bn->N, bn->ctx);

    /* Compose string A to send to server */
    toServer.erase();
    toServer += bn->A;

    return ER_OK;
}

QStatus Crypto_SRP::ClientFinish(const qcc::String& id, const qcc::String& pwd)
{
    Crypto_SHA1 sha1;
    size_t lenN = BN_num_bytes(bn->N);
    uint8_t digest[Crypto_SHA1::DIGEST_SIZE];
    uint8_t* buf = new uint8_t[lenN];

    /* Compute u = SHA1(PAD(A) | PAD(B)); */
    sha1.Init();
    PAD(bn->A, buf, lenN);
    sha1.Update(buf, lenN);
    PAD(bn->B, buf, lenN);
    sha1.Update(buf, lenN);
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->u);

    /* Compute k = SHA1(N | PAD(g)) */
    sha1.Init();
    sha1.Update(bn->N);
    PAD(bn->g, buf, lenN);
    sha1.Update(buf, lenN);
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->k);

    /* Compute x = SHA1(s | (SHA1(I | ":" | P)) */
    sha1.Init();
    sha1.Update(id);
    sha1.Update(":");
    sha1.Update(pwd);
    sha1.GetDigest(digest);
    /* outer SHA1 */
    sha1.Init();
    sha1.Update(bn->s);
    sha1.Update(digest, sizeof(digest));
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->x);

    /* Calculate premaster secret for client = (B - (k * g^x)) ^ (a + (u * x)) % N  */

    /* (B - (k * g^x)) */
    BIGNUM* tmp1 = BN_new();
    BN_mod_exp(tmp1, bn->g, bn->x, bn->N, bn->ctx);
    BN_mul(tmp1, tmp1, bn->k, bn->ctx);
    BN_mod_sub(tmp1, bn->B, tmp1, bn->N, bn->ctx);

    /* (a + (u * x)) */
    BIGNUM* tmp2 = BN_new();
    BN_mul(tmp2, bn->u, bn->x, bn->ctx);
    BN_add(tmp2, tmp2, bn->a);

    BN_mod_exp(bn->pms, tmp1, tmp2, bn->N, bn->ctx);
    BN_free(tmp1);
    BN_free(tmp2);

    delete [] buf;

    return ER_OK;
}

/*
 * Called with N, g. s. and v initialized.
 */
void Crypto_SRP::ServerCommon(qcc::String& toClient)
{
    size_t lenN = BN_num_bytes(bn->N);
    Crypto_SHA1 sha1;
    uint8_t digest[Crypto_SHA1::DIGEST_SIZE];
    uint8_t* buf = new uint8_t[lenN];

    /* Generate server random number */
    if (test) {
        BN_bin2bn(test_b, sizeof(test_b), bn->b);
    } else {
        BN_rand(bn->b, 32, -1, 0);
    }

    /* Compute k = SHA1(N | PAD(g)) */
    sha1.Init();
    sha1.Update(bn->N);
    PAD(bn->g, buf, lenN);
    sha1.Update(buf, lenN);
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->k);

    /* Compute B = (k*v + g^b % N) %N */
    BIGNUM* tmp = BN_new();
    BN_mod_exp(bn->B, bn->g, bn->b, bn->N, bn->ctx);
    BN_mul(tmp, bn->k, bn->v, bn->ctx);
    BN_mod_add(bn->B, tmp, bn->B, bn->N, bn->ctx);
    BN_free(tmp);

    /* Compose string s:B to send to client */
    toClient.erase();
    toClient += bn->N;
    toClient += ":";
    toClient += bn->g;
    toClient += ":";
    toClient += bn->s;
    toClient += ":";
    toClient += bn->B;

    delete [] buf;
}

QStatus Crypto_SRP::ServerInit(const qcc::String& verifier, qcc::String& toClient)
{
    /* Parse N, g, s, and v from verifier string */
    QStatus status = Parse_Parameters(verifier, bn->N, bn->g, bn->s, bn->v);
    if (status != ER_OK) {
        return status;
    }
    ServerCommon(toClient);
    return ER_OK;
}

QStatus Crypto_SRP::ServerInit(const qcc::String& id, const qcc::String& pwd, qcc::String& toClient)
{
    Crypto_SHA1 sha1;
    uint8_t digest[Crypto_SHA1::DIGEST_SIZE];

    /* Prime and generator */
    BN_bin2bn(Prime1024, sizeof(Prime1024), bn->N);
    BN_set_word(bn->g, 2);

    /* Generate the salt */
    if (test) {
        BN_bin2bn(test_s, sizeof(test_s), bn->s);
    } else {
        BN_rand(bn->s, SALT_LEN, -1, 0);
    }

    /* Compute x = SHA1(s | (SHA1(I | ":" | P)) */
    sha1.Init();
    sha1.Update(id);
    sha1.Update(":");
    sha1.Update(pwd);
    sha1.GetDigest(digest);
    /* outer SHA1 */
    sha1.Init();
    sha1.Update(bn->s);
    sha1.Update(digest, sizeof(digest));
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->x);

    /* Compute v = g^x % N */
    BN_mod_exp(bn->v, bn->g, bn->x, bn->N, bn->ctx);

    ServerCommon(toClient);
    return ER_OK;

}

qcc::String Crypto_SRP::ServerGetVerifier()
{
    qcc::String verifier;

    verifier += bn->N;
    verifier += ":";
    verifier += bn->g;
    verifier += ":";
    verifier += bn->s;
    verifier += ":";
    verifier += bn->v;

    return verifier;
}

/* fromClient string is A */
QStatus Crypto_SRP::ServerFinish(const qcc::String fromClient)
{
    uint8_t digest[Crypto_SHA1::DIGEST_SIZE];
    qcc::String str = fromClient;
    Crypto_SHA1 sha1;
    size_t lenN = BN_num_bytes(bn->N);

    /* Parse out A */
    qcc::String val = HexStringToByteString(str);
    BN_bin2bn((const uint8_t*)val.data(), val.size(), bn->A);

    /*
     * Check that A is valid - A is computed %N so should be <= N and cannot be zero.
     */
    if (BN_is_zero(bn->A) || (BN_cmp(bn->A, bn->N) >= 0)) {
        return ER_CRYPTO_ILLEGAL_PARAMETERS;
    }

    uint8_t* buf = new uint8_t[lenN];

    /* Compute u = SHA1(PAD(A) | PAD(B)); */
    sha1.Init();
    PAD(bn->A, buf, lenN);
    sha1.Update(buf, lenN);
    PAD(bn->B, buf, lenN);
    sha1.Update(buf, lenN);
    sha1.GetDigest(digest);
    BN_bin2bn(digest, Crypto_SHA1::DIGEST_SIZE, bn->u);

    /* Calculate premaster secret for server = ((A * v^u) ^ b %N) */

    /* tmp = (A * v^u) */
    BIGNUM* tmp = BN_new();
    BN_mod_exp(tmp, bn->v, bn->u, bn->N, bn->ctx);
    BN_mod_mul(tmp, tmp, bn->A, bn->N, bn->ctx);
    /* pms = tmp ^ b % N  */
    BN_mod_exp(bn->pms, tmp, bn->b, bn->N, bn->ctx);
    BN_free(tmp);

    delete [] buf;

    return ER_OK;
}

void Crypto_SRP::GetPremasterSecret(KeyBlob& premaster)
{
    size_t sz = BN_num_bytes(bn->pms);
    uint8_t* pms = new uint8_t[sz];
    BN_bn2bin(bn->pms, pms);
    premaster.Set(pms, sz, KeyBlob::GENERIC);
    delete [] pms;
}

QStatus Crypto_SRP::TestVector()
{

    QStatus status;
    Crypto_SRP* server = new Crypto_SRP;
    Crypto_SRP* client = new Crypto_SRP;
    qcc::String toClient;
    qcc::String toServer;

    test = true;
    status = server->ServerInit(test_I, test_P, toClient);
    if (status != ER_OK) {
        QCC_LogError(status, ("SRP ServerInit failed"));
        goto TestFail;
    }

    status = client->ClientInit(toClient, toServer);
    if (status != ER_OK) {
        QCC_LogError(status, ("SRP ClientInit failed"));
        goto TestFail;
    }

    status = server->ServerFinish(toServer);
    if (status != ER_OK) {
        QCC_LogError(status, ("SRP ServerFinish failed"));
        goto TestFail;
    }
    status = client->ClientFinish(test_I, test_P);
    if (status != ER_OK) {
        QCC_LogError(status, ("SRP ClientFinish failed"));
        goto TestFail;
    }
    BN_bin2bn(test_pms, sizeof(test_pms), bn->pms);
    if ((BN_cmp(bn->pms, client->bn->pms) != 0)) {
        status = ER_FAIL;
        QCC_LogError(status, ("SRP client premaster secret is incorrect"));
        goto TestFail;
    }
    if ((BN_cmp(bn->pms, server->bn->pms) != 0)) {
        status = ER_FAIL;
        QCC_LogError(status, ("SRP server premaster secret is incorrect"));
        goto TestFail;
    }

    delete client;
    delete server;
    test = false;
    return ER_OK;

TestFail:

    test = false;
    delete client;
    delete server;
    return ER_FAIL;
}

}
