/*
**
** Copyright (C) 2011, The Android Open Source Project
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
/*
 *  tslib/src/ts_read_raw_module.c
 *
 *  Original version:
 *  Copyright (C) 2001 Russell King.
 *
 *  Rewritten for the Linux input device API:
 *  Copyright (C) 2002 Nicolas Pitre
 *
 * This file is placed under the LGPL.  Please see the file
 * COPYING for more details.
 *
 * $Id: input-raw.c,v 1.5 2005/02/26 01:47:23 kergoth Exp $
 *
 * Read raw pressure, x, y, and timestamp from a touchscreen device.
 */
#include "config.h"
#include <errno.h>
#include <stdio.h>

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/time.h>
#include <sys/types.h>

#include <linux/input.h>
#ifndef EV_SYN /* 2.4 kernel headers */
# define EV_SYN 0x00
#endif

#include "tslib-private.h"

#include <utils/Log.h>

struct tslib_input {
    struct tslib_module_info module;

    int current_x;
    int current_y;
    int current_p;

    int sane_fd;
    int using_syn;
};

static int check_fd(struct tslib_input *i)
{
    struct tsdev *ts = i->module.dev;
    int version;
    u_int32_t bit;
    u_int64_t absbit;

    if (! ((ioctl(ts->fd, EVIOCGVERSION, &version) >= 0) &&
        (version >= EV_VERSION) &&
        (ioctl(ts->fd, EVIOCGBIT(0, sizeof(bit) * 8), &bit) >= 0) &&
        (bit & (1 << EV_ABS)) &&
        (ioctl(ts->fd, EVIOCGBIT(EV_ABS, sizeof(absbit) * 8), &absbit) >= 0) &&
        (absbit & (1 << ABS_X)) &&
        (absbit & (1 << ABS_Y)) && (absbit & (1 << ABS_PRESSURE)))) {
        fprintf(stderr, "selected device is not a touchscreen I understand\n");
        return -1;
    }

    if (bit & (1 << EV_SYN))
        i->using_syn = 1;

    return 0;
}

//ts_input_read is equipped only to handle MAX_NUMBER_OF_EVENTS at a time.
static int ts_input_read(struct tslib_module_info *inf,
             struct ts_sample *samp, int nr)
{
    struct tslib_input *i = (struct tslib_input *)inf;
    struct tsdev *ts = inf->dev;
    int ret = nr;
    int total = 0;
    int eventsRead = 0;
    int eventsProcessed = 0;

    LOGV("Input-raw module read enter, samp->totalEvents = %d",samp->total_events);

    if (i->sane_fd == 0)
        i->sane_fd = check_fd(i);

    if (i->sane_fd == -1)
        return 0;

    if (i->using_syn) {
        struct input_event *ev;
        if(samp->total_events == MAX_NUMBER_OF_EVENTS){
            //essentially we have not received a SYN since MAX_NUMBER_OF_EVENTS
            //(never happens as of now),discarding the received events
            LOGE("Input-raw module, No SYN received since %d events.",
                    MAX_NUMBER_OF_EVENTS);
            samp->total_events = 0;
            return -1;
        }

        ret = read(ts->fd, &samp->ev[samp->total_events],
                sizeof(struct input_event) *
                (MAX_NUMBER_OF_EVENTS - samp->total_events));

        if (ret % (int)sizeof(struct input_event)) {
            //checking if we read the correct number of bytes.
            LOGE("Input-raw module, error during read call, ret val=%d,"
                    " errno=%d",ret,errno);
            samp->total_events = 0;
            return -1;
        }

        eventsRead = ret / (int)sizeof(struct input_event);
        LOGV("Input-raw, read %d events\n",eventsRead);

        while ((total < nr) && (eventsProcessed < eventsRead)) {
            ev = &(samp->ev[samp->total_events + eventsProcessed]);
            LOGV("Input-Raw: %d, got: t0=%d, t1=%d, type=%d, code=%d, v=%d",
                    (samp->total_events + eventsProcessed),
                    (int) ev->time.tv_sec,(int) ev->time.tv_usec, ev->type,
                    ev->code,ev->value);

            eventsProcessed++;

            switch (ev->type) {
            case EV_KEY:
                switch (ev->code) {
                case BTN_TOUCH:
                    if (ev->value == 0) {
                        /* pen up */
                        i->current_p = 0;
                    }
                    break;
                }
                break;

            case EV_SYN:
                /* Fill out a new complete event */
                samp->x = i->current_x;
                samp->y = i->current_y;
                //Added just for Android
                // If this SYN event is the one following
                // a KEY event keep pressure 0 Change
                // current to 255 so next event will have down action
                if (i->current_p == 0) {
                    samp->pressure = 0;
                    samp->x = 0;
                    samp->y = 0;
                    i->current_p = 255;
                }
                else {
                    samp->pressure = i->current_p;
                }
                samp->tv = ev->time;
                LOGV("Input-raw read -----> x = %d, y = %d, pressure = %d",
                        samp->x, samp->y, samp->pressure);
                samp->tsSampleReady = 1;
                total++;
                break;
            case EV_ABS:
                switch (ev->code) {
                case ABS_X:
                    i->current_x = ev->value;
                    break;
                case ABS_Y:
                    i->current_y = ev->value;
                    break;
                case ABS_PRESSURE:
                    i->current_p = ev->value;
                    break;
                }
                break;
            }
        } //end of while ((total < nr) && (eventsProcessed < eventsRead))
        ret = total;
        samp->total_events += eventsProcessed;
    } else {
        struct input_event ev;
        unsigned char *p = (unsigned char *) &ev;
        int len = sizeof(struct input_event);

        while (total < nr) {
            ret = read(ts->fd, p, len);
            if (ret == -1) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            if (ret < (int)sizeof(struct input_event)) {
                /* short read
                 * restart read to get the rest of the event
                 */
                p += ret;
                len -= ret;
                continue;
            }
            /* successful read of a whole event */

            if (ev.type == EV_ABS) {
                switch (ev.code) {
                case ABS_X:
                    if (ev.value != 0) {
                        samp->x = i->current_x = ev.value;
                        samp->y = i->current_y;
                        samp->pressure = i->current_p;
                    } else {
                        fprintf(stderr, "tslib: dropped x = 0\n");
                        continue;
                    }
                    break;
                case ABS_Y:
                    if (ev.value != 0) {
                        samp->x = i->current_x;
                        samp->y = i->current_y = ev.value;
                        samp->pressure = i->current_p;
                    } else {
                        fprintf(stderr, "tslib: dropped y = 0\n");
                        continue;
                    }
                    break;
                case ABS_PRESSURE:
                    samp->x = i->current_x;
                    samp->y = i->current_y;
                    samp->pressure = i->current_p = ev.value;
                    break;
                }
                samp->tv = ev.time;
    #ifdef DEBUG
                fprintf(stderr, "RAW---------------------------> %d %d %d\n",
                    samp->x, samp->y, samp->pressure);
    #endif   /*DEBUG*/
                LOGV("Input-raw read -----> x = %d, y = %d, \
                pressure = %d", samp->x, samp->y, samp->pressure);
                samp++;
                total++;
            } else if (ev.type == EV_KEY) {
                switch (ev.code) {
                case BTN_TOUCH:
                    if (ev.value == 0) {
                        /* pen up */
                        samp->x = 0;
                        samp->y = 0;
                        samp->pressure = 0;
                        samp->tv = ev.time;
                        samp++;
                        total++;
                    }
                    break;
                }
            } else {
                fprintf(stderr, "tslib: Unknown event type %d\n", ev.type);
            }
            p = (unsigned char *) &ev;
        }
        ret = total;
    }

    LOGV("Input-raw module read exit, retval=%d",ret);

    return ret;
}

static int ts_input_fini(struct tslib_module_info *inf)
{
    free(inf);
    return 0;
}

static const struct tslib_ops __ts_input_ops = {
    .read   = ts_input_read,
    .fini   = ts_input_fini,
};

TSAPI struct tslib_module_info *mod_init(struct tsdev *dev, const char *params)
{
    struct tslib_input *i;

    LOGV("mod_init of input raw");
    i = malloc(sizeof(struct tslib_input));
    if (i == NULL)
        return NULL;

    i->module.ops = &__ts_input_ops;
    i->current_x = 0;
    i->current_y = 0;
    i->current_p = 0;
    i->sane_fd = 0;
    i->using_syn = 0;
    return &(i->module);
}
