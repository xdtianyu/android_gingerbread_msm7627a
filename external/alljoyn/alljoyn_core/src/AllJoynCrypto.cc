/**
 * @file
 * Class for encapsulating message encryption and decryption operations.
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

#include <string.h>

#include <qcc/Debug.h>
#include <qcc/Crypto.h>
#include <qcc/KeyBlob.h>
#include <qcc/Util.h>

#include <Status.h>

#include "AllJoynCrypto.h"

#define QCC_MODULE "ALLJOYN_AUTH"

using namespace std;
using namespace qcc;

namespace ajn {

const size_t Crypto::ExpansionBytes = 8;

const size_t Crypto::NonceBytes = 16;


QStatus Crypto::Encrypt(const KeyBlob& keyBlob, uint8_t* msg, size_t hdrLen, size_t& bodyLen, const KeyBlob& nonce)
{
    size_t msgLen = hdrLen + bodyLen;
    QStatus status;
    if (!msg) {
        return ER_BAD_ARG_2;
    }
    switch (keyBlob.GetType()) {
    case KeyBlob::AES:
    {
        QCC_DbgHLPrintf(("Encrypt key:   %s", BytesToHexString(keyBlob.GetData(), keyBlob.GetSize()).c_str()));
        QCC_DbgHLPrintf(("        nonce: %s", BytesToHexString(nonce.GetData(), nonce.GetSize()).c_str()));
        Crypto_AES aes(keyBlob, Crypto_AES::ENCRYPT);
        status = aes.Encrypt_CCM(msg, msgLen, hdrLen, nonce, ExpansionBytes);
    }
    break;

    default:
        status = ER_BUS_KEYBLOB_OP_INVALID;
        QCC_LogError(status, ("Key type %d not supported for message encryption", keyBlob.GetType()));
        break;
    }
    if (status == ER_OK) {
        bodyLen = (uint32_t)(msgLen - hdrLen);
    }
    return status;
}

QStatus Crypto::Decrypt(const KeyBlob& keyBlob, uint8_t* msg, size_t hdrLen, size_t& bodyLen, const KeyBlob& nonce)
{
    size_t msgLen = hdrLen + bodyLen;
    QStatus status;
    if (!msg) {
        return ER_BAD_ARG_2;
    }
    switch (keyBlob.GetType()) {
    case KeyBlob::AES:
    {
        QCC_DbgHLPrintf(("Decrypt key:   %s", BytesToHexString(keyBlob.GetData(), keyBlob.GetSize()).c_str()));
        QCC_DbgHLPrintf(("        nonce: %s", BytesToHexString(nonce.GetData(), nonce.GetSize()).c_str()));
        Crypto_AES aes(keyBlob, Crypto_AES::ENCRYPT);
        status = aes.Decrypt_CCM(msg, msgLen, hdrLen, nonce, ExpansionBytes);
    }
    break;

    default:
        status = ER_BUS_KEYBLOB_OP_INVALID;
        QCC_LogError(status, ("Key type %d not supported for message decryption", keyBlob.GetType()));
        break;
    }
    if (status == ER_OK) {
        bodyLen = (uint32_t)(msgLen - hdrLen);
    } else {
        status = ER_BUS_MESSAGE_DECRYPTION_FAILED;
    }
    return status;
}

QStatus Crypto::HashHeaderFields(const HeaderFields& hdrFields, qcc::KeyBlob& keyBlob)
{
    Crypto_SHA1 sha1;
    uint8_t digest[Crypto_SHA1::DIGEST_SIZE];

    sha1.Init();

    for (uint32_t fieldId = ALLJOYN_HDR_FIELD_PATH; fieldId < ArraySize(hdrFields.field); fieldId++) {
        if (!HeaderFields::Compressible[fieldId]) {
            continue;
        }
        const MsgArg* field = &hdrFields.field[fieldId];
        uint8_t buf[8];
        size_t pos = 0;
        buf[pos++] = (uint8_t)fieldId;
        buf[pos++] = (uint8_t)field->typeId;
        switch (field->typeId) {
        case ALLJOYN_SIGNATURE:
            sha1.Update((const uint8_t*)field->v_signature.sig, field->v_signature.len);
            break;

        case ALLJOYN_OBJECT_PATH:
        case ALLJOYN_STRING:
            sha1.Update((const uint8_t*)field->v_string.str, field->v_string.len);
            break;

        case ALLJOYN_UINT32:
            /* Write integer as little endian */
            buf[pos++] = (uint8_t)(field->v_uint32 >> 0);
            buf[pos++] = (uint8_t)(field->v_uint32 >> 8);
            buf[pos++] = (uint8_t)(field->v_uint32 >> 16);
            buf[pos++] = (uint8_t)(field->v_uint32 >> 24);
            break;

        default:
            break;
        }
        sha1.Update(buf, pos);
    }

    sha1.GetDigest(digest);
    keyBlob.Set(digest, sizeof(digest), KeyBlob::GENERIC);
    return ER_OK;
}

}
