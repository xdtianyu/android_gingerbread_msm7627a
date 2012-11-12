/**
 * @file GUID.cc
 *
 * Implements GUID
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

#include <qcc/GUID.h>
#include <qcc/Crypto.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <Status.h>

using namespace std;
using namespace qcc;

#define QCC_MODULE "GUID"

namespace qcc {

GUID::GUID() : value(), shortValue()
{
    Crypto_BigNum val;
    val.GenerateRandomValue(8 * SIZE);
    val.RenderBinary(guid, SIZE);
}

GUID::GUID(uint8_t init) : value(), shortValue()
{
    memset(guid, init, SIZE);
}

bool GUID::Compare(const qcc::String& other)
{
    uint8_t them[SIZE];
    if (HexStringToBytes(other, them, SIZE) == SIZE) {
        return memcmp(guid, them, SIZE) == 0;
    } else {
        return false;
    }
}

bool GUID::IsGUID(const qcc::String& str, bool exactLen)
{
    if (exactLen && str.length() != (2 * SIZE)) {
        return false;
    } else {
        uint8_t hex[SIZE];
        return HexStringToBytes(str, hex, SIZE) == SIZE;
    }
}

const qcc::String& GUID::ToString() const
{
    if (value.empty()) {
        value = BytesToHexString(guid, SIZE, true);
    }
    return value;
}


const qcc::String& GUID::ToShortString() const
{
    if (shortValue.empty()) {
        char outBytes[SHORT_SIZE + 1];
        outBytes[SHORT_SIZE] = '\0';
        for (size_t i = 0; i < SHORT_SIZE; ++i) {
            uint8_t cur = (guid[i] & 0x3F); /* gives a number between 0 and 63 */
            if (cur < 10) {
                outBytes[i] = (cur + '0');
            } else if (cur < 36) {
                outBytes[i] = ((cur - 10) + 'A');
            } else if (cur < 62) {
                outBytes[i] = ((cur - 36) + 'a');
            } else if (cur == 63) {
                outBytes[i] = '_';
            } else {
                outBytes[i] = '-';
            }
        }
        shortValue = outBytes;
    }
    return shortValue;
}

GUID::GUID(const qcc::String& hexStr) : value(), shortValue()
{
    size_t size = HexStringToBytes(hexStr, guid, SIZE);
    if (size < SIZE) {
        memset(guid + size, 0, SIZE - size);
    }
}

GUID& GUID::operator =(const GUID& other)
{
    if (this != &other) {
        memcpy(guid, other.guid, sizeof(guid));
    }
    return *this;
}

GUID::GUID(const GUID& other) : value(), shortValue()
{
    memcpy(guid, other.guid, sizeof(guid));
}

uint8_t* GUID::Render(uint8_t* data, size_t len) const
{
    if (len < SIZE) {
        len = SIZE;
    }
    memcpy(data, guid, len);
    return data;
}

void GUID::SetBytes(const uint8_t* rawBytes)
{
    ::memcpy(guid, rawBytes, SIZE);
    value.clear();
    shortValue.clear();
}

}
