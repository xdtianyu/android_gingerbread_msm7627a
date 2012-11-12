#ifndef CNE_EVENT_H
#define CNE_EVENT_H
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
#include "cne.h"

// Max number of fd's we watch at any one time.  Increase if necessary.
#define MAX_FD_EVENTS 128

typedef void (*cnd_event_cb)(int fd, void *userdata);
typedef int (*cneIntFnType)(void);
typedef void (*cneProcessCmdFnType)(int, int, void*,size_t);
typedef void (*cneRegMsgCbFnType)(cne_messageCbType);

struct cnd_event {
    struct cnd_event *next;
    struct cnd_event *prev;

    int fd;
    int index;
    int persist;
    cnd_event_cb func;
    void *param;
};

// Initialize internal data structs
int cnd_event_init();

// Initialize an event
void cnd_event_set(struct cnd_event * ev, int fd, int persist, cnd_event_cb func, void * param);

// Add event to watch list
void cnd_event_add(struct cnd_event * ev);


// Remove event from watch list
void cnd_event_del(struct cnd_event * ev);

// Event loop
void cnd_event_loop();


// Dump watch table for debugging
void cnd_dump_watch_table();

#endif /* CNE_EVENT_H */
