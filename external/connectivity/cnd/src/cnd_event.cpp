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

#define LOG_TAG "CND_EVENT"
#define LOCAL_TAG "CND_EVENT_DEBUG"

#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <cnd_event.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <dlfcn.h>
#include "cutils/properties.h"
#include "cne.h"
#include "cnd.h"

static pthread_mutex_t listMutex;
#define MUTEX_ACQUIRE() pthread_mutex_lock(&listMutex)
#define MUTEX_RELEASE() pthread_mutex_unlock(&listMutex)
#define MUTEX_INIT() pthread_mutex_init(&listMutex, NULL)
#define MUTEX_DESTROY() pthread_mutex_destroy(&listMutex)

cneProcessCmdFnType cne_processCommand;
cneRegMsgCbFnType cne_regMessageCb;

static fd_set readFds;
static int nfds = 0;

static struct cnd_event * watch_table[MAX_FD_EVENTS];
static struct cnd_event pending_list;

static void init_list(struct cnd_event * list)
{
    memset(list, 0, sizeof(struct cnd_event));
    list->next = list;
    list->prev = list;
    list->fd = -1;
}

static void addToList(struct cnd_event * ev, struct cnd_event * list)
{
    ev->next = list;
    ev->prev = list->prev;
    ev->prev->next = ev;
    list->prev = ev;

}

static void removeFromList(struct cnd_event * ev)
{

    ev->next->prev = ev->prev;
    ev->prev->next = ev->next;
    ev->next = NULL;
    ev->prev = NULL;
}


static void removeWatch(struct cnd_event * ev, int index)
{

    CNE_LOGV("removeWatch: fd=%d, index=%d", ev->fd, index);

    watch_table[index] = NULL;
    ev->index = -1;

    FD_CLR(ev->fd, &readFds);

    if (ev->fd+1 == nfds) {
        int n = 0;

        for (int i = 0; i < MAX_FD_EVENTS; i++) {
            struct cnd_event * rev = watch_table[i];

            if ((rev != NULL) && (rev->fd > n)) {
                n = rev->fd;
            }
        }
        nfds = n + 1;

    }
}

static void processReadReadyEvent(fd_set * rfds, int n)
{

    CNE_LOGV("processReadReadyEvent: num of fd set=%d, rfds0=%ld", n, rfds->fds_bits[0]);

    MUTEX_ACQUIRE();

    for (int i = 0; (i < MAX_FD_EVENTS) && (n > 0); i++) {
        struct cnd_event * rev = watch_table[i];
        CNE_LOGD("processReadReadyEvent: watch_table index=%d", i);
        if (rev == NULL) {
            CNE_LOGD("processReadReadyEvent: REV is NULL at i=%d", i);
        }
        if (rev != NULL && FD_ISSET(rev->fd, rfds)) {
            addToList(rev, &pending_list);
            CNE_LOGV("processReadReadyEvent: add to pendingList at watch_table"
                     "index=%d, fd=%d", i, rev->fd);
            if (rev->persist == 0) {
                CNE_LOGV("processReadReadyEvent: Should not get here, fd=%d", rev->fd);
                 removeWatch(rev, i);
            }
            n--;
        }
    }

    MUTEX_RELEASE();

}

static void firePendingEvent(void)
{

    struct cnd_event * ev = pending_list.next;
    while (ev != &pending_list) {
        struct cnd_event * next = ev->next;
        removeFromList(ev);
        ev->func(ev->fd, ev->param);
        ev = next;
    }

}

// Initialize internal data structs
int cnd_event_init()
{

    MUTEX_INIT();
    FD_ZERO(&readFds);
    init_list(&pending_list);
    memset(watch_table, 0, sizeof(watch_table));

    void* cneLibHandle;
    cneIntFnType cne_svc_init = NULL;
    char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
    int len = property_get("persist.cne.UseCne", prop_value, "none");
    prop_value[len] = '\0';
    if(strcasecmp(prop_value, "vendor") == 0)
    {
      CNE_LOGV("loading vendor cne library");
      cneLibHandle = dlopen("/system/lib/libcne.so",RTLD_NOW);
    }
    else
    {
      CNE_LOGV("loading refcne library");
      cneLibHandle = dlopen("/system/lib/librefcne.so",RTLD_NOW);
    }

    if(cneLibHandle != NULL)
    {
      cne_svc_init = (cneIntFnType)dlsym(cneLibHandle, "cne_svc_init");
      cne_processCommand = (cneProcessCmdFnType)dlsym(cneLibHandle,
                                                      "cne_processCommand");
      cne_regMessageCb = (cneRegMsgCbFnType)dlsym(cneLibHandle,
                                                  "cne_regMessageCb");
    }
    else
    {
      CNE_LOGV("cne library load failed.");
    }
    if(cne_svc_init == NULL || cne_processCommand == NULL ||
         cne_regMessageCb == NULL)
    {
      CNE_LOGD("dlsym ret'd cne_svc_init=%x cne_processCommand=%x cne_regMessageCb=%x",
           (unsigned int)cne_svc_init,
           (unsigned int)cne_processCommand,
           (unsigned int)cne_regMessageCb);
    }
    else
    {
      return(cne_svc_init());
    }

    return CNE_SERVICE_DISABLED;
}

// Initialize an event
void cnd_event_set(struct cnd_event * ev, int fd, int persist, cnd_event_cb func, void * param)
{
    memset(ev, 0, sizeof(struct cnd_event));

    ev->fd = fd;
    ev->index = -1;
    ev->persist = persist;
    ev->func = func;
    ev->param = param;

    fcntl(fd, F_SETFL, O_NONBLOCK);

}

// Add event to watch list
void cnd_event_add(struct cnd_event * ev)
{

    CNE_LOGV("cnd_event_add-called:fd=%d, readFds0=%ld", ev->fd, readFds.fds_bits[0]);

    MUTEX_ACQUIRE();

    for (int i = 0; i < MAX_FD_EVENTS; i++) {
        if (watch_table[i] == NULL) {
            watch_table[i] = ev;
            ev->index = i;
            CNE_LOGV("cnd_event_add-before: add at index=%d for fd=%d, readFds0=%ld",
                     i, ev->fd, readFds.fds_bits[0]);
            FD_SET(ev->fd, &readFds);
            if (ev->fd >= nfds)
                nfds = ev->fd+1;
            CNE_LOGV("cnd_event_add-after: add at index=%d for fd=%d, readFds0=%ld",
                     i, ev->fd, readFds.fds_bits[0]);
            break;
        }
    }
    MUTEX_RELEASE();


}


// Remove event from watch or timer list
void cnd_event_del(struct cnd_event * ev)
{

    CNE_LOGV("cnd_event_del: index=%d", ev->index);
    MUTEX_ACQUIRE();

    if (ev->index < 0 || ev->index >= MAX_FD_EVENTS) {
        return;
    }

    removeWatch(ev, ev->index);

    MUTEX_RELEASE();

}


void cnd_dump_watch_table(void)
{
   struct cnd_event * ev;
   for (int i = 0; i < MAX_FD_EVENTS; i++) {
        if (watch_table[i] != NULL) {
            ev = watch_table[i];
            CNE_LOGV("cnd_dump_watch_table: at i=%d , fd=%d", i, ev->fd);
         }
   }

   return;
}

void cnd_event_loop(void)
{
    int n;
    fd_set rfds;
    int s_fdCommand;

    CNE_LOGV("cnd_event_loop: started, nfds=%d",nfds);

    for (;;) {
      // make local copy of read fd_set
      memcpy(&rfds, &readFds, sizeof(fd_set));

      CNE_LOGV("cnd_event_loop: waiting for select nfds=%d, rfds0=%ld", nfds, rfds.fds_bits[0]);

      n = select(nfds, &rfds, NULL, NULL, NULL);
      if (n < 0) {
        if (errno == EINTR)
          continue;
        CNE_LOGD("cnd_event_loop: select error (%d)", errno);
        return;
      }

      if (n == 0)
        CNE_LOGV("cnd_event_loop: select timedout");
      else if (n > 0)
        CNE_LOGV("cnd_event_loop: select ok,n=%d, rfds0=%ld",n, rfds.fds_bits[0]);

      // Check for read-ready events
      processReadReadyEvent(&rfds, n);
      // Fire pending event
      firePendingEvent();
    }
}
