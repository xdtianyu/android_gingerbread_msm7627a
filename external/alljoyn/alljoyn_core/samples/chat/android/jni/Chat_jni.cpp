/*
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
 */

#include "org_alljoyn_bus_samples_chat_Chat.h"
#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/DBusStd.h>
#include <alljoyn/AllJoynStd.h>
#include <qcc/Log.h>
#include <qcc/String.h>
#include <new>
#include <android/log.h>
#include <assert.h>


#include <qcc/platform.h>

#define LOG_TAG  "AllJoynChat"

/* Missing (from NDK) log macros (cutils/log.h) */
#ifndef LOGD
#define LOGD(...) (__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef LOGI
#define LOGI(...) (__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef LOGE
#define LOGE(...) (__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

using namespace ajn;

/* constants */
static const char* CHAT_SERVICE_INTERFACE_NAME = "org.alljoyn.bus.samples.chat";
static const char* CHAT_SERVICE_WELL_KNOWN_NAME = "org.alljoyn.bus.samples.chat";
static const char* CHAT_SERVICE_OBJECT_PATH = "/chatService";
static const char* NAME_PREFIX = "org.alljoyn.bus.samples.chat";
static const int CHAT_PORT = 27;

/* Forward declaration */
class ChatObject;
class MyBusListener;

/* Static data */
static BusAttachment* s_bus = NULL;
static ChatObject* s_chatObj = NULL;
static qcc::String s_connectName;
static qcc::String s_advertisedName;
static MyBusListener* s_busListener = NULL;
static SessionId s_sessionId = 0;
static qcc::String sessionName;


class MyBusListener : public BusListener, public SessionPortListener, public SessionListener {
  public:
    MyBusListener(JavaVM* vm, jobject& jobj) : vm(vm), jobj(jobj) { }

    void FoundAdvertisedName(const char* name, TransportMask transport, const char* namePrefix)
    {
        const char* convName = name + strlen(NAME_PREFIX);
        LOGD("Discovered chat conversation: %s \n", name);

        /* Join the conversation */

        SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
        QStatus status = s_bus->JoinSession(name, CHAT_PORT, NULL, s_sessionId, opts);
        if (ER_OK == status) {
            LOGD("Joined conversation \"%s\"\n", name);
        } else {
            LOGD("JoinSession failed status=%s\n", QCC_StatusText(status));
        }
    }

    /* Accept an incoming JoinSession request */
    bool AcceptSessionJoiner(SessionPort sessionPort, const char* joiner, const SessionOpts& opts)
    {
        if (sessionPort != CHAT_PORT) {
            LOGE("Rejecting join attempt on non-chat session port %d\n", sessionPort);
            return false;
        }

        LOGD("Accepting join session request from %s (opts.proximity=%x, opts.traffic=%x, opts.transports=%x)\n",
             joiner, opts.proximity, opts.traffic, opts.transports);


        return true;
    }

    void SessionJoined(SessionPort sessionPort, SessionId id, const char* joiner)
    {
        s_sessionId = id;
        LOGD("SessionJoined with %s (id=%u)\n", joiner, id);
    }


    void NameOwnerChanged(const char* busName, const char* previousOwner, const char* newOwner)
    {
    }
  private:
    JavaVM* vm;
    jobject jobj;
};


/* Bus object */
class ChatObject : public BusObject {
  public:

    ChatObject(BusAttachment& bus, const char* path, JavaVM* vm, jobject jobj) : BusObject(bus, path), vm(vm), jobj(jobj)
    {
        QStatus status;

        /* Add the chat interface to this object */
        const InterfaceDescription* chatIntf = bus.GetInterface(CHAT_SERVICE_INTERFACE_NAME);
        assert(chatIntf);
        AddInterface(*chatIntf);

        /* Store the Chat signal member away so it can be quickly looked up when signals are sent */
        chatSignalMember = chatIntf->GetMember("Chat");
        assert(chatSignalMember);

        /* Register signal handler */
        status =  bus.RegisterSignalHandler(this,
                                            static_cast<MessageReceiver::SignalHandler>(&ChatObject::ChatSignalHandler),
                                            chatSignalMember,
                                            NULL);

        if (ER_OK != status) {
            LOGE("Failed to register s_advertisedNamesignal handler for ChatObject::Chat (%s)", QCC_StatusText(status));
        }
    }

    /** Send a Chat signal */
    QStatus SendChatSignal(const char* msg) {
        MsgArg chatArg("s", msg);
        uint8_t flags = 0;
        return Signal(NULL, s_sessionId, *chatSignalMember, &chatArg, 1, 0, flags);
    }

    /** Receive a signal from another Chat client */
    void ChatSignalHandler(const InterfaceDescription::Member* member, const char* srcPath, Message& msg)
    {
        /* Inform Java GUI of this message */
        JNIEnv* env;
        vm->AttachCurrentThread(&env, NULL);
        jclass jcls = env->GetObjectClass(jobj);
        jmethodID mid = env->GetMethodID(jcls, "ChatCallback", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (mid == 0) {
            LOGE("Failed to get Java ChatCallback");
        } else {

            jstring jSender = env->NewStringUTF(msg->GetSender());
            jstring jChatStr = env->NewStringUTF(msg->GetArg(0)->v_string.str);
            env->CallVoidMethod(jobj, mid, jSender, jChatStr);
            env->DeleteLocalRef(jSender);
            env->DeleteLocalRef(jChatStr);
        }
    }


    void ObjectRegistered(void) {
        LOGD("\n Object registered \n");
    }

    /** Release the well-known name if it was acquired */
    void ReleaseName() {
        if (s_bus) {
            uint32_t disposition = 0;

            const ProxyBusObject& dbusObj = s_bus->GetDBusProxyObj();
            Message reply(*s_bus);
            MsgArg arg;
            arg.Set("s", s_advertisedName.c_str());
            QStatus status = dbusObj.MethodCall(org::freedesktop::DBus::InterfaceName,
                                                "ReleaseName",
                                                &arg,
                                                1,
                                                reply,
                                                5000);
            if (ER_OK == status) {
                disposition = reply->GetArg(0)->v_uint32;
            }
            if ((ER_OK != status) || (disposition != DBUS_RELEASE_NAME_REPLY_RELEASED)) {
                LOGE("Failed to release name %s (%s, disposition=%d)", s_advertisedName.c_str(), QCC_StatusText(status), disposition);
            }
        }
    }


  private:
    JavaVM* vm;
    jobject jobj;
    const InterfaceDescription::Member* chatSignalMember;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize AllJoyn and connect to local daemon.
 */
JNIEXPORT jint JNICALL Java_org_alljoyn_bus_samples_chat_Chat_jniOnCreate(JNIEnv* env, jobject jobj)
{


    /* Set AllJoyn logging */
    //QCC_SetLogLevels("ALLJOYN=7;ALL=1");
    QCC_UseOSLogging(true);

    /* Create message bus */
    s_bus = new BusAttachment("chat", true);
    QStatus status = ER_OK;
    const char* daemonAddr = "unix:abstract=alljoyn";

    /* Create org.alljoyn.bus.samples.chat interface */
    InterfaceDescription* chatIntf = NULL;
    status = s_bus->CreateInterface(CHAT_SERVICE_INTERFACE_NAME, chatIntf);
    if (ER_OK == status) {
        chatIntf->AddSignal("Chat", "s",  "str", 0);
        chatIntf->Activate();
    } else {
        LOGE("Failed to create interface \"%s\" (%s)", CHAT_SERVICE_INTERFACE_NAME, QCC_StatusText(status));
    }

    /* Start the msg bus */
    if (ER_OK == status) {
        status = s_bus->Start();
        if (ER_OK != status) {
            LOGE("BusAttachment::Start failed (%s)", QCC_StatusText(status));
        }
    }

    /* Connect to the daemon */
    if (ER_OK == status) {
        status = s_bus->Connect(daemonAddr);
        if (ER_OK != status) {
            LOGE("BusAttachment::Connect(\"%s\") failed (%s)", daemonAddr, QCC_StatusText(status));
        }
    }

    /* Create and register the bus object that will be used to send out signals */
    JavaVM* vm;
    env->GetJavaVM(&vm);
    s_chatObj = new ChatObject(*s_bus, CHAT_SERVICE_OBJECT_PATH, vm, jobj);
    s_bus->RegisterBusObject(*s_chatObj);
    LOGD("\n Bus Object created and registered \n");

    /* Register a bus listener in order to get discovery indications */
    if (ER_OK == status) {
        s_busListener = new MyBusListener(vm, jobj);
        s_bus->RegisterBusListener(*s_busListener);
    }


    if (NULL == s_bus) {
        return jboolean(false);
    }

    return (int) ER_OK;
}




JNIEXPORT jboolean JNICALL Java_org_alljoyn_bus_samples_chat_Chat_advertise(JNIEnv* env,
                                                                            jobject jobj,
                                                                            jstring advertiseStrObj)
{

    jboolean iscopy;

    const char* advertisedNameStr = env->GetStringUTFChars(advertiseStrObj, &iscopy);
    s_advertisedName = "";
    s_advertisedName += NAME_PREFIX;
    s_advertisedName += ".";
    s_advertisedName += advertisedNameStr;

    /* Request name */
    QStatus status = s_bus->RequestName(s_advertisedName.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE);
    if (ER_OK != status) {
        LOGE("RequestName(%s) failed (status=%s)\n", s_advertisedName.c_str(), QCC_StatusText(status));
    } else {
        LOGD("\n Request Name was successful");
    }

    /* Bind the session port*/
    SessionOpts opts(SessionOpts::TRAFFIC_MESSAGES, true, SessionOpts::PROXIMITY_ANY, TRANSPORT_ANY);
    if (ER_OK == status) {
        SessionPort sp = CHAT_PORT;
        status = s_bus->BindSessionPort(sp, opts, *s_busListener);
        if (ER_OK != status) {
            LOGE("BindSessionPort failed (%s)\n", QCC_StatusText(status));
        } else {
            LOGD("\n Bind Session Port to %u was successful \n", CHAT_PORT);
        }
    }

    /* Advertise the name */
    if (ER_OK == status) {
        status = s_bus->AdvertiseName(s_advertisedName.c_str(), opts.transports);
        if (status != ER_OK) {
            LOGD("Failed to advertise name %s (%s) \n", s_advertisedName.c_str(), QCC_StatusText(status));
        } else {
            LOGD("\n Name %s was successfully advertised", s_advertisedName.c_str());
        }
    }

    env->ReleaseStringUTFChars(advertiseStrObj, advertisedNameStr);
    return (jboolean) true;
}

JNIEXPORT jboolean JNICALL Java_org_alljoyn_bus_samples_chat_Chat_joinSession(JNIEnv* env,
                                                                              jobject jobj,
                                                                              jstring joinSessionObj) {

    LOGD("\n Inside Join session");

    jboolean iscopy;
    const char* sessionNameStr = env->GetStringUTFChars(joinSessionObj, &iscopy);
    sessionName = "";
    sessionName += NAME_PREFIX;
    sessionName += ".";
    sessionName += sessionNameStr;
    LOGD("\n Name of the session to be joined %s ", sessionName.c_str());

    /* Call joinSession method since we have the name */
    QStatus status = s_bus->FindAdvertisedName(sessionName.c_str());
    if (ER_OK != status) {
        LOGE("\n Error while calling FindAdvertisedName \n");
    }



/*
        while(!s_joinComplete){
                sleep(1);
        }
 */
    return (jboolean) true;
}


/**
 * Request the local daemon to disconnect from the remote daemon.
 */
JNIEXPORT jboolean JNICALL Java_org_alljoyn_bus_samples_chat_Chat_disconnect(JNIEnv* env,
                                                                             jobject jobj)
{
    if ((NULL == s_bus) || s_connectName.empty()) {
        return jboolean(false);
    }
    /* Send a disconnect message to the daemon */
    Message reply(*s_bus);
    const ProxyBusObject& alljoynObj = s_bus->GetAllJoynProxyObj();
    MsgArg disconnectArg("s", s_connectName.c_str());
    QStatus status = alljoynObj.MethodCall(org::alljoyn::Bus::InterfaceName, "Disconnect", &disconnectArg, 1, reply, 4000);
    if (ER_OK != status) {
        LOGE("%s.Disonnect(%s) failed", org::alljoyn::Bus::InterfaceName, s_connectName.c_str(), QCC_StatusText(status));
    }
    s_connectName.clear();
    return jboolean(ER_OK == status);
}

/**
 * Called when SimpleClient Java application exits. Performs AllJoyn cleanup.
 */
JNIEXPORT void JNICALL Java_org_alljoyn_bus_samples_chat_Chat_jniOnDestroy(JNIEnv* env, jobject jobj)
{
    /* Deallocate bus */
    if (NULL != s_bus) {
        delete s_bus;
        s_bus = NULL;
    }

    /* Deregister the ServiceObject. */
    if (s_chatObj) {
        s_chatObj->ReleaseName();
        //s_bus->DeregisterBusObject(*s_chatObj);
        delete s_chatObj;
        s_chatObj = NULL;
    }

    if (NULL != s_busListener) {
        delete s_busListener;
        s_busListener = NULL;
    }
}


/**
 * Send a broadcast ping message to all handlers registered for org.codeauora.alljoyn.samples.chat.Chat signal.
 */
JNIEXPORT jint JNICALL Java_org_alljoyn_bus_samples_chat_Chat_sendChatMsg(JNIEnv* env,
                                                                          jobject jobj,
                                                                          jstring chatMsgObj)
{
    /* Send a signal */
    jboolean iscopy;
    const char* chatMsg = env->GetStringUTFChars(chatMsgObj, &iscopy);
    QStatus status = s_chatObj->SendChatSignal(chatMsg);
    if (ER_OK != status) {
        LOGE("Chat", "Sending signal failed (%s)", QCC_StatusText(status));
    }
    env->ReleaseStringUTFChars(chatMsgObj, chatMsg);
    return (jint) status;
}


#ifdef __cplusplus
}

#endif
