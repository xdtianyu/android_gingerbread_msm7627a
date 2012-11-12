/**
 * @file
 *
 * This file defines the method hash table class
 *
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

#include "MethodTable.h"

/** @internal */
#define QCC_MODULE "ALLJOYN"

using namespace qcc;
using namespace std;

namespace ajn {

MethodTable::~MethodTable()
{
    lock.Lock();
    std::hash_map<Key, Entry*, Hash, Equal>::iterator iter = hashTable.begin();
    while (iter != hashTable.end()) {
        delete iter->second;
        hashTable.erase(iter);
        iter = hashTable.begin();
    }
    lock.Unlock();
}

void MethodTable::Add(BusObject* object,
                      MessageReceiver::MethodHandler func,
                      const InterfaceDescription::Member* member)
{
    Entry* entry = new Entry(object, func, member);
    lock.Lock();
    hashTable[Key(object->GetPath(), entry->ifaceStr.empty() ? NULL : entry->ifaceStr.c_str(), member->name.c_str())] = entry;

    /* Method calls don't require an interface so we need to add an entry with a NULL interface */
    if (!entry->ifaceStr.empty()) {
        hashTable[Key(object->GetPath(), NULL, member->name.c_str())] = new Entry(*entry);
    }
    lock.Unlock();
}

const MethodTable::Entry* MethodTable::Find(const char* objectPath,
                                            const char* iface,
                                            const char* methodName)
{
    const Entry* entry = NULL;
    Key key(objectPath, iface, methodName);
    lock.Lock();
    std::hash_map<Key, Entry*, Hash, Equal>::iterator iter = hashTable.find(key);
    if (iter != hashTable.end()) {
        entry = iter->second;
    }
    lock.Unlock();
    return entry;
}

void MethodTable::RemoveAll(BusObject* object)
{
    std::hash_map<Key, Entry*, Hash, Equal>::iterator iter;
    /*
     * Iterate over all entries deleting all entries that reference the object
     */
    lock.Lock();
    iter = hashTable.begin();
    while (iter != hashTable.end()) {
        if (iter->second->object == object) {
            Entry* deleteMe = iter->second;
            hashTable.erase(iter);
            delete deleteMe;
            iter = hashTable.begin();
        } else {
            ++iter;
        }
    }
    lock.Unlock();
}

void MethodTable::AddAll(BusObject* object)
{
    object->InstallMethods(*this);
}

}

