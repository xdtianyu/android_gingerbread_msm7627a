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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include <sound/asound.h>
#include "alsa_audio.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1
#define LOG_NDEBUG 1
static pcm_flag = 1;
static debug = 0;

static struct option long_options[] =
{
    {"pcm", 0, 0, 'P'},
    {"debug", 0, 0, 'V'},
    {"Mmap", 0, 0, 'M'},
    {"HW", 1, 0, 'D'},
    {"Rate", 1, 0, 'R'},
    {"channel", 1, 0, 'C'},
    {0, 0, 0, 0}
};

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;       /* sample_rate * num_channels * bps / 8 */
    uint16_t block_align;     /* num_channels * bps / 8 */
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

static int set_params(struct pcm *pcm)
{
     struct snd_pcm_hw_params *params;
     struct snd_pcm_sw_params *sparams;

     unsigned long periodSize, bufferSize, reqBuffSize;
     unsigned int periodTime, bufferTime;
     unsigned int requestedRate = pcm->rate;

     params = (struct snd_pcm_hw_params*) calloc(1, sizeof(struct snd_pcm_hw_params));
     if (!params) {
          fprintf(stderr, "Aplay:Failed to allocate ALSA hardware parameters!");
          return -ENOMEM;
     }

     param_init(params);

     param_set_mask(params, SNDRV_PCM_HW_PARAM_ACCESS,
                    (pcm->flags & PCM_MMAP)? SNDRV_PCM_ACCESS_MMAP_INTERLEAVED : SNDRV_PCM_ACCESS_RW_INTERLEAVED);
     param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
                    SNDRV_PCM_FORMAT_S16_LE);
     param_set_mask(params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                    SNDRV_PCM_SUBFORMAT_STD);
     param_set_min(params, SNDRV_PCM_HW_PARAM_PERIOD_TIME, 1000);
     param_set_int(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
     param_set_int(params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                    pcm->channels - 1 ? 32 : 16);
     param_set_int(params, SNDRV_PCM_HW_PARAM_CHANNELS,
                    pcm->channels);
     param_set_int(params, SNDRV_PCM_HW_PARAM_RATE, pcm->rate);
     param_set_hw_refine(pcm, params);

     if (param_set_hw_params(pcm, params)) {
         fprintf(stderr, "Aplay:cannot set hw params");
         return -errno;
     }
     if (debug)
         param_dump(params);

     pcm->buffer_size = pcm_buffer_size(params);
     pcm->period_size = pcm_period_size(params);
     pcm->period_cnt = pcm->buffer_size/pcm->period_size;
     if (debug) {
        fprintf (stderr,"period_cnt = %d\n", pcm->period_cnt);
        fprintf (stderr,"period_size = %d\n", pcm->period_size);
        fprintf (stderr,"buffer_size = %d\n", pcm->buffer_size);
     }
     sparams = (struct snd_pcm_sw_params*) calloc(1, sizeof(struct snd_pcm_sw_params));
     if (!sparams) {
         fprintf(stderr, "Aplay:Failed to allocate ALSA software parameters!\n");
         return -ENOMEM;
     }
     // Get the current software parameters
    sparams->tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sparams->period_step = 1;
    sparams->avail_min = (pcm->flags & PCM_MONO) ? pcm->period_size/2 : pcm->period_size/4;
    /* start after at least two periods are prefilled */
    sparams->start_threshold = (pcm->flags & PCM_MONO) ? pcm->period_size : pcm->period_size/2;
    sparams->stop_threshold = (pcm->flags & PCM_MONO) ? pcm->buffer_size/2 : pcm->buffer_size/4;
    sparams->xfer_align = (pcm->flags & PCM_MONO) ? pcm->period_size/2 : pcm->period_size/4; /* needed for old kernels */
    sparams->silence_size = 0;
    sparams->silence_threshold = 0;

    if (param_set_sw_params(pcm, sparams)) {
        fprintf(stderr, "Aplay:cannot set sw params");
        return -errno;
    }
    if (debug) {
       fprintf (stderr,"sparams->avail_min= %d\n", sparams->avail_min);
       fprintf (stderr," sparams->start_threshold= %d\n", sparams->start_threshold);
       fprintf (stderr," sparams->stop_threshold= %d\n", sparams->stop_threshold);
       fprintf (stderr," sparams->xfer_align= %d\n", sparams->xfer_align);
       fprintf (stderr," sparams->boundary= %d\n", sparams->boundary);
    }
    return 0;
}

int play_file(unsigned rate, unsigned channels, int fd, unsigned count,
              unsigned flags, const char *device)
{
    struct pcm *pcm;
    struct mixer *mixer;
    struct pcm_ctl *ctl = NULL;
    unsigned bufsize;
    char *data;
    long avail;
    long frames;
    int nfds = 1;
    struct snd_xferi x;
    unsigned offset = 0;
    int err;
    static int start = 0;
    struct pollfd pfd[1];

    flags |= PCM_OUT;

    if (channels == 1)
        flags |= PCM_MONO;
    else
        flags |= PCM_STEREO;

    if (debug)
        flags |= DEBUG_ON;
    else
        flags |= DEBUG_OFF;

    pcm = pcm_open(flags, device);
    if (pcm < 0)
        return pcm;

    if (!pcm_ready(pcm)) {
        pcm_close(pcm);
        return -EBADFD;
    }
    pcm->channels = channels;
    pcm->rate = rate;
    pcm->flags = flags;
    if (set_params(pcm)) {
        fprintf(stderr, "Aplay:params setting failed\n");
        pcm_close(pcm);
        return -errno;
    }

    if (!pcm_flag) {
       if (pcm_prepare(pcm)) {
          fprintf(stderr, "Aplay:Failed in pcm_prepare\n");
          pcm_close(pcm);
          return -errno;
       }
        while(1);
    }

    if (flags & PCM_MMAP) {
        if (mmap_buffer(pcm)) {
             fprintf(stderr, "Aplay:params setting failed\n");
             pcm_close(pcm);
             return -errno;
        }
       if (pcm_prepare(pcm)) {
          fprintf(stderr, "Aplay:Failed in pcm_prepare\n");
          pcm_close(pcm);
          return -errno;
       }
        bufsize = pcm->period_size;
        if (debug)
          fprintf(stderr, "Aplay:bufsize = %d\n", bufsize);
        data = calloc(1, bufsize);
        if (!data) {
           fprintf(stderr, "Aplay:could not allocate %d bytes\n", count);
           pcm_close(pcm);
           return -ENOMEM;
        }

        pfd[0].fd = pcm->fd;
        pfd[0].events = POLLOUT;
        while (read(fd, data, bufsize) == bufsize) {
            x.buf = data;
            x.frames = (pcm->flags & PCM_MONO) ? (bufsize / 2) : (bufsize / 4);
            frames = x.frames;

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
                     fprintf(stderr, "Aplay:Failed in sync_ptr \n");
                     /* we failed to make our window -- try to restart */
                     pcm->underruns++;
                     pcm->running = 0;
                     continue;
                }
                avail = pcm_avail(pcm);
                if (debug)
                    fprintf(stderr, "Aplay:avail 1 = %d frames = %d\n",avail, frames);
                if (avail < 0)
                    return avail;
                if (avail < pcm->sw_p->avail_min) {
                    poll(pfd, nfds, TIMEOUT_INFINITE);
                    continue;
                }
                if (x.frames > avail)
                    frames = avail;
                if (debug) {
                    fprintf(stderr, "Aplay:avail = %d frames = %d\n",avail, frames);
                    fprintf(stderr, "Aplay:sync_ptr->s.status.hw_ptr %ld  pcm->buffer_size %d  sync_ptr->c.control.appl_ptr %ld\n",
                            pcm->sync_ptr->s.status.hw_ptr,
                            pcm->buffer_size,
                            pcm->sync_ptr->c.control.appl_ptr);
                }
                err = mmap_transfer(pcm, data , offset, frames);
                if (err == EPIPE) {
                    fprintf(stderr, "Aplay:Failed in mmap_transfer \n");
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
                    fprintf(stderr, "Aplay:Failed in sync_ptr 2 \n");
                    /* we failed to make our window -- try to restart */
                    pcm->underruns++;
                    pcm->running = 0;
                    continue;
                }
                if (debug) {
                    fprintf(stderr, "Aplay:sync_ptr 2 \n");
                    fprintf(stderr, "Aplay:sync_ptr->s.status.hw_ptr %ld  pcm->buffer_size %d  sync_ptr->c.control.appl_ptr %ld\n",
                            pcm->sync_ptr->s.status.hw_ptr,
                            pcm->buffer_size,
                            pcm->sync_ptr->c.control.appl_ptr);
                }
                if (pcm->sync_ptr->c.control.appl_ptr >= pcm->sw_p->start_threshold) {
                    if(start)
                        goto xyz;
                    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START)) {
                        err = -errno;
                        if (errno == EPIPE) {
                            fprintf(stderr, "Aplay:Failed in SNDRV_PCM_IOCTL_START\n");
                            /* we failed to make our window -- try to restart */
                            pcm->underruns++;
                            pcm->running = 0;
                            continue;
                        } else {
                            fprintf(stderr, "Aplay:Error no %d \n", errno);
                            return -errno;
                        }
                    } else
                        start = 1;
                }
xyz:
                offset += frames;
                break;
            }
        }
    } else {
       if (pcm_prepare(pcm)) {
          fprintf(stderr, "Aplay:Failed in pcm_prepare\n");
          pcm_close(pcm);
          return -errno;
       }
        bufsize = pcm->period_size;
        if (debug)
          fprintf(stderr, "Aplay:bufsize = %d\n", bufsize);
        data = calloc(1, bufsize);
        if (!data) {
            fprintf(stderr, "Aplay:could not allocate %d bytes\n", count);
            pcm_close(pcm);
            return -ENOMEM;
        }

        while (read(fd, data, bufsize) == bufsize) {
            if (pcm_write(pcm, data, bufsize)){
                fprintf(stderr, "Aplay: pcm_write failed\n");
                break;
            }
        }
    }
    fprintf(stderr, "Aplay: Done playing\n");
    free(data);
    pcm_close(pcm);
    return 0;
}

int play_wav(const char *fg, int rate, int ch, const char *device, const char *fn)
{
    struct wav_header hdr;
    int fd;
    unsigned flag = 0;

    if (pcm_flag) {
        if(!fn) {
            fd = fileno(stdin);
        } else {
            fd = open(fn, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Aplay:aplay: cannot open '%s'\n", fn);
                return fd;
            }
        }
        if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
            fprintf(stderr, "Aplay:aplay: cannot read header\n");
            return -errno;
        }
        if (debug)
            fprintf(stderr, "Aplay:aplay: %d ch, %d hz, %d bit, %s\n",
                hdr.num_channels, hdr.sample_rate, hdr.bits_per_sample,
                hdr.audio_format == FORMAT_PCM ? "PCM" : "unknown");

        if ((hdr.riff_id != ID_RIFF) ||
            (hdr.riff_fmt != ID_WAVE) ||
            (hdr.fmt_id != ID_FMT)) {
            fprintf(stderr, "Aplay:aplay: '%s' is not a riff/wave file\n", fn);
            return -EINVAL;
        }
        if ((hdr.audio_format != FORMAT_PCM) ||
            (hdr.fmt_sz != 16)) {
            fprintf(stderr, "Aplay:aplay: '%s' is not pcm format\n", fn);
            return -EINVAL;
        }
        if (hdr.bits_per_sample != 16) {
            fprintf(stderr, "Aplay:aplay: '%s' is not 16bit per sample\n", fn);
            return -EINVAL;
        }
    } else {
        fd = -EBADFD;
        hdr.sample_rate = rate;
        hdr.num_channels = ch;
    }
    if (!strcmp(fg, "M")) {
        fprintf(stderr, "Aplay:aplay: '%s' is playing in MMAP as input is %s\n", fn, fg);
        flag = PCM_MMAP;
    } else if (!strcmp(fg, "N")) {
        fprintf(stderr, "Aplay:aplay: '%s' is playing in non-MMAP as input is %s\n", fn, fg);
        flag = PCM_NMMAP;
    }
    return play_file(hdr.sample_rate, hdr.num_channels, fd, hdr.data_sz, flag, device);
}

int main(int argc, char **argv)
{
    int option_index = 0;
    int c;
    int ch = 2;
    int rate = 44100;
    char *mmap = "N";
    char *device = "hw:0,0";
    char *filename;

    if (argc <2) {
          printf("Usage: aplay [options] <file>\n"
                "options:\n"
                "-D <hw:C,D>	-- Alsa PCM by name\n"
                "-M		-- Mmap stream\n"
                "-P		-- Hostless steam[No PCM]\n"
		"-C             -- Channels\n"
		"-R             -- Rate\n"
                "-V		-- verbose\n"
                "<file> \n");
                return 0;
     }
     while ((c = getopt_long(argc, argv, "PVMD:R:C:", long_options, &option_index)) != -1) {
       switch (c) {
       case 'P':
          pcm_flag = 0;
          break;
       case 'V':
          debug = 1;
          break;
       case 'M':
          mmap = "M";
          break;
       case 'D':
          device = optarg;
          break;
       case 'R':
          rate = (int)strtol(optarg, NULL, 0);
          break;
       case 'C':
          ch = (int)strtol(optarg, NULL, 0);
          break;
       default:
          printf("Usage: aplay [options] <file>\n"
                "options:\n"
                "-D <hw:C,D>	-- Alsa PCM by name\n"
                "-M		-- Mmap stream\n"
                "-P		-- Hostless steam[No PCM]\n"
                "-V		-- verbose\n"
                "-C		-- Channels\n"
		"-R             -- Rate\n"
                "<file> \n");
          return -EINVAL;
       }

    }
    filename = (char*) calloc(1, 30);
    if (!filename) {
          fprintf(stderr, "Aplay:Failed to allocate filename!");
          return -ENOMEM;
    }
    if (optind > argc - 1) {
       filename = NULL;
    } else {
       strcpy(filename, argv[optind++]);
    }

    if (pcm_flag) {
        play_wav(mmap, rate, ch, device, filename);
    } else {
        play_wav(mmap, rate, ch, device, "dummy");
    }
    free(filename);
    return 0;
}

