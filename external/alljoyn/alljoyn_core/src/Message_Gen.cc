/**
 * @file
 *
 * This file implements the message generation side of the message bus message class
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

#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Debug.h>
#include <qcc/Socket.h>
#include <qcc/time.h>
#include <qcc/Util.h>

#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/Message.h>
#include <alljoyn/MsgArg.h>

#include "LocalTransport.h"
#include "PeerState.h"
#include "KeyStore.h"
#include "CompressionRules.h"
#include "BusUtil.h"
#include "AllJoynCrypto.h"
#include "AllJoynPeerObj.h"
#include "SignatureUtils.h"
#include "BusInternal.h"

#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {


#define Marshal8(n) \
    do { \
        *((uint64_t*)bufPos) = n; \
        bufPos += 8; \
    } while (0)

#define Marshal4(n) \
    do { \
        *((uint32_t*)bufPos) = n; \
        bufPos += 4; \
    } while (0)

#define Marshal2(n) \
    do { \
        *((uint16_t*)bufPos) = n; \
        bufPos += 2; \
    } while (0)

#define Marshal1(n) \
    do { \
        *bufPos++ = n; \
    } while (0)

#define MarshalBytes(data, len) \
    do { \
        memcpy(bufPos, data, len); \
        bufPos += len; \
    } while (0)

#define MarshalReversed(data, len) \
    do { \
        uint8_t* p = ((uint8_t*)data) + len; \
        while (p-- != (uint8_t*)data) { \
            *bufPos++ = *p; \
        } \
    } while (0)

#define MarshalPad2() \
    do { \
        size_t pad = PadBytes(bufPos, 2); \
        if (pad) { Marshal1(0); } \
    } while (0)

#define MarshalPad4() \
    do { \
        size_t pad = PadBytes(bufPos, 4); \
        if (pad & 1) { Marshal1(0); } \
        if (pad & 2) { Marshal2(0); } \
    } while (0)

#define MarshalPad8() \
    do { \
        size_t pad = PadBytes(bufPos, 8); \
        if (pad & 1) { Marshal1(0); } \
        if (pad & 2) { Marshal2(0); } \
        if (pad & 4) { Marshal4(0); } \
    } while (0)

/*
 * Round up to a multiple of 8
 */
#define ROUNDUP8(n)  (((n) + 7) & ~7)

/*
 * Round up to a multiple of 8
 */
#define ROUNDUP4(n)  (((n) + 3) & ~3)

static inline QStatus CheckedArraySize(size_t sz, uint32_t& len)
{
    if (sz > ALLJOYN_MAX_ARRAY_LEN) {
        QStatus status = ER_BUS_BAD_LENGTH;
        QCC_LogError(status, ("Array too big"));
        return status;
    } else {
        len = (uint32_t)sz;
        return ER_OK;
    }
}

QStatus _Message::MarshalArgs(const MsgArg* arg, size_t numArgs)
{
    QStatus status = ER_OK;
    size_t alignment;
    uint32_t len;

    while (numArgs--) {
        if (!arg) {
            status = ER_BUS_BAD_VALUE;
            break;
        }
        // QCC_DbgPrintf(("MarshalArgs @%ld %s", bufPos - bodyPtr, arg->ToString().c_str()));
        switch (arg->typeId) {
        case ALLJOYN_DICT_ENTRY:
            MarshalPad8();
            status = MarshalArgs(arg->v_dictEntry.key, 1);
            if (status == ER_OK) {
                status = MarshalArgs(arg->v_dictEntry.val, 1);
            }
            break;

        case ALLJOYN_STRUCT:
            MarshalPad8();
            status = MarshalArgs(arg->v_struct.members, arg->v_struct.numMembers);
            break;

        case ALLJOYN_ARRAY:
            if (!arg->v_array.elemSig) {
                status = ER_BUS_BAD_VALUE;
                break;
            }
            MarshalPad4();
            alignment = SignatureUtils::AlignmentForType((AllJoynTypeId)(arg->v_array.elemSig[0]));
            if (arg->v_array.numElements > 0) {
                if (!arg->v_array.elements) {
                    status = ER_BUS_BAD_VALUE;
                    break;
                }
                /*
                 * Check elements conform to the expected signature type
                 */
                for (size_t i = 0; i < arg->v_array.numElements; i++) {
                    if (!arg->v_array.elements[i].HasSignature(arg->v_array.GetElemSig())) {
                        status = ER_BUS_BAD_VALUE;
                        QCC_LogError(status, ("Array element[%d] does not have expected signature \"%s\"", i, arg->v_array.GetElemSig()));
                        break;
                    }
                }
                if (status == ER_OK) {
                    uint8_t* lenPos = bufPos;
                    bufPos += 4;
                    /* Length does not include padding for first element, so pad to 8 byte boundary if required. */
                    if (alignment == 8) {
                        MarshalPad8();
                    }
                    uint8_t* elemPos = bufPos;
                    status = MarshalArgs(arg->v_array.elements, arg->v_array.numElements);
                    if (status != ER_OK) {
                        break;
                    }
                    status = CheckedArraySize(bufPos - elemPos, len);
                    if (status != ER_OK) {
                        break;
                    }
                    /* Patch in length */
                    uint8_t* tmpPos = bufPos;
                    bufPos = lenPos;
                    if (endianSwap) {
                        MarshalReversed(&len, 4);
                    } else {
                        Marshal4(len);
                    }
                    bufPos = tmpPos;
                }
            } else {
                Marshal4(0);
                if (alignment == 8) {
                    MarshalPad8();
                }
            }
            break;

        case ALLJOYN_BOOLEAN_ARRAY:
            status = CheckedArraySize(4 * arg->v_scalarArray.numElements, len);
            if (status != ER_OK) {
                break;
            }
            if (len && !arg->v_scalarArray.v_bool) {
                status = ER_BUS_BAD_VALUE;
                break;
            }
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&len, 4);
            } else {
                Marshal4(len);
            }
            for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                uint32_t b = arg->v_scalarArray.v_bool[i] ? 1 : 0;
                if (endianSwap) {
                    MarshalReversed(b, 4);
                } else {
                    Marshal4(b);
                }
            }
            break;

        case ALLJOYN_INT32_ARRAY:
        case ALLJOYN_UINT32_ARRAY:
            status = CheckedArraySize(4 * arg->v_scalarArray.numElements, len);
            if (status != ER_OK) {
                break;
            }
            if (len && !arg->v_scalarArray.v_uint32) {
                status = ER_BUS_BAD_VALUE;
                break;
            }
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&len, 4);
                for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                    MarshalReversed(arg->v_scalarArray.v_uint32[i], 4);
                }
            } else {
                Marshal4(len);
                MarshalBytes(arg->v_scalarArray.v_uint32, len);
            }
            break;

        case ALLJOYN_DOUBLE_ARRAY:
        case ALLJOYN_UINT64_ARRAY:
        case ALLJOYN_INT64_ARRAY:
            MarshalPad4();
            status = CheckedArraySize(8 * arg->v_scalarArray.numElements, len);
            if (status != ER_OK) {
                break;
            }
            if (len > 0) {
                if (!arg->v_scalarArray.v_uint64) {
                    status = ER_BUS_BAD_VALUE;
                    break;
                }
                if (endianSwap) {
                    MarshalReversed(&len, 4);
                    MarshalPad8();
                    for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                        MarshalReversed(arg->v_scalarArray.v_uint64[i], 8);
                    }
                } else {
                    Marshal4(len);
                    MarshalPad8();
                    MarshalBytes(arg->v_scalarArray.v_uint64, len);
                }
            } else {
                /* Even empty arrays are padded to the element type alignment boundary */
                Marshal4(0);
                MarshalPad8();
            }
            break;

        case ALLJOYN_INT16_ARRAY:
        case ALLJOYN_UINT16_ARRAY:
            status = CheckedArraySize(2 * arg->v_scalarArray.numElements, len);
            if (status != ER_OK) {
                break;
            }
            if (len && !arg->v_scalarArray.v_uint16) {
                status = ER_BUS_BAD_VALUE;
                break;
            }
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&len, 4);
                for (size_t i = 0; i < arg->v_scalarArray.numElements; i++) {
                    MarshalReversed(arg->v_scalarArray.v_uint16[i], 2);
                }
            } else {
                Marshal4(len);
                MarshalBytes(arg->v_scalarArray.v_uint16, len);
            }
            break;

        case ALLJOYN_BYTE_ARRAY:
            status = CheckedArraySize(arg->v_scalarArray.numElements, len);
            if (status != ER_OK) {
                break;
            }
            if (len && !arg->v_scalarArray.v_byte) {
                status = ER_BUS_BAD_VALUE;
                break;
            }
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&len, 4);
            } else {
                Marshal4(len);
            }
            MarshalBytes(arg->v_scalarArray.v_byte, arg->v_scalarArray.numElements);
            break;

        case ALLJOYN_BOOLEAN:
            MarshalPad4();
            if (arg->v_bool) {
                if (endianSwap) {
                    uint32_t b = 1;
                    MarshalReversed(&b, 4);
                } else {
                    Marshal4(1);
                }
            } else {
                Marshal4(0);
            }
            break;

        case ALLJOYN_INT32:
        case ALLJOYN_UINT32:
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&arg->v_uint32, 4);
            } else {
                Marshal4(arg->v_uint32);
            }
            break;

        case ALLJOYN_DOUBLE:
        case ALLJOYN_UINT64:
        case ALLJOYN_INT64:
            MarshalPad8();
            if (endianSwap) {
                MarshalReversed(&arg->v_uint64, 8);
            } else {
                Marshal8(arg->v_uint64);
            }
            break;

        case ALLJOYN_SIGNATURE:
            if (arg->v_signature.sig) {
                if (arg->v_signature.sig[arg->v_signature.len]) {
                    status = ER_BUS_NOT_NUL_TERMINATED;
                    break;
                }
                Marshal1(arg->v_signature.len);
                MarshalBytes((void*)arg->v_signature.sig, arg->v_signature.len + 1);
            } else {
                Marshal1(0);
                Marshal1(0);
            }
            break;

        case ALLJOYN_INT16:
        case ALLJOYN_UINT16:
            MarshalPad2();
            if (endianSwap) {
                MarshalReversed(&arg->v_uint16, 2);
            } else {
                Marshal2(arg->v_uint16);
            }
            break;

        case ALLJOYN_OBJECT_PATH:
            if (!arg->v_string.str || (arg->v_string.len == 0)) {
                status = ER_BUS_BAD_OBJ_PATH;
                break;
            }

        // FALLTHROUGH
        case ALLJOYN_STRING:
            MarshalPad4();
            if (arg->v_string.str) {
                if (arg->v_string.str[arg->v_string.len]) {
                    status = ER_BUS_NOT_NUL_TERMINATED;
                    break;
                }
                if (endianSwap) {
                    MarshalReversed(&arg->v_string.len, 4);
                } else {
                    Marshal4(arg->v_string.len);
                }
                MarshalBytes((void*)arg->v_string.str, arg->v_string.len + 1);
            } else {
                Marshal4(0);
                Marshal1(0);
            }
            break;

        case ALLJOYN_VARIANT:
        {
            /* First byte is reserved for the length */
            char sig[257];
            size_t len = 0;
            status = SignatureUtils::MakeSignature(arg->v_variant.val, 1, sig + 1, len);
            if (status == ER_OK) {
                sig[0] = (char)len;
                MarshalBytes(sig, len + 2);
                status = MarshalArgs(arg->v_variant.val, 1);
            }
        }
        break;

        case ALLJOYN_BYTE:
            Marshal1(arg->v_byte);
            break;

        case ALLJOYN_HANDLE:
        {
            uint32_t index = 0;
            /* Check if handle is already listed */
            while ((index < numHandles) && (handles[index] != arg->v_handle.fd)) {
                ++index;
            }
            /* If handle was not found expand handle array */
            if (index == numHandles) {
                qcc::SocketFd* h = new qcc::SocketFd[numHandles + 1];
                memcpy(h, handles, numHandles * sizeof(qcc::SocketFd));
                delete [] handles;
                handles = h;
                status = qcc::SocketDup(arg->v_handle.fd, handles[numHandles++]);
                if (status != ER_OK) {
                    --numHandles;
                    break;
                }
            }
            /* Marshal the index of the handle */
            MarshalPad4();
            if (endianSwap) {
                MarshalReversed(&index, 4);
            } else {
                Marshal4(index);
            }
        }
        break;

        default:
            status = ER_BUS_BAD_VALUE_TYPE;
            break;
        }
        if (status != ER_OK) {
            break;
        }
        ++arg;
    }
    return status;
}

QStatus _Message::Deliver(RemoteEndpoint& endpoint)
{
    QStatus status = ER_OK;
    Sink& sink = endpoint.GetSink();
    uint8_t* buf = reinterpret_cast<uint8_t*>(msgBuf);
    size_t len = bufEOD - buf;
    size_t pushed;

    if (len == 0) {
        status = ER_BUS_EMPTY_MESSAGE;
        QCC_LogError(status, ("Message is empty"));
        return status;
    }
    /*
     * Handles can only be passed if that feature was negotiated.
     */
    if (handles && !endpoint.GetFeatures().handlePassing) {
        status = ER_BUS_HANDLES_NOT_ENABLED;
        QCC_LogError(status, ("Handle passing was not negotiated on this connection"));
        return status;
    }
    /*
     * If the mssage has a TTL check if it has expired
     */
    if (ttl && IsExpired()) {
        QCC_DbgHLPrintf(("TTL has expired - discarding message %s", Description().c_str()));
        return ER_OK;
    }
    /*
     * Push the message to the endpoint sink (only push handles in the first chunk)
     */
    if (handles) {
        status = sink.PushBytesAndFds(buf, len, pushed, handles, numHandles, endpoint.GetProcessId());
    } else {
        status = sink.PushBytes(buf, len, pushed);
    }
    /*
     * Continue pushing until we are done
     */
    while ((status == ER_OK) && (pushed != len)) {
        len -= pushed;
        buf += pushed;
        status = sink.PushBytes(buf, len, pushed);
    }
    if (status == ER_OK) {
        QCC_DbgHLPrintf(("Deliver message %s", Description().c_str()));
        QCC_DbgPrintf(("%s", ToString().c_str()));
    } else {
        QCC_LogError(status, ("Failed to deliver message %s", Description().c_str()));
    }
    return status;
}


/*
 * Map from our enumeration type to the wire protocol values
 */
static const uint8_t FieldTypeMapping[] = {
    0,  /* ALLJOYN_HDR_FIELD_INVALID           */
    1,  /* ALLJOYN_HDR_FIELD_PATH              */
    2,  /* ALLJOYN_HDR_FIELD_INTERFACE         */
    3,  /* ALLJOYN_HDR_FIELD_MEMBER            */
    4,  /* ALLJOYN_HDR_FIELD_ERROR_NAME        */
    5,  /* ALLJOYN_HDR_FIELD_REPLY_SERIAL      */
    6,  /* ALLJOYN_HDR_FIELD_DESTINATION       */
    7,  /* ALLJOYN_HDR_FIELD_SENDER            */
    8,  /* ALLJOYN_HDR_FIELD_SIGNATURE         */
    9,  /* ALLJOYN_HDR_FIELD_HANDLES           */
    16, /* ALLJOYN_HDR_FIELD_TIMESTAMP         */
    17, /* ALLJOYN_HDR_FIELD_TIME_TO_LIVE      */
    18, /* ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN */
    19  /* ALLJOYN_HDR_FIELD_SESSION_ID        */
};

/*
 * After the header fields are marshaled all of the strings in the MsgArgs point into the buffer.
 */
void _Message::MarshalHeaderFields()
{
    /*
     * Marshal the header fields
     */
    for (uint32_t fieldId = ALLJOYN_HDR_FIELD_PATH; fieldId < ArraySize(hdrFields.field); fieldId++) {
        MsgArg* field = &hdrFields.field[fieldId];
        if (field->typeId != ALLJOYN_INVALID) {
            if ((msgHeader.flags & ALLJOYN_FLAG_COMPRESSED) && HeaderFields::Compressible[fieldId]) {
                /*
                 * Stabilize the field to ensure that any strings etc, are copied into the message.
                 */
                field->Stabilize();
                continue;
            }
            /*
             * Header fields align on an 8 byte boundary
             */
            MarshalPad8();
            Marshal1(FieldTypeMapping[fieldId]);
            /*
             * We relocate the string pointers in the fields to point to the marshaled versions to
             * so the lifetime of the message is not bound to the lifetime of values passed in.
             */
            const char* str;
            switch (field->typeId) {
            case ALLJOYN_SIGNATURE:
                Marshal1(1);
                Marshal1((uint8_t)ALLJOYN_SIGNATURE);
                Marshal1(0);
                Marshal1(field->v_signature.len);
                str = field->v_signature.sig;
                field->v_signature.sig = (char*)bufPos;
                MarshalBytes((void*)str, field->v_signature.len + 1);
                break;

            case ALLJOYN_UINT32:
                Marshal1(1);
                Marshal1((uint8_t)ALLJOYN_UINT32);
                Marshal1(0);
                if (endianSwap) {
                    MarshalReversed(&field->v_uint32, 4);
                } else {
                    Marshal4(field->v_uint32);
                }
                break;

            case ALLJOYN_OBJECT_PATH:
            case ALLJOYN_STRING:
                Marshal1(1);
                Marshal1((uint8_t)field->typeId);
                Marshal1(0);
                if (endianSwap) {
                    MarshalReversed(&field->v_string.len, 4);
                } else {
                    Marshal4(field->v_string.len);
                }
                str = field->v_string.str;
                field->v_string.str = (char*)bufPos;
                MarshalBytes((void*)str, field->v_string.len + 1);
                break;

            default:
                /*
                 * Use standard variant marshaling for the other cases.
                 */
            {
                MsgArg variant(ALLJOYN_VARIANT);
                variant.v_variant.val = field;
                MarshalArgs(&variant, 1);
                variant.v_variant.val = NULL;
            }
            break;
            }
        }
    }
    /*
     * Header must be zero-padded to end on an 8 byte boundary
     */
    MarshalPad8();
}


/*
 * Calculate space required for the header fields
 */
size_t _Message::ComputeHeaderLen()
{
    size_t hdrLen = 0;
    for (uint32_t fieldId = ALLJOYN_HDR_FIELD_PATH; fieldId < ArraySize(hdrFields.field); fieldId++) {
        if ((msgHeader.flags & ALLJOYN_FLAG_COMPRESSED) && HeaderFields::Compressible[fieldId]) {
            continue;
        }
        MsgArg* field = &hdrFields.field[fieldId];
        if (field->typeId != ALLJOYN_INVALID) {
            hdrLen = ROUNDUP8(hdrLen) + SignatureUtils::GetSize(field, 1, 4);
        }
    }
    msgHeader.headerLen = static_cast<uint32_t>(hdrLen);
    return ROUNDUP8(sizeof(msgHeader) + hdrLen);
}


QStatus _Message::MarshalMessage(const qcc::String& expectedSignature,
                                 const qcc::String& destination,
                                 AllJoynMessageType msgType,
                                 const MsgArg* args,
                                 uint8_t numArgs,
                                 uint8_t flags,
                                 uint32_t sessionId)
{
    char signature[256];
    QStatus status = ER_OK;
    size_t argsLen = (numArgs == 0) ? 0 : SignatureUtils::GetSize(args, numArgs);
    size_t hdrLen = 0;

    if (!bus.IsStarted()) {
        return ER_BUS_BUS_NOT_STARTED;
    }
    /*
     * Toggle the autostart flag bit which is a 0 over the air but we prefer as a 1.
     */
    flags ^= ALLJOYN_FLAG_AUTO_START;
    /*
     * We marshal new messages in native endianess
     */
    endianSwap = false;
    msgHeader.endian = this->myEndian;
    msgHeader.msgType = (uint8_t)msgType;
    msgHeader.flags = flags;
    msgHeader.majorVersion = ALLJOYN_MAJOR_PROTOCOL_VERSION;
    msgHeader.serialNum = bus.GetInternal().NextSerial();
    /*
     * Encryption will typically make the body length slightly larger because the encryption
     * algorithm adds appends a MAC block to the end of the encrypted data.
     */
    if (flags & ALLJOYN_FLAG_ENCRYPTED) {
        QCC_DbgHLPrintf(("Encrypting messge to %s", destination.empty() ? "broadcast listeners" : destination.c_str()));
        msgHeader.bodyLen = static_cast<uint32_t>(argsLen + ajn::Crypto::ExpansionBytes);
    } else {
        msgHeader.bodyLen = static_cast<uint32_t>(argsLen);
    }
    /*
     * Keep the old message buffer around until we are done because some of the strings we are
     * marshaling may point into the old message.
     */
    uint64_t* oldMsgBuf = msgBuf;
    /*
     * Clear out stale message data
     */
    bodyPtr = NULL;
    bufPos = NULL;
    bufEOD = NULL;
    msgBuf = NULL;
    /*
     * There should be a mapping for every field type
     */
    assert(ArraySize(FieldTypeMapping) == ArraySize(hdrFields.field));
    /*
     * Add the destination if there is one.
     */
    if (!destination.empty()) {
        hdrFields.field[ALLJOYN_HDR_FIELD_DESTINATION].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_DESTINATION].v_string.str = destination.c_str();
        hdrFields.field[ALLJOYN_HDR_FIELD_DESTINATION].v_string.len = destination.size();
    }
    /*
     * Sender is obtained from the bus
     */
    const qcc::String& sender = bus.GetInternal().GetLocalEndpoint().GetUniqueName();
    if (!sender.empty()) {
        hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].v_string.str = sender.c_str();
        hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].v_string.len = sender.size();
    }
    /*
     * If there are arguments build the signature
     */
    if (numArgs > 0) {
        size_t sigLen = 0;
        status = SignatureUtils::MakeSignature(args, numArgs, signature, sigLen);
        if (status != ER_OK) {
            goto ExitMarshalMessage;
        }
        if (sigLen > 0) {
            hdrFields.field[ALLJOYN_HDR_FIELD_SIGNATURE].typeId = ALLJOYN_SIGNATURE;
            hdrFields.field[ALLJOYN_HDR_FIELD_SIGNATURE].v_signature.sig = signature;
            hdrFields.field[ALLJOYN_HDR_FIELD_SIGNATURE].v_signature.len = (uint8_t)sigLen;
        }
    } else {
        signature[0] = 0;
        hdrFields.field[ALLJOYN_HDR_FIELD_SIGNATURE].typeId = ALLJOYN_INVALID;
    }
    /*
     * Check the signature computed from the args matches the expected signature.
     */
    if (expectedSignature != signature) {
        status = ER_BUS_UNEXPECTED_SIGNATURE;
        QCC_LogError(status, ("MarshalMessage expected signature \"%s\" got \"%s\"", expectedSignature.c_str(), signature));
        goto ExitMarshalMessage;
    }
    /* Check if we are adding a session id */
    if (sessionId != 0) {
        hdrFields.field[ALLJOYN_HDR_FIELD_SESSION_ID].v_uint32 = sessionId;
        hdrFields.field[ALLJOYN_HDR_FIELD_SESSION_ID].typeId = ALLJOYN_UINT32;
    } else {
        hdrFields.field[ALLJOYN_HDR_FIELD_SESSION_ID].typeId = ALLJOYN_INVALID;
    }
    /*
     * Check if we are to do header compression. We must do this last after all the other fields
     * have been initialized.
     */
    if ((msgHeader.flags & ALLJOYN_FLAG_COMPRESSED)) {
        hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].v_uint32 = bus.GetInternal().GetCompressionRules().GetToken(hdrFields);
        hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].typeId = ALLJOYN_UINT32;
    } else {
        hdrFields.field[ALLJOYN_HDR_FIELD_COMPRESSION_TOKEN].typeId = ALLJOYN_INVALID;
    }
    /*
     * Calculate space required for the header fields
     */
    hdrLen = ComputeHeaderLen();
    /*
     * Check that total packet size is within limits
     */
    if ((hdrLen + argsLen) > ALLJOYN_MAX_PACKET_LEN) {
        status = ER_BUS_BAD_BODY_LEN;
        QCC_LogError(status, ("Message size %d exceeds maximum size", hdrLen + argsLen));
        goto ExitMarshalMessage;
    }
    /*
     * Allocate buffer for entire message.
     */
    msgBuf = new uint64_t[(hdrLen + msgHeader.bodyLen + 7) / 8];
    /*
     * Initialize the buffer and copy in the message header
     */
    bufPos = (uint8_t*)msgBuf;
    memcpy(bufPos, &msgHeader, sizeof(msgHeader));
    bufPos += sizeof(msgHeader);
    /*
     * Perfom endian-swap on the buffer so the header member remains in native endianess.
     */
    if (endianSwap) {
        MessageHeader* hdr = (MessageHeader*)msgBuf;
        EndianSwap32(hdr->bodyLen);
        EndianSwap32(hdr->serialNum);
        EndianSwap32(hdr->headerLen);
    }
    /*
     * Marshal the header fields
     */
    MarshalHeaderFields();
    assert((bufPos - (uint8_t*)msgBuf) == static_cast<ptrdiff_t>(hdrLen));
    if (msgHeader.bodyLen == 0) {
        bufEOD = bufPos;
        bodyPtr = NULL;
        goto ExitMarshalMessage;
    }
    /*
     * Marshal the message body
     */
    bodyPtr = bufPos;
    status = MarshalArgs(args, numArgs);
    if (status != ER_OK) {
        goto ExitMarshalMessage;
    }
    /*
     * If there handles to be marshalled we need to patch up the message header to add the
     * ALLJOYN_HDR_FIELD_HANDLES field. Since handles are rare it is more efficient to do a
     * re-marshal than to parse out handle occurences in every message.
     */
    if (handles) {
        hdrFields.field[ALLJOYN_HDR_FIELD_HANDLES].Set("u", numHandles);
        status = ReMarshal(NULL);
        if (status != ER_OK) {
            goto ExitMarshalMessage;
        }
    }
    /*
     * Assert that our two different body size computations agree
     */
    assert((bufPos - bodyPtr) == (ptrdiff_t)argsLen);
    /*
     * Encrypt and/or authenticate the message.
     */
    if (msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED) {
        PeerStateTable* peerStateTable = bus.GetInternal().GetPeerStateTable();
        KeyBlob key;
        KeyBlob nonce;
        /*
         * Broadcast messages an encrypted with the group key, point-to-point messages use the session key.
         */
        if (destination.empty()) {
            peerStateTable->GetGroupKeyAndNonce(key, nonce);
        } else {
            status = peerStateTable->GetPeerState(destination)->GetKeyAndNonce(key, nonce, PEER_SESSION_KEY);
        }
        if (status != ER_OK) {
            goto ExitMarshalMessage;
        }
        /*
         * Make the nonce unique for this message.
         */
        nonce.Xor((uint8_t*)(&msgHeader.serialNum), sizeof(msgHeader.serialNum));
        /*
         * If the message header is compressed a hash of the compressed header fields is xor'd with
         * the nonce. This is to prevent a security attack where an attacker provides a bogus
         * expansion rule.
         */
        if (msgHeader.flags & ALLJOYN_FLAG_COMPRESSED) {
            KeyBlob hdrHash;
            ajn::Crypto::HashHeaderFields(hdrFields, hdrHash);
            nonce ^= hdrHash;
        }
        status = ajn::Crypto::Encrypt(key, (uint8_t*)msgBuf, hdrLen, argsLen, nonce);
        if (status != ER_OK) {
            goto ExitMarshalMessage;
        }
        authMechanism = key.GetTag();
        assert(msgHeader.bodyLen == argsLen);
    }
    bufEOD = bodyPtr + msgHeader.bodyLen;
    while (numArgs--) {
        QCC_DbgPrintf(("\n%s\n", args->ToString().c_str()));
        ++args;
    }

ExitMarshalMessage:

    /*
     * Don't need the old message buffer any more
     */
    delete [] oldMsgBuf;

    if (status == ER_OK) {
        QCC_DbgHLPrintf(("MarshalMessage: %d+%d %s", hdrLen, msgHeader.bodyLen, Description().c_str()));
    } else {
        QCC_LogError(status, ("MarshalMessage: %s", Description().c_str()));
        delete [] msgBuf;
        msgBuf = NULL;
        bodyPtr = NULL;
        bufPos = NULL;
        bufEOD = NULL;
        ClearHeader();
    }
    return status;
}


QStatus _Message::HelloMessage(bool isBusToBus, bool allowRemote, uint32_t& serial)
{
    QStatus status;
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    if (isBusToBus) {
        /* org.alljoyn.Bus.BusHello */
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].typeId = ALLJOYN_OBJECT_PATH;
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.str = org::alljoyn::Bus::ObjectPath;
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.len = strlen(org::alljoyn::Bus::ObjectPath);

        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.str = org::alljoyn::Bus::InterfaceName;
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.len = strlen(org::alljoyn::Bus::InterfaceName);

        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.str = "BusHello";
        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.len = 8;

        qcc::String guid = bus.GetInternal().GetGlobalGUID().ToString();
        MsgArg args[2];
        args[0].Set("s", guid.c_str());
        args[1].Set("u", ALLJOYN_PROTOCOL_VERSION);
        status = MarshalMessage("su",
                                org::alljoyn::Bus::WellKnownName,
                                MESSAGE_METHOD_CALL,
                                args,
                                ArraySize(args),
                                ALLJOYN_FLAG_AUTO_START | (allowRemote ? ALLJOYN_FLAG_ALLOW_REMOTE_MSG : 0),
                                0);
    } else {
        /* Standard org.freedesktop.DBus.Hello */
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].typeId = ALLJOYN_OBJECT_PATH;
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.str = org::freedesktop::DBus::ObjectPath;
        hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.len = strlen(org::freedesktop::DBus::ObjectPath);

        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.str = org::freedesktop::DBus::InterfaceName;
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.len = strlen(org::freedesktop::DBus::InterfaceName);

        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.str = "Hello";
        hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.len = 5;

        status = MarshalMessage("",
                                org::freedesktop::DBus::WellKnownName,
                                MESSAGE_METHOD_CALL,
                                NULL,
                                0,
                                ALLJOYN_FLAG_AUTO_START | (allowRemote ? ALLJOYN_FLAG_ALLOW_REMOTE_MSG : 0),
                                0);
    }

    /*
     * Return the serial number for this message
     */
    serial = msgHeader.serialNum;

    return status;
}


QStatus _Message::HelloReply(bool isBusToBus, const qcc::String& uniqueName)
{
    QStatus status;
    qcc::String guidStr;

    assert(msgHeader.msgType == MESSAGE_METHOD_CALL);
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Return serial number
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = msgHeader.serialNum;

    if (isBusToBus) {
        guidStr = bus.GetInternal().GetGlobalGUID().ToString();
        MsgArg args[3];
        args[0].Set("s", uniqueName.c_str());
        args[1].Set("s", guidStr.c_str());
        args[2].Set("u", ALLJOYN_PROTOCOL_VERSION);
        status = MarshalMessage("ssu", uniqueName, MESSAGE_METHOD_RET, args, ArraySize(args), 0, 0);
        QCC_DbgPrintf(("\n%s", ToString(args, 2).c_str()));
    } else {
        /* Destination and argument are both the unique name passed in. */
        MsgArg arg("s", uniqueName.c_str());
        status = MarshalMessage("s", uniqueName, MESSAGE_METHOD_RET, &arg, 1, 0, 0);
        QCC_DbgPrintf(("\n%s", ToString(&arg, 1).c_str()));
    }
    return status;
}


QStatus _Message::CallMsg(const qcc::String& signature,
                          const qcc::String& destination,
                          SessionId sessionId,
                          const qcc::String& objPath,
                          const qcc::String& iface,
                          const qcc::String& methodName,
                          uint32_t& serial,
                          const MsgArg* args,
                          size_t numArgs,
                          uint8_t flags)
{
    QStatus status;

    /*
     * Validate flags
     */
    if (flags & ~(ALLJOYN_FLAG_NO_REPLY_EXPECTED | ALLJOYN_FLAG_AUTO_START | ALLJOYN_FLAG_ENCRYPTED | ALLJOYN_FLAG_COMPRESSED)) {
        return ER_BUS_BAD_HDR_FLAGS;
    }
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Check object name
     */
    if (!IsLegalObjectPath(objPath.c_str())) {
        status = ER_BUS_BAD_OBJ_PATH;
        goto ExitCallMsg;
    }
    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].typeId = ALLJOYN_OBJECT_PATH;
    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.str = objPath.c_str();
    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.len = objPath.size();
    /*
     * Member name is required
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.str = methodName.c_str();
    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.len = methodName.size();
    /*
     * Interface name can be NULL
     */
    if (!iface.empty()) {
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].typeId = ALLJOYN_STRING;
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.str = iface.c_str();
        hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.len = iface.size();
    }
    /*
     * Destination is required
     */
    if (destination.empty()) {
        status = ER_BUS_BAD_BUS_NAME;
        goto ExitCallMsg;
    }
    /*
     * Build method call message
     */
    status = MarshalMessage(signature, destination, MESSAGE_METHOD_CALL, args, numArgs, flags, sessionId);
    if (status == ER_OK) {
        /*
         * Return the serial number for this message
         */
        serial = msgHeader.serialNum;
    }

ExitCallMsg:
    return status;
}


QStatus _Message::SignalMsg(const qcc::String& signature,
                            const char* destination,
                            SessionId sessionId,
                            const qcc::String& objPath,
                            const qcc::String& iface,
                            const qcc::String& signalName,
                            const MsgArg* args,
                            size_t numArgs,
                            uint8_t flags,
                            uint16_t timeToLive)
{
    QStatus status;

    /*
     * Validate flags - ENCRYPTED and COMPRESSED are the only flag applicable to signals
     */
    if (flags & ~(ALLJOYN_FLAG_ENCRYPTED | ALLJOYN_FLAG_COMPRESSED | ALLJOYN_FLAG_GLOBAL_BROADCAST)) {
        return ER_BUS_BAD_HDR_FLAGS;
    }
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Check object name is legal
     */
    if (!IsLegalObjectPath(objPath.c_str())) {
        status = ER_BUS_BAD_OBJ_PATH;
        goto ExitSignalMsg;
    }

    /* NULL destination is allowed */
    if (!destination) {
        destination = "";
    }

    /*
     * If signal has a ttl timestamp the message and set the ttl and timestamp headers.
     */
    if (timeToLive) {
        timestamp = GetTimestamp();
        ttl = timeToLive;
        hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].typeId = ALLJOYN_UINT16;
        hdrFields.field[ALLJOYN_HDR_FIELD_TIME_TO_LIVE].v_uint16 = ttl;
        hdrFields.field[ALLJOYN_HDR_FIELD_TIMESTAMP].typeId = ALLJOYN_UINT32;
        hdrFields.field[ALLJOYN_HDR_FIELD_TIMESTAMP].v_uint32 = timestamp;
    }

    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].typeId = ALLJOYN_OBJECT_PATH;
    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.str = objPath.c_str();
    hdrFields.field[ALLJOYN_HDR_FIELD_PATH].v_string.len = objPath.size();

    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.str = signalName.c_str();
    hdrFields.field[ALLJOYN_HDR_FIELD_MEMBER].v_string.len = signalName.size();

    hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.str = iface.c_str();
    hdrFields.field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.len = iface.size();

    /*
     * Build signal message
     */
    status = MarshalMessage(signature, destination, MESSAGE_SIGNAL, args, numArgs, flags, sessionId);

ExitSignalMsg:
    return status;
}


QStatus _Message::ReplyMsg(const MsgArg* args,
                           size_t numArgs)
{
    QStatus status;
    SessionId sessionId = GetSessionId();

    /*
     * Destination is sender of method call
     */
    qcc::String destination = hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].v_string.str;

    assert(msgHeader.msgType == MESSAGE_METHOD_CALL);

    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Return serial number from call
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = msgHeader.serialNum;
    /*
     * Build method return message (encrypted if the method call was encrypted)
     */
    status = MarshalMessage(replySignature, destination, MESSAGE_METHOD_RET, args,
                            numArgs, msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED, sessionId);
    return status;
}


QStatus _Message::ErrorMsg(const char* errorName,
                           const char* description)
{
    QStatus status;
    qcc::String destination = hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].v_string.str;
    SessionId sessionId = GetSessionId();

    assert(msgHeader.msgType == MESSAGE_METHOD_CALL);

    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Error name is required
     */
    if ((errorName == NULL) || (*errorName == 0)) {
        status = ER_BUS_BAD_ERROR_NAME;
        goto ExitErrorMsg;
    }
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.str = errorName;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.len = strlen(errorName);
    /*
     * Return serial number
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = msgHeader.serialNum;
    /*
     * Build error message
     */
    if ('\0' == description[0]) {
        status = MarshalMessage("", destination, MESSAGE_ERROR, NULL, 0, msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED, sessionId);
    } else {
        MsgArg arg("s", description);
        status = MarshalMessage("s", destination, MESSAGE_ERROR, &arg, 1, msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED, sessionId);
    }

ExitErrorMsg:
    return status;
}


QStatus _Message::ErrorMsg(QStatus status)
{
    qcc::String destination = hdrFields.field[ALLJOYN_HDR_FIELD_SENDER].v_string.str;
    qcc::String msg = QCC_StatusText(status);
    uint16_t msgStatus = status;

    assert(msgHeader.msgType == MESSAGE_METHOD_CALL);
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * Error name is required
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.str = org::alljoyn::Bus::ErrorName;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.len = strlen(org::alljoyn::Bus::ErrorName);
    /*
     * Return serial number
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = msgHeader.serialNum;
    /*
     * Build error message
     */
    MsgArg args[2];
    size_t numArgs = 2;
    MsgArg::Set(args, numArgs, "sq", msg.c_str(), msgStatus);
    return MarshalMessage("sq", destination, MESSAGE_ERROR, args, numArgs, msgHeader.flags & ALLJOYN_FLAG_ENCRYPTED, GetSessionId());
}


void _Message::ErrorMsg(const char* errorName,
                        uint32_t replySerial)
{
    SessionId sessionId = GetSessionId();
    /*
     * Clear any stale header fields
     */
    ClearHeader();
    /*
     * An error name is required so add one if necessary.
     */
    if ((errorName == NULL) || (*errorName == 0)) {
        errorName = "err.unknown";
    }
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.str = errorName;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.len = strlen(errorName);
    /*
     * Return serial number
     */
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = replySerial;
    /*
     * Build error message
     */
    MarshalMessage("", "", MESSAGE_ERROR, NULL, 0, 0, sessionId);
}

void _Message::ErrorMsg(QStatus status,
                        uint32_t replySerial)
{
    qcc::String msg = QCC_StatusText(status);
    uint16_t msgStatus = status;
    SessionId sessionId = GetSessionId();

    /*
     * Clear any stale header fields
     */
    ClearHeader();

    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].typeId = ALLJOYN_STRING;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.str = org::alljoyn::Bus::ErrorName;
    hdrFields.field[ALLJOYN_HDR_FIELD_ERROR_NAME].v_string.len = strlen(org::alljoyn::Bus::ErrorName);

    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].typeId = ALLJOYN_UINT32;
    hdrFields.field[ALLJOYN_HDR_FIELD_REPLY_SERIAL].v_uint32 = replySerial;

    MsgArg args[2];
    size_t numArgs = 2;
    MsgArg::Set(args, numArgs, "sq", msg.c_str(), msgStatus);
    MarshalMessage("sq", "", MESSAGE_ERROR, args, numArgs, 0, sessionId);
}

QStatus _Message::GetExpansion(uint32_t token, MsgArg& replyArg)
{
    QStatus status = ER_OK;
    const HeaderFields* expFields = bus.GetInternal().GetCompressionRules().GetExpansion(token);
    if (expFields) {
        MsgArg* hdrArray = new MsgArg[ALLJOYN_HDR_FIELD_UNKNOWN];
        size_t numElements = 0;
        /*
         * Reply arg is an array of structs with signature "(yv)"
         */
        for (uint32_t fieldId = ALLJOYN_HDR_FIELD_PATH; fieldId < ArraySize(expFields->field); fieldId++) {
            MsgArg* val = NULL;
            const MsgArg* exp = &expFields->field[fieldId];
            switch (exp->typeId) {
            case ALLJOYN_OBJECT_PATH:
                val = new MsgArg("o", exp->v_string.str);
                break;

            case ALLJOYN_STRING:
                val = new MsgArg("s", exp->v_string.str);
                break;

            case  ALLJOYN_SIGNATURE:
                val = new MsgArg("g", exp->v_signature.sig);
                break;

            case  ALLJOYN_UINT32:
                val = new MsgArg("u", exp->v_uint32);
                break;

            case  ALLJOYN_UINT16:
                val = new MsgArg("q", exp->v_uint16);
                break;

            default:
                break;
            }
            if (val) {
                uint8_t id = FieldTypeMapping[fieldId];
                hdrArray[numElements].Set("(yv)", id, val);
                hdrArray[numElements].SetOwnershipFlags(MsgArg::OwnsArgs);
                numElements++;
            }
        }
        replyArg.Set("a(yv)", numElements, hdrArray);
    } else {
        status = ER_BUS_CANNOT_EXPAND_MESSAGE;
        QCC_LogError(status, ("No expansion rule for token %u", token));
    }
    return status;
}

}
