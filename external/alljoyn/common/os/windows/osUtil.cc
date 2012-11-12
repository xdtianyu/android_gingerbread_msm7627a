/**
 * @file
 *
 * OS specific utility functions
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
#include <windows.h>
#include <process.h>
#include <lmcons.h>
#define SECURITY_WIN32
#include <security.h>
#include <secext.h>

#include <qcc/Crypto.h>
#include <qcc/Util.h>
#include <qcc/String.h>
#include <qcc/Debug.h>



#define QCC_MODULE  "UTIL"


namespace qcc {

uint32_t GetPid()
{
    return static_cast<uint32_t>(_getpid());
}

static uint32_t ComputeId(const char* buf, size_t len)
{
    QCC_DbgPrintf(("ComputeId %s", buf));
    uint32_t digest[Crypto_SHA1::DIGEST_SIZE / sizeof(uint32_t)];
    qcc::Crypto_SHA1 sha1;
    sha1.Init();
    sha1.Update((const uint8_t*)buf, (size_t)len);
    sha1.GetDigest((uint8_t*)digest);
    return digest[0];
}

uint32_t GetUid()
{
    /*
     * Windows has no equivalent of getuid so fake one by creating a hash of the user name.
     */
    char buf[UNLEN];
    ULONG len = UNLEN;
    if (GetUserNameExA(NameUniqueId, buf, &len)) {
        return ComputeId((char*)buf, len);
    } else {
        return ComputeId("nobody", sizeof("nobody") - 1);
    }
}

uint32_t qcc::GetGid()
{
    /*
     * Windows has no equivalent of getgid so fake one by creating a hash of the user's domain name.
     */
    char buf[UNLEN];
    ULONG len = UNLEN;
    if (GetUserNameExA(NameDnsDomain, buf, &len)) {
        qcc::String gp((char*)buf, len);
        size_t pos = gp.find_first_of('\\');
        if (pos != qcc::String::npos) {
            gp.erase(pos);
        }
        return ComputeId((const char*)gp.c_str(), gp.size());
    } else {
        return ComputeId("nogroup", sizeof("nogroup") - 1);
    }
}

uint32_t GetUsersUid(const char* name)
{
    return ComputeId(name, strlen(name));
}

uint32_t GetUsersGid(const char* name)
{
    return ComputeId(name, strlen(name));
}

qcc::String qcc::GetHomeDir()
{
    return Environ::GetAppEnviron()->Find("USERPROFILE");
}

QStatus GetDirListing(const char* path, DirListing& listing)
{
    return ER_NOT_IMPLEMENTED;
}


QStatus Exec(const char* exec, const ExecArgs& args, const qcc::Environ& envs)
{
    return ER_NOT_IMPLEMENTED;
}


QStatus ExecAs(const char* user, const char* exec, const ExecArgs& args, const qcc::Environ& envs)
{
    return ER_NOT_IMPLEMENTED;
}

}
