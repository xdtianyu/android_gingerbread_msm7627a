/**
 * @file
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

#ifndef _QCC_STRINGMAPKEY_H
#define _QCC_STRINGMAPKEY_H

#include <qcc/platform.h>

#include <qcc/String.h>
#include <string.h>

#if defined(__GNUC__) && !defined(ANDROID)
#define HASH_NAMESPACE __gnu_cxx
#include <ext/hash_map>
namespace std {
using namespace __gnu_cxx;
}
#else
#define HASH_NAMESPACE std
#include <hash_map>
#endif

namespace qcc {

/**
 * StringMapKey is useful when creating a std:map that wants a std::string
 * as its key. It is preferable to using a std::string directly since
 * doing so would require all lookup operations on the map to first create
 * a std::string (rather than using a simple const char*).
 */
class StringMapKey {
  public:
    /**
     * Create a backed version of the StringMapKey
     * Typically, this constructor is used when inserting into a map
     * since it causes storage to be allocated for the string
     *
     * @param key   String whose value will be copied into StringMapKey
     */
    StringMapKey(const qcc::String& key) : charPtr(NULL), str(key) { }

    /**
     * Create an unbacked version of the StringMapKey
     * Typically, this constructor is used when forming a key to pass to
     * map::find(), etc. The StringMapKey is a simple container for the
     * passed in char*. The char* arg to this constructor must remain
     * valid for the life of the StringMapKey.
     *
     * @param key   char* whose value (but not contents) will be stored
     *              in the StringMapKey.
     */
    StringMapKey(const char* key) : charPtr(key), str() { }

    /**
     * Get a char* representation of this StringKeyMap
     *
     * @return  char* representation this StringKeyMap
     */
    inline const char* c_str() const { return charPtr ? charPtr : str.c_str(); }

    /**
     * Return true if StringMapKey is empty.
     *
     * @return true iff StringMapKey is empty.
     */
    inline bool empty() const { return charPtr ? charPtr[0] == '\0' : str.empty(); }

    /**
     * Return the size of the contained string.
     *
     * @return size of the contained string.
     */
    inline size_t size() const { return charPtr ? strlen(charPtr) : str.size(); }

    /**
     * Less than operation
     */
    inline bool operator<(const StringMapKey& other) const { return ::strcmp(c_str(), other.c_str()) < 0; }

    /**
     * Equals operation
     */
    inline bool operator==(const StringMapKey& other) const { return ::strcmp(c_str(), other.c_str()) == 0; }

  private:
    const char* charPtr;
    qcc::String str;
};

}  // End of qcc namespace

/**
 * Functor to compute a hash for StringMapKey suitable for use with
 * std::hash_map and std::hash_set.
 */
namespace HASH_NAMESPACE {

template <>
struct hash<qcc::StringMapKey>{
    inline size_t operator()(const qcc::StringMapKey& k) const { return HASH_NAMESPACE::hash<const char*>() (k.c_str()); }
};

}
/**
 * Functor to compute StrictWeakOrder
 */
namespace std {

template <>
struct less<qcc::StringMapKey>{
    inline bool operator()(const qcc::StringMapKey& a, const qcc::StringMapKey& b) const { return ::strcmp(a.c_str(), b.c_str()) < 0; }
};

}
#endif
