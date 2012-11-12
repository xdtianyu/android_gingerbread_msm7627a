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

#include <dirent.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <qcc/Debug.h>
#include <qcc/String.h>
#include <qcc/Environ.h>
#include <qcc/Util.h>

#include <Status.h>


#define QCC_MODULE  "UTIL"

using namespace std;

uint32_t qcc::GetPid()
{
    return static_cast<uint32_t>(getpid());
}

uint32_t qcc::GetUid()
{
    return static_cast<uint32_t>(getuid());
}

uint32_t qcc::GetGid()
{
    return static_cast<uint32_t>(getgid());
}

uint32_t qcc::GetUsersUid(const char* name)
{
    uint32_t uid = -1;
    if (name) {
        struct passwd* pwent = getpwnam(name);
        if (pwent) {
            uid = pwent->pw_uid;
        }
    }
    return uid;
}

uint32_t qcc::GetUsersGid(const char* name)
{
    uint32_t gid = -1;
    if (name) {
        struct passwd* pwent = getpwnam(name);
        if (pwent) {
            gid = pwent->pw_gid;
        }
    }
    return gid;
}

qcc::String qcc::GetHomeDir()
{
    /* Defaulting to '/' handles both the plain posix and Android cases. */
    return Environ::GetAppEnviron()->Find("HOME", "/");
}

QStatus qcc::GetDirListing(const char* path, DirListing& listing)
{
    DIR* dir;
    struct dirent* entry;

    dir = opendir(path);
    if (!dir) {
        return ER_OS_ERROR;
    }
    while ((entry = readdir(dir)) != NULL) {
        listing.push_back(entry->d_name);
    }
    closedir(dir);
    return ER_OK;
}


QStatus qcc::Exec(const char* exec, const ExecArgs& args, const Environ& envs)
{
    pid_t pid;

    pid = fork();

    if (pid == 0) {
        pid_t sid = setsid();
        if (sid < 0) {
            QCC_LogError(ER_OS_ERROR, ("Failed to set session ID for new process"));
            return ER_OS_ERROR;
        }
        char** argv(new char*[args.size() + 2]);    // Need extra entry for executable name
        char** env(new char*[envs.Size() + 1]);
        int index = 0;
        ExecArgs::const_iterator it;
        Environ::const_iterator envit;

        argv[index] = strdup(exec);
        ++index;

        for (it = args.begin(); it != args.end(); ++it, ++index) {
            argv[index] = strdup(it->c_str());
        }
        argv[index] = NULL;

        for (envit = envs.Begin(), index = 0; envit != envs.End(); ++envit, ++index) {
            qcc::String var((envit->first + "=" + envit->second));
            env[index] = strdup(var.c_str());
        }
        env[index] = NULL;

        execve(exec, argv, env); // will never return if successful.
    } else if (pid == -1) {
        return ER_OS_ERROR;
#ifndef NDEBUG
    } else {
        QCC_DbgPrintf(("Started %s with PID: %d", exec, pid));
#endif
    }
    return ER_OK;
}


QStatus qcc::ExecAs(const char* user, const char* exec, const ExecArgs& args, const Environ& envs)
{
    pid_t pid;

    pid = fork();

    if (pid == 0) {
        pid_t sid = setsid();
        if (sid < 0) {
            QCC_LogError(ER_OS_ERROR, ("Failed to set session ID for new process"));
            return ER_OS_ERROR;
        }
        char** argv(new char*[args.size() + 2]);    // Need extra entry for executable name
        char** env(new char*[envs.Size() + 1]);
        int index = 0;
        ExecArgs::const_iterator it;
        Environ::const_iterator envit;

        argv[index] = strdup(exec);
        ++index;

        for (it = args.begin(); it != args.end(); ++it, ++index) {
            argv[index] = strdup(it->c_str());
        }
        argv[index] = NULL;

        for (envit = envs.Begin(), index = 0; envit != envs.End(); ++envit, ++index) {
            qcc::String var((envit->first + "=" + envit->second));
            env[index] = strdup(var.c_str());
        }
        env[index] = NULL;

        struct passwd* pwent = getpwnam(user);
        if (!pwent) {
            return ER_FAIL;
        }

        setuid(pwent->pw_uid);

        execve(exec, argv, env); // will never return if successful.
    } else if (pid == -1) {
        return ER_OS_ERROR;
#ifndef NDEBUG
    } else {
        QCC_DbgPrintf(("Started %s with PID: %d", exec, pid));
#endif
    }
    return ER_OK;
}
