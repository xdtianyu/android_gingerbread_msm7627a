/**
 * @file
 * Class for managing header compression information
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
#ifndef _ALLJOYN_COMPRESSION_RULES_H
#define _ALLJOYN_COMPRESSION_RULES_H

#ifndef __cplusplus
#error Only include CompressionRules.h in C++ code.
#endif

#include <qcc/platform.h>
#include <qcc/String.h>
#include <qcc/Util.h>
#include <qcc/Mutex.h>

#include <alljoyn/Message.h>

#include <Status.h>

#include <map>
#include <set>

#if defined(__GNUC__) && !defined(ANDROID)
#include <ext/hash_map>
namespace std {
using namespace __gnu_cxx;
}
#else
#include <hash_map>
#endif

#include "Adler32.h"


namespace ajn {

/**
 * This class maintains a list of header compression rules for header field compression and provides
 * methods that map from a expanded header to a compression token and back. This class is used by
 * the marshaling code to compress a header before sending it.
 */
class CompressionRules {

  public:

    /**
     * Add a new expansion rule to the expansion table. This is an expansion that was received from
     * a remote peer. Note that 0 is an invalid token value.
     *
     * @param hdrFields  The header fields to add.
     * @param token      The compression token for the header fields.
     *
     * @return ER_OK if the expansion rule was added.
     */
    void AddExpansion(const HeaderFields& hdrFields, uint32_t token);

    /**
     * Get the compression token for the specified header fields.
     *
     * @param hdrFields  The header fields to look up.
     *
     * @return  An existing token or a newly allocated token,
     */
    uint32_t GetToken(const HeaderFields& hdrFields);

    /**
     * Perform the lookup of the expansion given a compression token. Note that token must
     * be non-zero.
     *
     * @param token  The compression token to lookup.
     *
     * @return  The expansion for the compression token or NULL if there is no such expansion.
     */
    const HeaderFields* GetExpansion(uint32_t token);

    /**
     * Destructor
     */
    ~CompressionRules();

  private:

    /**
     * Add a compression/expansion rule.
     */
    void Add(const HeaderFields& hdrFields, uint32_t token);

    /**
     * Mutex to protect compression rules
     */
    qcc::Mutex lock;

    /**
     * Hash funcion for header compression. Hash value is computed over member and interface only.
     * on the reasonable assumption that there will only be one compression for a specific message.
     */
    struct HdrFieldHash {
        inline size_t operator()(const HeaderFields* k) const {
            Adler32 adler;
            size_t hash = 0;
            if (k->field[ALLJOYN_HDR_FIELD_MEMBER].typeId == ALLJOYN_STRING) {
                hash = adler.Update((uint8_t*)k->field[ALLJOYN_HDR_FIELD_MEMBER].v_string.str, k->field[ALLJOYN_HDR_FIELD_MEMBER].v_string.len);
            }
            if (k->field[ALLJOYN_HDR_FIELD_INTERFACE].typeId == ALLJOYN_STRING) {
                hash = adler.Update((uint8_t*)k->field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.str, k->field[ALLJOYN_HDR_FIELD_INTERFACE].v_string.len);
            }
            return hash;
        }
    };

    /**
     * Function for testing compressible message header fields for equality.
     */
    struct HdrFieldsEq {
        inline bool operator()(const HeaderFields* k1, const HeaderFields* k2) const {
            const MsgArg* f1 = k1->field;
            const MsgArg* f2 = k2->field;
            for (int i = 0; i < ALLJOYN_HDR_FIELD_UNKNOWN; i++, f1++, f2++) {
                if (HeaderFields::Compressible[i]) {
                    if (f1->typeId != f2->typeId) {
                        return false;
                    }
                    switch (f1->typeId) {
                    case ALLJOYN_INVALID:
                        break;

                    case ALLJOYN_STRING:
                    case ALLJOYN_OBJECT_PATH:
                        if (strcmp(f1->v_string.str, f2->v_string.str) != 0) {
                            return false;
                        }
                        break;

                    case ALLJOYN_SIGNATURE:
                        if (strcmp(f1->v_signature.sig, f2->v_signature.sig) != 0) {
                            return false;
                        }
                        break;

                    case ALLJOYN_UINT16:
                        if (f1->v_uint16 != f2->v_uint16) {
                            return false;
                        }
                        break;

                    case ALLJOYN_UINT32:
                        if (f1->v_uint32 != f2->v_uint32) {
                            return false;
                        }
                        break;

                    default:
                        assert(!"invalid header field type");
                    }
                }
            }
            return true;
        }
    };

    /**
     * The header compression mapping from header fields to compression token
     */
    std::hash_map<const ajn::HeaderFields*, uint32_t, HdrFieldHash, HdrFieldsEq> fieldMap;

    /*
     * The header expansion mapping from compression token to header fields
     */
    std::map<uint32_t, const ajn::HeaderFields*> tokenMap;

    /**
     * Set to track when a token expansion has been requested.
     */
    std::set<uint32_t> pending;

};

}

#endif
