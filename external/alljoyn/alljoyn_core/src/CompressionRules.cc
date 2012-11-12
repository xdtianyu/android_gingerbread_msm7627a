/**
 * @file
 * Implementation of CompressionRules methods.
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

#include <qcc/Util.h>
#include <qcc/Mutex.h>
#include <qcc/Debug.h>
#include <Status.h>

#include <alljoyn/BusAttachment.h>
#include <alljoyn/Message.h>

#include "Adler32.h"
#include "CompressionRules.h"

#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {

void CompressionRules::Add(const HeaderFields& hdrFields, uint32_t token)
{
    HeaderFields* expFields = new HeaderFields;
    /*
     * Copy compressible fields.
     */
    for (size_t i = 0; i < ArraySize(expFields->field); i++) {
        if (HeaderFields::Compressible[i]) {
            expFields->field[i] = hdrFields.field[i];
        }
    }
    /*
     * Add forward and reverse mapping.
     */
    tokenMap[token] = expFields;
    fieldMap[expFields] = token;
    QCC_DbgHLPrintf(("Added compression/expansion rule %u <-->\n%s", token, expFields->ToString().c_str()));
}

void CompressionRules::AddExpansion(const HeaderFields& hdrFields, uint32_t token)
{
    if (token) {
        lock.Lock();
        if (fieldMap.count(&hdrFields) == 0) {
            Add(hdrFields, token);
        } else {
            QCC_LogError(ER_FAIL, ("Compression token collision %u", token));
        }
        pending.erase(token);
        lock.Unlock();
    }
}

uint32_t CompressionRules::GetToken(const HeaderFields& hdrFields)
{
    uint32_t token;
    lock.Lock();
    hash_map<const HeaderFields*, uint32_t, HdrFieldHash, HdrFieldsEq>::iterator iter = fieldMap.find(&hdrFields);
    if (iter != fieldMap.end()) {
        token = iter->second;
    } else {
        /*
         * Allocate a random token (check it isn't zero and not in use)
         */
        do { token = Rand32(); } while (token && GetExpansion(token));
        Add(hdrFields, token);
    }
    lock.Unlock();
    return token;
}

const HeaderFields* CompressionRules::GetExpansion(uint32_t token)
{
    const HeaderFields* expansion = NULL;
    if (token) {
        lock.Lock();
        map<uint32_t, const HeaderFields*>::iterator iter = tokenMap.find(token);
        expansion = (iter != tokenMap.end()) ? iter->second : NULL;
        lock.Unlock();
    }
    return expansion;
}

CompressionRules::~CompressionRules()
{
    map<uint32_t, const ajn::HeaderFields*>::iterator iter = tokenMap.begin();
    while (iter != tokenMap.end()) {
        delete iter->second;
        iter++;
    }
}

}
