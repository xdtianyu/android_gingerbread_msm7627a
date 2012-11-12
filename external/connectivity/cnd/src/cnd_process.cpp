/*
** Copyright 2006, The Android Open Source Project
** Copyright (c) 2010, 2011 Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#define LOG_TAG "CND_PROCESS"
#define LOCAL_TAG "CND_PROCESS_DEBUG"
#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0

#include <cutils/sockets.h>
#include <cutils/jstring.h>
#include <cutils/record_stream.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <pthread.h>
#include <binder/Parcel.h>
#include <cutils/jstring.h>

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <alloca.h>
#include <sys/un.h>
#include <assert.h>
#include <netinet/in.h>
#include <cutils/properties.h>
#include <dirent.h>
#include <cnd_event.h>
#include <cnd.h>
#include <cne.h>
#include <cnd_iproute2.h>


namespace android {

#define SOCKET_NAME_CND "cnd"

// match with constant in .java
#define MAX_COMMAND_BYTES (8 * 1024)

// Basically: memset buffers that the client library
// shouldn't be using anymore in an attempt to find
// memory usage issues sooner.
#define MEMSET_FREED 1

#define NUM_ELEMS(a)     (sizeof (a) / sizeof (a)[0])

/* Constants for response types */
#define SOLICITED_RESPONSE 0
#define UNSOLICITED_MESSAGE 1

typedef struct {
    int commandNumber;
    void (*dispatchFunction) (Parcel &p, struct RequestInfo *pRI);
    int(*responseFunction) (Parcel &p, void *response, size_t responselen);
} CommandInfo;

typedef struct {
    int messageNumber;
    int (*responseFunction) (Parcel &p, void *response, size_t responselen);
} UnsolMessageInfo;

typedef struct RequestInfo {
    int32_t token;      //this is not CND_Token
    int fd;
    CommandInfo *pCI;
    struct RequestInfo *p_next;
    char cancelled;
} RequestInfo;


/*******************************************************************/

static int s_registerCalled = 0;

static pthread_t s_tid_dispatch;

static int s_started = 0;

static int s_fdListen = -1;

static struct cnd_event s_commands_event[MAX_FD_EVENTS];
static struct cnd_event s_listen_event;
static int command_index = 0;
static int cneServiceEnabled = 0;

static pthread_mutex_t s_pendingRequestsMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_writeMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_startupMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_startupCond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t s_dispatchMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_dispatchCond = PTHREAD_COND_INITIALIZER;

static RequestInfo *s_pendingRequests = NULL;

static RequestInfo *s_toDispatchHead = NULL;
static RequestInfo *s_toDispatchTail = NULL;


/*******************************************************************/
static void dispatchVoid (Parcel& p, RequestInfo *pRI);
static void dispatchString (Parcel& p, RequestInfo *pRI);
static void dispatchStrings (Parcel& p, RequestInfo *pRI);
static void dispatchInt (Parcel& p, RequestInfo *pRI);
static void dispatchInts (Parcel& p, RequestInfo *pRI);
static void dispatchWlanInfo(Parcel &p, RequestInfo *pRI);
static void dispatchWwanInfo(Parcel &p, RequestInfo *pRI);
static void dispatchWlanScanResults(Parcel &p, RequestInfo *pRI);
static void dispatchRaw(Parcel& p, RequestInfo *pRI);
static void dispatchRatStatus(Parcel &p, RequestInfo *pRI);
static void dispatchIproute2Cmd(Parcel &p, RequestInfo *pRI);
static int responseInts(Parcel &p, void *response, size_t responselen);
static int responseStrings(Parcel &p, void *response, size_t responselen);
static int responseString(Parcel &p, void *response, size_t responselen);
static int responseVoid(Parcel &p, void *response, size_t responselen);
static int responseRaw(Parcel &p, void *response, size_t responselen);
static int responseStartTrans(Parcel &p, void *response, size_t responselen);
static int sendResponseRaw (const void *data, size_t dataSize, int fdCommand);
static int sendResponse (Parcel &p, int fd);
static int rspCompatibleNws(Parcel &p, void *response, size_t responselen);
static int evtMorePrefNw(Parcel &p, void *response, size_t responselen);
static int eventRatChange (Parcel &p, void *response, size_t responselen);
static char *strdupReadString(Parcel &p);
static void writeStringToParcel(Parcel &p, const char *s);
static void memsetString (char *s);
static int writeData(int fd, const void *buffer, size_t len);
static void unsolicitedMessage(int unsolMessage, void *data, size_t datalen, int fd);
static void processCommand (int command, void *data, size_t datalen, CND_Token t);
static int processCommandBuffer(void *buffer, size_t buflen, int fd);
static void onCommandsSocketClosed(void);
static void processCommandsCallback(int fd, void *param);
static void listenCallback (int fd, void *param);
static void *eventLoop(void *param);
static int checkAndDequeueRequestInfo(struct RequestInfo *pRI);
static void cnd_commandComplete(CND_Token t, CND_Errno e, void *response, size_t responselen);

extern "C" const char * requestToString(int request);
extern "C" cneProcessCmdFnType cne_processCommand;
extern "C" cneRegMsgCbFnType cne_regMessageCb;

/** Index == commandNumber */
static CommandInfo s_commands[] = {
#include "cnd_commands.h"
};

static UnsolMessageInfo s_unsolMessages[] = {
#include "cnd_unsol_messages.h"
};

#define TEMP_BUFFER_SIZE		(80)


void cnd_sendUnsolicitedMsg(int targetFd, int msgType, int dataLen, void *data)
{
  int fd;

  CNE_LOGV ("cnd_sendUnsolicitedMsg: Fd=%d, msgType=%d, datalen=%d\n",
        targetFd, msgType, dataLen);

  //unsolicitedMessage(msgType, data, dataLen, fd);
  unsolicitedMessage(msgType, data, dataLen, targetFd);


}

static void
processCommand (int command, void *data, size_t datalen, CND_Token t)
{


  RequestInfo *reqInfo = (RequestInfo *)t;
  CNE_LOGV ("processCommand: command=%d, datalen=%d, fd=%d", command, datalen, reqInfo->fd);


  /* Special handling for iproute2 command to setup iproute2 table */
  if ((command == CNE_REQUEST_CONFIG_IPROUTE2_CMD) && (cneServiceEnabled))
  {

    int cmd = 0;
    unsigned char *ipAddr = NULL;
    unsigned char *gatewayAddr = NULL;
    unsigned char *ifName = NULL;

    if ((data == NULL) || (datalen ==0)) {
      CNE_LOGD ("processCommand: invalid data");
      return;
    }

    cmd = ((int *)data)[0];
    ifName = ((unsigned char **)data)[1];
    ipAddr = ((unsigned char **)data)[2];
    gatewayAddr = ((unsigned char **)data)[3];
 
    CNE_LOGV ("processCommand: iproute2cmd=%d, ipAddr=%s, gatewayAddr=%s, "
          "ifName=%s", cmd, ipAddr, gatewayAddr, ifName);

    cnd_iproute2* cnd_iproute2_ptr = cnd_iproute2::getInstance();
    if (cnd_iproute2_ptr != NULL) {
      // Call iproute2 API
      switch(cmd)
      {
      case CNE_IPROUTE2_ADD_ROUTING:
        cnd_iproute2::getInstance()->addRoutingTable(ifName, ipAddr, gatewayAddr);
        break;
      case CNE_IPROUTE2_DELETE_ROUTING:
      case CNE_IPROUTE2_DELETE_HOST_ROUTING:
        cnd_iproute2::getInstance()->deleteRoutingTable(ifName);
        break;
      case CNE_IPROUTE2_DELETE_DEFAULT_IN_MAIN:
      case CNE_IPROUTE2_DELETE_HOST_DEFAULT_IN_MAIN:
        cnd_iproute2::getInstance()->deleteDefaultEntryInMainTable(ifName);
        break;
      case CNE_IPROUTE2_REPLACE_DEFAULT_ENTRY_IN_MAIN:
      case CNE_IPROUTE2_REPLACE_HOST_DEFAULT_ENTRY_IN_MAIN:
        cnd_iproute2::getInstance()->replaceDefaultEntryInMainTable(ifName, gatewayAddr);
        break;
      case CNE_IPROUTE2_ADD_HOST_IN_MAIN:
        cnd_iproute2::getInstance()->addCustomEntryInMainTable(ipAddr, ifName, gatewayAddr);
        break;
      case CNE_IPROUTE2_DELETE_HOST_IN_MAIN:
        cnd_iproute2::getInstance()->deleteDeviceCustomEntriesInMainTable(ifName);
        break;
      default:
        CNE_LOGD ("processCommand: not iproute2 command=%d", command);
        break;
      }
    }

    return;

  }

  if(cne_processCommand != NULL)
  {
    cne_processCommand(reqInfo->fd, command, data, datalen);
  }
  else
  {
    CNE_LOGD("cne_processCommand is NULL");
  }
  cnd_commandComplete(t, CND_E_SUCCESS, NULL, 0);
  return;
}

static char *
strdupReadString(Parcel &p)
{
    size_t stringlen;
    const char16_t *s16;

    s16 = p.readString16Inplace(&stringlen);

    return strndup16to8(s16, stringlen);
}

static void writeStringToParcel(Parcel &p, const char *s)
{
    char16_t *s16;
    size_t s16_len;
    s16 = strdup8to16(s, &s16_len);
    p.writeString16(s16, s16_len);
    free(s16);
}

static void
memsetString (char *s)
{
    if (s != NULL) {
        memset (s, 0, strlen(s));
    }
}

/** Callee expects NULL */
static void
dispatchVoid (Parcel& p, RequestInfo *pRI)
{

    processCommand(pRI->pCI->commandNumber, NULL, 0, pRI);
}

/** Callee expects const char * */
static void
dispatchString (Parcel& p, RequestInfo *pRI)
{
    status_t status;
    size_t datalen;
    size_t stringlen;
    char *string8 = NULL;

    string8 = strdupReadString(p);

    CNE_LOGV ("dispatchString: strlen=%d", strlen(string8));
    processCommand(pRI->pCI->commandNumber, string8, strlen(string8), pRI);


#ifdef MEMSET_FREED
    memsetString(string8);
#endif

    free(string8);
    return;
}

/** Callee expects const char ** */
static void
dispatchStrings (Parcel &p, RequestInfo *pRI)
{
    int32_t countStrings;
    status_t status;
    size_t datalen;
    char **pStrings;

    status = p.readInt32 (&countStrings);

    if (status != NO_ERROR){
      CNE_LOGD ("dispatchStrings: invalid block");
      return;
    }

    if (countStrings == 0) {
        // just some non-null pointer
        pStrings = (char **)alloca(sizeof(char *));
        datalen = 0;
    } else if (((int)countStrings) == -1) {
        pStrings = NULL;
        datalen = 0;
    } else {
        datalen = sizeof(char *) * countStrings;

        pStrings = (char **)alloca(datalen);

        for (int i = 0 ; i < countStrings ; i++) {
            pStrings[i] = strdupReadString(p);

        }
    }

    processCommand(pRI->pCI->commandNumber, pStrings, datalen, pRI);

    if (pStrings != NULL) {
        for (int i = 0 ; i < countStrings ; i++) {
#ifdef MEMSET_FREED
            memsetString (pStrings[i]);
#endif
            free(pStrings[i]);
        }

#ifdef MEMSET_FREED
        memset(pStrings, 0, datalen);
#endif
    }

    return;
}

/** Callee expects const int * */
static void
dispatchInt (Parcel &p, RequestInfo *pRI)
{
    status_t status;
    int32_t value;

    status = p.readInt32 (&value);
    if (status != NO_ERROR) {
        CNE_LOGD ("dispatchInt: invalid block");
        return;
    }

    CNE_LOGV ("dispatchInt: int32_t=%d", value);

    processCommand(pRI->pCI->commandNumber, const_cast<int *>(&value), sizeof(int32_t), pRI);
}

/** Callee expects const int * */
static void
dispatchInts (Parcel &p, RequestInfo *pRI)
{
    int32_t count;
    status_t status;
    size_t datalen;
    int *pInts;

    status = p.readInt32 (&count);

    if (status != NO_ERROR || count == 0) {
        CNE_LOGD ("dispatchInts: invalid block");
        return;
    }

    datalen = sizeof(int) * count;
    pInts = (int *)alloca(datalen);

    if (pInts == NULL) {
        CNE_LOGW ("dispatchInts: alloc failed");
        return;
    }

    for (int i = 0 ; i < count ; i++) {
        int32_t t;

        status = p.readInt32(&t);
        pInts[i] = (int)t;

        if (status != NO_ERROR) {
            CNE_LOGD ("dispatchInts: invalid block");
            return;
        }
   }

   processCommand(pRI->pCI->commandNumber, const_cast<int *>(pInts),
                       datalen, pRI);

#ifdef MEMSET_FREED
    memset(pInts, 0, datalen);
#endif

    return;

}


static void
dispatchWlanInfo(Parcel &p, RequestInfo *pRI)
{
    int32_t t;
    status_t status;
    CneWlanInfoType args;

    memset(&args, 0, sizeof(args));

    status = p.readInt32 (&t);
    args.type = (int)t;
    status = p.readInt32 (&t);
    args.status = (int)t;
    status = p.readInt32 (&t);
    args.rssi = (int)t;
    args.ssid = strdupReadString(p);
    args.ipAddr = strdupReadString(p);
    args.iface = strdupReadString(p);
    args.timeStamp = strdupReadString(p);

    if (status != NO_ERROR){
      CNE_LOGD ("dispatchWlanInfo: invalid block");
      return;
    }


    CNE_LOGV ("dispatchWlanInfo: state=%d, rssi=%d, ssid=%s, ipAddr=%s, "
          "timeStamp=%s",
          args.status, args.rssi, args.ssid, args.ipAddr, args.timeStamp);

    processCommand(pRI->pCI->commandNumber, &args, sizeof(args), pRI);

    return;
}

static void
dispatchWlanScanResults(Parcel &p, RequestInfo *pRI)
{
    int32_t t;
    status_t status;
    CneWlanScanResultsType args;
    int32_t numItems;

    status = p.readInt32 (&t);

    if (status != NO_ERROR){
      CNE_LOGD ("dispatchWlanScanResults: invalid block");
      return;
    }

    args.numItems = (int)t;
    int max = (t < CNE_MAX_SCANLIST_SIZE)? t:CNE_MAX_SCANLIST_SIZE;

    for (int i = 0; i < max; i++)
    {
        args.numItems = max;
        status = p.readInt32 (&t);
        args.scanList[i].level = (int)t;
        status = p.readInt32 (&t);
        args.scanList[i].frequency = (int)t;
        args.scanList[i].ssid = strdupReadString(p);
        args.scanList[i].bssid = strdupReadString(p);
        args.scanList[i].capabilities = strdupReadString(p);

        if (status != NO_ERROR){
          CNE_LOGD ("dispatchWlanScanResults: invalid block");
          return;
        }
    }


    processCommand(pRI->pCI->commandNumber, &args, sizeof(args), pRI);

    return;
}

static void
dispatchWwanInfo(Parcel &p, RequestInfo *pRI)
{
    int32_t t;
    status_t status;
    CneWwanInfoType args;

    memset(&args, 0, sizeof(args));

    status = p.readInt32 (&t);
    args.type = (int)t;
    status = p.readInt32 (&t);
    args.status = (int)t;
    status = p.readInt32 (&t);
    args.rssi = (int)t;
    status = p.readInt32 (&t);
    args.roaming = (int)t;
    args.ipAddr = strdupReadString(p);
    args.iface = strdupReadString(p);
    args.timeStamp = strdupReadString(p);

    if (status != NO_ERROR){
      CNE_LOGD ("dispatchWwanInfo: invalid block");
      return;
    }

    CNE_LOGV ("dispatchWwanInfo: type=%d, state=%d, rssi=%d, roaming=%d, "
          "ipAddr=%s, timeStamp=%s", args.type, args.status, args.rssi,
          args.roaming, args.ipAddr, args.timeStamp);

    processCommand(pRI->pCI->commandNumber, &args, sizeof(args), pRI);

    return;
}

static void
dispatchRatStatus(Parcel &p, RequestInfo *pRI)
{
    int32_t t;
    status_t status;
    CneRatStatusType args;

    memset(&args, 0, sizeof(args));

    status = p.readInt32 (&t);
    args.rat = (cne_rat_type)t;
    status = p.readInt32 (&t);
    args.ratStatus = (cne_network_state_enum_type)t;
    args.ipAddr = strdupReadString(p);

    if (status != NO_ERROR){
        CNE_LOGD ("dispatchRatStatus: invalid block");
        return;
    }

    CNE_LOGV ("dispatchRatStatus: type=%d, ratStatus=%d, ipAddr=%s",
          args.rat, args.ratStatus, args.ipAddr);


    processCommand(pRI->pCI->commandNumber, &args, sizeof(args), pRI);

    return;
}

static void
dispatchIproute2Cmd(Parcel &p, RequestInfo *pRI)
{
    int32_t t;
    status_t status;
    CneIpRoute2CmdType args;

    memset(&args, 0, sizeof(args));

    status = p.readInt32 (&t);
    args.cmd = t;
    args.ifName = strdupReadString(p);
    args.ipAddr = strdupReadString(p);
    args.gatewayAddr = strdupReadString(p);

    if ((status != NO_ERROR) || (args.ifName == NULL)) {
        CNE_LOGD ("dispatchIproute2Cmd: invalid block");
        return;
    }


    CNE_LOGV ("dispatchIproute2Cmd: cmd=%d, ifName=%s, ipAddr=%s, gatewayAddr=%s",
          args.cmd, args.ifName, args.ipAddr, args.gatewayAddr);

    processCommand(pRI->pCI->commandNumber, &args, sizeof(args), pRI);

    if (args.ifName != NULL) {
      free(args.ifName);
    }
    if (args.ipAddr != NULL) {
      free(args.ipAddr);
    }
    if (args.gatewayAddr != NULL) {
      free(args.gatewayAddr);
    }
    return;
}

static void
dispatchRaw(Parcel &p, RequestInfo *pRI)
{
    int32_t len;
    status_t status;
    const void *data;

    status = p.readInt32(&len);

    if (status != NO_ERROR){
       CNE_LOGD ("dispatchRaw: invalid block");
       return;
   }

    // The java code writes -1 for null arrays
    if (((int)len) == -1) {
        data = NULL;
        len = 0;
    }

    data = p.readInplace(len);

    processCommand(pRI->pCI->commandNumber, const_cast<void *>(data), len, pRI);

    return;
}

static int
writeData(int fd, const void *buffer, size_t len)
{
    size_t writeOffset = 0;
    const uint8_t *toWrite;

    toWrite = (const uint8_t *)buffer;

    CNE_LOGV ("writeData: fd=%d, len=%d, offset=%d", fd, len, writeOffset);
    while (writeOffset < len) {
        CNE_LOGV ("writeData in loop: fd=%d, len=%d, offset=%d", fd, len, writeOffset);
        ssize_t written;
        do {
            written = write (fd, toWrite + writeOffset,
                                len - writeOffset);
        } while (written < 0 && errno == EINTR);

        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            CNE_LOGD ("writeData: unexpected error on write errno:%d", errno);
            close(fd);
            return -1;
        }
    }

    return 0;
}

static int
sendResponseRaw (const void *data, size_t dataSize, int fdCommand)
{
    int ret;
    uint32_t header;

    if (fdCommand < 0) {
        return -1;
    }

    if (dataSize > MAX_COMMAND_BYTES) {
        CNE_LOGD("sendResponseRaw: packet larger than %u (%u)",
                MAX_COMMAND_BYTES, (unsigned int )dataSize);

        return -1;
    }

    pthread_mutex_lock(&s_writeMutex);

    header = htonl(dataSize);


    CNE_LOGD("sendResponseRaw: fd=%d, datasize=%d, header=%d",
              fdCommand, dataSize, header);
    ret = writeData(fdCommand, (void *)&header, sizeof(header));

    if (ret < 0) {
        return ret;
    }

    writeData(fdCommand, data, dataSize);

    if (ret < 0) {
      pthread_mutex_unlock(&s_writeMutex);
      return ret;
    }

    pthread_mutex_unlock(&s_writeMutex);

    return 0;
}

static int
sendResponse (Parcel &p, int fd)
{

    return sendResponseRaw(p.data(), p.dataSize(), fd);
}

/** response is an int* pointing to an array of ints*/

static int
responseInts(Parcel &p, void *response, size_t responselen)
{
    int numInts;

    if (response == NULL && responselen != 0) {
        CNE_LOGD("invalid response: NULL");
        return CND_E_INVALID_RESPONSE;
    }
    if (responselen % sizeof(int) != 0) {
        CNE_LOGD("invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(int));
        return CND_E_INVALID_RESPONSE;
    }

    int *p_int = (int *) response;

    numInts = responselen / sizeof(int *);
    p.writeInt32 (numInts);

    /* each int*/
    for (int i = 0 ; i < numInts ; i++) {

        p.writeInt32(p_int[i]);
    }



    return 0;
}

/** response is a char **, pointing to an array of char *'s */
static int responseStrings(Parcel &p, void *response, size_t responselen)
{
    int numStrings;

    if (response == NULL && responselen != 0) {
        CNE_LOGD("invalid response: NULL");
        return CND_E_INVALID_RESPONSE;
    }
    if (responselen % sizeof(char *) != 0) {
        CNE_LOGD("invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(char *));
        return CND_E_INVALID_RESPONSE;
    }

    if (response == NULL) {
        p.writeInt32 (0);
    } else {
        char **p_cur = (char **) response;

        numStrings = responselen / sizeof(char *);
        p.writeInt32 (numStrings);

        /* each string*/
        for (int i = 0 ; i < numStrings ; i++) {

            writeStringToParcel (p, p_cur[i]);
        }

    }
    return 0;
}


/**
 * NULL strings are accepted
 * FIXME currently ignores responselen
 */
static int responseString(Parcel &p, void *response, size_t responselen)
{

    CNE_LOGV("responseString called");
   /* one string only */
    writeStringToParcel(p, (const char *)response);

    return 0;
}

static int responseVoid(Parcel &p, void *response, size_t responselen)
{
    return 0;
}

static int responseRaw(Parcel &p, void *response, size_t responselen)
{
    if (response == NULL && responselen != 0) {
        CNE_LOGD("invalid response: NULL with responselen != 0");
        return CND_E_INVALID_RESPONSE;
    }

    // The java code reads -1 size as null byte array
    if (response == NULL) {
        p.writeInt32(-1);
    } else {
        CNE_LOGD("responseRaw len=%d\n", responselen);
        p.writeInt32(responselen);
        p.write(response, responselen);
    }

    return 0;
}


static int rspCompatibleNws(Parcel &p, void *response, size_t responselen)
{
    if (response == NULL && responselen != 0)
    {
        CNE_LOGD("invalid response: NULL");
        return CND_E_INVALID_RESPONSE;
    }

    cne_get_compatible_nws_evt_rsp_data_type *p_cur =
      ((cne_get_compatible_nws_evt_rsp_data_type *) response);

    p.writeInt32((int)p_cur->reg_id);
    p.writeInt32((int)p_cur->is_success);
    p.writeInt32((int)p_cur->best_rat);
    for(int index = 0; index<CNE_RAT_MAX; index++)
    {
      p.writeInt32((int)p_cur->rat_pref_order[index]);
    }
    writeStringToParcel(p,p_cur->ip_addr);
    p.writeInt32((int)p_cur->fl_bw_est);
    p.writeInt32((int)p_cur->rl_bw_est);
    return 0;
}

static int evtMorePrefNw(Parcel &p, void *response, size_t responselen)
{
    if (response == NULL && responselen != 0)
    {
        CNE_LOGD("invalid response: NULL");
        return CND_E_INVALID_RESPONSE;
    }

    cne_pref_rat_avail_evt_data_type *p_cur =
      ((cne_pref_rat_avail_evt_data_type *) response);

    p.writeInt32((int)p_cur->reg_id);
    p.writeInt32((int)p_cur->rat);
    writeStringToParcel(p,p_cur->ip_addr);
    p.writeInt32((int)p_cur->fl_bw_est);
    p.writeInt32((int)p_cur->rl_bw_est);
    return 0;
}

static int eventRatChange(Parcel &p, void *response, size_t responselen)
{
    if (response == NULL && responselen != 0)
    {
        CNE_LOGD("invalid response: NULL");
        return CND_E_INVALID_RESPONSE;
    }

    CneRatInfoType *p_cur = ((CneRatInfoType *) response);
    p.writeInt32((int)p_cur->rat);

    if (p_cur->rat == CNE_RAT_WLAN)
    {
      writeStringToParcel (p, p_cur->wlan.ssid);
    }
    return 0;
}

static int
checkAndDequeueRequestInfo(struct RequestInfo *pRI)
{
    int ret = 0;

    if (pRI == NULL) {
        return 0;
    }

    pthread_mutex_lock(&s_pendingRequestsMutex);

    for(RequestInfo **ppCur = &s_pendingRequests
        ; *ppCur != NULL
        ; ppCur = &((*ppCur)->p_next)
    ) {
        if (pRI == *ppCur) {
            ret = 1;

            *ppCur = (*ppCur)->p_next;
            break;
        }
    }

    pthread_mutex_unlock(&s_pendingRequestsMutex);

    return ret;
}

static void onCommandsSocketClosed()
{
    int ret;
    RequestInfo *p_cur;

    /* mark pending requests as "cancelled" so we dont report responses */

    ret = pthread_mutex_lock(&s_pendingRequestsMutex);
    assert (ret == 0);

    p_cur = s_pendingRequests;

    for (p_cur = s_pendingRequests
            ; p_cur != NULL
            ; p_cur  = p_cur->p_next
    ) {
        p_cur->cancelled = 1;
    }

    ret = pthread_mutex_unlock(&s_pendingRequestsMutex);
    assert (ret == 0);
}

static void unsolicitedMessage(int unsolMessage, void *data, size_t datalen, int fd)
{
    int unsolMessageIndex;
    int ret;

    if (s_registerCalled == 0) {
        CNE_LOGD("unsolicitedMessage called before cnd_init ignored");
        return;
    }

    Parcel p;

    p.writeInt32 (UNSOLICITED_MESSAGE);
    p.writeInt32 (unsolMessage);

    ret = s_unsolMessages[unsolMessage]
                .responseFunction(p, data, datalen);

    if (ret != 0) {
        // Problem with the response. Don't continue;
        CNE_LOGD("unsolicitedMessage: problem with response");
	    return;
    }

    ret = sendResponse(p, fd);

    return;

}

static int
processCommandBuffer(void *buffer, size_t buflen, int fd)
{
    Parcel p;
    status_t status;
    int32_t request;
    int32_t token;
    RequestInfo *pRI;
    int ret;

    p.setData((uint8_t *) buffer, buflen);

    // status checked at end
    status = p.readInt32(&request);
    status = p.readInt32 (&token);

    CNE_LOGD("processCommandBuffer: fd=%d, requestcode=%d, token=%d",
         fd, request, token);

    if (status != NO_ERROR) {
        CNE_LOGD("processCommandBuffer: invalid request block");
        return -1;
    }

    if (request < 1 || request >= (int32_t)NUM_ELEMS(s_commands)) {
        CNE_LOGD("processCommandBuffer: unsupported request code %d token %d",
                  request, token);
        return -1;
    }

    pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));

    if (pRI == NULL) {
      CNE_LOGW("processCommandBuffer: calloc failed");
      return -1;
    }

    pRI->token = token;
    pRI->fd = fd;
    pRI->pCI = &(s_commands[request]);

    ret = pthread_mutex_lock(&s_pendingRequestsMutex);
    assert (ret == 0);

    pRI->p_next = s_pendingRequests;
    s_pendingRequests = pRI;

    ret = pthread_mutex_unlock(&s_pendingRequestsMutex);
    assert (ret == 0);

    pRI->pCI->dispatchFunction(p, pRI);

    return 0;
}

static void processCommandsCallback(int fd, void *param)
{
    RecordStream *p_rs;
    void *p_record;
    size_t recordlen;
    int ret;

    p_rs = (RecordStream *)param;

    for (;;) {
        /* loop until EAGAIN/EINTR, end of stream, or other error */
        ret = record_stream_get_next(p_rs, &p_record, &recordlen);
        CNE_LOGV ("processCommandsCallback: len=%d, ret=%d, fd=%d", recordlen, ret, fd);
        if (ret == 0 && p_record == NULL) {
	        CNE_LOGV ("processCommandsCallback: end of stream");
            break;
        } else if (ret < 0) {
            break;
        } else if (ret == 0) { /* && p_record != NULL */
            processCommandBuffer(p_record, recordlen, fd);

        }
    }
    CNE_LOGV ("processCommandsCallback: exit loop, ret=%d, errno=%d, fd=%d", 
              ret, errno, fd);
    if (ret == 0 || !(errno == EAGAIN || errno == EINTR || errno == EBADF)) {
        /* fatal error or end-of-stream */
        if (ret != 0) {
            CNE_LOGD("error on reading command socket errno:%d\n", errno);
        } else {
            CNE_LOGD("EOS.  Closing command socket.");
        }
        close(fd);
        /* remove from watch list */
        for(int i=0; i<MAX_FD_EVENTS; i++) {
            CNE_LOGD("processCommandsCallback: fd=%d, commandFd=%d",
                     fd, s_commands_event[i].fd);
            if (s_commands_event[i].fd == fd) {
                CNE_LOGD("processCommandsCallback: matched fd=%d for index=%d",
                         fd, i);
                cnd_event_del(&s_commands_event[i]);
                command_index--;
                break;
            }
        }
        record_stream_free(p_rs);
        /* notify CNE of the socket closed */
        cne_processCommand(fd, CNE_NOTIFY_SOCKET_CLOSED_CMD, NULL, 0);
        onCommandsSocketClosed();
    }

}

static void listenCallback (int fd, void *param)
{
    int ret;
    int err;
    RecordStream *p_rs;
    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof (peeraddr);
    struct ucred creds;
    socklen_t szCreds = sizeof(creds);
    int s_fdCommand;

    CNE_LOGD("listenCallback: fd=%d, s_fdListen=%d", fd, s_fdListen);
    assert (fd == s_fdListen);
    s_fdCommand = accept(s_fdListen, (sockaddr *) &peeraddr, &socklen);

    if (s_fdCommand < 0 ) {
      CNE_LOGD("Error on accept() errno:%d", errno);
      /* start listening for new connections again */
      cnd_event_add(&s_listen_event);
	  return;
    }

    errno = 0;
    err = getsockopt(s_fdCommand, SOL_SOCKET, SO_PEERCRED, &creds, &szCreds);
    ret = fcntl(s_fdCommand, F_SETFL, O_NONBLOCK);

    if (ret < 0) {
      CNE_LOGD("Error setting O_NONBLOCK errno = %d", errno);
    }

    CNE_LOGV("listenCallback: accept new connection, fd=%d", s_fdCommand);

    p_rs = record_stream_new(s_fdCommand, MAX_COMMAND_BYTES);

    // note: persistent = 1, not removed from table
    if (command_index >= MAX_FD_EVENTS)
    {
      CNE_LOGD ("Error: exceeding number of supported connection");
	  return;
    }

    CNE_LOGV("listenCallback: command_index=%d, fd=%d",
             command_index, s_fdCommand);

    cnd_event_set (&s_commands_event[command_index], s_fdCommand, 1,
                   processCommandsCallback, p_rs);
    cnd_event_add (&s_commands_event[command_index]);

    command_index++;

    return;

}


static void *
eventLoop(void *param)
{
    int ret;
    int filedes[2];

    CNE_LOGV ("eventLoop: s_started=%d", s_started);

    pthread_mutex_lock(&s_startupMutex);

    s_started = 1;
    pthread_cond_broadcast(&s_startupCond);

    pthread_mutex_unlock(&s_startupMutex);

    cnd_event_loop();


    return NULL;
}

extern "C" void
cnd_startEventLoop(void)
{
    int ret;
    pthread_attr_t attr;

    /* spin up eventLoop thread and wait for it to get started */
    s_started = 0;
    pthread_mutex_lock(&s_startupMutex);

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_dispatch, &attr, eventLoop, NULL);

    while (s_started == 0) {
        pthread_cond_wait(&s_startupCond, &s_startupMutex);
    }

    pthread_mutex_unlock(&s_startupMutex);

    if (ret < 0) {
        CNE_LOGD("Failed to create dispatch thread errno:%d", errno);
        return;
    }
}

extern "C" void
cnd_init (void)
{
    int ret;

    if (s_registerCalled > 0) {
        CNE_LOGD("cnd_init has been called more than once. "
                "Subsequent call ignored");
        return;
    }

    s_registerCalled = 1;
    cneServiceEnabled = cnd_event_init();
    if(cne_regMessageCb != NULL)
    {
      cne_regMessageCb(cnd_sendUnsolicitedMsg);
    }
    else
    {
      CNE_LOGD("cne_regMessageCb is NULL");
    }

    s_fdListen = android_get_control_socket(SOCKET_NAME_CND);
    if (s_fdListen < 0) {
        CNE_LOGD("Failed to get socket '" SOCKET_NAME_CND "'");
        exit(-1);
    }

    ret = listen(s_fdListen, 4);

    if (ret < 0) {
        CNE_LOGD("Failed to listen on control socket '%d': %s",
             s_fdListen, strerror(errno));
        exit(-1);
    }

    // persistent to accept multiple connections at same time
    cnd_event_set (&s_listen_event, s_fdListen, 1, listenCallback, NULL);
    cnd_event_add (&s_listen_event);


}


static void
cnd_commandComplete(CND_Token t, CND_Errno e, void *response, size_t responselen)
{
    RequestInfo *pRI;
    int ret;
    size_t errorOffset;

    pRI = (RequestInfo *)t;

    if (!checkAndDequeueRequestInfo(pRI)) {
        CNE_LOGD ("cnd_commandComplete: invalid Token");
        return;
    }

     if (pRI->cancelled == 0) {
        Parcel p;

        p.writeInt32 (SOLICITED_RESPONSE);
        p.writeInt32 (pRI->token);
        errorOffset = p.dataPosition();
        p.writeInt32 (e);

        if (e == CND_E_SUCCESS) {
            /* process response on success */
            ret = pRI->pCI->responseFunction(p, response, responselen);
            /* if an error occurred, rewind and mark it */
            if (ret != 0) {
                p.setDataPosition(errorOffset);
                p.writeInt32 (ret);
            }
        } else {
            CNE_LOGD ("cnd_commandComplete: Error");
        }

        if (pRI->fd < 0) {
            CNE_LOGD ("cnd_commandComplete: Command channel closed");
        }
        sendResponse(p, pRI->fd);
    }
}

} /* namespace android */


