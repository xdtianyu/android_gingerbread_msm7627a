/******************************************************************************
 * Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
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
 *
 ******************************************************************************/
#include <string.h>
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "bus-daemon-jni"

//
// The AllJoyn daemon has an alternate personality in that it is built as a
// static library, liballjoyn-daemon.a.  In this case, the entry point main() is
// replaced by a function called DaemonMain.  Calling DaemonMain() here
// essentially runs the AllJoyn daemon like it had been run on the command line.
//
extern int DaemonMain(int argc, char** argv, char* config);

void do_log(const char* format, ...)
{
    va_list arglist;

    va_start(arglist, format);
    __android_log_vprint(ANDROID_LOG_DEBUG, LOG_TAG, format, arglist);
    va_end(arglist);

    return;
}


void Java_org_alljoyn_bus_daemonservice_DaemonService_runDaemon(JNIEnv* env, jobject thiz, jobjectArray jargv, jstring jconfig)
{
    int i;
    jsize argc;

    do_log("runDaemon()\n");

    argc = (*env)->GetArrayLength(env, jargv);
    do_log("runDaemon(): argc = %d\n", argc);

    int nBytes = argc * sizeof(char*);
    do_log("runDaemon(): nBytes = %d\n", nBytes);

    do_log("runDaemon(): malloc(%d)\n", nBytes);
    char const** argv  = (char const**)malloc(nBytes);

    for (i = 0; i < argc; ++i) {
        do_log("runDaemon(): copy out string %d\n", i);
        jstring jstr = (*env)->GetObjectArrayElement(env, jargv, i);
        do_log("runDaemon(): set pointer in argv[%d]\n", i);
        argv[i] = (*env)->GetStringUTFChars(env, jstr, 0);
        do_log("runDaemon(): argv[%d] = %s\n", i, argv[i]);
    }

    char const* config = (*env)->GetStringUTFChars(env, jconfig, 0);
    do_log("runDaemon(): config = %s\n", config);

    //
    // Run the alljoyn-daemon providing the array of environment strings as the
    // (shell) environment for the daemon.
    //
    do_log("runDaemon(): calling DaemonMain()\n");
    int rc = DaemonMain(argc, (char**)argv, (char*)config);

    (*env)->ReleaseStringUTFChars(env, jconfig, config);

    free(argv);
}
