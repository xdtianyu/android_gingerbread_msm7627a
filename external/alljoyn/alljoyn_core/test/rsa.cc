/**
 * @file
 *
 * This file has units tests for the RSA cryto APIs
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

#include <qcc/Crypto.h>
#include <qcc/Debug.h>
#include <qcc/KeyBlob.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>
#include <qcc/Debug.h>

#include <alljoyn/version.h>

#include <Status.h>

#define QCC_MODULE "CRYPTO"

using namespace qcc;
using namespace std;

const char hw[] = "hello world";

int main(int argc, char** argv)
{
    QStatus status;
    qcc::String priStr;
    qcc::String pubStr;

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    Crypto_RSA pk;

    pk.Generate();

    pk.PublicToPEM(pubStr);
    printf("Public key:\n%s\n", pubStr.c_str());
    pk.PrivateToPEM(priStr, "pa55pHr@8e");
    printf("Private key:\n%s\n", priStr.c_str());

    size_t pkSize = pk.GetSize();

    size_t inLen;
    uint8_t* in = new uint8_t[pkSize];
    size_t outLen;
    uint8_t* out = new uint8_t[pkSize];

    /*
     * Test encryption with private key and decryption with public key
     */
    printf("Testing encryption/decryption\n");

    memcpy(in, hw, sizeof(hw));
    inLen = sizeof(hw);
    outLen = pkSize;
    status = pk.PrivateEncrypt(in, inLen, out, outLen);
    if (status == ER_OK) {
        printf("Encrypted size %u\n", static_cast<uint32_t>(outLen));
    } else {
        printf("PrivateEncrypt failed %s\n", QCC_StatusText(status));
        goto FailExit;
    }
    inLen = outLen;
    outLen = pkSize;
    memcpy(in, out, pkSize);
    status = pk.PublicDecrypt(in, inLen, out, outLen);
    if (status == ER_OK) {
        printf("Decrypted size %u\n", static_cast<uint32_t>(outLen));
        printf("Decrypted %s\n", out);
    } else {
        printf("PublicDecrypt failed %s\n", QCC_StatusText(status));
        goto FailExit;
    }

    {
        printf("Testing cert generation\n");
        /*
         * Test certificate generation
         */
        status = pk.MakeSelfCertificate("my name", "my app");
        printf("Cert:\n%s", pk.CertToString().c_str());
        status = pk.PrivateToPEM(priStr, "password1234");
        if (status != ER_OK) {
            printf("PrivateToPEM failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
        status = pk.PublicToPEM(pubStr);
        if (status != ER_OK) {
            printf("PublicToPEM failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }

        /*
         * Initialize separate private and public key instances and test encryption a decryption
         */
        Crypto_RSA pub(pubStr);
        Crypto_RSA pri(priStr, "password1234");

        memcpy(in, hw, sizeof(hw));

        inLen = sizeof(hw);
        outLen = pkSize;
        status = pub.PublicEncrypt(in, inLen, out, outLen);
        if (status == ER_OK) {
            printf("Encrypted size %u\n", static_cast<uint32_t>(outLen));
        } else {
            printf("PublicEncrypt failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }

        inLen = outLen;
        outLen = pkSize;
        memcpy(in, out, pkSize);
        status = pri.PrivateDecrypt(in, inLen, out, outLen);
        if (status == ER_OK) {
            printf("Decrypted size %u\n", static_cast<uint32_t>(outLen));
            printf("Decrypted %s\n", out);
        } else {
            printf("PrivateDecrypt failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
    }

    {
        printf("Testing empty passphrase\n");
        /*
         * Check we allow an empty password
         */
        status = pk.PrivateToPEM(priStr, "");
        if (status != ER_OK) {
            printf("PrivateToPEM failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
        status = pk.PublicToPEM(pubStr);
        if (status != ER_OK) {
            printf("PublicToPEM failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
        Crypto_RSA pub;
        status = pub.FromPEM(pubStr);
        if (status != ER_OK) {
            printf("FromPEM failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
        Crypto_RSA pri;
        status = pri.FromPKCS8(priStr, "");
        if (status != ER_OK) {
            printf("FromPKCS8 failed %s\n%s\n", QCC_StatusText(status), priStr.c_str());
            goto FailExit;
        }

        const char doc[] = "This document requires a signature";
        uint8_t signature[64];
        size_t sigLen = sizeof(signature);

        /*
         * Test Sign and Verify APIs
         */
        status = pri.Sign((const uint8_t*)doc, sizeof(doc), signature, sigLen);
        if (status != ER_OK) {
            printf("Sign failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
        status = pub.Verify((const uint8_t*)doc, sizeof(doc), signature, sigLen);
        if (status == ER_OK) {
            printf("Digital signature was verified\n");
        } else {
            printf("Verify failed %s\n", QCC_StatusText(status));
            goto FailExit;
        }
    }

    delete [] out;
    delete [] in;

    printf("!!!PASSED\n");
    return 0;

FailExit:
    delete [] out;
    delete [] in;

    printf("!!!FAILED\n");
    return -1;

}
