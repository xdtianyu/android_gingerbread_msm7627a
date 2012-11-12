/**
 * @file
 *
 * Common utility functions
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

#include <stdlib.h>

#include <qcc/platform.h>

#include <qcc/String.h>
#include <qcc/Crypto.h>
#include <qcc/Util.h>

#define QCC_MODULE  "UTIL"

using namespace std;


uint8_t qcc::Rand8()
{
    return (uint8_t)(qcc::Rand16() >> 8);
}

uint16_t qcc::Rand16()
{
    static bool seeded = false;
    if (!seeded) {
        srand(qcc::Rand32());
        seeded = true;
    }
    return (uint16_t)rand();
}

uint32_t qcc::Rand32()
{
    uint32_t r = 0;
    Crypto_BigNum bigNum;
    bigNum.GenerateRandomValue(32);
    bigNum.RenderBinary((uint8_t*)&r, sizeof(r));
    return r;
}


uint64_t qcc::Rand64()
{
    uint64_t r = 0;
    Crypto_BigNum bigNum;
    bigNum.GenerateRandomValue(64);
    bigNum.RenderBinary((uint8_t*)&r, sizeof(r));
    return r;
}


qcc::String qcc::RandomString(const char* prefix, size_t len)
{
    // 64 filename safe characters
    static const char c[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_+";
    qcc::String str(prefix);
    uint8_t* bits = new uint8_t[len];
    Crypto_BigNum bigNum;
    bigNum.GenerateRandomValue(8 * len);
    bigNum.RenderBinary(bits, len);

    for (size_t i = 0; i < len; ++i) {
        str += c[bits[i] & 0x3f];
    }
    delete[] bits;
    return str;
}
