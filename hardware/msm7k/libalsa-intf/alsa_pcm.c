/*
** Copyright 2010, The Android Open-Source Project
** Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define LOG_TAG "alsa_pcm"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <linux/ioctl.h>
#include "alsa_audio.h"

#define __force
#define __bitwise
#define __user

#define DEBUG 1

/* alsa parameter manipulation cruft */

#define PARAM_MAX SNDRV_PCM_HW_PARAM_LAST_INTERVAL
static int oops(struct pcm *pcm, int e, const char *fmt, ...);

static inline int param_is_mask(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline int param_is_interval(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static inline struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
    if (bit >= SNDRV_MASK_MAX)
        return;
    if (param_is_mask(n)) {
        struct snd_mask *m = param_to_mask(p, n);
        m->bits[0] = 0;
        m->bits[1] = 0;
        m->bits[bit >> 5] |= (1 << (bit & 31));
    }
}

void param_set_min(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
    }
}

void param_set_max(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->max = val;
    }
}

void param_set_int(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
        i->max = val;
        i->integer = 1;
    }
}

void param_init(struct snd_pcm_hw_params *p)
{
    int n;
    memset(p, 0, sizeof(*p));
    for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            m->bits[0] = ~0;
            m->bits[1] = ~0;
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
            struct snd_interval *i = param_to_interval(p, n);
            i->min = 0;
            i->max = ~0;
    }
}

/* debugging gunk */

#if DEBUG
static const char *param_name[PARAM_MAX+1] = {
    [SNDRV_PCM_HW_PARAM_ACCESS] = "access",
    [SNDRV_PCM_HW_PARAM_FORMAT] = "format",
    [SNDRV_PCM_HW_PARAM_SUBFORMAT] = "subformat",

    [SNDRV_PCM_HW_PARAM_SAMPLE_BITS] = "sample_bits",
    [SNDRV_PCM_HW_PARAM_FRAME_BITS] = "frame_bits",
    [SNDRV_PCM_HW_PARAM_CHANNELS] = "channels",
    [SNDRV_PCM_HW_PARAM_RATE] = "rate",
    [SNDRV_PCM_HW_PARAM_PERIOD_TIME] = "period_time",
    [SNDRV_PCM_HW_PARAM_PERIOD_SIZE] = "period_size",
    [SNDRV_PCM_HW_PARAM_PERIOD_BYTES] = "period_bytes",
    [SNDRV_PCM_HW_PARAM_PERIODS] = "periods",
    [SNDRV_PCM_HW_PARAM_BUFFER_TIME] = "buffer_time",
    [SNDRV_PCM_HW_PARAM_BUFFER_SIZE] = "buffer_size",
    [SNDRV_PCM_HW_PARAM_BUFFER_BYTES] = "buffer_bytes",
    [SNDRV_PCM_HW_PARAM_TICK_TIME] = "tick_time",
};

void param_dump(struct snd_pcm_hw_params *p)
{
    int n;

    for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            LOGV("%s = %08x%08x\n", param_name[n],
                   m->bits[1], m->bits[0]);
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
            struct snd_interval *i = param_to_interval(p, n);
            LOGV("%s = (%d,%d) omin=%d omax=%d int=%d empty=%d\n",
                   param_name[n], i->min, i->max, i->openmin,
                   i->openmax, i->integer, i->empty);
    }
    LOGV("info = %08x\n", p->info);
    LOGV("msbits = %d\n", p->msbits);
    LOGV("rate = %d/%d\n", p->rate_num, p->rate_den);
    LOGV("fifo = %d\n", (int) p->fifo_size);
}

static void info_dump(struct snd_pcm_info *info)
{
    LOGV("device = %d\n", info->device);
    LOGV("subdevice = %d\n", info->subdevice);
    LOGV("stream = %d\n", info->stream);
    LOGV("card = %d\n", info->card);
    LOGV("id = '%s'\n", info->id);
    LOGV("name = '%s'\n", info->name);
    LOGV("subname = '%s'\n", info->subname);
    LOGV("dev_class = %d\n", info->dev_class);
    LOGV("dev_subclass = %d\n", info->dev_subclass);
    LOGV("subdevices_count = %d\n", info->subdevices_count);
    LOGV("subdevices_avail = %d\n", info->subdevices_avail);
}
#else
void param_dump(struct snd_pcm_hw_params *p) {}
static void info_dump(struct snd_pcm_info *info) {}
#endif

int param_set_hw_refine(struct pcm *pcm, struct snd_pcm_hw_params *params)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_REFINE, params)) {
        LOGE("SNDRV_PCM_IOCTL_HW_REFINE failed");
        return -EPERM;
    }
    return 0;
}

int param_set_hw_params(struct pcm *pcm, struct snd_pcm_hw_params *params)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_PARAMS, params)) {
        return -EPERM;
    }
    pcm->hw_p = params;
    return 0;
}

int param_set_sw_params(struct pcm *pcm, struct snd_pcm_sw_params *sparams)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SW_PARAMS, sparams)) {
        return -EPERM;
    }
    pcm->sw_p = sparams;
    return 0;
}

int pcm_buffer_size(struct snd_pcm_hw_params *params)
{
    struct snd_interval *i = param_to_interval(params, SNDRV_PCM_HW_PARAM_BUFFER_BYTES);
            LOGV("%s = (%d,%d) omin=%d omax=%d int=%d empty=%d\n",
                   param_name[SNDRV_PCM_HW_PARAM_BUFFER_BYTES],
                   i->min, i->max, i->openmin,
                   i->openmax, i->integer, i->empty);
    return i->min;
}

int pcm_period_size(struct snd_pcm_hw_params *params)
{
    struct snd_interval *i = param_to_interval(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES);
            LOGV("%s = (%d,%d) omin=%d omax=%d int=%d empty=%d\n",
                   param_name[SNDRV_PCM_HW_PARAM_PERIOD_BYTES],
                   i->min, i->max, i->openmin,
                   i->openmax, i->integer, i->empty);
    return i->min;
}

const char* pcm_error(struct pcm *pcm)
{
    return pcm->error;
}

static int oops(struct pcm *pcm, int e, const char *fmt, ...)
{
    va_list ap;
    int sz;

    va_start(ap, fmt);
    vsnprintf(pcm->error, PCM_ERROR_MAX, fmt, ap);
    va_end(ap);
    sz = strlen(pcm->error);

    if (errno)
        snprintf(pcm->error + sz, PCM_ERROR_MAX - sz,
                 ": %s", strerror(e));
    return -1;
}

long pcm_avail(struct pcm *pcm)
{
     struct snd_pcm_sync_ptr *sync_ptr = pcm->sync_ptr;
     if (pcm->flags & DEBUG_ON) {
        LOGV("hw_ptr = %d buf_size = %d appl_ptr = %d\n",
                sync_ptr->s.status.hw_ptr,
                pcm->buffer_size,
                sync_ptr->c.control.appl_ptr);
     }
     if (pcm->flags & PCM_IN) {
          long avail = sync_ptr->s.status.hw_ptr - sync_ptr->c.control.appl_ptr;
        if (avail < 0)
                avail += pcm->sw_p->boundary;
        return avail;
     } else {
         long avail = sync_ptr->s.status.hw_ptr - sync_ptr->c.control.appl_ptr + ((pcm->flags & PCM_MONO) ? pcm->buffer_size/2 : pcm->buffer_size/4);
         if (avail < 0)
              avail += pcm->sw_p->boundary;
         else if ((unsigned long) avail >= pcm->sw_p->boundary)
              avail -= pcm->sw_p->boundary;
         return avail;
     }
}

int sync_ptr(struct pcm *pcm)
{
    int err;
    err = ioctl(pcm->fd, SNDRV_PCM_IOCTL_SYNC_PTR, pcm->sync_ptr);
    if (err < 0) {
        err = errno;
        LOGE("SNDRV_PCM_IOCTL_SYNC_PTR failed %d \n", err);
        return err;
    }

    return 0;
}

int mmap_buffer(struct pcm *pcm)
{
    int err, i;
    char *ptr;
    unsigned size;
    struct snd_pcm_channel_info ch;
    int channels = (pcm->flags & PCM_MONO) ? 1 : 2;

    size = pcm->buffer_size;
    if (pcm->flags & DEBUG_ON)
        LOGV("size = %d\n", size);
    pcm->addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED,
                           pcm->fd, 0);
    if (pcm->addr)
         return 0;
    else
         return -errno;
}

int mmap_transfer(struct pcm *pcm, void *data, unsigned offset,
                  long frames)
{
    struct snd_pcm_sync_ptr *sync_ptr = pcm->sync_ptr;
    unsigned long pcm_offset = 0;
    unsigned size;
    u_int8_t *dst_addr, *mmaped_addr;
    u_int8_t *src_addr = data;
    int channels = (pcm->flags & PCM_MONO) ? 1 : 2;
    unsigned int tmp = (pcm->flags & PCM_MONO) ? sync_ptr->c.control.appl_ptr*2 : sync_ptr->c.control.appl_ptr*4;

    pcm_offset = (tmp % (unsigned long)pcm->buffer_size);
    dst_addr = pcm->addr + pcm_offset;
    frames = frames * channels *2 ;

    while (frames-- > 0) {
        *(u_int8_t*)dst_addr = *(const u_int8_t*)src_addr;
         src_addr++;
         dst_addr++;
    }
    return 0;
}

int mmap_transfer_capture(struct pcm *pcm, void *data, unsigned offset,
                          long frames)
{
    struct snd_pcm_sync_ptr *sync_ptr = pcm->sync_ptr;
    unsigned long pcm_offset = 0;
    unsigned size;
    u_int8_t *dst_addr, *mmaped_addr;
    u_int8_t *src_addr;
    int channels = (pcm->flags & PCM_MONO) ? 1 : 2;
    unsigned int tmp = (pcm->flags & PCM_MONO) ? sync_ptr->c.control.appl_ptr*2 : sync_ptr->c.control.appl_ptr*4;

    pcm_offset = (tmp % (unsigned long)pcm->buffer_size);
    dst_addr = data;
    src_addr = pcm->addr + pcm_offset;
    frames = frames * channels *2 ;

    while (frames-- > 0) {
        *(u_int8_t*)dst_addr = *(const u_int8_t*)src_addr;
         src_addr++;
         dst_addr++;
    }
    return 0;
}

int pcm_prepare(struct pcm *pcm)
{
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE)) {
           LOGE("cannot prepare channel\n");
           return -errno;
    }
    pcm->running = 1;
    return 0;
}

static int pcm_write_mmap(struct pcm *pcm, void *data, unsigned count)
{
    struct snd_xferi x;
    long avail;
    long frames;
    int start;
    int err;
    struct pollfd *pfd;
    unsigned offset = 0;

            x.buf = data;
            x.frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);
            frames = x.frames;

        pfd = calloc(1, sizeof(struct pollfd));

        pfd[0].fd = pcm->fd;
        pfd[0].events = POLLOUT;
            for (;;) {
                if (!pcm->running) {
                    if (pcm_prepare(pcm))
                        return --errno;
                    pcm->running = 1;
                    start = 0;
                }
                pcm->sync_ptr->flags = SNDRV_PCM_SYNC_PTR_APPL | SNDRV_PCM_SYNC_PTR_AVAIL_MIN;//SNDRV_PCM_SYNC_PTR_HWSYNC;
                err = sync_ptr(pcm);
                if (err == EPIPE) {
                     LOGE("Aplay:Failed in sync_ptr\n");
                     /* we failed to make our window -- try to restart */
                     pcm->underruns++;
                     pcm->running = 0;
                     continue;
                }
                avail = pcm_avail(pcm);
                if (avail < 0)
                    return avail;
                if (avail < pcm->sw_p->avail_min) {
                    poll(pfd, 1, -1);
                    continue;
                }
                if (x.frames > avail)
                    frames = avail;
                    LOGE("Aplay:avail = %d frames = %d\n",avail, frames);
                    LOGE("Aplay:sync_ptr->s.status.hw_ptr %ld  pcm->buffer_size %d  sync_ptr->c.control.appl_ptr %ld\n",
                            pcm->sync_ptr->s.status.hw_ptr,
                            pcm->buffer_size,
                            pcm->sync_ptr->c.control.appl_ptr);
                err = mmap_transfer(pcm, data , offset, frames);
                if (err == EPIPE) {
                    LOGE("Aplay:Failed in mmap_transfer \n");
                    /* we failed to make our window -- try to restart */
                    pcm->underruns++;
                    pcm->running = 0;
                    continue;
                }
                x.frames -= frames;
                pcm->sync_ptr->c.control.appl_ptr += frames;
                pcm->sync_ptr->flags = 0;

                err = sync_ptr(pcm);
                if (err == EPIPE) {
                    LOGE("Aplay:Failed in sync_ptr 2 \n");
                    /* we failed to make our window -- try to restart */
                    pcm->underruns++;
                    pcm->running = 0;
                    continue;
                }
                    LOGE("Aplay:sync_ptr 2 \n");
                    LOGE("Aplay:sync_ptr->s.status.hw_ptr %ld  pcm->buffer_size %d  sync_ptr->c.control.appl_ptr %ld\n",
                            pcm->sync_ptr->s.status.hw_ptr,
                            pcm->buffer_size,
                            pcm->sync_ptr->c.control.appl_ptr);
                if (pcm->sync_ptr->c.control.appl_ptr >= pcm->sw_p->start_threshold) {
                    if(start)
                        goto xyz;
                    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START)) {
                        err = -errno;
                        if (errno == EPIPE) {
                            LOGE("Aplay:Failed in SNDRV_PCM_IOCTL_START\n");
                            /* we failed to make our window -- try to restart */
                            pcm->underruns++;
                            pcm->running = 0;
                            continue;
                        } else {
                            LOGE("Aplay:Error no %d \n", errno);
                            return -errno;
                        }
                    } else
                        start = 1;
                }
xyz:
                offset += frames;
                break;
            }
      return 0;
}

static int pcm_write_nmmap(struct pcm *pcm, void *data, unsigned count)
{
    struct snd_xferi x;

    if (pcm->flags & PCM_IN)
        return -EINVAL;
    x.buf = data;
    x.frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

    for (;;) {
        if (!pcm->running) {
            if (pcm_prepare(pcm))
                return -errno;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x)) {
            pcm->running = 0;
            if (errno == EPIPE) {
                    /* we failed to make our window -- try to restart */
                LOGE("Aplay:Underrun Error\n");
                pcm->underruns++;
                continue;
            }
            return errno;
        }
        if (pcm->flags & DEBUG_ON)
          LOGV("Sent frame\n");
        return 0;
    }
}

int pcm_write(struct pcm *pcm, void *data, unsigned count)
{
     if (pcm->flags & PCM_MMAP)
         return pcm_write_mmap(pcm, data, count);
     else
         return pcm_write_nmmap(pcm, data, count);
}

int pcm_read(struct pcm *pcm, void *data, unsigned count)
{
    struct snd_xferi x;

    if (!(pcm->flags & PCM_IN))
        return -EINVAL;

    x.buf = data;
    x.frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

    for (;;) {
        if (!pcm->running) {
            if (pcm_prepare(pcm))
                return -errno;
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START)) {
                LOGE("Arec:SNDRV_PCM_IOCTL_START failed\n");
                return -errno;
            }
            pcm->running = 1;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x)) {
            pcm->running = 0;
            if (errno == EPIPE) {
                /* we failed to make our window -- try to restart */
                LOGE("Arec:Overrun Error\n");
                pcm->underruns++;
                continue;
            }
            LOGE("Arec: error%d\n", errno);
            return -errno;
        }
        return 0;
    }
}

static struct pcm bad_pcm = {
    .fd = -1,
};

static int enable_timer(struct pcm *pcm) {

    pcm->timer_fd = open("/dev/snd/timer", O_RDWR | O_NONBLOCK);
    if (pcm->timer_fd < 0) {
       close(pcm->fd);
       LOGE("cannot open timer device 'timer'");
       return &bad_pcm;
    }
    int arg = 1;
    struct snd_timer_params timer_param;
    struct snd_timer_select sel;
    if (ioctl(pcm->timer_fd, SNDRV_TIMER_IOCTL_TREAD, &arg) < 0) {
           LOGE("extended read is not supported (SNDRV_TIMER_IOCTL_TREAD)\n");
    }
    memset(&sel, 0, sizeof(sel));
    sel.id.dev_class = SNDRV_TIMER_CLASS_PCM;
    sel.id.dev_sclass = SNDRV_TIMER_SCLASS_NONE;
    sel.id.card = pcm->card_no;
    sel.id.device = pcm->device_no;
    if (pcm->flags & PCM_IN)
        sel.id.subdevice = 1;
    else
        sel.id.subdevice = 0;

    if (pcm->flags & DEBUG_ON) {
        LOGE("sel.id.dev_class= %d\n", sel.id.dev_class);
        LOGE("sel.id.dev_sclass = %d\n", sel.id.dev_sclass);
        LOGE("sel.id.card = %d\n", sel.id.card);
        LOGE("sel.id.device = %d\n", sel.id.device);
        LOGE("sel.id.subdevice = %d\n", sel.id.subdevice);
    }
    if (ioctl(pcm->timer_fd, SNDRV_TIMER_IOCTL_SELECT, &sel) < 0) {
          LOGE("SNDRV_TIMER_IOCTL_SELECT failed.\n");
          close(pcm->timer_fd);
          close(pcm->fd);
          return &bad_pcm;
    }
    memset(&timer_param, 0, sizeof(struct snd_timer_params));
    timer_param.flags |= SNDRV_TIMER_PSFLG_AUTO;
    timer_param.ticks = 1;
    timer_param.filter = (1<<SNDRV_TIMER_EVENT_MSUSPEND) | (1<<SNDRV_TIMER_EVENT_MRESUME) | (1<<SNDRV_TIMER_EVENT_TICK);

    if (ioctl(pcm->timer_fd, SNDRV_TIMER_IOCTL_PARAMS, &timer_param)< 0) {
           LOGE("SNDRV_TIMER_IOCTL_PARAMS failed\n");
    }
    if (ioctl(pcm->timer_fd, SNDRV_TIMER_IOCTL_START) < 0) {
           close(pcm->timer_fd);
           LOGE("SNDRV_TIMER_IOCTL_START failed\n");
    }
    return 0;
}

static int disable_timer(struct pcm *pcm) {
     if (pcm == &bad_pcm)
         return 0;
     if (ioctl(pcm->timer_fd, SNDRV_TIMER_IOCTL_STOP) < 0)
         LOGE("SNDRV_TIMER_IOCTL_STOP failed\n");
     return close(pcm->timer_fd);
}

int pcm_close(struct pcm *pcm)
{
    if (pcm == &bad_pcm)
        return 0;

    if (pcm->flags & PCM_MMAP) {
        disable_timer(pcm);
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_DROP) < 0) {
            LOGE("Drop failed");
        }

        if (munmap(pcm->addr, pcm->buffer_size))
            LOGE("munmap failed");
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_FREE) < 0) {
            LOGE("HW_FREE failed");
        }
    }

    if (pcm->fd >= 0)
        close(pcm->fd);
    pcm->running = 0;
    pcm->buffer_size = 0;
    pcm->fd = -1;
    if (pcm->sw_p)
        free(pcm->sw_p);
    if (pcm->hw_p)
        free(pcm->hw_p);
    free(pcm);
    return 0;
}

struct pcm *pcm_open(unsigned flags, char *device)
{
    char dname[17];
    struct pcm *pcm;
    struct snd_pcm_info info;
    struct snd_pcm_hw_params params;
    struct snd_pcm_sw_params sparams;
    unsigned period_sz;
    unsigned period_cnt;
    char *tmp;

    if (flags & DEBUG_ON) {
        LOGV("pcm_open(0x%08x)",flags);
        LOGV("device %s\n",device);
    }

    pcm = calloc(1, sizeof(struct pcm));
    if (!pcm)
        return &bad_pcm;

    tmp = device+4;
    if ((strncmp(device, "hw:",3) != 0) || (strncmp(tmp, ",",1) != 0)){
        LOGE("Wrong device fromat\n");
        return -EINVAL;
    }

    if (flags & PCM_IN) {
        strcpy(dname, "/dev/snd/pcmC");
        tmp = device+3;
        strncat(dname, tmp,1) ;
        pcm->card_no = atoi(tmp);
        strcat(dname, "D");
        tmp = device+5;
        strncat(dname, tmp,1) ;
        pcm->device_no = atoi(tmp);
        strcat(dname, "c");
    } else {
        strcpy(dname, "/dev/snd/pcmC");
        tmp = device+3;
        strncat(dname, tmp,1) ;
        pcm->card_no = atoi(tmp);
        strcat(dname, "D");
        tmp = device+5;
        strncat(dname, tmp,1) ;
        pcm->device_no = atoi(tmp);
        strcat(dname, "p");
    }
    if (pcm->flags & DEBUG_ON)
        LOGV("Device name %s\n", dname);

    pcm->sync_ptr = calloc(1, sizeof(struct snd_pcm_sync_ptr));
    if (!pcm->sync_ptr) {
         free(pcm);
         return &bad_pcm;
    }
    pcm->flags = flags;

    pcm->fd = open(dname, O_RDWR);
    if (pcm->fd < 0) {
        LOGE("cannot open device '%s'", dname);
        return &bad_pcm;
    }
    if (pcm->flags & PCM_MMAP)
        enable_timer(pcm);

    if (pcm->flags & DEBUG_ON)
        LOGV("pcm_open() %s\n", dname);
    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_INFO, &info)) {
        LOGE("cannot get info - %s", dname);
    }
    if (pcm->flags & DEBUG_ON)
       info_dump(&info);

    return pcm;
}

int pcm_ready(struct pcm *pcm)
{
    return pcm->fd >= 0;
}
